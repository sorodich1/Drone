#ifndef POSITION_SENDER_H
#define POSITION_SENDER_H

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <string>
#include <memory>
#include <atomic>
#include <thread>

class PositionSender {
public:
    PositionSender();
    ~PositionSender();
    
    // Инициализация из существующей системы
    bool initFromSystem(std::shared_ptr<mavsdk::System> system);
    
    // Настройка сервера
    void setServerInfo(const std::string& host, int port, const std::string& endpoint = "/api/telemetry");
    
    // Отправка телеметрии
    bool sendCurrentTelemetry();
    
    // Управление потоковой отправкой
    void startStreaming(float interval_seconds = 1.0f);
    void stopStreaming();
    
    // Проверка состояния
    bool is_connected() const;
    bool is_streaming() const;

private:
    std::shared_ptr<mavsdk::Telemetry> telemetry_;
    
    std::string server_host_;
    int server_port_;
    std::string endpoint_;
    
    std::thread streaming_thread_;
    std::atomic<bool> streaming_active_{false};
    std::atomic<float> interval_seconds_{1.0f};
    
    // Внутренние методы
    void streamingLoop();
    
    struct TelemetryData {
        double latitude;
        double longitude;
        double altitude;
        double relative_altitude;
        double battery_voltage;
        double battery_percentage;
        bool gyro_healthy;
        bool accel_healthy;
        bool mag_healthy;
        std::string gps_status;
        int satellites;
    };
    
    TelemetryData getCurrentTelemetryData();
    std::string createTelemetryJson(const TelemetryData& data);
    bool sendToServer(const std::string& json_data);
};

#endif // POSITION_SENDER_H