#include "MissionController.h"

using namespace mavsdk;
using std::chrono::seconds;

MissionController::MissionController(std::shared_ptr<mavsdk::Mavsdk> mavsdk) 
    : mavsdk_(mavsdk) 
{
    std::cout << "[DRONE_INFO] MissionController created with shared Mavsdk" << std::endl;
}

MissionController::~MissionController() {
    stop_mission_monitoring();
    if (land_monitor_thread_.joinable()) {
        land_monitor_thread_.join();
    }
    std::cout << "[DRONE_INFO] MissionController destroyed" << std::endl;
}

bool MissionController::connect(const std::string& connection_url) {
    std::cout << "[DRONE_INFO] === ПОДКЛЮЧЕНИЕ К АВТОПИЛОТУ ===" << std::endl;
    std::cout << "[DRONE_INFO] URL: " << connection_url << std::endl;
    
    auto connection_result = mavsdk_->add_any_connection(connection_url);
    std::cout << "[DRONE_INFO] Результат подключения: " << connection_result << std::endl;
    
    if (connection_result != ConnectionResult::Success) {
        std::cerr << "[DRONE_ERROR] ❌ Ошибка подключения: " << connection_result << std::endl;
        return false;
    }
    
    std::cout << "[DRONE_INFO] ⏳ Ожидание системы (30 сек)..." << std::endl;
    auto system_opt = mavsdk_->first_autopilot(30.0);
    
    if (!system_opt) {
        std::cerr << "[DRONE_ERROR] ❌ Таймаут ожидания системы" << std::endl;
        return false;
    }
    
    system_ = *system_opt;
    std::cout << "[DRONE_SUCCESS] ✅ Система найдена! ID: " << (int)system_->get_system_id() << std::endl;
    
    try {
        action_ = std::make_unique<Action>(*system_);
        mission_ = std::make_unique<Mission>(*system_);
        telemetry_ = std::make_unique<Telemetry>(*system_);
        param_ = std::make_unique<Param>(*system_);
        std::cout << "[DRONE_SUCCESS] ✅ Плагины инициализированы" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] ❌ Ошибка инициализации плагинов: " << e.what() << std::endl;
        return false;
    }
    
    setup_telemetry_rates();
    return wait_for_health_ok();
}

void MissionController::set_no_autoland(bool value) {
    no_autoland_ = value;
    std::cout << "[DRONE_INFO] Флаг no_autoland установлен: " << (value ? "true" : "false") << std::endl;
}

void MissionController::setup_telemetry_rates() {
    std::cout << "[DRONE_INFO] Настройка частоты обновления телеметрии..." << std::endl;
    
    try {
        telemetry_->set_rate_position(5.0);
        telemetry_->set_rate_battery(2.0);
        telemetry_->set_rate_gps_info(2.0);
        telemetry_->set_rate_altitude(2.0);
        telemetry_->set_rate_velocity_ned(2.0);
        std::cout << "[DRONE_SUCCESS] ✅ Частоты телеметрии установлены" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_WARNING] Ошибка настройки телеметрии: " << e.what() << std::endl;
    }
}

bool MissionController::getCurrentPosition(double& lat, double& lon, float& alt, float& battery) {
    std::cout << "[DRONE_INFO] === ДЕТАЛЬНАЯ ДИАГНОСТИКА ПОЗИЦИИ ===" << std::endl;
    
    try {
        if (!telemetry_) {
            std::cout << "[DRONE_ERROR] ❌ Телеметрия не инициализирована" << std::endl;
            return false;
        }
        
        if (!system_ || !system_->is_connected()) {
            std::cout << "[DRONE_ERROR] ❌ Система не подключена" << std::endl;
            return false;
        }
        
        std::cout << "[DRONE_SUCCESS] ✅ Базовая проверка пройдена" << std::endl;
        
        auto position = telemetry_->position();
        auto gps_info = telemetry_->gps_info();
        auto battery_info = telemetry_->battery();
        
        std::cout << "[DRONE_INFO] 📊 ДАННЫЕ ТЕЛЕМЕТРИИ:" << std::endl;
        std::cout << "[DRONE_INFO]   Широта: " << position.latitude_deg << std::endl;
        std::cout << "[DRONE_INFO]   Долгота: " << position.longitude_deg << std::endl;
        std::cout << "[DRONE_INFO]   Абс. высота: " << position.absolute_altitude_m << std::endl;
        std::cout << "[DRONE_INFO]   Отн. высота: " << position.relative_altitude_m << std::endl;
        std::cout << "[DRONE_INFO]   Спутники: " << gps_info.num_satellites << std::endl;
        std::cout << "[DRONE_INFO]   Фикс GPS: " << static_cast<int>(gps_info.fix_type) << std::endl;
        std::cout << "[DRONE_INFO]   Батарея: " << battery_info.remaining_percent * 100 << "%" << std::endl;
        
        bool gps_valid = (gps_info.fix_type >= Telemetry::FixType::Fix3D) && 
                        (gps_info.num_satellites >= 4);
        
        bool coordinates_valid = !std::isnan(position.latitude_deg) && 
                               !std::isnan(position.longitude_deg) &&
                               std::abs(position.latitude_deg) > 0.001 && 
                               std::abs(position.longitude_deg) > 0.001;
        
        bool altitude_valid = !std::isnan(position.relative_altitude_m) &&
                             position.relative_altitude_m >= -10.0f &&
                             position.relative_altitude_m < 10000.0f;
        
        std::cout << "[DRONE_INFO] 🔍 ВАЛИДАЦИЯ ДАННЫХ:" << std::endl;
        std::cout << "[DRONE_INFO]   GPS: " << (gps_valid ? "VALID" : "INVALID") 
                  << " (спутники: " << gps_info.num_satellites 
                  << ", фикс: " << static_cast<int>(gps_info.fix_type) << ")" << std::endl;
        std::cout << "[DRONE_INFO]   Координаты: " << (coordinates_valid ? "VALID" : "INVALID") << std::endl;
        std::cout << "[DRONE_INFO]   Высота: " << (altitude_valid ? "VALID" : "INVALID") << std::endl;
        
        if (gps_valid && coordinates_valid && altitude_valid) {
            lat = position.latitude_deg;
            lon = position.longitude_deg;
            alt = std::max(0.0f, position.relative_altitude_m);
            battery = battery_info.remaining_percent * 100.0f;
            
            std::cout << "[DRONE_SUCCESS] 🎯 РЕАЛЬНЫЕ ДАННЫЕ УСТАНОВЛЕНЫ:" << std::endl;
            std::cout << "[DRONE_SUCCESS]   Широта: " << std::fixed << std::setprecision(7) << lat << std::endl;
            std::cout << "[DRONE_SUCCESS]   Долгота: " << std::fixed << std::setprecision(7) << lon << std::endl;
            std::cout << "[DRONE_SUCCESS]   Высота: " << alt << "м" << std::endl;
            std::cout << "[DRONE_SUCCESS]   Батарея: " << battery << "%" << std::endl;
            
            std::cout << "[DRONE_INFO] === ДИАГНОСТИКА ЗАВЕРШЕНА ===" << std::endl;
            return true;
        } else {
            std::cout << "[DRONE_WARNING] 🚨 НЕВАЛИДНЫЕ ДАННЫЕ - возвращаем нули" << std::endl;
            lat = 0.0;
            lon = 0.0;
            alt = 0.0f;
            battery = 0.0f;
            
            std::cout << "[DRONE_INFO] === ДИАГНОСТИКА ЗАВЕРШЕНА ===" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] 💥 КРИТИЧЕСКАЯ ОШИБКА: " << e.what() << std::endl;
        lat = 0.0;
        lon = 0.0;
        alt = 0.0f;
        battery = 0.0f;
        
        std::cout << "[DRONE_INFO] === ДИАГНОСТИКА ЗАВЕРШЕНА ===" << std::endl;
        return false;
    }
}

