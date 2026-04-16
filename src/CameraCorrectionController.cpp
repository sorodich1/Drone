#include "CameraCorrectionController.h"
#include "TakeoffLandController.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>

CameraCorrectionController::CameraCorrectionController(
    std::shared_ptr<mavsdk::Mavsdk> mavsdk, 
    std::shared_ptr<mavsdk::System> system,
    TakeoffLandController* takeoff_controller)
    : mavsdk_(mavsdk), system_(system), takeoff_controller_(takeoff_controller) {
    
    if (system_) {
        action_ = std::make_unique<mavsdk::Action>(system_);
        offboard_ = std::make_unique<mavsdk::Offboard>(system_);
        telemetry_ = std::make_unique<mavsdk::Telemetry>(system_);
    }
}

CameraCorrectionController::~CameraCorrectionController() {
    stop();
}

void CameraCorrectionController::setCameraParameters(float fov_horizontal, float fov_vertical,
                                                   int image_width, int image_height) {
    camera_fov_horizontal_ = fov_horizontal;
    camera_fov_vertical_ = fov_vertical;
    image_width_ = image_width;
    image_height_ = image_height;
    
    std::cout << "[CAMERA_CTRL] Camera configured" << std::endl;
}

void CameraCorrectionController::setPrecisionLandingMode(bool enable) {
    precision_mode_ = enable;
    std::cout << "[CAMERA_CTRL] Landing mode: " << (enable ? "ENABLED" : "DISABLED") << std::endl;
}

bool CameraCorrectionController::init() {
    if (!system_ || !action_ || !offboard_ || !telemetry_) {
        std::cerr << "[CAMERA_CTRL] ❌ Components not initialized" << std::endl;
        return false;
    }
    
    std::cout << "[CAMERA_CTRL] ✓ Controller initialized" << std::endl;
    
    try {
        telemetry_->subscribe_position([this](mavsdk::Telemetry::Position position) {
            current_altitude_ = position.relative_altitude_m;
        });
        
        std::cout << "[CAMERA_CTRL] ✓ Telemetry activated" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ❌ Init error: " << e.what() << std::endl;
        return false;
    }
    
    running_ = true;
    correction_thread_ = std::thread(&CameraCorrectionController::correctionWorker, this);
    
    std::cout << "[CAMERA_CTRL] 🚀 Ready" << std::endl;
    return true;
}

void CameraCorrectionController::stop() {
    running_ = false;
    queue_cv_.notify_all();
    
    if (correction_thread_.joinable()) {
        correction_thread_.join();
    }
    
    if (offboard_ && offboard_active_) {
        try {
            offboard_->stop();
            offboard_active_ = false;
        } catch (...) {}
    }
    
    std::cout << "[CAMERA_CTRL] Stopped" << std::endl;
}

bool CameraCorrectionController::addCorrection(float x, float y, bool in_position, float accuracy_px) {
    if (std::abs(x) > max_correction_distance_ || std::abs(y) > max_correction_distance_) {
        std::cout << "[CAMERA_CTRL] ⚠️ Correction too large, ignoring" << std::endl;
        return false;
    }
    
    CorrectionData correction{
        .x = x,
        .y = y,
        .in_position = in_position,
        .timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0,
        .accuracy_px = accuracy_px
    };
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        correction_queue_.push(correction);
        while (correction_queue_.size() > 5) {
            correction_queue_.pop();
        }
    }
    
    queue_cv_.notify_one();
    return true;
}

