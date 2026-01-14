// telemetry_monitor.cpp
#include "telemetry_monitor.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std::chrono;
using namespace std::this_thread;

TelemetryMonitor::TelemetryMonitor() = default;

TelemetryMonitor::~TelemetryMonitor() {
    stopMonitoring();
}

bool TelemetryMonitor::initialize(std::shared_ptr<mavsdk::System> system) {
    if (!system) return false;
    
    telemetry_ = std::make_unique<mavsdk::Telemetry>(system);
    
    // Настройка частоты обновления
    telemetry_->set_rate_position(2.0);
    telemetry_->set_rate_gps_info(2.0);
    telemetry_->set_rate_battery(2.0);
    
    return true;
}

void TelemetryMonitor::setTelemetryCallback(TelemetryCallback callback) {
    telemetry_callback_ = callback;
}

void TelemetryMonitor::startMonitoring() {
    if (monitoring_) return;
    
    monitoring_ = true;
    running_ = true;
    monitor_thread_ = std::thread(&TelemetryMonitor::monitoringThread, this);
}

void TelemetryMonitor::stopMonitoring() {
    running_ = false;
    monitoring_ = false;
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void TelemetryMonitor::monitoringThread() {
    while (running_) {
        if (telemetry_ && telemetry_callback_) {
            try {
                TelemetryData data{};
                
                auto position = telemetry_->position();
                data.latitude = position.latitude_deg;
                data.longitude = position.longitude_deg;
                data.altitude = position.absolute_altitude_m;
                data.relative_altitude = position.relative_altitude_m;
                
                auto battery = telemetry_->battery();
                data.battery_percent = battery.remaining_percent * 100.0f;
                
                auto gps_info = telemetry_->gps_info();
                data.satellites = gps_info.num_satellites;
                
                data.is_armed = telemetry_->armed();
                data.is_in_air = telemetry_->in_air();
                
                telemetry_callback_(data);
                
            } catch (const std::exception& e) {
                std::cerr << "Telemetry error: " << e.what() << std::endl;
            }
        }
        sleep_for(milliseconds(1000)); // 1 раз в секунду
    }
}