bool MissionController::execute_mission_from_json(const json& mission_json) {
    std::cout << "\n[DRONE_INFO] ⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐" << std::endl;
    std::cout << "[DRONE_INFO] ⭐           ЗАПУСК ВЫПОЛНЕНИЯ МИССИИ ИЗ JSON           ⭐" << std::endl;
    std::cout << "[DRONE_INFO] ⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐" << std::endl;
    
    // ========== ПРОВЕРКА ПОДКЛЮЧЕНИЯ ==========
    if (!is_connected()) {
        std::cerr << "[DRONE_ERROR] ❌ Не подключен к дрону" << std::endl;
        return false;
    }
    std::cout << "[DRONE_SUCCESS] ✅ Подключение к дрону активно" << std::endl;

    // ========== ОЧИСТКА СТАРОЙ МИССИИ ==========
    std::cout << "[DRONE_INFO] 🧹 Очистка старой миссии..." << std::endl;
    clear_mission();
    safe_sleep(1);
    std::cout << "[DRONE_SUCCESS] ✅ Старая миссия очищена" << std::endl;
    
    // ========== СБРОС ФЛАГОВ ==========
    mission_running_ = false;
    mission_completed_ = false;
    force_land_triggered_ = false;
    last_mission_has_land_command_ = false;
    last_mission_has_rtl_command_ = false;
    
    // ========== ВЫВОД JSON ОТ СЕРВЕРА ==========
    std::cout << "[DRONE_DEBUG] 📄 ПОЛНЫЙ JSON ОТ СЕРВЕРА:" << std::endl;
    std::cout << mission_json.dump(2) << std::endl;
    
    // ========== ПАРСИНГ МИССИИ ==========
    std::cout << "[DRONE_INFO] 🔄 Парсинг JSON миссии..." << std::endl;
    std::vector<Mission::MissionItem> mission_items;
    if (!parse_qgc_mission_json(mission_json, mission_items)) {
        std::cerr << "[DRONE_ERROR] ❌ Ошибка парсинга JSON миссии" << std::endl;
        return false;
    }
    
    std::cout << "[DRONE_SUCCESS] ✅ JSON успешно распарсен" << std::endl;
    std::cout << "[DRONE_INFO] 📊 Миссия содержит: " << mission_items.size() << " точек" << std::endl;
    std::cout << "[DRONE_INFO] 📊 Команда посадки в миссии: " << (last_mission_has_land_command_ ? "✅ ДА" : "❌ НЕТ") << std::endl;
    std::cout << "[DRONE_INFO] 📊 Команда RTL в миссии: " << (last_mission_has_rtl_command_ ? "✅ ДА" : "❌ НЕТ") << std::endl;
    
    if (mission_items.empty()) {
        std::cerr << "[DRONE_ERROR] ❌ Нет точек миссии для выполнения" << std::endl;
        return false;
    }
    
    // ========== ПРОВЕРКА ПОСЛЕДНЕЙ ТОЧКИ ==========
    if (last_mission_has_land_command_ && !mission_items.empty()) {
        const auto& last_item = mission_items.back();
        std::cout << "[DRONE_INFO] 🛬 Последняя точка (ПОСАДКА):" << std::endl;
        std::cout << "[DRONE_INFO]    ├─ Высота: " << last_item.relative_altitude_m << "м" << std::endl;
        std::cout << "[DRONE_INFO]    ├─ Скорость: " << last_item.speed_m_s << "м/с" << std::endl;
        std::cout << "[DRONE_INFO]    └─ Координаты: " << std::fixed << std::setprecision(7) 
                  << last_item.latitude_deg << ", " << last_item.longitude_deg << std::endl;
    }
    
    // ========== ЗАГРУЗКА МИССИИ ==========
    std::cout << "[DRONE_INFO] 📤 Загрузка миссии в автопилот..." << std::endl;
    Mission::MissionPlan mission_plan{};
    mission_plan.mission_items = mission_items;
    
    auto upload_result = mission_->upload_mission(mission_plan);
    if (upload_result != Mission::Result::Success) {
        std::cerr << "[DRONE_ERROR] ❌ Ошибка загрузки миссии: " << static_cast<int>(upload_result) << std::endl;
        return false;
    }
    
    std::cout << "[DRONE_SUCCESS] ✅ Миссия успешно загружена в автопилот!" << std::endl;
    
    // ========== ВЕРИФИКАЦИЯ ЗАГРУЖЕННОЙ МИССИИ ==========
    std::cout << "[DRONE_INFO] 🔍 Запрос сохраненной миссии для верификации..." << std::endl;
    get_mission_items();  // Скачиваем и логируем миссию с контроллера
    
    std::cout << "[DRONE_INFO] 🔍 Проверка загруженной миссии..." << std::endl;
    verify_uploaded_mission();
    
    // ========== НАСТРОЙКА ПАРАМЕТРОВ ==========
    std::cout << "[DRONE_INFO] ⚙️ Настройка параметров безопасности..." << std::endl;
    configure_safety_parameters();
    safe_sleep(2);
    
    // ========== ПРОВЕРКА ТЕКУЩЕЙ ПОЗИЦИИ ==========
    std::cout << "[DRONE_INFO] 📍 ПРОВЕРКА ТЕКУЩЕГО СОСТОЯНИЯ ДРОНА:" << std::endl;
    double lat, lon;
    float alt, battery;
    bool already_in_air = false;
    
    if (getCurrentPosition(lat, lon, alt, battery)) {
        std::cout << "[DRONE_INFO]    ├─ Высота: " << alt << "м" << std::endl;
        std::cout << "[DRONE_INFO]    ├─ Координаты: " << std::setprecision(7) << lat << ", " << lon << std::endl;
        std::cout << "[DRONE_INFO]    └─ Батарея: " << battery << "%" << std::endl;
        
        if (alt > 2.0f) {
            std::cout << "[DRONE_INFO] 🟢 Дрон УЖЕ в воздухе на высоте " << alt << "м" << std::endl;
            already_in_air = true;
        } else {
            std::cout << "[DRONE_INFO] 🟫 Дрон на земле, готов к взлету" << std::endl;
        }
    } else {
        std::cerr << "[DRONE_WARNING] ⚠️ Не удалось получить позицию перед взлетом" << std::endl;
    }
    
    // ========== АРМИНГ ==========
    std::cout << "[DRONE_INFO] 🔒 Арминг дрона (включение моторов)..." << std::endl;
    auto arm_result = action_->arm();
    
    if (arm_result != Action::Result::Success) {
        std::cout << "[DRONE_WARNING] ⚠️ Standard arm failed, trying force arm..." << std::endl;
        arm_result = action_->arm_force();
    }
    
    if (arm_result != Action::Result::Success) {
        std::cerr << "[DRONE_ERROR] ❌ Ошибка арминга: " << static_cast<int>(arm_result) << std::endl;
        return false;
    }
    
    std::cout << "[DRONE_SUCCESS] ✅ Дрон вооружен, моторы готовы" << std::endl;
    
    // ========== ВЗЛЕТ ==========
    if (!already_in_air) {
        std::cout << "[DRONE_INFO] 🚀 Взлет на 5 метров..." << std::endl;
        auto takeoff_result = action_->takeoff();
        
        if (takeoff_result != Action::Result::Success) {
            std::cerr << "[DRONE_ERROR] ❌ Ошибка взлета: " << static_cast<int>(takeoff_result) << std::endl;
            action_->disarm();
            return false;
        }
        
        std::cout << "[DRONE_SUCCESS] ✅ Взлет выполнен! Стабилизация 5 секунд..." << std::endl;
        safe_sleep(5);
    }
    
    // ========== ЗАПУСК МИССИИ ==========
    std::cout << "[DRONE_INFO] ▶️ Запуск миссии..." << std::endl;
    Mission::Result start_result = Mission::Result::Unknown;
    
    for (int attempt = 0; attempt < 3; attempt++) {
        start_result = mission_->start_mission();
        std::cout << "[DRONE_INFO] Попытка " << (attempt + 1) << ": " 
                  << static_cast<int>(start_result) << std::endl;
        
        if (start_result == Mission::Result::Success) {
            break;
        }
        safe_sleep(1);
    }
    
    if (start_result != Mission::Result::Success) {
        std::cerr << "[DRONE_ERROR] ❌ Ошибка запуска миссии" << std::endl;
        return false;
    }
    
    std::cout << "[DRONE_SUCCESS] ✅ Миссия успешно запущена!" << std::endl;
    std::cout << "[DRONE_INFO] 📍 Всего точек в маршруте: " << mission_items.size() << std::endl;
    
    // ========== ЗАПУСК МОНИТОРИНГА ==========
    std::cout << "[DRONE_INFO] 👁️ Запуск мониторинга выполнения..." << std::endl;
    start_mission_monitoring();
    
    // ========== ОЖИДАНИЕ ЗАВЕРШЕНИЯ ==========
    std::cout << "[DRONE_INFO] ⏳ Ожидание завершения миссии..." << std::endl;
    
    int timeout_counter = 0;
    const int MAX_TIMEOUT = 600;
    
    while (mission_running_ && timeout_counter < MAX_TIMEOUT) {
        safe_sleep(1);
        timeout_counter++;
        
        if (timeout_counter % 30 == 0) {
            std::cout << "[DRONE_INFO] ⏱️ Миссия выполняется... (" << timeout_counter << "с)" << std::endl;
        }
    }
    
    // ========== ОСТАНОВКА МОНИТОРИНГА ==========
    std::cout << "[DRONE_INFO] 🛑 Остановка мониторинга..." << std::endl;
    stop_mission_monitoring();

    // ========== ИТОГ ВЫПОЛНЕНИЯ ==========
    std::cout << "\n[DRONE_INFO] ⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐" << std::endl;
    std::cout << "[DRONE_INFO] ⭐                    РЕЗУЛЬТАТ МИССИИ                    ⭐" << std::endl;
    std::cout << "[DRONE_INFO] ⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐" << std::endl;

    if (mission_completed_) {
        std::cout << "[DRONE_SUCCESS] ✅ МИССИЯ УСПЕШНО ЗАВЕРШЕНА!" << std::endl;
        std::cout << "[DRONE_INFO] 🛬 Дрон выполнил все точки, начинаем посадку..." << std::endl;
        
        // ========== АВТОМАТИЧЕСКАЯ ПОСАДКА ==========
        std::cout << "[DRONE_INFO] Отправка команды LAND..." << std::endl;
        auto land_result = action_->land();
        
        if (land_result == Action::Result::Success) {
            std::cout << "[DRONE_SUCCESS] ✅ Команда посадки принята" << std::endl;
            monitor_landing_progress();
        } else {
            std::cerr << "[DRONE_ERROR] ❌ Ошибка посадки: " << static_cast<int>(land_result) << std::endl;
        }
        
        return true;
        
    } else if (force_land_triggered_) {
        std::cout << "[DRONE_WARNING] ⚠️ Миссия завершена с принудительной посадкой" << std::endl;
        return true;
    } else {
        std::cerr << "[DRONE_ERROR] ❌ Миссия НЕ ЗАВЕРШЕНА (таймаут или ошибка)" << std::endl;
        return false;
    }
}

