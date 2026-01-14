#ifndef ACTUATORCONTROLLER_H
#define ACTUATORCONTROLLER_H

class ActuatorController {
private:
    int pin_out_;
    int pin_in_;
    
public:
    ActuatorController();
    ~ActuatorController();
    
    void setActuatorState(bool should_extend);
    bool getCurrentState() const;
    void stopActuator();

private:
    void extend();
    void retract();
    void moveForDuration(bool direction, int duration_ms = 15000);
};

#endif