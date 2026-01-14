#pragma once
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <functional>
#include <atomic>
#include <memory>
#include <thread>

struct TelemetryData {
    double latitude;
    double longitude;
    float altitude;
    float relative_altitude;
    float battery_percent;
    int satellites;
    bool is_armed;
    bool is_in_air;
};

class TelemetryMonitor {
public:
    using TelemetryCallback = std::function<void(const TelemetryData& data)>;
    
    TelemetryMonitor();
    ~TelemetryMonitor();
    
    bool initialize(std::shared_ptr<mavsdk::System> system);
    void setTelemetryCallback(TelemetryCallback callback);
    void startMonitoring();
    void stopMonitoring();
    
private:
    void monitoringThread();
    
    std::unique_ptr<mavsdk::Telemetry> telemetry_;
    TelemetryCallback telemetry_callback_;
    
    std::atomic<bool> monitoring_{false};
    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
};
