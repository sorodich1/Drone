// drone_manager.cpp
#include "drone_manager.h"
#include <sstream>
#include <iostream>

DroneManager::DroneManager() = default;

DroneManager::~DroneManager() {
    telemetry_monitor_.stopMonitoring();
}

bool DroneManager::initialize(const std::string& connection_url) {
    if (!flight_controller_.initialize(connection_url)) {
        return false;
    }
    
    // Получаем систему из flight controller
    auto system = flight_controller_.getSystem();
    
    if (!system) return false;
    
    // Инициализация менеджеров
    if (!mission_manager_.initialize(system)) {
        std::cerr << "Mission manager init failed" << std::endl;
        return false;
    }
    
    if (!telemetry_monitor_.initialize(system)) {
        std::cerr << "Telemetry monitor init failed" << std::endl;
        return false;
    }
    
    // Устанавливаем callback'и
    flight_controller_.setStatusCallback(
        [this](const std::string& status, float altitude, bool armed) {
            onFlightStatus(status, altitude, armed);
        });
    
    mission_manager_.setMissionCallback(
        [this](const std::string& status, int current, int total) {
            onMissionStatus(status, current, total);
        });
    
    telemetry_monitor_.setTelemetryCallback(
        [this](const TelemetryData& data) {
            onTelemetryUpdate(data);
        });
    
    // Запускаем мониторинг
    telemetry_monitor_.startMonitoring();
    
    return true;
}

void DroneManager::setStatusCallback(StatusCallback callback) {
    status_callback_ = callback;
}

void DroneManager::commandTakeoff(float altitude) {
    flight_controller_.takeoff(altitude);
}

void DroneManager::commandLand() {
    flight_controller_.land();
}

void DroneManager::commandSetAltitude(float altitude) {
    flight_controller_.setAltitude(altitude);
}

void DroneManager::commandEmergencyStop() {
    flight_controller_.emergencyStop();
}

void DroneManager::commandUploadMission(const std::vector<MissionManager::MissionPoint>& points) {
    mission_manager_.uploadMission(points);
}

void DroneManager::commandStartMission() {
    mission_manager_.startMission();
}

void DroneManager::onFlightStatus(const std::string& status, float altitude, bool armed) {
    if (status_callback_) {
        std::stringstream data;
        data << "{\"status\":\"" << status 
             << "\",\"altitude\":" << altitude 
             << ",\"armed\":" << (armed ? "true" : "false") << "}";
        status_callback_("FLIGHT_STATUS", data.str());
    }
}

void DroneManager::onMissionStatus(const std::string& status, int current_item, int total_items) {
    if (status_callback_) {
        std::stringstream data;
        data << "{\"status\":\"" << status 
             << "\",\"current_item\":" << current_item 
             << ",\"total_items\":" << total_items << "}";
        status_callback_("MISSION_STATUS", data.str());
    }
}

void DroneManager::onTelemetryUpdate(const TelemetryData& data) {
    if (status_callback_) {
        std::stringstream json_data;
        json_data << "{"
                  << "\"latitude\":" << data.latitude << ","
                  << "\"longitude\":" << data.longitude << ","
                  << "\"altitude\":" << data.altitude << ","
                  << "\"relative_altitude\":" << data.relative_altitude << ","
                  << "\"battery\":" << data.battery_percent << ","
                  << "\"satellites\":" << data.satellites << ","
                  << "\"armed\":" << (data.is_armed ? "true" : "false") << ","
                  << "\"in_air\":" << (data.is_in_air ? "true" : "false")
                  << "}";
        status_callback_("TELEMETRY", json_data.str());
    }
}
