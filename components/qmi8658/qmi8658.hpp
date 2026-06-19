#pragma once

#include <stdint.h>
#include "esp_err.h"

class QMI8658 {
public:
    static constexpr uint8_t I2C_ADDR_LOW = 0x6A;
    static constexpr uint8_t I2C_ADDR_HIGH = 0x6B;

    QMI8658(int i2c_port = 0, uint8_t address = I2C_ADDR_HIGH);
    
    esp_err_t begin();
    esp_err_t read_sensors(float* acc_g, float* gyro_dps);
    
private:
    int m_port;
    uint8_t m_addr;

    esp_err_t write_reg(uint8_t reg, uint8_t val);
    esp_err_t read_reg(uint8_t reg, uint8_t* val);
    esp_err_t read_bytes(uint8_t reg, uint8_t* data, size_t len);
};