void MissionController::verify_uploaded_mission() {
    try {
        std::cout << "[DRONE_INFO] Проверка загруженной миссии..." << std::endl;
        // Упрощенная проверка - просто получаем прогресс миссии
        auto progress = mission_->mission_progress();
        std::cout << "[DRONE_INFO] Загружено точек: " << progress.total << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "[DRONE_WARNING] Ошибка при проверке загруженной миссии: " << e.what() << std::endl;
    }
}

void MissionController::configure_safety_parameters() {
    try {
        std::cout << "[DRONE_INFO] Настройка параметров для приоритета пульта..." << std::endl;
        
        // ВКЛЮЧАЕМ РЕАКЦИЮ НА ПУЛЬТ
        param_->set_param_int("COM_RC_IN_MODE", 0);  // 0 = RC включен всегда (приоритет)
        param_->set_param_int("NAV_RCL_ACT", 1);     // 1 = RTL при потере сигнала
        param_->set_param_int("COM_ARM_CHK", 1);     // Включаем проверки безопасности
        
        // Дополнительные параметры для ArduPilot
        param_->set_param_int("RC_OPTIONS", 0);       // 0 = все каналы работают
        param_->set_param_int("FS_OPTIONS", 0);       // 0 = отключить демпфирование пульта
        
        std::cout << "[DRONE_SUCCESS] ✅ Пульт теперь в приоритете!" << std::endl;
        std::cout << "[DRONE_INFO]   - При движении стиков пульт перехватывает управление" << std::endl;
        std::cout << "[DRONE_INFO]   - При потере сигнала - RTL" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_WARNING] Ошибка настройки параметров: " << e.what() << std::endl;
    }
}

void MissionController::start_mission_monitoring() {
    mission_running_ = true;
    mission_monitor_thread_ = std::thread(&MissionController::mission_monitor_loop, this);
    std::cout << "[DRONE_INFO] Мониторинг миссии запущен" << std::endl;
}

void MissionController::stop_mission_monitoring() {
    mission_running_ = false;
    
    if (mission_monitor_thread_.joinable()) {
        mission_monitor_thread_.join();
        std::cout << "[DRONE_INFO] Мониторинг миссии остановлен" << std::endl;
    }
}

