#include "PositionSender.h"
#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

PositionSender::PositionSender() 
    : server_port_(0) {
    std::cout << "[TELEMETRY] PositionSender initialized" << std::endl;
}

PositionSender::~PositionSender() {
    stopStreaming();
}

bool PositionSender::initFromSystem(std::shared_ptr<mavsdk::System> system) {
    try {
        telemetry_ = std::make_shared<mavsdk::Telemetry>(system);
        
        // Настраиваем частоту обновления
        telemetry_->set_rate_position(5.0);
        telemetry_->set_rate_battery(5.0);
        telemetry_->set_rate_gps_info(5.0);
        
        std::cout << "[TELEMETRY_SUCCESS] ✓ Telemetry initialized from existing system" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TELEMETRY_ERROR] Failed to init telemetry: " << e.what() << std::endl;
        return false;
    }
}

bool PositionSender::is_connected() const {
    return telemetry_ != nullptr;
}

bool PositionSender::is_streaming() const {
    return streaming_active_;
}

void PositionSender::setServerInfo(const std::string& host, int port, const std::string& endpoint) {
    server_host_ = host;
    server_port_ = port;
    endpoint_ = endpoint;
    std::cout << "[TELEMETRY] Server set to: " << host << ":" << port << endpoint << std::endl;
}

bool PositionSender::sendCurrentTelemetry() {
    std::cout << "[TELEMETRY_DEBUG] === НАЧАЛО ОТПРАВКИ ТЕЛЕМЕТРИИ ===" << std::endl;
    
    if (!telemetry_) {
        std::cerr << "[TELEMETRY_ERROR] Telemetry not initialized" << std::endl;
        return false;
    }
    
    if (server_host_.empty() || server_port_ == 0) {
        std::cerr << "[TELEMETRY_ERROR] Server not configured" << std::endl;
        return false;
    }
    
    try {
        std::cout << "[TELEMETRY_DEBUG] Получение данных телеметрии..." << std::endl;
        auto data = getCurrentTelemetryData();
        std::cout << "[TELEMETRY_DEBUG] Данные получены" << std::endl;
        
        std::string json_data = createTelemetryJson(data);
        
        std::cout << "[TELEMETRY_DEBUG] Отправка на сервер..." << std::endl;
        bool result = sendToServer(json_data);
        std::cout << "[TELEMETRY_DEBUG] Результат отправки: " << (result ? "УСПЕХ" : "ОШИБКА") << std::endl;
        
        std::cout << "[TELEMETRY_DEBUG] === КОНЕЦ ОТПРАВКИ ТЕЛЕМЕТРИИ ===" << std::endl;
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "[TELEMETRY_ERROR] Exception in sendCurrentTelemetry: " << e.what() << std::endl;
        return false;
    }
}

PositionSender::TelemetryData PositionSender::getCurrentTelemetryData() {
    TelemetryData data;
    
    try {
        // Позиция
        auto position = telemetry_->position();
        data.latitude = position.latitude_deg;
        data.longitude = position.longitude_deg;
        data.relative_altitude = position.relative_altitude_m;

        // ДЕТАЛЬНОЕ ЛОГИРОВАНИЕ
        std::cout << "[TELEMETRY_RAW] RAW POSITION DATA:" << std::endl;
        std::cout << "[TELEMETRY_RAW]   Latitude: " << position.latitude_deg << std::endl;
        std::cout << "[TELEMETRY_RAW]   Longitude: " << position.longitude_deg << std::endl;
        std::cout << "[TELEMETRY_RAW]   Absolute Alt: " << position.absolute_altitude_m << std::endl;
        std::cout << "[TELEMETRY_RAW]   Relative Alt: " << position.relative_altitude_m << std::endl;

        // ВАЛИДАЦИЯ ВЫСОТЫ
        float raw_altitude = position.absolute_altitude_m;
        
        // Фильтруем явные глюки (высоты более 9000м или менее -1000м)
        if (raw_altitude > 9000.0f || raw_altitude < -1000.0f) {
            std::cerr << "[TELEMETRY_WARNING] Нереальная высота: " 
                      << raw_altitude << " м. Использую relative + 30м." << std::endl;
            
            // Если absolute_altitude невалидный, используем relative_altitude с базовой высотой
            // 30 метров - приблизительная высота над уровнем моря для СПб
            data.altitude = 30.0f + position.relative_altitude_m;
        } else {
            data.altitude = raw_altitude;
        }
        
        // Батарея
        auto battery = telemetry_->battery();
        data.battery_percentage = battery.remaining_percent * 100.0;
        data.battery_voltage = battery.voltage_v;
        
        // GPS
        auto gps_info = telemetry_->gps_info();
        data.satellites = gps_info.num_satellites;
        
        // Статус GPS
        switch (gps_info.fix_type) {
            case mavsdk::Telemetry::FixType::NoGps:
                data.gps_status = "NO_GPS";
                break;
            case mavsdk::Telemetry::FixType::NoFix:
                data.gps_status = "NO_FIX";
                break;
            case mavsdk::Telemetry::FixType::Fix2D:
                data.gps_status = "2D_FIX";
                break;
            case mavsdk::Telemetry::FixType::Fix3D:
                data.gps_status = "3D_FIX";
                break;
            default:
                data.gps_status = "UNKNOWN";
        }
        
        // Датчики (упрощенно)
        data.gyro_healthy = true;
        data.accel_healthy = true;
        data.mag_healthy = true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TELEMETRY_ERROR] Failed to get telemetry: " << e.what() << std::endl;
        // Устанавливаем значения по умолчанию
        data = TelemetryData();
    }
    
    return data;
}

