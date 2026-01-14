#ifndef LEDCONTROLLER_H
#define LEDCONTROLLER_H

class LEDController {
public:
    LEDController();
    ~LEDController();
    
    void turnOnRed();      // Включить КРАСНЫЙ свет
    void turnOnGreen();    // Включить ЗЕЛЕНЫЙ свет
    void turnOff();        // Выключить свет
    
    void setLEDState(int state);  // 0=выкл, 1=зеленый, 2=красный
    int getCurrentState() const;
    
private:
    int green_led_pin_;    // GPIO25 - управляет ЗЕЛЕНЫМ светодиодом
    int red_led_pin_;      // GPIO8  - управляет КРАСНЫМ светодиодом
    int current_state_;
};

#endif // LEDCONTROLLER_H