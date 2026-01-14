#include "CameraCorrectionController.h"
#include <iostream>
#include <algorithm>
#include <cmath>

CameraCorrectionController::CameraCorrectionController(
    std::shared_ptr<mavsdk::Mavsdk> mavsdk, 
    std::shared_ptr<mavsdk::System> system)
    : mavsdk_(mavsdk), system_(system) {
    
    if (system_) {
        action_ = std::make_unique<mavsdk::Action>(system_);
        offboard_ = std::make_unique<mavsdk::Offboard>(system_);
        telemetry_ = std::make_unique<mavsdk::Telemetry>(system_);
    }
}

CameraCorrectionController::~CameraCorrectionController() {
    stop();
}

void CameraCorrectionController::setCameraParameters(float fov_horizontal, float fov_vertical,
                                                   int image_width, int image_height) {
    camera_fov_horizontal_ = fov_horizontal;
    camera_fov_vertical_ = fov_vertical;
    image_width_ = image_width;
    image_height_ = image_height;
    
    std::cout << "[CAMERA_CTRL] Камера сконфигурирована: "
              << "FOV " << fov_horizontal << "x" << fov_vertical
              << ", разрешение " << image_width << "x" << image_height << std::endl;
}

void CameraCorrectionController::setPrecisionLandingMode(bool enable) {
    precision_mode_ = enable;
    std::cout << "[CAMERA_CTRL] Режим точной посадки: " 
              << (enable ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") << std::endl;
}

bool CameraCorrectionController::init() {
    if (!system_ || !action_ || !offboard_ || !telemetry_) {
        std::cerr << "[CAMERA_CTRL] ❌ Критическая ошибка: компоненты MAVSDK не инициализированы" << std::endl;
        return false;
    }
    
    std::cout << "[CAMERA_CTRL] ✓ Инициализация контроллера коррекции" << std::endl;
    std::cout << "[CAMERA_CTRL] 🚁 Подключение к дрону..." << std::endl;
    
    try {
        // Проверяем связь
        if (!telemetry_->health_all_ok()) {
            std::cout << "[CAMERA_CTRL] ⚠️  Не все системы дрона готовы" << std::endl;
        }
        
        // Подписываемся на позицию для обновления высоты
        telemetry_->subscribe_position([this](mavsdk::Telemetry::Position position) {
            current_altitude_ = position.relative_altitude_m;
        });
        
        std::cout << "[CAMERA_CTRL] ✓ Телеметрия активирована" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ❌ Ошибка инициализации: " << e.what() << std::endl;
        return false;
    }
    
    running_ = true;
    correction_thread_ = std::thread(&CameraCorrectionController::correctionWorker, this);
    
    std::cout << "[CAMERA_CTRL] 🚀 Контроллер готов к работе" << std::endl;
    return true;
}

void CameraCorrectionController::stop() {
    running_ = false;
    queue_cv_.notify_all();
    
    if (correction_thread_.joinable()) {
        correction_thread_.join();
    }
    
    // Останавливаем offboard режим если активен
    if (offboard_) {
        try {
            offboard_->stop();
        } catch (...) {
            // Игнорируем ошибки при остановке
        }
    }
    
    std::cout << "[CAMERA_CTRL] Контроллер остановлен" << std::endl;
}

bool CameraCorrectionController::addCorrection(float x, float y, bool in_position, float accuracy_px) {
    CorrectionData correction{
        .x = std::clamp(x, -max_correction_distance_, max_correction_distance_),
        .y = std::clamp(y, -max_correction_distance_, max_correction_distance_),
        .in_position = in_position,
        .timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0,
        .accuracy_px = accuracy_px
    };
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        correction_queue_.push(correction);
        
        // Ограничиваем очередь
        while (correction_queue_.size() > 5) {
            correction_queue_.pop();
        }
    }
    
    queue_cv_.notify_one();
    return true;
}

