// CameraCorrectionController.h
#ifndef CAMERA_CORRECTION_CONTROLLER_H
#define CAMERA_CORRECTION_CONTROLLER_H

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>  // < ��� ���� ���������!
#include <queue>
#include <atomic>
#include <string>
#include <chrono>
#include <vector>

class CameraCorrectionController {
public:
    struct CorrectionData {
        float x = 0.0f;
        float y = 0.0f;
        bool in_position = false;
        double timestamp = 0.0;
        float accuracy_px = 0.0f;
    };

    struct LandingAttempt {
        int attempt_number = 0;
        std::chrono::time_point<std::chrono::steady_clock> attempt_time;
        bool success = false;
        std::string error_message;
    };

public:
    CameraCorrectionController(std::shared_ptr<mavsdk::Mavsdk> mavsdk, 
                              std::shared_ptr<mavsdk::System> system);
    ~CameraCorrectionController();

    CameraCorrectionController(const CameraCorrectionController&) = delete;
    CameraCorrectionController& operator=(const CameraCorrectionController&) = delete;

    bool init();
    void stop();

    void setCameraParameters(float fov_horizontal, float fov_vertical,
                           int image_width, int image_height);
    
    void setPrecisionLandingMode(bool enable);
    
    bool addCorrection(float x, float y, bool in_position = false, float accuracy_px = 0.0f);
    
    bool isActive() const;
    std::string getLandingStatus();
    int getLandingAttemptsCount();
    
    void setMissionMode(bool enabled) { mission_mode_active_ = enabled; }
    bool isInMissionMode() const { return mission_mode_active_; }

private:
    std::shared_ptr<mavsdk::Mavsdk> mavsdk_;
    std::shared_ptr<mavsdk::System> system_;
    std::unique_ptr<mavsdk::Action> action_;
    std::unique_ptr<mavsdk::Offboard> offboard_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;

    std::thread correction_thread_;
    std::atomic<bool> running_{false};
    
    std::queue<CorrectionData> correction_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;  // < ������ ���� INCLUDE!

    std::atomic<bool> correction_active_{false};
    std::atomic<bool> landing_in_progress_{false};
    std::atomic<bool> offboard_active_{false};
    std::atomic<bool> mission_mode_active_{false};
    std::atomic<float> current_altitude_{0.0f};

    float camera_fov_horizontal_ = 60.0f;
    float camera_fov_vertical_ = 45.0f;
    int image_width_ = 640;
    int image_height_ = 480;

    const float max_correction_distance_ = 10.0f;
    
    std::vector<LandingAttempt> landing_attempts_;
    std::mutex landing_mutex_;
    const int max_landing_attempts_ = 3;
    const int landing_attempt_interval_ = 2;
    bool precision_mode_ = false;

private:
    void correctionWorker();
    void processCorrection(const CorrectionData& correction);
    
    bool applyPositionCorrection(float dx, float dy);
    bool enableOffboardMode();
    bool startPrecisionLanding();
    bool executePrecisionLanding();
    
    float calculateRequiredMovement(float pixel_offset, bool is_horizontal);
};

#endif // CAMERA_CORRECTION_CONTROLLER_H