void CameraCorrectionController::correctionWorker() {
    std::cout << "[CAMERA_CTRL] 🚁 Correction worker thread started" << std::endl;
    
    while (running_) {
        CorrectionData correction;
        bool has_correction = false;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (queue_cv_.wait_for(lock, std::chrono::milliseconds(50), 
                [this]() { return !correction_queue_.empty() || !running_; })) {
                
                if (!correction_queue_.empty()) {
                    correction = correction_queue_.front();
                    correction_queue_.pop();
                    has_correction = true;
                }
            }
        }
        
        if (has_correction) {
            processCorrection(correction);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CameraCorrectionController::processCorrection(const CorrectionData& correction) {
    std::cout << "[CAMERA_CTRL] 📍 Correction: X=" << correction.x << "m, Y=" << correction.y 
              << "m, Landing=" << (correction.in_position ? "YES" : "NO") << std::endl;
    
    correction_active_ = true;
    
    if (telemetry_ && telemetry_->in_air() && !landing_in_progress_) {
        applyPositionCorrection(correction.x, correction.y);
    }
    
    if (correction.in_position && telemetry_->in_air() && !landing_in_progress_) {
        std::cout << "[CAMERA_CTRL] 🛬 LANDING SIGNAL - CALLING execute_landing_only()!" << std::endl;
        startPrecisionLanding();
    }
    
    correction_active_ = false;
}

bool CameraCorrectionController::applyPositionCorrection(float dx, float dy) {
    if (!action_ || !offboard_ || !telemetry_) {
        return false;
    }
    
    try {
        if (!telemetry_->in_air()) {
            return false;
        }
        
        auto position = telemetry_->position_velocity_ned();
        auto attitude = telemetry_->attitude_euler();
        float yaw_deg = attitude.yaw_deg;
        
        auto gps_position = telemetry_->position();
        float current_height = gps_position.relative_altitude_m;
        
        mavsdk::Offboard::PositionNedYaw target_position{};
        target_position.north_m = position.position.north_m + dx;
        target_position.east_m = position.position.east_m + dy;
        target_position.down_m = position.position.down_m;
        target_position.yaw_deg = yaw_deg;
        
        std::cout << "[CAMERA_CTRL] 📍 Move: dX=" << dx << "m, dY=" << dy << "m";
        std::cout << " (height: " << current_height << "m)" << std::endl;
        
        auto flight_mode = telemetry_->flight_mode();
        
        if (flight_mode != mavsdk::Telemetry::FlightMode::Offboard) {
            if (!enableOffboardMode()) {
                return false;
            }
        }
        
        offboard_->set_position_ned(target_position);
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ❌ Error: " << e.what() << std::endl;
        return false;
    }
}

bool CameraCorrectionController::enableOffboardMode() {
    try {
        if (!telemetry_->in_air()) {
            std::cerr << "[CAMERA_CTRL] ❌ Cannot enable offboard on ground" << std::endl;
            return false;
        }
        
        if (mission_mode_active_) {
            std::cerr << "[CAMERA_CTRL] ❌ Mission mode active" << std::endl;
            return false;
        }
        
        auto position = telemetry_->position_velocity_ned();
        auto attitude = telemetry_->attitude_euler();
        float yaw_deg = attitude.yaw_deg;
        
        mavsdk::Offboard::PositionNedYaw current_position{};
        current_position.north_m = position.position.north_m;
        current_position.east_m = position.position.east_m;
        current_position.down_m = position.position.down_m;
        current_position.yaw_deg = yaw_deg;
        
        offboard_->set_position_ned(current_position);
        
        auto result = offboard_->start();
        
        if (result != mavsdk::Offboard::Result::Success) {
            std::cerr << "[CAMERA_CTRL] ❌ Offboard start failed" << std::endl;
            return false;
        }
        
        offboard_active_ = true;
        
        auto gps_position = telemetry_->position();
        std::cout << "[CAMERA_CTRL] ✅ Offboard mode activated (height: " 
                  << gps_position.relative_altitude_m << "m)" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA_CTRL] ❌ Offboard error: " << e.what() << std::endl;
        return false;
    }
}

bool CameraCorrectionController::startPrecisionLanding() {
    if (!telemetry_ || !takeoff_controller_) {
        std::cerr << "[CAMERA_CTRL] ❌ Takeoff controller not available" << std::endl;
        return false;
    }
    
    if (landing_in_progress_) {
        std::cout << "[CAMERA_CTRL] ⚠️ Landing already in progress" << std::endl;
        return false;
    }
    
    std::cout << "[CAMERA_CTRL] 🛬 STARTING LANDING VIA TakeoffLandController!" << std::endl;
    landing_in_progress_ = true;
    
    bool success = takeoff_controller_->execute_landing_only();
    
    if (success) {
        std::cout << "[CAMERA_CTRL] ✅ Landing completed successfully!" << std::endl;
    } else {
        std::cerr << "[CAMERA_CTRL] ❌ Landing failed!" << std::endl;
    }
    
    landing_in_progress_ = false;
    return success;
}

bool CameraCorrectionController::isActive() const {
    return correction_active_;
}

std::string CameraCorrectionController::getLandingStatus() {
    if (landing_in_progress_) {
        return "landing_in_progress";
    } else if (telemetry_ && !telemetry_->in_air()) {
        return "landed";
    }
    return "not_started";
}

int CameraCorrectionController::getLandingAttemptsCount() {
    return landing_in_progress_ ? 1 : 0;
}