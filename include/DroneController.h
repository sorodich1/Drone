#ifndef DRONECONTROLLER_H
#define DRONECONTROLLER_H

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <string>
#include <memory>
#include <functional>

struct FlightMission {
    double target_altitude = 5.0;
    double hover_time = 5.0;
    std::string mission_name = "Default Mission";
};

class DroneController {
public:
    DroneController();
    ~DroneController();
    
    bool connect(const std::string& connection_url);
    bool executeMission(const FlightMission& mission);
    bool isConnected() const;
    bool isArmed() const;
    bool isInAir() const;
    double getAltitude() const;
    
    // Callbacks для статуса
    using StatusCallback = std::function<void(const std::string&)>;
    void setStatusCallback(StatusCallback callback);

private:
    std::unique_ptr<mavsdk::Mavsdk> mavsdk_;
    std::shared_ptr<mavsdk::System> system_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;
    std::unique_ptr<mavsdk::Action> action_;
    
    StatusCallback status_callback_;
    bool connected_ = false;
    
    void logStatus(const std::string& message);
    bool preFlightChecks();
    bool arm();
    bool takeoff(double altitude);
    bool land();
    bool waitForAltitude(double target_altitude, int timeout_seconds = 30);
    bool waitForLanding(int timeout_seconds = 60);
};

#endif
