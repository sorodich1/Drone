#ifndef CAMERA_CORRECTION_CONTROLLER_H
#define CAMERA_CORRECTION_CONTROLLER_H

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

class TakeoffLandController;

class CameraCorrectionController {
public:
    struct CorrectionData {
        float x;
        float y;
        bool in_position;
        double timestamp;
        float accuracy_px;
    };

    CameraCorrectionController(std::shared_ptr<mavsdk::Mavsdk> mavsdk, 
                               std::shared_ptr<mavsdk::System> system,
                               TakeoffLandController* takeoff_controller);
    ~CameraCorrectionController();

    void setCameraParameters(float fov_horizontal, float fov_vertical,
                           int image_width, int image_height);
    void setPrecisionLandingMode(bool enable);
    bool init();
    void stop();

    bool addCorrection(float x, float y, bool in_position, float accuracy_px);

    bool isActive() const;
    std::string getLandingStatus();
    int getLandingAttemptsCount();

private:
    std::shared_ptr<mavsdk::Mavsdk> mavsdk_;
    std::shared_ptr<mavsdk::System> system_;
    TakeoffLandController* takeoff_controller_;
    
    std::unique_ptr<mavsdk::Action> action_;
    std::unique_ptr<mavsdk::Offboard> offboard_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;

    float camera_fov_horizontal_{60.0f};
    float camera_fov_vertical_{45.0f};
    int image_width_{320};
    int image_height_{240};

    const float max_correction_distance_{5.0f};

    std::queue<CorrectionData> correction_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::thread correction_thread_;
    std::atomic<bool> running_{true};

    std::atomic<bool> correction_active_{false};
    std::atomic<bool> landing_in_progress_{false};
    std::atomic<bool> offboard_active_{false};
    std::atomic<bool> mission_mode_active_{false};
    std::atomic<bool> precision_mode_{true};

    float current_altitude_{0.0f};

    void correctionWorker();
    void processCorrection(const CorrectionData& correction);
    bool applyPositionCorrection(float dx, float dy);
    bool enableOffboardMode();
    bool startPrecisionLanding();
};

#endif // CAMERA_CORRECTION_CONTROLLER_H