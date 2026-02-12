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
    
    try {
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
    if (offboard_ && offboard_active_) {
        try {
            offboard_->stop();
            offboard_active_ = false;
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
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CameraCorrectionController::processCorrection(const CorrectionData& correction) {
    std::cout << "[CAMERA_CTRL] 📍 Коррекция: X=" << correction.x << "m, Y=" << correction.y 
              << "m, В позиции=" << (correction.in_position ? "ДА" : "НЕТ") 
              << ", Точность=" << correction.accuracy_px << "px" << std::endl;
    
    correction_active_ = true;
    
    // ВСЕГДА применяем коррекцию позиции, если дрон в воздухе
    if (telemetry_ && telemetry_->in_air()) {
        // Проверяем, нужно ли двигаться (смещение > 5см)
        if (std::abs(correction.x) > 0.05f || std::abs(correction.y) > 0.05f) {
            applyPositionCorrection(correction.x, correction.y);
        } else {
            std::cout << "[CAMERA_CTRL] ✓ Дрон точно над меткой (смещение < 5см)" << std::endl;
        }
    }
    
    // Если дрон в позиции И включен режим точной посадки И дрон ещё не садится
    if (correction.in_position && precision_mode_ && !landing_in_progress_ && telemetry_->in_air()) {
        std::cout << "[CAMERA_CTRL] 🎯 Дрон в позиции для посадки!" << std::endl;
        startPrecisionLanding();
    }
    
    correction_active_ = false;
}

bool CameraCorrectionController::applyPositionCorrection(float dx, float dy) {
    if (!action_ || !offboard_ || !telemetry_) {
        std::cerr << "[CAMERA_CTRL] ? ���������� �� ����������������" << std::endl;
        return false;
    }
    
    try {
        if (!telemetry_->in_air()) {
            std::cout << "[CAMERA_CTRL] ?? ���� �� �����, ��������� �� ���������" << std::endl;
            return false;
        }
        
        // �������� �������
        auto position = telemetry_->position_velocity_ned();
        
        // �������� ���� ��������!
        auto attitude = telemetry_->attitude_euler();
        float yaw_deg = attitude.yaw_deg;
        
        mavsdk::Offboard::PositionNedYaw target_position{};
        target_position.north_m = position.position.north_m + dx;
        target_position.east_m = position.position.east_m + dy;
        target_position.down_m = position.position.down_m;
        target_position.yaw_deg = yaw_deg;  // < ����������!
        
        std::cout << "[CAMERA_CTRL] ?? ���������: dX=" << dx << "m, dY=" << dy << "m";
        std::cout << " (������: " << -position.position.down_m << "m, ����: " << yaw_deg << "�)" << std::endl;
        
        auto flight_mode = telemetry_->flight_mode();
        
        if (flight_mode != mavsdk::Telemetry::FlightMode::Offboard) {
            if (!enableOffboardMode()) {
                return false;
            }
        }
        
        offboard_->set_position_ned(target_position);
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ? ������: " << e.what() << std::endl;
        return false;
    }
}

bool CameraCorrectionController::enableOffboardMode() {
    try {
        if (!telemetry_->in_air()) {
            std::cerr << "[CAMERA_CTRL] ? ������ �������� offboard �� �����" << std::endl;
            return false;
        }
        
        if (mission_mode_active_) {
            std::cerr << "[CAMERA_CTRL] ? ������ �������� offboard �� ����� ������" << std::endl;
            return false;
        }
        
        auto position = telemetry_->position_velocity_ned();
        
        // �������� ���� ��������!
        auto attitude = telemetry_->attitude_euler();
        float yaw_deg = attitude.yaw_deg;
        
        mavsdk::Offboard::PositionNedYaw current_position{};
        current_position.north_m = position.position.north_m;
        current_position.east_m = position.position.east_m;
        current_position.down_m = position.position.down_m;
        current_position.yaw_deg = yaw_deg;  // < ����������!
        
        offboard_->set_position_ned(current_position);
        
        auto result = offboard_->start();
        
        if (result != mavsdk::Offboard::Result::Success) {
            std::cerr << "[CAMERA_CTRL] ? ������ ������� offboard: " << int(result) << std::endl;
            return false;
        }
        
        offboard_active_ = true;
        std::cout << "[CAMERA_CTRL] ? Offboard ����� ����������� (������: " 
                  << -position.position.down_m << "m, ����: " << yaw_deg << "�)" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ? ������ ��������� offboard: " << e.what() << std::endl;
        return false;
    }
}

bool CameraCorrectionController::startPrecisionLanding() {
    if (!telemetry_ || !offboard_ || !action_) {
        std::cerr << "[CAMERA_CTRL] ❌ Компоненты не инициализированы" << std::endl;
        return false;
    }
    
    std::cout << "[CAMERA_CTRL] 🛬 НАЧАЛО ТОЧНОЙ ПОСАДКИ" << std::endl;
    landing_in_progress_ = true;
    
    // Запускаем посадку в отдельном потоке
    std::thread([this]() {
        executePrecisionLanding();
    }).detach();
    
    return true;
}

bool CameraCorrectionController::executePrecisionLanding() {
    try {
        if (mission_mode_active_) {
            std::cerr << "[CAMERA_CTRL] ? ������� �������� - ������ �������" << std::endl;
            landing_in_progress_ = false;
            return false;
        }
        
        if (telemetry_->flight_mode() != mavsdk::Telemetry::FlightMode::Offboard) {
            if (!enableOffboardMode()) {
                std::cerr << "[CAMERA_CTRL] ? �� ������� �������� offboard ��� �������" << std::endl;
                landing_in_progress_ = false;
                return false;
            }
        }
        
        const float LANDING_SPEED = 0.2f;
        const float MIN_HEIGHT = 0.2f;
        
        std::cout << "[CAMERA_CTRL] ?? ������� ������� ��������..." << std::endl;
        
        while (running_ && landing_in_progress_ && !mission_mode_active_) {
            auto position = telemetry_->position_velocity_ned();
            float current_height = -position.position.down_m;
            
            std::cout << "[CAMERA_CTRL]   ������: " << current_height << "m" << std::endl;
            
            if (current_height <= MIN_HEIGHT) {
                std::cout << "[CAMERA_CTRL] ?? ������������ �� ��������� �������" << std::endl;
                break;
            }
            
            float new_down = position.position.down_m + (LANDING_SPEED * 0.1f);
            
            // �������� ���� ��������!
            auto attitude = telemetry_->attitude_euler();
            float yaw_deg = attitude.yaw_deg;
            
            mavsdk::Offboard::PositionNedYaw target{};
            target.north_m = position.position.north_m;
            target.east_m = position.position.east_m;
            target.down_m = new_down;
            target.yaw_deg = yaw_deg;  // < ����������!
            
            offboard_->set_position_ned(target);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (mission_mode_active_) {
            std::cout << "[CAMERA_CTRL] ?? ������� �������� - ������ ������" << std::endl;
            landing_in_progress_ = false;
            return false;
        }
        
        if (offboard_active_) {
            offboard_->stop();
            offboard_active_ = false;
        }
        
        std::cout << "[CAMERA_CTRL] ?? ��������� �������..." << std::endl;
        auto result = action_->land();
        
        if (result != mavsdk::Action::Result::Success) {
            std::cerr << "[CAMERA_CTRL] ? ������ ������� �������" << std::endl;
            landing_in_progress_ = false;
            return false;
        }
        
        while (running_ && telemetry_->in_air()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "[CAMERA_CTRL] ?? ������� �������!" << std::endl;
        landing_in_progress_ = false;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ? ������ �������: " << e.what() << std::endl;
        landing_in_progress_ = false;
        return false;
    }
}

bool CameraCorrectionController::isActive() const {
    return correction_active_;
}

std::string CameraCorrectionController::getLandingStatus() {
    if (landing_in_progress_) {
        return "landing_in_progress";
    }
    return "not_started";
}

int CameraCorrectionController::getLandingAttemptsCount() {
    return landing_in_progress_ ? 1 : 0;
}
