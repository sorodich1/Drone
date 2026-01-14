#pragma once
#include "flight_controller.h"
#include "mission_manager.h"
#include "telemetry_monitor.h"
#include <functional>
#include <memory>
#include <vector>

class DroneManager {
public:
    using StatusCallback = std::function<void(const std::string& type, const std::string& data)>;
    
    DroneManager();
    ~DroneManager();
    
    bool initialize(const std::string& connection_url);
    void setStatusCallback(StatusCallback callback);
    
    // Команды от ASP.NET сервера
    void commandTakeoff(float altitude);
    void commandLand();
    void commandSetAltitude(float altitude);
    void commandEmergencyStop();
    void commandUploadMission(const std::vector<MissionManager::MissionPoint>& points);
    void commandStartMission();
    
    // Статус
    bool isConnected() const { return flight_controller_.isConnected(); }
    
private:
    void onFlightStatus(const std::string& status, float altitude, bool armed);
    void onMissionStatus(const std::string& status, int current_item, int total_items);
    void onTelemetryUpdate(const TelemetryData& data);
    
    FlightController flight_controller_;
    MissionManager mission_manager_;
    TelemetryMonitor telemetry_monitor_;
    
    StatusCallback status_callback_;
};
