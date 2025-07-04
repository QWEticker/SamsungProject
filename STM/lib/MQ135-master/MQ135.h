#ifndef MQ135_H
#define MQ135_H

#include "mbed.h"
#include <cmath>

// Константы
#define RLOAD 10.0f                 // Сопротивление нагрузки на плате (в кОм)
#define PARA 116.6020682f           // Параметры для расчёта PPM
#define PARB 2.769034857f
#define ATMOCO2 397.13f              // Уровень CO₂ в атмосфере (ppm)

class MQ135 {
public:
    explicit MQ135(PinName pin);

    void calibrate();               // Калибровка датчика
    float getResistance();          // Получить текущее сопротивление
    float getPPM();                  // Получить концентрацию CO₂ в ppm
    float getRZero();                // Получить RZero (сопротивление в чистом воздухе)

private:
    AnalogIn _pin;
    float rZero;                     // Сопротивление датчика в чистом воздухе
};
#endif