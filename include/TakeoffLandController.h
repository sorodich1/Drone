#pragma once

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <memory>

class TakeoffLandController {
public:
    TakeoffLandController(std::shared_ptr<mavsdk::System> system);
    ~TakeoffLandController();
    
    bool execute_takeoff_land_mission(float takeoff_altitude);
    bool execute_landing_only();
    bool is_connected() const;
    
private:
    std::shared_ptr<mavsdk::System> system_;
    std::unique_ptr<mavsdk::Action> action_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;
    
    bool initialize_plugins();
    bool pre_flight_checks();
    bool arm_drone();
    bool takeoff_to_altitude(float altitude);
    bool wait_for_altitude(float target_altitude, int timeout_seconds = 30);
    bool land_and_wait();
};