void CameraCorrectionController::correctionWorker() {
    std::cout << "[CAMERA_CTRL] 🚁 Запущен поток обработки коррекций" << std::endl;
    
    while (running_) {
        CorrectionData correction;
        bool has_correction = false;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (queue_cv_.wait_for(lock, std::chrono::milliseconds(50), 
                [this]() { return !correction_queue_.empty() || !running_; })) {
                
                if (!correction_queue_.empty()) {
                    correction = correction_queue_.front();
                    correction_queue_.pop();
                    has_correction = true;
                }
            }
        }
        
        if (has_correction) {
            processCorrection(correction);
        }
        
        // Проверяем статус посадки
        if (landing_in_progress_) {
            checkLandingStatus();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CameraCorrectionController::processCorrection(const CorrectionData& correction) {
    std::cout << "[CAMERA_CTRL] 📍 Коррекция: X=" << correction.x << "m, Y=" << correction.y 
              << "m, В позиции=" << (correction.in_position ? "ДА" : "НЕТ") 
              << ", Точность=" << correction.accuracy_px << "px" << std::endl;
    
    correction_active_ = true;
    
    if (correction.in_position && !landing_in_progress_) {
        std::cout << "[CAMERA_CTRL] 🎯 Дрон в позиции для посадки!" << std::endl;
        
        if (precision_mode_) {
            if (startPrecisionLanding()) {
                landing_in_progress_ = true;
                std::thread([this]() {
                    executeLanding();
                }).detach();
            }
        } else {
            // Прямая посадка без дополнительных коррекций
            landing_in_progress_ = true;
            std::thread([this]() {
                executeLanding();
            }).detach();
        }
    } 
    else if (std::abs(correction.x) > 0.05f || std::abs(correction.y) > 0.05f) {
        // Применяем коррекцию если смещение > 5 см
        applyPositionCorrection(correction.x, correction.y);
    }
    else {
        std::cout << "[CAMERA_CTRL] ✓ Позиция достаточно точна" << std::endl;
    }
    
    correction_active_ = false;
}

float CameraCorrectionController::calculateRequiredMovement(float pixel_offset, bool is_horizontal) {
    // Преобразуем смещение в пикселях в метры с учетом высоты
    float fov_rad = is_horizontal ? 
        camera_fov_horizontal_ * M_PI / 180.0f :
        camera_fov_vertical_ * M_PI / 180.0f;
    
    int image_size = is_horizontal ? image_width_ : image_height_;
    
    // Угловое смещение в радианах
    float angular_offset = (pixel_offset / image_size) * fov_rad;
    
    // Линейное смещение = высота * tan(угол)
    float movement = current_altitude_ * std::tan(angular_offset);
    
    return movement;
}

bool CameraCorrectionController::applyPositionCorrection(float dx, float dy) {
    if (!action_ || !offboard_ || !telemetry_) {
        std::cerr << "[CAMERA_CTRL] ❌ Компоненты не инициализированы" << std::endl;
        return false;
    }
    
    try {
        // Получаем текущую позицию
        auto position = telemetry_->position_velocity_ned();
        
        mavsdk::Offboard::PositionNedYaw target_position{};
        target_position.north_m = position.position.north_m + dx;
        target_position.east_m = position.position.east_m + dy;
        target_position.down_m = position.position.down_m;  // Сохраняем высоту
        target_position.yaw_deg = 0.0f;
        
        std::cout << "[CAMERA_CTRL] 🚀 Коррекция позиции:" << std::endl;
        std::cout << "[CAMERA_CTRL]   Смещение: dX=" << dx << "m, dY=" << dy << "m" << std::endl;
        std::cout << "[CAMERA_CTRL]   Новая позиция: N=" << target_position.north_m 
                  << "m, E=" << target_position.east_m << "m" << std::endl;
        
        // Переключаемся в offboard режим если нужно
        auto flight_mode = telemetry_->flight_mode();
        if (flight_mode != mavsdk::Telemetry::FlightMode::Offboard) {
            std::cout << "[CAMERA_CTRL] 🔄 Переход в offboard режим..." << std::endl;
            
            if (!switchToOffboardMode()) {
                std::cerr << "[CAMERA_CTRL] ❌ Не удалось перейти в offboard режим" << std::endl;
                return false;
            }
        }
        
        // Отправляем команду
        offboard_->set_position_ned(target_position);
        std::cout << "[CAMERA_CTRL] ✅ Команда коррекции отправлена" << std::endl;
        
        // Ждем стабилизации (упрощенная версия)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ❌ Ошибка применения коррекции: " << e.what() << std::endl;
        return false;
    }
}

bool CameraCorrectionController::switchToOffboardMode() {
    try {
        // Сначала убедимся что дрон в воздухе
        if (!telemetry_->in_air()) {
            std::cout << "[CAMERA_CTRL] ⚠️  Дрон на земле, взлетаем..." << std::endl;
            
            auto result = action_->arm();
            if (result != mavsdk::Action::Result::Success) {
                std::cerr << "[CAMERA_CTRL] ❌ Не удалось взлететь: " << int(result) << std::endl;
                return false;
            }
            
            result = action_->takeoff();
            if (result != mavsdk::Action::Result::Success) {
                std::cerr << "[CAMERA_CTRL] ❌ Ошибка взлета: " << int(result) << std::endl;
                return false;
            }
            
            // Ждем взлета
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        
        // Получаем текущую позицию для начальной команды
        auto position = telemetry_->position_velocity_ned();
        mavsdk::Offboard::PositionNedYaw start_position{};
        start_position.north_m = position.position.north_m;
        start_position.east_m = position.position.east_m;
        start_position.down_m = position.position.down_m;
        start_position.yaw_deg = 0.0f;
        
        // Запускаем offboard
        offboard_->set_position_ned(start_position);
        auto result = offboard_->start();
        
        if (result != mavsdk::Offboard::Result::Success) {
            std::cerr << "[CAMERA_CTRL] ❌ Ошибка запуска offboard: " << int(result) << std::endl;
            return false;
        }
        
        std::cout << "[CAMERA_CTRL] ✅ Offboard режим активирован" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ❌ Ошибка перехода в offboard: " << e.what() << std::endl;
        return false;
    }
}

bool CameraCorrectionController::startPrecisionLanding() {
    std::cout << "[CAMERA_CTRL] 🎯 Начало точной посадки" << std::endl;
    
    try {
        // Проверяем что дрон на безопасной высоте для посадки
        if (!ensureSafeAltitude()) {
            std::cerr << "[CAMERA_CTRL] ❌ Небезопасная высота для посадки" << std::endl;
            return false;
        }
        
        // Убеждаемся что в offboard режиме
        auto flight_mode = telemetry_->flight_mode();
        if (flight_mode != mavsdk::Telemetry::FlightMode::Offboard) {
            if (!switchToOffboardMode()) {
                return false;
            }
        }
        
        std::cout << "[CAMERA_CTRL] ✓ Готов к точной посадке" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ❌ Ошибка подготовки точной посадки: " << e.what() << std::endl;
        return false;
    }
}

bool CameraCorrectionController::ensureSafeAltitude() {
    try {
        if (current_altitude_ < 2.0f) {
            std::cout << "[CAMERA_CTRL] ⚠️  Слишком низко (" << current_altitude_ << "m), поднимаемся..." << std::endl;
            
            // Набираем безопасную высоту
            auto position = telemetry_->position_velocity_ned();
            mavsdk::Offboard::PositionNedYaw target_position{};
            target_position.north_m = position.position.north_m;
            target_position.east_m = position.position.east_m;
            target_position.down_m = -5.0f;  // Поднимаемся до 5 метров
            target_position.yaw_deg = 0.0f;
            
            offboard_->set_position_ned(target_position);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            return true;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraCorrectionController::executeLanding() {
    std::lock_guard<std::mutex> lock(landing_mutex_);
    
    for (int attempt = 1; attempt <= max_landing_attempts_ && running_; attempt++) {
        std::cout << "[CAMERA_CTRL] 🛬 Попытка посадки #" << attempt << std::endl;
        
        LandingAttempt landing_attempt{
            .attempt_number = attempt,
            .attempt_time = std::chrono::steady_clock::now(),
            .success = false,
            .error_message = ""
        };
        
        try {
            // Останавливаем offboard режим перед посадкой
            if (offboard_) {
                offboard_->stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Отправляем команду посадки
            std::cout << "[CAMERA_CTRL] 📤 Отправка команды посадки..." << std::endl;
            auto result = action_->land();
            
            if (result != mavsdk::Action::Result::Success) {
                throw std::runtime_error("Ошибка команды посадки: " + std::to_string(int(result)));
            }
            
            std::cout << "[CAMERA_CTRL] ✅ Команда посадки отправлена" << std::endl;
            
            // Ждем подтверждения посадки
            if (waitForLandingConfirmation()) {
                std::cout << "[CAMERA_CTRL] 🎉 ПОСАДКА УСПЕШНА!" << std::endl;
                landing_attempt.success = true;
                landing_attempts_.push_back(landing_attempt);
                landing_in_progress_ = false;
                return true;
            } else {
                landing_attempt.error_message = "Таймаут подтверждения посадки";
                std::cout << "[CAMERA_CTRL] ⚠️  Посадка не подтверждена, повтор..." << std::endl;
            }
            
        } catch (const std::exception& e) {
            landing_attempt.error_message = e.what();
            std::cerr << "[CAMERA_CTRL] ❌ Ошибка посадки: " << e.what() << std::endl;
        }
        
        landing_attempts_.push_back(landing_attempt);
        
        // Пауза между попытками
        if (attempt < max_landing_attempts_) {
            std::cout << "[CAMERA_CTRL] ⏳ Ожидание " << landing_attempt_interval_ 
                      << " сек перед следующей попыткой..." << std::endl;
            
            auto start = std::chrono::steady_clock::now();
            while (running_ && std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start).count() < landing_attempt_interval_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    
    std::cout << "[CAMERA_CTRL] ❌ Все попытки посадки исчерпаны" << std::endl;
    landing_in_progress_ = false;
    return false;
}

bool CameraCorrectionController::waitForLandingConfirmation(float timeout_sec) {
    if (!telemetry_) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    std::cout << "[CAMERA_CTRL] ⏱️  Ожидание подтверждения посадки (" << timeout_sec << "s)..." << std::endl;
    
    while (running_ && std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count() < timeout_sec) {
        
        try {
            // Проверяем что дрон больше не в воздухе
            if (!telemetry_->in_air()) {
                std::cout << "[CAMERA_CTRL] ✓ Дрон приземлился" << std::endl;
                return true;
            }
            
            // Проверяем высоту
            auto position = telemetry_->position();
            if (position.relative_altitude_m < 0.3f) {  // 30 см от земли
                std::cout << "[CAMERA_CTRL] ✓ Дрон близко к земле (" 
                          << position.relative_altitude_m << "m)" << std::endl;
                return true;
            }
            
            // Периодически логируем статус
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            
            if (elapsed % 2 == 0) {
                std::cout << "[CAMERA_CTRL]   Высота: " << position.relative_altitude_m 
                          << "m, В воздухе: " << (telemetry_->in_air() ? "ДА" : "НЕТ") << std::endl;
            }
            
        } catch (...) {
            // Игнорируем временные ошибки телеметрии
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    return false;
}

void CameraCorrectionController::checkLandingStatus() {
    if (!telemetry_ || !landing_in_progress_) return;
    
    try {
        static auto last_log = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 1) {
            auto position = telemetry_->position();
            std::cout << "[CAMERA_CTRL] 📊 Статус: Высота=" << position.relative_altitude_m 
                      << "m, В воздухе=" << (telemetry_->in_air() ? "ДА" : "НЕТ") << std::endl;
            last_log = now;
        }
        
    } catch (...) {
        // Игнорируем ошибки телеметрии
    }
}

bool CameraCorrectionController::isActive() const {
    return correction_active_;
}

std::string CameraCorrectionController::getLandingStatus() {
    std::lock_guard<std::mutex> lock(landing_mutex_);
    
    if (landing_attempts_.empty()) {
        return "not_started";
    }
    
    auto& last_attempt = landing_attempts_.back();
    if (last_attempt.success) {
        return "landed";
    } else if (landing_in_progress_) {
        return "landing_in_progress";
    } else {
        return "landing_failed";
    }
}

int CameraCorrectionController::getLandingAttemptsCount() {
    std::lock_guard<std::mutex> lock(landing_mutex_);
    return landing_attempts_.size();
}