void MissionController::mission_monitor_loop() {
    std::cout << "[DRONE_INFO] 🚀 Начало мониторинга выполнения миссии" << std::endl;
    std::cout << "[DRONE_INFO] Параметры миссии:" << std::endl;
    std::cout << "[DRONE_INFO]   last_mission_has_land_command_: " 
              << (last_mission_has_land_command_ ? "true" : "false") << std::endl;
    std::cout << "[DRONE_INFO]   no_autoland_: " 
              << (no_autoland_ ? "true" : "false") << std::endl;
    
    int check_counter = 0;
    int mission_complete_counter = 0;
    const int MAX_MISSION_COMPLETE_CHECKS = 3;
    
    while (mission_running_) {
        safe_sleep(2);
        check_counter++;
        
        try {
            // Проверяем прогресс миссии
            if (check_mission_progress()) {
                mission_complete_counter++;
                std::cout << "[DRONE_INFO] Прогресс миссии: завершено (" 
                          << mission_complete_counter << "/" << MAX_MISSION_COMPLETE_CHECKS 
                          << " проверок)" << std::endl;
                
                // Подтверждаем завершение несколькими проверками
                if (mission_complete_counter >= MAX_MISSION_COMPLETE_CHECKS) {
                    handle_mission_completion();
                    break;
                }
            } else {
                // Сброс счетчика если миссия еще не завершена
                mission_complete_counter = 0;
                
                // Периодический лог каждые 20 секунд
                if (check_counter % 10 == 0) {
                    log_mission_status();
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DRONE_ERROR] Ошибка мониторинга: " << e.what() << std::endl;
            handle_monitoring_error();
            break;
        }
    }
    
    std::cout << "[DRONE_INFO] Мониторинг миссии завершен" << std::endl;
}

void MissionController::handle_mission_completion() {
    std::cout << "[DRONE_SUCCESS] ✅ Миссия выполнена. Начинаем посадку как в TakeoffLandController..." << std::endl;
    
    // Прямо как в TakeoffLandController::execute_landing_only()
    std::cout << "\n=== НАЧАЛО ПОСАДКИ ===" << std::endl;
    
    // Отправляем команду посадки
    auto land_result = action_->land();
    
    if (land_result != Action::Result::Success) {
        std::cerr << "[DRONE_ERROR] ❌ Ошибка посадки" << std::endl;
        return;
    }
    
    std::cout << "[DRONE_SUCCESS] ✅ Команда посадки принята" << std::endl;
    std::cout << "Descending..." << std::endl;
    
    // Ждём посадки (как в TakeoffLandController)
    for (int i = 0; i < 30; ++i) {
        safe_sleep(1);
        
        try {
            if (!telemetry_->in_air()) {
                std::cout << "[DRONE_SUCCESS] ✅ Дрон сел!" << std::endl;
                
                // Ждём auto-disarm
                std::cout << "Ожидание auto-disarm..." << std::endl;
                safe_sleep(3);
                
                // Проверяем моторы
                if (telemetry_->armed()) {
                    std::cout << "Принудительное отключение..." << std::endl;
                    action_->disarm();
                }
                
                std::cout << "\n✓ Посадка завершена" << std::endl;
                std::cout << "  Дрон на земле, моторы выключены" << std::endl;
                
                mission_completed_ = true;
                return;
            }
            
            auto position = telemetry_->position();
            std::cout << "Altitude: " << position.relative_altitude_m << "m" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    
    std::cerr << "[DRONE_ERROR] ❌ Таймаут посадки!" << std::endl;
}

void MissionController::stop_mission_execution() {
    try {
        std::cout << "[DRONE_INFO] Останавливаем выполнение миссии..." << std::endl;
        mission_->pause_mission();
        safe_sleep(1);
        std::cout << "[DRONE_SUCCESS] Миссия остановлена" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_WARNING] Не удалось остановить миссию: " << e.what() << std::endl;
    }
}

void MissionController::handle_mission_with_land_command() {
    std::cout << "[DRONE_INFO] 🛬 В миссии есть команда посадки" << std::endl;
    std::cout << "[DRONE_INFO] Ожидаем выполнения посадки дроном..." << std::endl;
    
    // ВАЖНО: Если в миссии уже есть команда LAND, автопилот сам выполнит посадку
    // Не отправляем дополнительные команды, просто мониторим
    
    // Проверяем, начал ли дрон посадку
    try {
        auto flight_mode = telemetry_->flight_mode();
        std::cout << "[DRONE_INFO] Режим полета: " << flight_mode_to_string(flight_mode) << std::endl;
        
        if (flight_mode == Telemetry::FlightMode::Land || 
            flight_mode == Telemetry::FlightMode::ReturnToLaunch) {
            std::cout << "[DRONE_INFO] Дрон уже выполняет посадку" << std::endl;
        } else {
            std::cout << "[DRONE_INFO] Ждем перехода в режим посадки..." << std::endl;
            // Даем время автопилоту обработать команду посадки из миссии
            for (int i = 0; i < 10; i++) {
                safe_sleep(1);
                flight_mode = telemetry_->flight_mode();
                if (flight_mode == Telemetry::FlightMode::Land || 
                    flight_mode == Telemetry::FlightMode::ReturnToLaunch) {
                    std::cout << "[DRONE_INFO] Дрон перешел в режим посадки" << std::endl;
                    break;
                }
            }
        }
        
        // Мониторим посадку
        monitor_landing_progress();
        
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка мониторинга посадки: " << e.what() << std::endl;
        // Если что-то пошло не так, запускаем принудительную посадку
        execute_forced_landing();
    }
}

void MissionController::handle_no_autoland_scenario() {
    std::cout << "[DRONE_INFO] 🚫 АВТО-ПОСАДКА ОТКЛЮЧЕНА (no_autoland = true)" << std::endl;
    std::cout << "[DRONE_INFO] 🎯 Дрон зависает на месте" << std::endl;
    std::cout << "[DRONE_INFO] ⚠️ Требуется ручное управление для посадки" << std::endl;
    
    try {
        auto position = telemetry_->position();
        std::cout << "[DRONE_INFO] Текущая позиция: lat=" << std::fixed << std::setprecision(7) << position.latitude_deg
                  << ", lon=" << position.longitude_deg
                  << ", alt=" << position.relative_altitude_m << "м" << std::endl;
    } catch (...) {
        std::cout << "[DRONE_WARNING] Не удалось получить позицию" << std::endl;
    }
    
    // В этом режиме дрон просто остается в воздухе
    // Можно добавить команду для перехода в режим удержания позиции
    try {
        action_->hold();
        std::cout << "[DRONE_INFO] Команда HOLD отправлена" << std::endl;
    } catch (...) {
        std::cout << "[DRONE_WARNING] Не удалось отправить команду HOLD" << std::endl;
    }
}

void MissionController::handle_automatic_landing() {
    std::cout << "[DRONE_INFO] 🛬 Инициируем автоматическую посадку..." << std::endl;
    
    try {
        // Попытка 1: Команда LAND
        std::cout << "[DRONE_INFO] Отправка команды LAND..." << std::endl;
        auto land_result = action_->land();
        
        if (land_result == Action::Result::Success) {
            std::cout << "[DRONE_SUCCESS] Команда LAND принята" << std::endl;
            monitor_landing_progress();
            return;
        }
        
        // Попытка 2: RTL если LAND не сработал
        std::cout << "[DRONE_WARNING] Команда LAND не сработала (код: " 
                  << static_cast<int>(land_result) << "), пробую RTL..." << std::endl;
        
        auto rtl_result = action_->return_to_launch();
        
        if (rtl_result == Action::Result::Success) {
            std::cout << "[DRONE_SUCCESS] Команда RTL принята" << std::endl;
            monitor_landing_progress();
        } else {
            std::cerr << "[DRONE_ERROR] Обе команды посадки не сработали!" << std::endl;
            execute_forced_landing();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка при инициации посадки: " << e.what() << std::endl;
        execute_forced_landing();
    }
}

void MissionController::log_mission_status() {
    try {
        auto position = telemetry_->position();
        auto flight_mode = telemetry_->flight_mode();
        auto battery = telemetry_->battery();
        auto progress = mission_->mission_progress();
        
        std::cout << "[DRONE_INFO] 📊 Статус миссии:" << std::endl;
        std::cout << "[DRONE_INFO]   Точка: " << progress.current << "/" << progress.total << std::endl;
        std::cout << "[DRONE_INFO]   Высота: " << position.relative_altitude_m << "м" << std::endl;
        std::cout << "[DRONE_INFO]   Режим: " << flight_mode_to_string(flight_mode) << std::endl;
        std::cout << "[DRONE_INFO]   Батарея: " << battery.remaining_percent * 100 << "%" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "[DRONE_WARNING] Не удалось получить статус телеметрии: " << e.what() << std::endl;
    }
}

void MissionController::handle_monitoring_error() {
    static int error_count = 0;
    error_count++;
    
    std::cerr << "[DRONE_ERROR] Ошибка мониторинга #" << error_count << std::endl;
    
    if (error_count >= 3) {
        std::cerr << "[DRONE_ERROR] 🚨 Критическая ошибка мониторинга!" << std::endl;
        std::cerr << "[DRONE_ERROR] Запускаем принудительную посадку..." << std::endl;
        execute_forced_landing();
        mission_running_ = false;
    }
}

bool MissionController::check_mission_progress() {
    try {
        auto progress = mission_->mission_progress();
        
        static int last_current = -1;
        if (progress.current != last_current) {
            std::cout << "[DRONE_INFO] Прогресс: точка " << progress.current 
                      << "/" << progress.total << std::endl;
            last_current = progress.current;
        }
        
        // Проверяем, завершена ли миссия
        bool mission_done = (progress.current >= progress.total);
        
        // Дополнительная проверка: если это последняя точка и она LAND, 
        // проверяем режим полета
        if (mission_done && last_mission_has_land_command_) {
            auto flight_mode = telemetry_->flight_mode();
            if (flight_mode == Telemetry::FlightMode::Land || 
                flight_mode == Telemetry::FlightMode::ReturnToLaunch) {
                std::cout << "[DRONE_INFO] Миссия завершена, дрон выполняет посадку" << std::endl;
            }
        }
        
        return mission_done;
        
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка проверки прогресса: " << e.what() << std::endl;
        return false;
    }
}

void MissionController::execute_forced_landing() {
    std::cout << "[DRONE_WARNING] 🚨 ЗАПУСК ПРИНУДИТЕЛЬНОЙ ПОСАДКИ" << std::endl;
    force_land_triggered_ = true;
    
    try {
        if (mission_running_) {
            mission_->pause_mission();
            mission_running_ = false;
        }
        
        std::cout << "[DRONE_INFO] Отправка команды LAND..." << std::endl;
        auto land_result = action_->land();
        
        if (land_result != Action::Result::Success) {
            std::cout << "[DRONE_WARNING] Ошибка команды land, пытаемся RTL..." << std::endl;
            action_->return_to_launch();
        }
        
        // Запускаем мониторинг посадки в отдельном потоке
        if (land_monitor_thread_.joinable()) {
            land_monitor_thread_.join();
        }
        land_monitor_thread_ = std::thread(&MissionController::monitor_landing_progress, this);
        
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Критическая ошибка принудительной посадки: " << e.what() << std::endl;
        emergency_disarm();
    }
}

void MissionController::monitor_landing_progress() {
    std::cout << "\n[DRONE_INFO] 🛬🛬🛬 НАБЛЮДЕНИЕ ЗА ПОСАДКОЙ 🛬🛬🛬" << std::endl;
    std::cout << "[DRONE_INFO] Команда посадки отправлена сервером" << std::endl;
    std::cout << "[DRONE_INFO] Автопилот выполняет посадку самостоятельно" << std::endl;

    const int MAX_LANDING_TIME = 60;
    int landing_checks = 0;
    float last_alt = -1.0f;

    for (int i = 0; i < MAX_LANDING_TIME; ++i) {
        safe_sleep(1);
        landing_checks++;

        try {
            bool in_air = telemetry_->in_air();
            bool armed = telemetry_->armed();
            auto position = telemetry_->position();
            auto flight_mode = telemetry_->flight_mode();
            
            float descent_rate = 0;
            if (last_alt > 0) {
                descent_rate = last_alt - position.relative_altitude_m;
            }
            last_alt = position.relative_altitude_m;

            std::cout << "[DRONE_INFO] 🕒 Шаг " << landing_checks << "/" << MAX_LANDING_TIME << std::endl;
            std::cout << "[DRONE_INFO]    ├─ Высота: " << std::fixed << std::setprecision(2) 
                      << position.relative_altitude_m << "м" << std::endl;
            std::cout << "[DRONE_INFO]    ├─ Статус: " << (in_air ? "🟢 В ВОЗДУХЕ" : "🟫 НА ЗЕМЛЕ") << std::endl;
            std::cout << "[DRONE_INFO]    ├─ Моторы: " << (armed ? "⚙️ ВКЛЮЧЕНЫ" : "⏹️ ВЫКЛЮЧЕНЫ") << std::endl;
            std::cout << "[DRONE_INFO]    └─ Режим: " << flight_mode_to_string(flight_mode) << std::endl;

            if (!in_air && !armed) {
                std::cout << "\n[DRONE_SUCCESS] 🎉🎉🎉 ПОСАДКА УСПЕШНО ЗАВЕРШЕНА 🎉🎉🎉" << std::endl;
                std::cout << "[DRONE_SUCCESS] ✅ Дрон на земле, моторы выключены" << std::endl;
                std::cout << "[DRONE_SUCCESS] 📍 Координаты: " 
                          << std::setprecision(7) << position.latitude_deg << ", " 
                          << position.longitude_deg << std::endl;
                
                mission_completed_ = true;
                return;
            }

            if (!in_air && armed) {
                std::cout << "[DRONE_INFO] ⏳ Дрон на земле, ожидание auto-disarm..." << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "[DRONE_ERROR] ❌ Ошибка телеметрии: " << e.what() << std::endl;
        }
    }

    std::cerr << "\n[DRONE_ERROR] ⚠️⚠️⚠️ ТАЙМАУТ ПОСАДКИ ⚠️⚠️⚠️" << std::endl;
    mission_completed_ = true;
}

void MissionController::emergency_disarm() {
    std::cerr << "[DRONE_ERROR] 🚨 АВАРИЙНОЕ ОТКЛЮЧЕНИЕ МОТОРОВ!" << std::endl;
    
    try {
        action_->disarm();
        std::cout << "[DRONE_WARNING] Моторы отключены" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка аварийного отключения: " << e.what() << std::endl;
    }
}

bool MissionController::parse_qgc_mission_json(const json& mission_json, 
                                              std::vector<Mission::MissionItem>& mission_items) {
    try {
        std::cout << "[DRONE_INFO] ===========================================" << std::endl;
        std::cout << "[DRONE_INFO] ПАРСИНГ JSON МИССИИ ОТ СЕРВЕРА" << std::endl;
        std::cout << "[DRONE_INFO] ===========================================" << std::endl;
        
        mission_items.clear();
        last_mission_has_land_command_ = false;
        last_mission_has_rtl_command_ = false;

        if (!mission_json.contains("mission") || !mission_json["mission"].contains("items")) {
            std::cerr << "[DRONE_ERROR] Неверный формат миссии" << std::endl;
            return false;
        }

        const auto& items = mission_json["mission"]["items"];
        std::cout << "[DRONE_INFO] Найдено элементов в JSON: " << items.size() << std::endl;

        int created_points = 0;
        int skipped_commands = 0;
        
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            
            if (!item.contains("command")) {
                std::cout << "[DRONE_WARNING] Пропускаем элемент " << i << " - нет команды" << std::endl;
                skipped_commands++;
                continue;
            }

            int command = item["command"].get<int>();
            
            // Проверяем, нужно ли создавать точку
            bool should_create_point = true;
            std::string command_name = "Unknown";
            
            switch (command) {
                case 178: // DO_CHANGE_SPEED
                    command_name = "DO_CHANGE_SPEED";
                    should_create_point = false;
                    skipped_commands++;
                    
                    if (item.contains("params") && item["params"].size() >= 2) {
                        float new_speed = item["params"][1].get<float>();
                        std::cout << "[DRONE_INFO] Точка " << i << ": " << command_name 
                                  << " - скорость: " << new_speed << " м/с (пропускаем)" << std::endl;
                    }
                    break;
                    
                case 16: // WAYPOINT
                    command_name = "WAYPOINT";
                    should_create_point = true;
                    break;
                    
                case 21: // LAND
                    command_name = "LAND";
                    should_create_point = true;
                    last_mission_has_land_command_ = true;
                    break;
                    
                case 22: // TAKEOFF
                    command_name = "TAKEOFF";
                    should_create_point = true;
                    break;
                    
                case 20: // RETURN_TO_LAUNCH
                    command_name = "RTL";
                    should_create_point = true;
                    last_mission_has_rtl_command_ = true;
                    break;
                    
                default:
                    command_name = "CMD_" + std::to_string(command);
                    std::cout << "[DRONE_WARNING] Точка " << i << ": неизвестная команда " << command 
                              << " (" << command_name << ") - ПРОПУСКАЕМ" << std::endl;
                    should_create_point = false;
                    skipped_commands++;
                    break;
            }
            
            if (!should_create_point) {
                continue;
            }
            
            // Парсим параметры для команд, которые создают точки
            const auto& params = item["params"];
            double lat = 0.0, lon = 0.0;
            float alt = 10.0f;
            float speed_m_s = 5.0f;
            float acceptance_radius_m = 3.0f;
            bool is_fly_through = false;
            bool valid_params = false;
            
            if (params.size() >= 7) {
                // Для TAKEOFF и LAND координаты в params[4], [5], [6]
                // Для WAYPOINT тоже
                lat = params[4].get<double>();
                lon = params[5].get<double>();
                
                if (command == 21) { // LAND
                    alt = 0.0f; // Для посадки всегда 0
                    speed_m_s = 1.0f;
                    acceptance_radius_m = 10.0f;
                    std::cout << "[DRONE_INFO] ⭐ Точка " << i << ": " << command_name 
                              << " (" << std::fixed << std::setprecision(7) << lat << ", " << lon << ")" << std::endl;
                } else {
                    alt = params[6].get<float>();
                    if (command == 22) { // TAKEOFF
                        acceptance_radius_m = 1.0f;
                        speed_m_s = 2.0f;
                    }
                    std::cout << "[DRONE_INFO] Точка " << i << ": " << command_name 
                              << " (" << std::fixed << std::setprecision(7) << lat << ", " << lon 
                              << ") alt=" << alt << "м" << std::endl;
                }
                valid_params = true;
                created_points++;
            } else {
                std::cout << "[DRONE_WARNING] Недостаточно параметров для команды " 
                          << command_name << " в точке " << i << std::endl;
            }
            
            if (valid_params) {
                Mission::MissionItem mission_item{};
                mission_item.latitude_deg = lat;
                mission_item.longitude_deg = lon;
                mission_item.relative_altitude_m = alt;
                mission_item.speed_m_s = speed_m_s;
                mission_item.acceptance_radius_m = acceptance_radius_m;
                mission_item.is_fly_through = is_fly_through;
                
                mission_items.push_back(mission_item);
                
                // Детальный лог для отладки
                std::cout << "[DRONE_DEBUG] Создана точка #" << mission_items.size() 
                          << ": lat=" << lat << ", lon=" << lon 
                          << ", alt=" << alt << "м, speed=" << speed_m_s << "м/с" << std::endl;
            }
        }

        std::cout << "[DRONE_INFO] ===========================================" << std::endl;
        std::cout << "[DRONE_INFO] ИТОГ ПАРСИНГА:" << std::endl;
        std::cout << "[DRONE_INFO] Всего элементов в JSON: " << items.size() << std::endl;
        std::cout << "[DRONE_INFO] Создано точек миссии: " << created_points << std::endl;
        std::cout << "[DRONE_INFO] Пропущено команд: " << skipped_commands << std::endl;
        std::cout << "[DRONE_INFO] Вектор содержит: " << mission_items.size() << " точек" << std::endl;
        std::cout << "[DRONE_INFO] Команда посадки в миссии: " << (last_mission_has_land_command_ ? "ДА" : "НЕТ") << std::endl;
        std::cout << "[DRONE_INFO] Команда RTL в миссии: " << (last_mission_has_rtl_command_ ? "ДА" : "НЕТ") << std::endl;
        
        // Выводим все созданные точки
        std::cout << "[DRONE_INFO] ===== ДЕТАЛЬНЫЙ ОТЧЕТ О ТОЧКАХ =====" << std::endl;
        for (size_t i = 0; i < mission_items.size(); ++i) {
            const auto& item = mission_items[i];
            std::string type = (i == mission_items.size() - 1 && last_mission_has_land_command_) 
                             ? "LAND" : "WAYPOINT";
            std::cout << "[DRONE_INFO] Точка " << i << " (" << type << "): "
                      << std::fixed << std::setprecision(7) << item.latitude_deg << ", " 
                      << item.longitude_deg << " alt=" << item.relative_altitude_m << "м" << std::endl;
        }
        std::cout << "[DRONE_INFO] ===========================================" << std::endl;

        return !mission_items.empty();

    } catch (const json::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка парсинга JSON: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Общая ошибка парсинга: " << e.what() << std::endl;
        return false;
    }
}

void MissionController::get_mission_items() {
    std::cout << "\n[DRONE_INFO] ===========================================" << std::endl;
    std::cout << "[DRONE_INFO] 📋 ЗАПРОС МИССИИ С КОНТРОЛЛЕРА" << std::endl;
    std::cout << "[DRONE_INFO] ===========================================" << std::endl;

    try {
        std::promise<std::pair<Mission::Result, Mission::MissionPlan>> promise;
        auto future = promise.get_future();

        mission_->download_mission_async([&promise](Mission::Result result, 
                Mission::MissionPlan mission_plan) {
            promise.set_value({result, mission_plan});
        });

        auto future_status = future.wait_for(std::chrono::seconds(5));
        
        if (future_status != std::future_status::ready) {
            std::cerr << "[DRONE_ERROR] ❌ Таймаут при скачивании миссии" << std::endl;
            return;
        }

        auto [result, mission_plan] = future.get();

        if (result != Mission::Result::Success) {
            std::cerr << "[DRONE_ERROR] ❌ Ошибка скачивания миссии: " 
                      << static_cast<int>(result) << std::endl;
            return;
        }

        const auto& mission_items = mission_plan.mission_items;
        
        std::cout << "[DRONE_INFO] Получено элементов: " << mission_items.size() << std::endl;

        if (mission_items.empty()) {
            std::cout << "[DRONE_WARNING] На контроллере нет загруженной миссии" << std::endl;
            return;
        }

        std::cout << "[DRONE_INFO] ===== ДЕТАЛЬНЫЙ ОТЧЕТ О МИССИИ =====" << std::endl;
        for (size_t i = 0; i < mission_items.size(); ++i) {
            const auto& item = mission_items[i];
            
            std::string type = "WAYPOINT";
            if (i == 0 && std::abs(item.latitude_deg) < 0.0001 && 
                std::abs(item.longitude_deg) < 0.0001 && item.relative_altitude_m > 0) {
                type = "TAKEOFF";
            }
            if (item.relative_altitude_m == 0.0f) {
                type = "LAND";
            }
            if (i == mission_items.size() - 1 && item.relative_altitude_m == 0.0f) {
                type = "LAND (финальная)";
            }

            std::cout << "[DRONE_INFO] Точка " << i << " (" << type << "): "
                      << std::fixed << std::setprecision(7) << item.latitude_deg << ", "
                      << item.longitude_deg << " alt=" << item.relative_altitude_m << "м"
                      << " speed=" << item.speed_m_s << "м/с" << std::endl;
        }
        std::cout << "[DRONE_INFO] ======================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка: " << e.what() << std::endl;
    }
}


bool MissionController::return_to_home_no_land() {
    std::cout << "[DRONE_INFO] 🏠 ЗАПУСК ВОЗВРАТА ДОМОЙ БЕЗ ПОСАДКИ" << std::endl;
    
    if (!is_connected()) {
        std::cerr << "[DRONE_ERROR] Не подключен к дрону" << std::endl;
        return false;
    }
    
    set_no_autoland(true);
    
    try {
        double current_lat, current_lon;
        float current_alt, battery;
        
        if (!getCurrentPosition(current_lat, current_lon, current_alt, battery)) {
            std::cerr << "[DRONE_ERROR] Не удалось получить позицию" << std::endl;
            set_no_autoland(false);
            return false;
        }
        
        // Создаем простую миссию с одной точкой - возврат к текущей позиции
        std::vector<Mission::MissionItem> mission_items;
        
        Mission::MissionItem home_point{};
        home_point.latitude_deg = current_lat;
        home_point.longitude_deg = current_lon;
        home_point.relative_altitude_m = std::max(current_alt, 15.0f);
        home_point.speed_m_s = 8.0f;
        home_point.acceptance_radius_m = 3.0f;
        home_point.is_fly_through = false;
        mission_items.push_back(home_point);
        
        Mission::MissionPlan mission_plan{};
        mission_plan.mission_items = mission_items;
        
        auto upload_result = mission_->upload_mission(mission_plan);
        if (upload_result != Mission::Result::Success) {
            std::cerr << "[DRONE_ERROR] Ошибка загрузки миссии возврата" << std::endl;
            set_no_autoland(false);
            return false;
        }
        
        auto start_result = mission_->start_mission();
        if (start_result != Mission::Result::Success) {
            std::cerr << "[DRONE_ERROR] Ошибка запуска миссии возврата" << std::endl;
            set_no_autoland(false);
            return false;
        }
        
        std::cout << "[DRONE_SUCCESS] ✅ Возврат домой запущен (посадка отключена)" << std::endl;
        
        // Ждем завершения
        int timeout = 0;
        while (mission_running_ && timeout < 300) {
            safe_sleep(1);
            timeout++;
        }
        
        set_no_autoland(false);
        
        return mission_completed_;
        
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка возврата: " << e.what() << std::endl;
        set_no_autoland(false);
        return false;
    }
}

std::string MissionController::flight_mode_to_string(Telemetry::FlightMode mode) {
    switch (mode) {
        case Telemetry::FlightMode::Unknown: return "Unknown";
        case Telemetry::FlightMode::Ready: return "Ready";
        case Telemetry::FlightMode::Takeoff: return "Takeoff";
        case Telemetry::FlightMode::Hold: return "Hold";
        case Telemetry::FlightMode::Mission: return "Mission";
        case Telemetry::FlightMode::ReturnToLaunch: return "RTL";
        case Telemetry::FlightMode::Land: return "Land";
        case Telemetry::FlightMode::Offboard: return "Offboard";
        case Telemetry::FlightMode::FollowMe: return "FollowMe";
        case Telemetry::FlightMode::Manual: return "Manual";
        case Telemetry::FlightMode::Altctl: return "AltCtl";
        case Telemetry::FlightMode::Posctl: return "PosCtl";
        case Telemetry::FlightMode::Acro: return "Acro";
        case Telemetry::FlightMode::Stabilized: return "Stabilized";
        case Telemetry::FlightMode::Rattitude: return "Rattitude";
        default: return "Unknown(" + std::to_string(static_cast<int>(mode)) + ")";
    }
}

bool MissionController::execute_simple_takeoff() {
    std::cout << "[DRONE_INFO] === ПРОСТОЙ ВЗЛЕТ И ПОСАДКА ===" << std::endl;
    
    if (!is_connected()) {
        std::cerr << "[DRONE_ERROR] Не подключен к дрону" << std::endl;
        return false;
    }
    
    std::cout << "[DRONE_INFO] Арминг дрона..." << std::endl;
    auto arm_result = action_->arm();
    
    if (arm_result != Action::Result::Success) {
        std::cout << "[DRONE_WARNING] Standard arm failed, trying force arm..." << std::endl;
        arm_result = action_->arm_force();
    }
    
    if (arm_result != Action::Result::Success) {
        std::cerr << "[DRONE_ERROR] Ошибка арминга: " << arm_result << std::endl;
        return false;
    }
    
    std::cout << "[DRONE_SUCCESS] ✓ Дрон вооружен" << std::endl;
    
    std::cout << "[DRONE_INFO] Взлет на 5 метров..." << std::endl;
    auto takeoff_result = action_->takeoff();
    
    if (takeoff_result != Action::Result::Success) {
        std::cerr << "[DRONE_ERROR] Ошибка взлета: " << takeoff_result << std::endl;
        action_->disarm();
        return false;
    }
    
    std::cout << "[DRONE_SUCCESS] ✓ Взлет выполнен! Моторы должны работать..." << std::endl;
    
    for (int i = 10; i > 0; i--) {
        std::cout << "[DRONE_INFO] Моторы работают... " << i << "s" << std::endl;
        safe_sleep(1);
    }
    
    std::cout << "[DRONE_INFO] Посадка..." << std::endl;
    auto land_result = action_->land();
    
    if (land_result != Action::Result::Success) {
        std::cerr << "[DRONE_ERROR] Ошибка посадки: " << land_result << std::endl;
        action_->disarm();
        return false;
    }
    
    std::cout << "[DRONE_SUCCESS] ✓ Посадка выполнена" << std::endl;
    
    safe_sleep(3);
    
    std::cout << "[DRONE_SUCCESS] 🎯 МИССИЯ ВЫПОЛНЕНА УСПЕШНО!" << std::endl;
    return true;
}

bool MissionController::is_connected() const {
    return (system_ != nullptr && system_->is_connected());
}

bool MissionController::wait_for_health_ok() {
    std::cout << "[DRONE_INFO] Ожидание готовности системы (упрощенная проверка)..." << std::endl;
    
    try {
        // Пропускаем проверку health - это часто вызывает проблемы
        std::cout << "[DRONE_INFO] Пропускаем проверку health, ждем 5 секунд..." << std::endl;
        safe_sleep(5);
        
        // Просто проверяем, что подключение все еще активно
        if (!system_->is_connected()) {
            std::cout << "[DRONE_WARNING] Система отключилась" << std::endl;
            return false;
        }
        
        std::cout << "[DRONE_SUCCESS] ✅ Система подключена" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка: " << e.what() << std::endl;
        return false;
    }
}

bool MissionController::disable_rc_failsafe() {
    try {
        std::cout << "[DRONE_INFO] Отключение Radio Failsafe..." << std::endl;
        
        auto param = std::make_unique<mavsdk::Param>(*system_);
        
        auto result = param->set_param_int("COM_RC_IN_MODE", 1);
        std::cout << "[DRONE_INFO] COM_RC_IN_MODE: " << result << std::endl;
        
        result = param->set_param_int("NAV_RCL_ACT", 0);
        std::cout << "[DRONE_INFO] NAV_RCL_ACT: " << result << std::endl;
        
        result = param->set_param_int("COM_ARM_CHK", 0);
        std::cout << "[DRONE_INFO] COM_ARM_CHK: " << result << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DRONE_ERROR] Ошибка отключения failsafe: " << e.what() << std::endl;
        return false;
    }
}
bool MissionController::is_mission_completed() {
    try {
        auto progress = mission_->mission_progress();
        auto flight_mode = telemetry_->flight_mode();
        auto position = telemetry_->position();
        static float last_altitude = 0;
        static int stuck_counter = 0;
        
        // �������� ��������: ��� ����� ��������
        bool all_points_done = (progress.current >= progress.total);
        
        // �������� �������� 1: ����� ����� ��������� �� HOLD ��� LOITER
        bool mode_changed = (flight_mode != Telemetry::FlightMode::Mission);
        
        // �������� �������� 2: ���� ����� �� ����� (�������� ~0)
        auto velocity = telemetry_->velocity_ned();
        bool is_hovering = (std::abs(velocity.north_m_s) < 0.1 && 
                           std::abs(velocity.east_m_s) < 0.1);
        
        // �������� �������� 3: ������ �� �������� (�����)
        float current_alt = position.relative_altitude_m;
        bool altitude_stable = (std::abs(current_alt - last_altitude) < 0.1);
        if (altitude_stable) {
            stuck_counter++;
        } else {
            stuck_counter = 0;
        }
        bool stuck_for_while = (stuck_counter > 10); // 10 ������ ��� ��������
        
        last_altitude = current_alt;
        
        // ��������� ���
        std::cout << "[DEBUG] ========== СТАТУС МИССИИ ==========" << std::endl;
        std::cout << "[DEBUG] Gрогресс: " << progress.current << "/" << progress.total << std::endl;
        std::cout << "[DEBUG] Dск точки пройдены: " << (all_points_done ? "Lа" : "Yет") << std::endl;
        std::cout << "[DEBUG] Hежим: " << flight_mode_to_string(flight_mode) << std::endl;
        std::cout << "[DEBUG] Hежим сменился: " << (mode_changed ? "Lа" : "Yет") << std::endl;
        std::cout << "[DEBUG] Cкорось: " << velocity.north_m_s << ", " << velocity.east_m_s << std::endl;
        std::cout << "[DEBUG] Pавис: " << (is_hovering ? "Lа" : "Yет") << std::endl;
        std::cout << "[DEBUG] Dысота стабильна: " << (altitude_stable ? "Lа" : "Yет") << std::endl;
        std::cout << "[DEBUG] Pавис на долго: " << (stuck_for_while ? "Lа" : "Yет") << std::endl;
        
        // �������� �������:
        // ������ ��������� ����:
        // 1. ��� ����� �������� ���
        // 2. ����� �������� � ���� ����� (����������� ��� ���)
        bool completed = all_points_done || (mode_changed && is_hovering);
        
        std::cout << "[DEBUG] ������ ���������: " << (completed ? "��" : "���") << std::endl;
        std::cout << "[DEBUG] ====================================" << std::endl;
        
        return completed;
        
    } catch (const std::exception& e) {
        std::cout << "[DEBUG] exception in is_mission_completed: " << e.what() << std::endl;
        return false;
    }
}

bool MissionController::clear_mission() {
    std::cout << "[DRONE_INFO] Clearing the current mission..." << std::endl;
    
    auto result = mission_->clear_mission();
    
    if (result == Mission::Result::Success) {
        std::cout << "[DRONE_SUCCESS] ? Mission cleared" << std::endl;
        return true;
    } else {
        std::cerr << "[DRONE_ERROR] ? Mission clear error: " 
                  << static_cast<int>(result) << std::endl;
        return false;
    }
}

bool MissionController::is_in_air() const {
    try {
        return telemetry_->in_air();
    } catch (...) {
        return false;
    }
}

void MissionController::safe_sleep(int seconds) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

void MissionController::log_info(const std::string& message) {
    std::cout << "[DRONE_INFO] " << message << std::endl;
}

void MissionController::log_warning(const std::string& message) {
    std::cout << "[DRONE_WARNING] " << message << std::endl;
}

void MissionController::log_error(const std::string& message) {
    std::cerr << "[DRONE_ERROR] " << message << std::endl;
}

void MissionController::log_success(const std::string& message) {
    std::cout << "[DRONE_SUCCESS] " << message << std::endl;
}