std::string PositionSender::createTelemetryJson(const TelemetryData& data) {
    std::cout << "[TELEMETRY_DEBUG] Создание JSON из данных..." << std::endl;
    std::cout << "[TELEMETRY_DEBUG] Latitude: " << data.latitude << std::endl;
    std::cout << "[TELEMETRY_DEBUG] Longitude: " << data.longitude << std::endl;
    std::cout << "[TELEMETRY_DEBUG] Battery %: " << data.battery_percentage << std::endl;
    
    nlohmann::json json_data;
    
    // ОБЯЗАТЕЛЬНЫЕ поля - должны точно совпадать с DroneTelemetryDto
    json_data["Latitude"] = data.latitude;
    json_data["Longitude"] = data.longitude;
    json_data["Altitude"] = data.altitude;
    json_data["RelativeAltitude"] = data.relative_altitude;
    json_data["BatteryVoltage"] = data.battery_voltage;
    json_data["BatteryPercentage"] = data.battery_percentage;
    json_data["Gyro"] = data.gyro_healthy;
    json_data["Accel"] = data.accel_healthy;
    json_data["Mag"] = data.mag_healthy;
    json_data["GpsStatus"] = data.gps_status;
    json_data["Satellites"] = data.satellites;
    
    // Таймстамп в формате ISO 8601 с Z
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json_data["Timestamp"] = ss.str();  // ВАЖНО: "Timestamp" с заглавной T
    
    std::string result = json_data.dump();
    std::cout << "[TELEMETRY_DEBUG] Создан JSON: " << result << std::endl;
    
    return result;
}

bool PositionSender::sendToServer(const std::string& json_data) {
    std::cout << "[TELEMETRY] Отправка на " << server_host_ << ":" << server_port_ << endpoint_ << std::endl;
    
    try {
        httplib::Client client(server_host_, server_port_);
        client.set_connection_timeout(3);
        
        auto response = client.Post(endpoint_.c_str(), json_data, "application/json");
        
        if (response) {
            // ЛОГИРУЕМ ОТВЕТ СЕРВЕРА!
            std::cout << "[TELEMETRY] Ответ сервера - Статус: " << response->status 
                      << ", Тело: " << response->body << std::endl;
            
            if (response->status == 200) {
                std::cout << "[TELEMETRY_SUCCESS] ✓ Успешно!" << std::endl;
                return true;
            } else {
                std::cerr << "[TELEMETRY_ERROR] ❌ Ошибка сервера: " << response->status << std::endl;
                return false;
            }
        } else {
            std::cerr << "[TELEMETRY_ERROR] ❌ Нет ответа от сервера" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[TELEMETRY_ERROR] Исключение: " << e.what() << std::endl;
        return false;
    }
}

void PositionSender::startStreaming(float interval_seconds) {
    if (!is_connected()) {
        std::cerr << "[TELEMETRY_ERROR] Cannot start streaming: not connected" << std::endl;
        return;
    }
    
    if (streaming_active_) {
        std::cout << "[TELEMETRY_INFO] Streaming already active" << std::endl;
        return;
    }
    
    if (interval_seconds > 0) {
        interval_seconds_ = interval_seconds;
    }
    
    streaming_active_ = true;
    streaming_thread_ = std::thread(&PositionSender::streamingLoop, this);
    
    std::cout << "[TELEMETRY_INFO] Telemetry streaming started with interval " 
              << interval_seconds_ << "s" << std::endl;
}

void PositionSender::stopStreaming() {
    streaming_active_ = false;
    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
        std::cout << "[TELEMETRY_INFO] Telemetry streaming stopped" << std::endl;
    }
}

void PositionSender::streamingLoop() {
    while (streaming_active_) {
        try {
            sendCurrentTelemetry();
            
            int sleep_ms = static_cast<int>(interval_seconds_ * 1000);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            
        } catch (const std::exception& e) {
            std::cerr << "[TELEMETRY_ERROR] Streaming loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}