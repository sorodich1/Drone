#include "DroneController.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono;
using namespace std::this_thread;

DroneController::DroneController() {
    // Инициализация будет в connect
}

DroneController::~DroneController() {
    if (isArmed() && action_) {
        land();
        waitForLanding(10);
    }
}

bool DroneController::connect(const std::string& connection_url) {
    logStatus("Connecting to: " + connection_url);
    
    // Инициализация MAVSDK как в рабочем коде
    mavsdk_ = std::make_unique<mavsdk::Mavsdk>(mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation});
    auto connection_result = mavsdk_->add_any_connection(connection_url);

    if (connection_result != mavsdk::ConnectionResult::Success) {
        logStatus("Connection failed");
        return false;
    }

    logStatus("Waiting for autopilot...");
    auto system = mavsdk_->first_autopilot(10.0);
    if (!system) {
        logStatus("Timed out waiting for autopilot");
        return false;
    }
    
    system_ = system.value();
    logStatus("✓ Connected to autopilot");

    // Инициализация плагинов как в рабочем коде
    telemetry_ = std::make_unique<mavsdk::Telemetry>(system_);
    action_ = std::make_unique<mavsdk::Action>(system_);

    // Настройка телеметрии
    telemetry_->set_rate_position(2.0);

    connected_ = true;
    return true;
}

bool DroneController::executeMission(const FlightMission& mission) {
    if (!connected_) {
        logStatus("Error: Not connected to drone");
        return false;
    }

    logStatus("=== STARTING MISSION: " + mission.mission_name + " ===");
    logStatus("Target altitude: " + std::to_string(mission.target_altitude) + "m");
    logStatus("Hover time: " + std::to_string(mission.hover_time) + "s");

    // Pre-flight checks
    if (!preFlightChecks()) {
        logStatus("Pre-flight checks failed");
        return false;
    }

    // === ВЗЛЕТ ===
    logStatus("=== TAKEOFF SEQUENCE ===");
    
    if (!arm()) {
        logStatus("Arming failed");
        return false;
    }

    if (!takeoff(mission.target_altitude)) {
        logStatus("Takeoff failed");
        return false;
    }

    if (!waitForAltitude(mission.target_altitude)) {
        logStatus("Failed to reach target altitude");
        return false;
    }

    // === ПАУЗА НА ВЕРШИНЕ ===
    logStatus("=== HOVERING ===");
    logStatus("Hovering at " + std::to_string(mission.target_altitude) + "m for " + 
              std::to_string(mission.hover_time) + " seconds...");
    
    for (int i = mission.hover_time; i > 0; --i) {
        auto position = telemetry_->position();
        logStatus("Hovering... " + std::to_string(i) + "s remaining (Alt: " + 
                 std::to_string(position.relative_altitude_m) + "m)");
        sleep_for(seconds(1));
    }

    // === ПОСАДКА ===
    logStatus("=== LANDING ===");
    if (!land()) {
        logStatus("Land command failed");
        return false;
    }

    if (!waitForLanding()) {
        logStatus("Landing timeout");
        return false;
    }

    logStatus("✓ Mission completed successfully!");
    return true;
}

bool DroneController::preFlightChecks() {
    logStatus("=== PRE-FLIGHT CHECKS ===");
    
    // Проверка GPS
    auto gps_info = telemetry_->gps_info();
    logStatus("GPS: " + std::to_string(gps_info.num_satellites) + " satellites");
    
    // Проверка здоровья системы
    auto health = telemetry_->health();
    logStatus("Sensors - Gyro: " + std::string(health.is_gyrometer_calibration_ok ? "OK" : "BAD") +
              ", Accel: " + std::string(health.is_accelerometer_calibration_ok ? "OK" : "BAD"));
    
    // Проверка батареи
    auto battery = telemetry_->battery();
    logStatus("Battery: " + std::to_string(battery.remaining_percent * 100) + "%");

    return (health.is_global_position_ok && health.is_home_position_ok);
}

bool DroneController::arm() {
    logStatus("Arming motors...");
    auto arm_result = action_->arm();
    
    if (arm_result != mavsdk::Action::Result::Success) {
        logStatus("Arming failed");
        logStatus("Trying force arm...");
        arm_result = action_->arm_force();
    }
    
    if (arm_result != mavsdk::Action::Result::Success) {
        logStatus("Force arming also failed");
        return false;
    }
    
    logStatus("✓ Motors armed");
    return true;
}

bool DroneController::takeoff(double altitude) {
    logStatus("Taking off to " + std::to_string(altitude) + " meters...");
    auto takeoff_result = action_->takeoff();
    
    if (takeoff_result != mavsdk::Action::Result::Success) {
        logStatus("Takeoff failed");
        action_->disarm();
        return false;
    }
    
    logStatus("✓ Takeoff command accepted");
    return true;
}

bool DroneController::land() {
    logStatus("Initiating landing...");
    auto land_result = action_->land();
    
    if (land_result != mavsdk::Action::Result::Success) {
        logStatus("Land command failed");
        return false;
    }
    
    logStatus("✓ Landing command accepted");
    return true;
}

bool DroneController::waitForAltitude(double target_altitude, int timeout_seconds) {
    logStatus("Ascending to target altitude...");
    bool reached_target = false;
    
    for (int i = 0; i < timeout_seconds; ++i) {
        auto position = telemetry_->position();
        double current_alt = position.relative_altitude_m;
        
        logStatus("Altitude: " + std::to_string(current_alt) + "m / " + std::to_string(target_altitude) + "m");
        
        if (current_alt >= target_altitude - 0.5) {
            logStatus("✓ REACHED TARGET ALTITUDE");
            reached_target = true;
            break;
        }
        
        sleep_for(seconds(1));
    }
    
    if (!reached_target) {
        logStatus("⚠️ Warning: May not have reached target altitude");
    }
    
    return reached_target;
}

bool DroneController::waitForLanding(int timeout_seconds) {
    logStatus("Waiting for landing...");
    
    for (int i = 0; i < timeout_seconds; ++i) {
        if (!telemetry_->in_air()) {
            logStatus("✓ Landed successfully!");
            
            // Ожидание автоматического disarm
            sleep_for(seconds(3));
            
            // Финальная проверка
            if (telemetry_->armed()) {
                logStatus("System still armed, forcing disarm...");
                action_->disarm();
            }
            
            return true;
        }
        
        auto position = telemetry_->position();
        logStatus("Descending... Altitude: " + std::to_string(position.relative_altitude_m) + "m");
        sleep_for(seconds(1));
    }
    
    logStatus("Landing timeout exceeded");
    return false;
}

void DroneController::setStatusCallback(StatusCallback callback) {
    status_callback_ = callback;
}

void DroneController::logStatus(const std::string& message) {
    std::cout << "[DRONE] " << message << std::endl;
    if (status_callback_) {
        status_callback_(message);
    }
}

bool DroneController::isConnected() const { 
    return connected_; 
}

bool DroneController::isArmed() const { 
    return telemetry_ ? telemetry_->armed() : false; 
}

bool DroneController::isInAir() const { 
    return telemetry_ ? telemetry_->in_air() : false; 
}

double DroneController::getAltitude() const { 
    return telemetry_ ? telemetry_->position().relative_altitude_m : 0.0; 
}
