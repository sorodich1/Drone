#ifndef MISSIONCONTROLLER_H
#define MISSIONCONTROLLER_H

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/mission/mission.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/param/param.h>

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <iomanip>
#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class MissionController {
public:
    MissionController(std::shared_ptr<mavsdk::Mavsdk> mavsdk);
    ~MissionController();
    
    // Основные методы
    bool connect(const std::string& connection_url);
    bool execute_mission_from_json(const json& mission_json);
    bool return_to_home_no_land();
    bool execute_simple_takeoff();
    bool getCurrentPosition(double& lat, double& lon, float& alt, float& battery);
    std::shared_ptr<mavsdk::System> getSystem() const { return system_; }
    
    // Настройки
    void set_no_autoland(bool value);
    
    // Проверка состояния
    bool is_connected() const;
    
    // Управление миссией
    void start_mission_monitoring();
    void stop_mission_monitoring();
    
    // Служебные методы
    void log_info(const std::string& message);
    void log_warning(const std::string& message);
    void log_error(const std::string& message);
    void log_success(const std::string& message);
    
private:
    // Вспомогательные методы
    void setup_telemetry_rates();
    bool wait_for_health_ok();
    void safe_sleep(int seconds);
    
    // Мониторинг миссии
    void mission_monitor_loop();
    bool check_mission_progress();
    void handle_mission_completion();
    void stop_mission_execution();
    void handle_mission_with_land_command();
    void handle_no_autoland_scenario();
    void handle_automatic_landing();
    void log_mission_status();
    void handle_monitoring_error();
    std::string flight_mode_to_string(mavsdk::Telemetry::FlightMode mode);
    
    // Посадка и аварийные процедуры
    void monitor_landing_progress();
    void execute_forced_landing();
    void emergency_disarm();
    
    // Парсинг миссии
    bool parse_qgc_mission_json(const json& mission_json, 
                                std::vector<mavsdk::Mission::MissionItem>& mission_items);
    
    // Настройка безопасности
    bool disable_rc_failsafe();
    void configure_safety_parameters();
    void verify_uploaded_mission();
    
    // Члены класса
    std::shared_ptr<mavsdk::Mavsdk> mavsdk_;
    std::shared_ptr<mavsdk::System> system_;
    std::unique_ptr<mavsdk::Action> action_;
    std::unique_ptr<mavsdk::Mission> mission_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;
    std::unique_ptr<mavsdk::Param> param_;
    
    // Флаги состояния
    std::atomic<bool> mission_running_{false};
    std::atomic<bool> mission_completed_{false};
    std::atomic<bool> force_land_triggered_{false};
    std::atomic<bool> last_mission_has_land_command_{false};
    std::atomic<bool> last_mission_has_rtl_command_{false};
    std::atomic<bool> no_autoland_{false};
    
    // Потоки
    std::thread mission_monitor_thread_;
    std::thread land_monitor_thread_;
};

#endif // MISSIONCONTROLLER_H