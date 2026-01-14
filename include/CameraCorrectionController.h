#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>
#include <vector>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

struct CorrectionData {
    float x;                // Смещение по X в метрах
    float y;                // Смещение по Y в метрах
    bool in_position;       // Дрон в позиции для посадки
    double timestamp;       // Временная метка
    float accuracy_px;      // Точность в пикселях
};

struct LandingAttempt {
    int attempt_number;
    std::chrono::steady_clock::time_point attempt_time;
    bool success;
    std::string error_message;
};

class CameraCorrectionController {
public:
    CameraCorrectionController(std::shared_ptr<mavsdk::Mavsdk> mavsdk, 
                               std::shared_ptr<mavsdk::System> system);
    ~CameraCorrectionController();
    
    bool init();
    void stop();
    
    bool addCorrection(float x, float y, bool in_position, float accuracy_px);
    bool isActive() const;
    
    std::string getLandingStatus();
    int getLandingAttemptsCount();
    
    // Новые методы для калибровки
    void setCameraParameters(float fov_horizontal, float fov_vertical, 
                           int image_width, int image_height);
    void setPrecisionLandingMode(bool enable);
    
private:
    void correctionWorker();
    void processCorrection(const CorrectionData& correction);
    
    // Основные функции управления
    bool applyPositionCorrection(float dx, float dy);
    bool startPrecisionLanding();
    bool executeLanding();
    bool waitForLandingConfirmation(float timeout_sec = 10.0f);
    
    // Вспомогательные функции
    bool switchToOffboardMode();
    bool ensureSafeAltitude();
    float calculateRequiredMovement(float pixel_offset, bool is_horizontal);
    void updateCurrentPosition();
    void checkLandingStatus();  // ДОБАВИТЬ ЭТУ СТРОКУ!
    
    // Потоковые переменные
    std::thread correction_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> correction_active_{false};
    
    // Очередь коррекций
    std::queue<CorrectionData> correction_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Посадка
    std::atomic<bool> landing_in_progress_{false};
    std::vector<LandingAttempt> landing_attempts_;
    std::mutex landing_mutex_;
    
    // Параметры
    const float max_correction_distance_ = 10.0f;
    const int max_landing_attempts_ = 3;
    const float landing_attempt_interval_ = 5.0f;
    const float precision_threshold_ = 0.1f;  // 10 см точность
    
    // Параметры камеры для точной посадки
    float camera_fov_horizontal_ = 60.0f;     // градусы
    float camera_fov_vertical_ = 45.0f;       // градусы
    int image_width_ = 320;
    int image_height_ = 240;
    float current_altitude_ = 0.0f;
    bool precision_mode_ = true;
    
    // MAVSDK компоненты
    std::shared_ptr<mavsdk::Mavsdk> mavsdk_;
    std::shared_ptr<mavsdk::System> system_;
    std::unique_ptr<mavsdk::Action> action_;
    std::unique_ptr<mavsdk::Offboard> offboard_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;
    
    // Текущая позиция
    mavsdk::Telemetry::PositionNed current_position_;
    std::mutex position_mutex_;
};