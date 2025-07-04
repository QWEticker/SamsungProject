#include "MQ135.h"

MQ135::MQ135(PinName pin) : _pin(pin), rZero(0.0f) {}

void MQ135::calibrate() {
    // Среднее значение за несколько измерений
    float val = 0.0f;
    for (int i = 0; i < 50; i++) {
        val += _pin.read();
        ThisThread::sleep_for(10ms);
    }
    val /= 50.0f;

    // Вычисляем сопротивление датчика
    float vrl = val * 3.3f;         // Напряжение на нагрузке
    float rs = ((3.3f / vrl) - 1.0f) * RLOAD;
    rZero = rs;                      // Сохраняем как RZero
}

float MQ135::getResistance() {
    float val = _pin.read();
    float vrl = val * 3.3f;
    return ((3.3f / vrl) - 1.0f) * RLOAD;
}

float MQ135::getPPM() {
    float rs = getResistance();
    return PARA * powf(rs / rZero, -PARB);
}

float MQ135::getRZero() {
    return rZero;
}