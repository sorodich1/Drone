#include "TakeoffLandController.h"
#include <iostream>  
#include <thread>   
#include <chrono>   

using namespace std::chrono;
using namespace std::this_thread;

TakeoffLandController::TakeoffLandController(std::shared_ptr<mavsdk::System> system) 
    : system_(system) 
{
    initialize_plugins();
}

TakeoffLandController::~TakeoffLandController() {
    std::cout << "Cleaning up TakeoffLandController" << std::endl;
}

bool TakeoffLandController::initialize_plugins() {
    try {
        action_ = std::make_unique<mavsdk::Action>(*system_);
        telemetry_ = std::make_unique<mavsdk::Telemetry>(*system_);
        
        // Настройка телеметрии
        telemetry_->set_rate_position(2.0);
        
        std::cout << "✓ TakeoffLandController plugins initialized" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to initialize TakeoffLandController: " << e.what() << std::endl;
        return false;
    }
}

bool TakeoffLandController::is_connected() const {
    return system_ != nullptr && system_->is_connected();
}

bool TakeoffLandController::pre_flight_checks() {
    std::cout << "\n=== PRE-FLIGHT CHECKS ===" << std::endl;
    
    try {
        // Проверка GPS
        auto gps_info = telemetry_->gps_info();
        std::cout << "GPS: " << gps_info.num_satellites << " satellites, ";
        std::cout << "Fix: " << static_cast<int>(gps_info.fix_type) << std::endl;
        
        // Проверка здоровья системы
        auto health = telemetry_->health();
        std::cout << "Sensors - Gyro: " << (health.is_gyrometer_calibration_ok ? "OK" : "BAD");
        std::cout << ", Accel: " << (health.is_accelerometer_calibration_ok ? "OK" : "BAD") << std::endl;
        
        // Проверка батареи
        auto battery = telemetry_->battery();
        std::cout << "Battery: " << (battery.remaining_percent * 100) << "%" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Pre-flight check failed: " << e.what() << std::endl;
        return false;
    }
}

bool TakeoffLandController::arm_drone() {
    std::cout << "Arming motors..." << std::endl;
    mavsdk::Action::Result arm_result = action_->arm();
    
    if (arm_result != mavsdk::Action::Result::Success) {
        std::cerr << "Standard arm failed: " << arm_result << std::endl;
        std::cout << "Trying force arm..." << std::endl;
        arm_result = action_->arm_force();
    }
    
    if (arm_result != mavsdk::Action::Result::Success) {
        std::cerr << "Force arming also failed: " << arm_result << std::endl;
        return false;
    }
    
    std::cout << "✓ Motors armed" << std::endl;
    return true;
}

bool TakeoffLandController::takeoff_to_altitude(float altitude) {
    std::cout << "Taking off to " << altitude << " meters..." << std::endl;
    mavsdk::Action::Result takeoff_result = action_->takeoff();
    
    if (takeoff_result != mavsdk::Action::Result::Success) {
        std::cerr << "Takeoff failed: " << takeoff_result << std::endl;
        return false;
    }
    
    std::cout << "✓ Takeoff command accepted" << std::endl;
    return true;
}

bool TakeoffLandController::wait_for_altitude(float target_altitude, int timeout_seconds) {
    std::cout << "Ascending to " << target_altitude << "m..." << std::endl;
    bool reached_target = false;
    
    for (int i = 0; i < timeout_seconds; ++i) {
        try {
            auto position = telemetry_->position();
            double current_alt = position.relative_altitude_m;
            
            std::cout << "Altitude: " << current_alt << "m / " << target_altitude << "m" << std::endl;
            
            if (current_alt >= target_altitude - 0.5) {
                std::cout << "✓ REACHED TARGET ALTITUDE" << std::endl;
                reached_target = true;
                break;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error reading altitude: " << e.what() << std::endl;
        }
        
        sleep_for(seconds(1));
    }
    
    return reached_target;
}

bool TakeoffLandController::land_and_wait() {
    std::cout << "Initiating landing..." << std::endl;
    mavsdk::Action::Result land_result = action_->land();
    
    if (land_result != mavsdk::Action::Result::Success) {
        std::cerr << "Land command failed: " << land_result << std::endl;
        return false;
    }
    
    std::cout << "✓ Landing command accepted" << std::endl;
    std::cout << "Descending..." << std::endl;
    
    // Ждем посадки
    for (int i = 0; i < 30; ++i) {
        try {
            if (!telemetry_->in_air()) {
                std::cout << "✓ Landed successfully!" << std::endl;
                return true;
            }
            
            auto position = telemetry_->position();
            std::cout << "Altitude: " << position.relative_altitude_m << "m" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error during landing: " << e.what() << std::endl;
        }
        
        sleep_for(seconds(1));
    }
    
    std::cerr << "⚠️ Landing timeout - drone may still be in air" << std::endl;
    return false;
}

bool TakeoffLandController::execute_takeoff_land_mission(float takeoff_altitude) {
    if (!is_connected()) {
        std::cerr << "❌ Не подключен к дрону" << std::endl;
        return false;
    }
    
    std::cout << "=== АВТОНОМНЫЙ ВЗЛЕТ ===" << std::endl;
    std::cout << "Целевая высота: " << takeoff_altitude << " метров" << std::endl;
    
    // Предполетные проверки
    if (!pre_flight_checks()) {
        std::cerr << "❌ Предполетные проверки не прошли" << std::endl;
        return false;
    }
    
    // Арминг
    if (!arm_drone()) {
        return false;
    }
    
    // Взлет
    if (!takeoff_to_altitude(takeoff_altitude)) {
        action_->disarm();
        return false;
    }
    
    // Ожидание набора высоты
    if (!wait_for_altitude(takeoff_altitude)) {
        std::cout << "⚠️ Взлет выполнен, но целевая высота не достигнута" << std::endl;
    }

    std::cout << "\n✓ Взлет завершен, дрон в воздухе на ~" << takeoff_altitude << "м" << std::endl;
    std::cout << "  Ожидаю команду посадки..." << std::endl;
    
    return true;
}

bool TakeoffLandController::execute_landing_only(){
    if (!is_connected()) return false;

    // Проверяем, что дрон в воздухе
    try {
        if (!telemetry_->in_air()) {
            std::cout << "Дрон уже на земле" << std::endl;
            return true; // Уже на земле - успех
        }
    } catch (...) {
        // Не можем проверить - все равно пытаемся посадить
    }

    std::cout << "\n=== НАЧАЛО ПОСАДКИ ===" << std::endl;

    // Посадка
    if (!land_and_wait()) {
        return false;
    }

    std::cout << "Ожидание auto-disarm..." << std::endl;

    sleep_for(seconds(3));

    try {
        if (telemetry_->armed()) {
            std::cout << "Система все еще находится под охраной, принудительное снятие с охраны..." << std::endl;
            action_->disarm();
        }
    } catch (...) {
        // Игнорируем ошибки
    }

    std::cout << "\n✓ Посадка завершена" << std::endl;
    std::cout << "  Дрон на земле, моторы выключены" << std::endl;

    return true;
}