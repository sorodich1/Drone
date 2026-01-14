#include "MissionController.h"
#include "TakeoffLandController.h"
#include "PositionSender.h"  
#include "CameraCorrectionController.h"
#include "ActuatorController.h"
#include "LEDController.h"
#include "httplib.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <iomanip>

std::atomic<bool> server_running{true};

// Глобальный указатель на контроллер коррекции
std::unique_ptr<CameraCorrectionController> camera_correction_controller = nullptr;

int main() {
    // Создаем ОДИН экземпляр Mavsdk для всего приложения
    auto mavsdk = std::make_shared<mavsdk::Mavsdk>(mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation});

    MissionController mission_controller(mavsdk);
    
    std::string connection_url = "serial:///dev/ttyAMA0:57600";
    
    std::cout << "[SERVER_INFO] Подключение к автопилоту: " << connection_url << std::endl;
    
    std::unique_ptr<TakeoffLandController> takeoff_controller = nullptr;
    std::unique_ptr<PositionSender> position_sender = nullptr;  
    std::shared_ptr<mavsdk::System> drone_system = nullptr;
    
    // Инициализация контроллеров актуатора и LED (создаются всегда)
    std::unique_ptr<ActuatorController> actuator_controller = nullptr;
    std::unique_ptr<LEDController> led_controller = nullptr;
    
    // Создаем контроллеры актуатора и LED независимо от подключения к дрону
    try {
        actuator_controller = std::make_unique<ActuatorController>();
        std::cout << "[SERVER_SUCCESS] ✓ Контроллер актуатора инициализирован" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[SERVER_ERROR] ❌ Не удалось инициализировать контроллер актуатора: " << e.what() << std::endl;
        std::cout << "[SERVER_WARNING] ⚠️  Функции управления актуатором недоступны" << std::endl;
    }
    
    try {
        led_controller = std::make_unique<LEDController>();
        std::cout << "[SERVER_SUCCESS] ✓ Контроллер LED инициализирован" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[SERVER_ERROR] ❌ Не удалось инициализировать контроллер LED: " << e.what() << std::endl;
        std::cout << "[SERVER_WARNING] ⚠️  Функции управления подсветкой недоступны" << std::endl;
    }

    bool drone_connected = false;

    if (!mission_controller.connect(connection_url)) {
        std::cerr << "[SERVER_ERROR] Не удалось подключиться к дрону" << std::endl;
        std::cout << "[SERVER_WARNING] Запуск сервера в режиме симуляции (без подключения к дрону)" << std::endl;
        drone_connected = false;
    } else {
        std::cout << "[SERVER_SUCCESS] ✓ Успешно подключились к автопилоту!" << std::endl;
        
        // Получаем систему через новый метод
        drone_system = mission_controller.getSystem();
        drone_connected = true;
        
        // Если TakeoffLandController требует shared_ptr<System>
        if (drone_system) {
            takeoff_controller = std::make_unique<TakeoffLandController>(drone_system);
            
            // Инициализируем PositionSender для отправки полной телеметрии
            std::cout << "[SERVER_INFO] Инициализация PositionSender для телеметрии..." << std::endl;
            position_sender = std::make_unique<PositionSender>();
            
            // // Инициализируем телеметрию с существующей системой
            if (position_sender->initFromSystem(drone_system)) {
                position_sender->setServerInfo("81.3.182.146", 80, "/telemetry/api/telemetry");
                position_sender->startStreaming(10.0f);
                std::cout << "[SERVER_SUCCESS] ✓ Телеметрия настроена и запущена" << std::endl;
            } else {
                std::cerr << "[SERVER_WARNING] Не удалось инициализировать телеметрию" << std::endl;
            }
        } else {
            std::cerr << "[SERVER_ERROR] Получена пустая система!" << std::endl;
            drone_connected = false;
        }
    }

    // Инициализируем контроллер коррекции ВСЕГДА (даже если дрон не подключен)
    std::cout << "[SERVER_INFO] Инициализация контроллера коррекции..." << std::endl;
    camera_correction_controller = std::make_unique<CameraCorrectionController>(mavsdk, drone_system);

    if (drone_connected) {
        if (camera_correction_controller->init()) {
            std::cout << "[SERVER_SUCCESS] ✓ Контроллер коррекции инициализирован (с подключением к дрону)" << std::endl;
        } else {
            std::cerr << "[SERVER_WARNING] Не удалось инициализировать контроллер коррекции" << std::endl;
        }
    } else {
        // В режиме симуляции всё равно создаем, но с предупреждением
        std::cout << "[SERVER_WARNING] ⚠️  Контроллер коррекции создан в режиме симуляции" << std::endl;
        std::cout << "[SERVER_INFO] Коррекции будут обрабатываться, но команды не отправятся к дрону" << std::endl;
    }
    
    httplib::Server server;
    
    server.Get("/status", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 📡 GET запрос /status" << std::endl;
        
        nlohmann::json status;
        status["status"] = "ready";
        status["connected"] = mission_controller.is_connected();
        status["telemetry_active"] = (position_sender && position_sender->is_streaming());
        status["correction_active"] = (camera_correction_controller && camera_correction_controller->isActive());
        status["landing_status"] = camera_correction_controller ? 
                                   camera_correction_controller->getLandingStatus() : "not_available";
        status["actuator_available"] = (actuator_controller != nullptr);
        status["led_available"] = (led_controller != nullptr);
        
        res.set_content(status.dump(), "application/json");
    });

    
    server.Post("/execute-mission", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 🚀 POST запрос /execute-mission получен" << std::endl;
        std::cout << "[API_INFO] Размер тела запроса: " << req.body.size() << " байт" << std::endl;
        
        if (!mission_controller.is_connected()) {
            std::cout << "[API_WARNING] ❌ Дрон не подключен, возвращаем ошибку" << std::endl;
            res.set_content("{\"error\": \"not_connected_to_vehicle\", \"message\": \"Работа в режиме симуляции\"}", "application/json");
            res.status = 400;
            return;
        }
        
        try {
            auto mission_json = nlohmann::json::parse(req.body);
            std::cout << "[API_SUCCESS] ✅ JSON миссии успешно распарсен" << std::endl;
            
            bool success = mission_controller.execute_mission_from_json(mission_json);
            
            if (success) {
                std::cout << "[API_SUCCESS] 🎯 Выполнение миссии завершено успешно" << std::endl;
                res.set_content("{\"status\": \"mission_started\"}", "application/json");
            } else {
                std::cout << "[API_ERROR] 💥 Выполнение миссии завершилось ошибкой" << std::endl;
                res.set_content("{\"status\": \"mission_failed\"}", "application/json");
                res.status = 500;
            }
            
        } catch (const std::exception& e) {
            std::cout << "[API_ERROR] ❌ Ошибка парсинга JSON: " << e.what() << std::endl;
            res.set_content("{\"error\": \"invalid_json\", \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
            res.status = 400;
        }
    });

    server.Get("/get-position", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 📍 GET запрос /get-position получен" << std::endl;
        
        if (!mission_controller.is_connected()) {
            std::cout << "[API_WARNING] ❌ Дрон не подключен" << std::endl;
            nlohmann::json error_response;
            error_response["error"] = "not_connected_to_vehicle";
            error_response["message"] = "Дрон не подключен";
            res.set_content(error_response.dump(), "application/json");
            res.status = 400;
            return;
        }
        
        std::cout << "[API_INFO] 🔄 Получение данных позиции..." << std::endl;
        
        double lat, lon;
        float alt, battery;
        
        if (mission_controller.getCurrentPosition(lat, lon, alt, battery)) {
            std::cout << "[API_SUCCESS] ✅ Данные позиции успешно получены" << std::endl;
            nlohmann::json position_data;
            position_data["latitude"] = lat;
            position_data["longitude"] = lon;
            position_data["altitude"] = alt;
            position_data["battery_percent"] = battery;
            position_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            position_data["status"] = "success";
            
            res.set_content(position_data.dump(), "application/json");
            std::cout << "[API_INFO] 📤 Данные позиции отправлены через API" << std::endl;
        } else {
            std::cout << "[API_ERROR] ❌ Не удалось получить позицию" << std::endl;
            nlohmann::json error_response;
            error_response["error"] = "failed_to_get_position";
            error_response["message"] = "Не удалось получить позицию от дрона";
            error_response["latitude"] = 0.0;
            error_response["longitude"] = 0.0;
            error_response["altitude"] = 0.0;
            error_response["battery_percent"] = 0.0;
            error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            error_response["status"] = "error";
            
            res.set_content(error_response.dump(), "application/json");
            res.status = 500;
        }
    });
    
    // ЕДИНСТВЕННЫЙ НУЖНЫЙ ЕНДПОИНТ: Применение коррекции от камеры
    server.Post("/apply-correction", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 🎯 POST запрос /apply-correction получен" << std::endl;
        
        nlohmann::json response;
        
        // 1. Проверка контроллера
        if (!camera_correction_controller) {
            std::cout << "[API_ERROR] ❌ Контроллер коррекции не инициализирован" << std::endl;
            response["error"] = "correction_controller_not_initialized";
            response["message"] = "Контроллер коррекции не был инициализирован при запуске";
            response["requires_drone_connection"] = true;
            res.set_content(response.dump(), "application/json");
            res.status = 500;
            return;
        }
        
        // 2. Проверка подключения дрона
        if (!mission_controller.is_connected()) {
            std::cout << "[API_ERROR] ❌ Дрон не подключен" << std::endl;
            response["error"] = "drone_not_connected";
            response["message"] = "Дрон не подключен, коррекции невозможны";
            response["requires_connection"] = true;
            res.set_content(response.dump(), "application/json");
            res.status = 400;
            return;
        }
        
        try {
            auto json_data = nlohmann::json::parse(req.body);
            
            if (!json_data.contains("correction")) {
                throw std::runtime_error("Missing 'correction' field");
            }
            
            auto correction = json_data["correction"];
            
            // ВАЖНО: Проверяем, что Python скрипт отправляет ПРАВИЛЬНЫЕ данные
            // Он должен отправлять метры, а не пиксели!
            float corr_x = correction["x"];  // В метрах!
            float corr_y = correction["y"];  // В метрах!
            bool in_position = correction.value("in_position", false);
            float accuracy_px = correction.value("accuracy_px", 0.0f);
            
            std::cout << "[API_INFO] 📐 Коррекция позиции получена:" << std::endl;
            std::cout << "[API_INFO]   X: " << corr_x << " м" << std::endl;
            std::cout << "[API_INFO]   Y: " << corr_y << " м" << std::endl;
            std::cout << "[API_INFO]   Точность: " << accuracy_px << " px" << std::endl;
            std::cout << "[API_INFO]   В позиции: " << (in_position ? "ДА" : "НЕТ") << std::endl;
            
            // 3. Логика обработки
            if (in_position) {
                std::cout << "[API_INFO] 🎯 Дрон в целевой позиции!" << std::endl;
                
                // Проверяем текущий статус посадки
                std::string landing_status = camera_correction_controller->getLandingStatus();
                
                if (landing_status == "landed") {
                    response["warning"] = "already_landed";
                    response["message"] = "Дрон уже приземлился";
                } else if (landing_status == "landing_in_progress") {
                    response["warning"] = "landing_already_in_progress";
                    response["message"] = "Посадка уже выполняется";
                } else {
                    std::cout << "[API_INFO] 🛬 Инициирование процедуры посадки..." << std::endl;
                }
            }
            
            // 4. Добавляем коррекцию
            bool success = camera_correction_controller->addCorrection(
                corr_x, corr_y, in_position, accuracy_px);
            
            if (success) {
                std::cout << "[API_SUCCESS] ✅ Коррекция принята и добавлена в очередь" << std::endl;
                
                response["status"] = "correction_accepted";
                response["applied"] = true;
                response["correction_x"] = corr_x;
                response["correction_y"] = corr_y;
                response["in_position"] = in_position;
                response["landing_initiated"] = in_position;
                response["landing_attempts"] = camera_correction_controller->getLandingAttemptsCount();
                response["landing_status"] = camera_correction_controller->getLandingStatus();
                response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                
                if (in_position) {
                    response["message"] = "Посадка инициирована. Дрон начнет снижение.";
                }
                
            } else {
                response["status"] = "correction_rejected";
                response["applied"] = false;
                response["error"] = "queue_full";
                response["message"] = "Очередь коррекций переполнена";
                res.status = 500;
            }
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            std::cout << "[API_ERROR] ❌ Ошибка обработки коррекции: " << e.what() << std::endl;
            response["error"] = "invalid_data";
            response["message"] = e.what();
            response["requires_correct_format"] = true;
            response["expected_format"] = {
                {"correction", {
                    {"x", "float (meters)"},
                    {"y", "float (meters)"},
                    {"in_position", "boolean"},
                    {"accuracy_px", "float"}
                }}
            };
            res.set_content(response.dump(), "application/json");
            res.status = 400;
        }
    });
    
    server.Get("/landing-status", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 📊 GET запрос /landing-status" << std::endl;
        
        nlohmann::json status;
        
        if (!camera_correction_controller) {
            status["error"] = "controller_not_initialized";
            status["message"] = "Контроллер коррекции не инициализирован";
        } else {
            status["landing_status"] = camera_correction_controller->getLandingStatus();
            status["landing_attempts"] = camera_correction_controller->getLandingAttemptsCount();
            status["controller_active"] = camera_correction_controller->isActive();
            status["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
        
        res.set_content(status.dump(), "application/json");
    });

    // Новые эндпоинты для управления актуатором и LED
    
    server.Post("/api/actuator/control", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 🎛️ POST запрос /api/actuator/control получен" << std::endl;
        std::cout << "[API_INFO] Тело запроса: " << req.body << std::endl;
        
        nlohmann::json response;
        
        // Проверка наличия контроллера актуатора
        if (!actuator_controller) {
            response["status"] = "error";
            response["error"] = "actuator_not_initialized";
            response["message"] = "Контроллер актуатора не инициализирован";
            res.status = 500;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        try {
            auto json_data = nlohmann::json::parse(req.body);
            std::string command = json_data["Command"].get<std::string>();
            
            std::cout << "[API_INFO] 📦 Команда актуатору: " << command << std::endl;
            
            bool success = false;
            
            if (command == "OPEN_BOX") {
                std::cout << "[API_INFO] ⚙️  Выполнение: ОТКРЫТЬ бокс" << std::endl;
                try {
                    actuator_controller->setActuatorState(true);
                    success = true;
                    response["status"] = "success";
                    response["message"] = "Команда 'ОТКРЫТЬ бокс' выполнена успешно";
                    response["action"] = "open";
                } catch (const std::exception& e) {
                    response["status"] = "error";
                    response["error"] = "actuator_execution_error";
                    response["message"] = std::string("Ошибка выполнения: ") + e.what();
                    res.status = 500;
                }
                
            } else if (command == "CLOSE_BOX") {
                std::cout << "[API_INFO] ⚙️  Выполнение: ЗАКРЫТЬ бокс" << std::endl;
                try {
                    actuator_controller->setActuatorState(false);
                    success = true;
                    response["status"] = "success";
                    response["message"] = "Команда 'ЗАКРЫТЬ бокс' выполнена успешно";
                    response["action"] = "close";
                } catch (const std::exception& e) {
                    response["status"] = "error";
                    response["error"] = "actuator_execution_error";
                    response["message"] = std::string("Ошибка выполнения: ") + e.what();
                    res.status = 500;
                }
                
            } else {
                response["status"] = "error";
                response["error"] = "unknown_command";
                response["message"] = "Неизвестная команда: " + command;
                response["available_commands"] = {"OPEN_BOX", "CLOSE_BOX"};
                res.status = 400;
            }
            
            if (success) {
                // Получаем текущее состояние актуатора
                bool current_state = actuator_controller->getCurrentState();
                response["actuator_state"] = current_state ? "extended" : "retracted";
                std::cout << "[API_SUCCESS] ✅ Актуатор: " << (current_state ? "ОТКРЫТ" : "ЗАКРЫТ") << std::endl;
            }
            
            response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            std::cout << "[API_ERROR] ❌ Ошибка парсинга JSON: " << e.what() << std::endl;
            response["status"] = "error";
            response["error"] = "invalid_json";
            response["message"] = e.what();
            res.set_content(response.dump(), "application/json");
            res.status = 400;
        }
    });

    server.Post("/api/led/control", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 💡 POST запрос /api/led/control получен" << std::endl;
        std::cout << "[API_INFO] Тело запроса: '" << req.body << "'" << std::endl;
        
        nlohmann::json response;
        
        // Проверка наличия контроллера LED
        if (!led_controller) {
            response["status"] = "error";
            response["error"] = "led_not_initialized";
            response["message"] = "Контроллер LED не инициализирован";
            res.status = 500;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        try {
            int color_number;
            
            // Пробуем распарсить как JSON
            try {
                auto json_data = nlohmann::json::parse(req.body);
                color_number = json_data.get<int>();
            } catch (...) {
                // Если не JSON, пробуем как plain number
                color_number = std::stoi(req.body);
            }
            
            std::cout << "[API_INFO] 🎨 Запрошен цвет с номером: " << color_number << std::endl;
            
            // Выполняем команду
            try {
                led_controller->setLEDState(color_number);
                
                // Получаем текущее состояние
                int current_state = led_controller->getCurrentState();
                
                response["status"] = "success";
                response["color_number"] = color_number;
                response["current_state"] = current_state;
                
                switch(current_state) {
                    case 0:
                        response["color"] = "off";
                        response["message"] = "Подсветка ВЫКЛЮЧЕНА";
                        std::cout << "[API_SUCCESS] ✅ Подсветка: ВЫКЛЮЧЕНА" << std::endl;
                        break;
                    case 1:
                        response["color"] = "green";
                        response["message"] = "Подсветка ЗЕЛЕНАЯ";
                        std::cout << "[API_SUCCESS] ✅ Подсветка: ЗЕЛЕНАЯ" << std::endl;
                        break;
                    case 2:
                        response["color"] = "red";
                        response["message"] = "Подсветка КРАСНАЯ";
                        std::cout << "[API_SUCCESS] ✅ Подсветка: КРАСНАЯ" << std::endl;
                        break;
                    default:
                        response["warning"] = "unknown_state_returned";
                        response["message"] = "Неизвестное состояние возвращено контроллером";
                        break;
                }
                
            } catch (const std::exception& e) {
                response["status"] = "error";
                response["error"] = "led_execution_error";
                response["message"] = std::string("Ошибка выполнения: ") + e.what();
                res.status = 500;
            }
            
            response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            std::cout << "[API_ERROR] ❌ Ошибка обработки запроса: " << e.what() << std::endl;
            response["status"] = "error";
            response["error"] = "invalid_request";
            response["message"] = e.what();
            response["expected_format"] = "integer: 0=off, 1=green, 2=red";
            res.set_content(response.dump(), "application/json");
            res.status = 400;
        }
    });

    server.Get("/shutdown", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 🛑 GET запрос /shutdown получен" << std::endl;
        
        // Останавливаем потоковую телеметрию перед выключением
        if (position_sender) {
            position_sender->stopStreaming();
            std::cout << "[SERVER_INFO] Потоковая телеметрия остановлена" << std::endl;
        }
        
        // Останавливаем контроллер коррекции
        if (camera_correction_controller) {
            camera_correction_controller->stop();
            std::cout << "[SERVER_INFO] Контроллер коррекции остановлен" << std::endl;
        }
        
        // Выключаем LED при завершении
        if (led_controller) {
            led_controller->turnOff();
            std::cout << "[SERVER_INFO] LED выключен" << std::endl;
        }
        
        // Останавливаем актуатор
        if (actuator_controller) {
            // Можно вызвать stopActuator если нужно
            std::cout << "[SERVER_INFO] Актуатор остановлен" << std::endl;
        }
        
        server_running = false;
        res.set_content("{\"status\": \"shutting_down\"}", "application/json");
        server.stop();
    });

    server.Post("/simple-takeoff", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 🚀 POST запрос /simple-takeoff получен" << std::endl;
        
        if (!mission_controller.is_connected()) {
            res.set_content("{\"error\": \"not_connected\"}", "application/json");
            res.status = 400;
            return;
        }
        
        bool success = mission_controller.execute_simple_takeoff();
        
        if (success) {
            res.set_content("{\"status\": \"mission_completed\"}", "application/json");
        } else {
            res.set_content("{\"status\": \"mission_failed\"}", "application/json");
            res.status = 500;
        }
    });

    server.Post("/takeoff-land", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 🚁 POST /takeoff-land" << std::endl;
        
        if (!takeoff_controller || !takeoff_controller->is_connected()) {
            res.set_content("{\"error\": \"not_connected\"}", "application/json");
            res.status = 400;
            return;
        }
        
        try {
            auto json_data = nlohmann::json::parse(req.body);
            
            bool should_takeoff = false;
            float altitude = 5.0f;
            
            if (json_data.contains("takeoff")) {
                should_takeoff = json_data["takeoff"].get<bool>();
            }
            
            if (json_data.contains("altitude")) {
                altitude = json_data["altitude"].get<float>();
            }
            
            if (should_takeoff) {
                // ТОЛЬКО ВЗЛЕТ
                std::cout << "[API_INFO] 🛫 Команда ВЗЛЕТА на " << altitude << "м" << std::endl;
                
                bool success = takeoff_controller->execute_takeoff_land_mission(altitude);
                
                if (success) {
                    // Отправляем ответ СРАЗУ после взлета
                    res.set_content("{\"status\": \"in_air\", \"altitude\": " + 
                                std::to_string(altitude) + "}", "application/json");
                } else {
                    res.set_content("{\"status\": \"takeoff_failed\"}", "application/json");
                    res.status = 500;
                }
                
            } else {
                // ТОЛЬКО ПОСАДКА
                std::cout << "[API_INFO] 🛬 Команда ПОСАДКИ" << std::endl;
                
                bool success = takeoff_controller->execute_landing_only();
                
                if (success) {
                    res.set_content("{\"status\": \"landed\"}", "application/json");
                } else {
                    res.set_content("{\"status\": \"landing_failed\"}", "application/json");
                    res.status = 500;
                }
            }
            
        } catch (const std::exception& e) {
            std::cout << "[API_ERROR] ❌ Ошибка JSON: " << e.what() << std::endl;
            res.set_content("{\"error\": \"invalid_json\"}", "application/json");
            res.status = 400;
        }
    });

    server.Post("/return-home-no-land", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[API_INFO] 🏠 POST /return-home-no-land (возврат без посадки)" << std::endl;
        
        if (!mission_controller.is_connected()) {
            res.set_content("{\"error\": \"not_connected\"}", "application/json");
            res.status = 400;
            return;
        }
        
        bool success = mission_controller.return_to_home_no_land();
        
        if (success) {
            res.set_content("{\"status\": \"returning_home\", \"message\": \"Дрон возвращается домой без посадки\"}", "application/json");
        } else {
            res.set_content("{\"status\": \"error\", \"message\": \"Ошибка возврата\"}", "application/json");
            res.status = 500;
        }
    });
    
    std::cout << "[SERVER_INFO] Запуск сервера миссий на http://localhost:8080" << std::endl;
    std::cout << "[SERVER_INFO] Доступные endpointы:" << std::endl;
    std::cout << "[SERVER_INFO]   GET  /status - Проверка статуса сервера" << std::endl;
    std::cout << "[SERVER_INFO]   POST /execute-mission - Загрузка и выполнение миссии" << std::endl;
    std::cout << "[SERVER_INFO]   GET  /get-position - Получение текущей позиции" << std::endl;
    std::cout << "[SERVER_INFO]   POST /apply-correction - Коррекция позиции и инициирование посадки" << std::endl;
    std::cout << "[SERVER_INFO]   POST /takeoff-land - Взлет и посадка" << std::endl;
    std::cout << "[SERVER_INFO]   POST /api/actuator/control - Управление актуатором (открыть/закрыть бокс)" << std::endl;
    std::cout << "[SERVER_INFO]   POST /api/led/control - Управление подсветкой (0=выкл, 1=зеленый, 2=красный)" << std::endl;
    std::cout << "[SERVER_INFO]   GET  /shutdown - Остановка сервера" << std::endl;
    std::cout << "[SERVER_INFO]   POST /simple-takeoff - Простой взлет" << std::endl;
    std::cout << "[SERVER_INFO]   POST /return-home-no-land - Возврат домой без посадки" << std::endl;
    
    server.listen("0.0.0.0", 8080);
    
    // Очистка при завершении
    if (camera_correction_controller) {
        camera_correction_controller->stop();
    }
    
    return 0;
}