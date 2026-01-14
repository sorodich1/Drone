#include "ActuatorController.h"
#include <wiringPi.h>
#include <iostream>
#include <atomic>

// Переменная для отслеживания состояния
static std::atomic<bool> actuator_extended{false};

ActuatorController::ActuatorController() {
    wiringPiSetupGpio();
    pin_out_ = 23;  // GPIO23
    pin_in_ = 24;   // GPIO24
    
    pinMode(pin_out_, OUTPUT);
    pinMode(pin_in_, OUTPUT);
    
    stopActuator();
    actuator_extended = false;  // Начальное состояние - закрыт
    std::cout << "✅ Актуатор инициализирован (состояние: ЗАКРЫТ)" << std::endl;
}

ActuatorController::~ActuatorController() {
    stopActuator();
}

void ActuatorController::extend() {
    // ВЫДВИГАНИЕ: GPIO23=HIGH, GPIO24=LOW
    digitalWrite(pin_out_, HIGH);
    digitalWrite(pin_in_, LOW);
    actuator_extended = true;
    std::cout << "🔼 Выдвижение: GPIO23=HIGH, GPIO24=LOW" << std::endl;
}

void ActuatorController::retract() {
    // ЗАДВИЖКА: GPIO23=LOW, GPIO24=HIGH
    digitalWrite(pin_out_, LOW);
    digitalWrite(pin_in_, HIGH);
    actuator_extended = false;
    std::cout << "🔽 Задвижка: GPIO23=LOW, GPIO24=HIGH" << std::endl;
}

void ActuatorController::stopActuator() {
    digitalWrite(pin_out_, LOW);
    digitalWrite(pin_in_, LOW);
    std::cout << "⏹️  Стоп: GPIO23=LOW, GPIO24=LOW" << std::endl;
}

void ActuatorController::moveForDuration(bool direction, int duration_ms) {
    // direction: false = выдвигать, true = задвигать
    if (direction) {
        retract();      // true = задвигать
    } else {
        extend();       // false = выдвигать
    }
    
    delay(duration_ms);
    stopActuator();
}

void ActuatorController::setActuatorState(bool should_extend) {
    // should_extend: true = открыть (выдвинуть), false = закрыть (задвинуть)
    std::cout << "🎛️ Команда актуатору: " << (should_extend ? "ОТКРЫТЬ" : "ЗАКРЫТЬ") << std::endl;
    
    if (should_extend) {
        // ОТКРЫТЬ = выдвигать
        std::cout << "  Действие: ВЫДВИЖЕНИЕ" << std::endl;
        std::cout << "  GPIO23=HIGH, GPIO24=LOW" << std::endl;
        moveForDuration(false, 10000);  // false = выдвигать, 10 секунд
    } else {
        // ЗАКРЫТЬ = задвигать
        std::cout << "  Действие: ЗАДВИЖКА" << std::endl;
        std::cout << "  GPIO23=LOW, GPIO24=HIGH" << std::endl;
        moveForDuration(true, 10000);   // true = задвигать, 10 секунд
    }
    
    std::cout << "✅ Актуатор " << (should_extend ? "ОТКРЫТ" : "ЗАКРЫТ") << std::endl;
}

bool ActuatorController::getCurrentState() const {
    return actuator_extended.load();  // true = открыт, false = закрыт
}