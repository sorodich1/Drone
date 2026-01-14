#include "LEDController.h"
#include <wiringPi.h>
#include <iostream>

LEDController::LEDController() : current_state_(0) {
    // Инициализация GPIO
    wiringPiSetupGpio();

    green_led_pin_ = 25;   // GPIO25 - ЗЕЛЕНЫЙ светодиод
    red_led_pin_ = 8;      // GPIO8  - КРАСНЫЙ светодиод
    
    pinMode(green_led_pin_, OUTPUT);
    pinMode(red_led_pin_, OUTPUT);
    
    // Изначально выключаем все светодиоды
    turnOff();
    std::cout << "✅ Контроллер светодиодной ленты инициализирован" << std::endl;
    std::cout << "🟢 Зеленый светодиод на GPIO " << green_led_pin_ << " (бывший red_pin_)" << std::endl;
    std::cout << "🔴 Красный светодиод на GPIO " << red_led_pin_ << " (бывший green_pin_)" << std::endl;
    std::cout << "⚫ Выключено: GPIO" << green_led_pin_ << "=HIGH, GPIO" << red_led_pin_ << "=HIGH" << std::endl;
}

LEDController::~LEDController() {
    // Выключаем светодиоды при разрушении объекта
    turnOff();
}

void LEDController::turnOnRed() {
    // Включить КРАСНЫЙ: GPIO25=LOW, GPIO8=HIGH
    digitalWrite(green_led_pin_, LOW);   // Зеленый выключен
    digitalWrite(red_led_pin_, HIGH);    // Красный включен
    current_state_ = 2;
    std::cout << "🔴 Включен КРАСНЫЙ свет" << std::endl;
    std::cout << "  GPIO" << green_led_pin_ << "=LOW (зеленый выкл)" << std::endl;
    std::cout << "  GPIO" << red_led_pin_ << "=HIGH (красный вкл)" << std::endl;
}

void LEDController::turnOnGreen() {
    // Включить ЗЕЛЕНЫЙ: GPIO25=HIGH, GPIO8=LOW
    digitalWrite(green_led_pin_, HIGH);  // Зеленый включен
    digitalWrite(red_led_pin_, LOW);     // Красный выключен
    current_state_ = 1;
    std::cout << "🟢 Включен ЗЕЛЕНЫЙ свет" << std::endl;
    std::cout << "  GPIO" << green_led_pin_ << "=HIGH (зеленый вкл)" << std::endl;
    std::cout << "  GPIO" << red_led_pin_ << "=LOW (красный выкл)" << std::endl;
}

void LEDController::turnOff() {
    // Выключить: GPIO25=HIGH, GPIO8=HIGH
    digitalWrite(green_led_pin_, HIGH);
    digitalWrite(red_led_pin_, HIGH);
    current_state_ = 0;
    std::cout << "⚫ Свет ВЫКЛЮЧЕН" << std::endl;
    std::cout << "  GPIO" << green_led_pin_ << "=HIGH" << std::endl;
    std::cout << "  GPIO" << red_led_pin_ << "=HIGH" << std::endl;
}

void LEDController::setLEDState(int state) {
    std::cout << "🎨 Установка состояния LED: " << state << std::endl;
    
    switch(state) {
        case 0:
            turnOff();
            break;
        case 1:
            turnOnGreen();
            break;
        case 2:
            turnOnRed();
            break;
        default:
            std::cout << "❌ Неизвестное состояние светодиода: " << state << std::endl;
            std::cout << "   Допустимые значения: 0=выкл, 1=зеленый, 2=красный" << std::endl;
            break;
    }
}

int LEDController::getCurrentState() const {
    return current_state_;
}