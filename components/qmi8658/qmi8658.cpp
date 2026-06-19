#include "qmi8658.hpp"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "QMI8658";

QMI8658::QMI8658(int i2c_port, uint8_t address)
    : m_port(i2c_port), m_addr(address) {}

esp_err_t QMI8658::write_reg(uint8_t reg, uint8_t val) {
    auto res = lgfx::i2c::writeRegister8(m_port, m_addr, reg, val, 0, 400000);
    return res ? ESP_OK : ESP_FAIL;
}

esp_err_t QMI8658::read_reg(uint8_t reg, uint8_t* val) {
    auto res = lgfx::i2c::readRegister(m_port, m_addr, reg, val, 1, 400000);
    return res ? ESP_OK : ESP_FAIL;
}

esp_err_t QMI8658::read_bytes(uint8_t reg, uint8_t* data, size_t len) {
    auto res = lgfx::i2c::readRegister(m_port, m_addr, reg, data, len, 400000);
    return res ? ESP_OK : ESP_FAIL;
}

esp_err_t QMI8658::begin() {
    uint8_t chip_id = 0;
    esp_err_t err = read_reg(0x00, &chip_id); // WHO_AM_I register is 0x00
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I. Err: 0x%x", err);
        return err;
    }
    
    if (chip_id != 0x05) {
        ESP_LOGE(TAG, "Invalid chip ID: 0x%02X (expected 0x05)", chip_id);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "QMI8658 detected successfully. ID: 0x%02X", chip_id);
    
    // 1. Enable register address auto-increment
    err = write_reg(0x02, 0x60); // CTRL1
    if (err != ESP_OK) return err;

    // 2. Configure Accelerometer (CTRL2)
    // 0x03: Scale = +-2g, ODR = 1000Hz (highly sensitive)
    err = write_reg(0x03, 0x03);
    if (err != ESP_OK) return err;
 
    // 3. Configure Gyroscope (CTRL3)
    // 0x03: Scale = +-16dps, ODR = 1000Hz (highly sensitive)
    err = write_reg(0x04, 0x03);
    if (err != ESP_OK) return err;
 
    // 4. Enable both Accelerometer and Gyroscope (CTRL7)
    // 0x03: Enable Gyro and Accel
    err = write_reg(0x08, 0x03);
    if (err != ESP_OK) return err;
 
    ESP_LOGI(TAG, "QMI8658 configured and enabled.");
    return ESP_OK;
}
 
esp_err_t QMI8658::read_sensors(float* acc_g, float* gyro_dps) {
    uint8_t data[12];
    esp_err_t err = read_bytes(0x35, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }
 
    int16_t ax = (int16_t)((data[1] << 8) | data[0]);
    int16_t ay = (int16_t)((data[3] << 8) | data[2]);
    int16_t az = (int16_t)((data[5] << 8) | data[4]);
 
    int16_t gx = (int16_t)((data[7] << 8) | data[6]);
    int16_t gy = (int16_t)((data[9] << 8) | data[8]);
    int16_t gz = (int16_t)((data[11] << 8) | data[10]);
 
    // Divisors adjusted for new sensitive scales (+-2g -> 16384 LSB/g, +-16dps -> 2048 LSB/dps)
    acc_g[0] = (float)ax / 16384.0f;
    acc_g[1] = (float)ay / 16384.0f;
    acc_g[2] = (float)az / 16384.0f;
 
    gyro_dps[0] = (float)gx / 2048.0f;
    gyro_dps[1] = (float)gy / 2048.0f;
    gyro_dps[2] = (float)gz / 2048.0f;
 
    return ESP_OK;
}
