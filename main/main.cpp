#define LGFX_USE_V1
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_cpu.h"
#include "rom/ets_sys.h"
#include "soc/gpio_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qmi8658.hpp"
#include <LovyanGFX.hpp>
#include <stdio.h>

// all credits going to Antigravity IDE,LovyanGFX,Qmi8658 drivers and all the
// communities I've learned from.

static const char *TAG = "Main";

// Custom LovyanGFX Configuration class for UEDX32480035E-WB-A v1.0 (3.5"
// 320x480)
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_CHSC6X _touch_instance; // Touch controller instance (CHSC6540)

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST; // Use SPI2 (FSPI) on ESP32-S3
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; // 40 MHz (safe and very fast)
      cfg.freq_read = 5000000;
      cfg.pin_sclk = 40; // SCK pin
      cfg.pin_mosi = 45; // MOSI pin (IO45-TDI-MOSI)
      cfg.pin_miso = -1; // MISO pin (unused)
      cfg.pin_dc = 41;   // DC pin (IO41-LCD-RS)
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 42;  // CS pin
      cfg.pin_rst = 39; // RST pin
      cfg.pin_busy = -1;
      cfg.memory_width = 320;
      cfg.memory_height = 480;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 13;  // Backlight Pin (IO13-BL-IN)
      cfg.freq = 44100; // PWM Frequency
      cfg.pwm_channel = 0;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;
      cfg.x_max = 320;
      cfg.y_min = 0;
      cfg.y_max = 480;
      cfg.pin_int = 4;          // Touch INT (IO4)
      cfg.pin_sda = 1;          // Touch SDA (IO1)
      cfg.pin_scl = 3;          // Touch SCL (IO3)
      cfg.pin_rst = 2;          // Touch RST (IO2)
      cfg.i2c_port = I2C_NUM_0; // Share Port 0
      cfg.i2c_addr = 0x2E;      // CHSC6540/FT5x06 address
      cfg.freq = 400000;
      cfg.bus_shared = true; // Shares I2C bus with QMI8658
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

static LGFX lcd;
static LGFX_Sprite sprite(&lcd); // Off-screen buffer to prevent flickering

// Manual ST7365P initialization sequence to override default settings
void init_st7365p() {
  ESP_LOGI(TAG, "Initializing ST7365P custom registers...");
  lcd.startWrite();

  // Sleep out
  lcd.writeCommand(0x11);
  vTaskDelay(pdMS_TO_TICKS(120));

  // Unlock Extension Command 2 Part I & II
  lcd.writeCommand(0xF0);
  lcd.writeData(0xC3);
  lcd.writeCommand(0xF0);
  lcd.writeData(0x96);

  // Memory Data Access Control (0x36) - Set to 0x28 or 0x48 etc.
  lcd.writeCommand(0x36);
  lcd.writeData(0x28);

  // Interface Pixel Format (0x3A)
  lcd.writeCommand(0x3A);
  lcd.writeData(0x55);

  // Display Inversion Control (0xB4)
  lcd.writeCommand(0xB4);
  lcd.writeData(0x01);

  // Entry Mode Set (0xB7)
  lcd.writeCommand(0xB7);
  lcd.writeData(0xC6);

  // Power Control 1 (0xC0)
  lcd.writeCommand(0xC0);
  lcd.writeData(0x80);
  lcd.writeData(0x04);

  // Power Control 2 (0xC1)
  lcd.writeCommand(0xC1);
  lcd.writeData(0x13);

  // VCOM Control 1 & 2 (0xC5)
  lcd.writeCommand(0xC5);
  lcd.writeData(0xA7);
  lcd.writeCommand(0xC5);
  lcd.writeData(0x16);

  // Display Output Ctrl Adjust (0xE8)
  static const uint8_t e8_data[] = {0x40, 0x8a, 0x00, 0x00,
                                    0x29, 0x19, 0xA5, 0x33};
  lcd.writeCommand(0xE8);
  for (int i = 0; i < 8; i++) {
    lcd.writeData(e8_data[i]);
  }

  // Positive Gamma Correction (0xE0)
  static const uint8_t e0_data[] = {0xF0, 0x19, 0x20, 0x10, 0x11, 0x0A, 0x46,
                                    0x44, 0x57, 0x09, 0x1A, 0x1B, 0x2A, 0x2D};
  lcd.writeCommand(0xE0);
  for (int i = 0; i < 14; i++) {
    lcd.writeData(e0_data[i]);
  }

  // Negative Gamma Correction (0xE1)
  static const uint8_t e1_data[] = {0xF0, 0x12, 0x1A, 0x0A, 0x0C, 0x18, 0x45,
                                    0x44, 0x56, 0x3F, 0x15, 0x11, 0x24, 0x26};
  lcd.writeCommand(0xE1);
  for (int i = 0; i < 14; i++) {
    lcd.writeData(e1_data[i]);
  }

  // Lock Extension Command 2 Part I & II
  lcd.writeCommand(0xF0);
  lcd.writeData(0x3C);
  lcd.writeCommand(0xF0);
  lcd.writeData(0x69);

  // Display Inversion ON (0x21)
  lcd.writeCommand(0x21);

  // Display ON (0x29)
  lcd.writeCommand(0x29);
  vTaskDelay(pdMS_TO_TICKS(50));

  lcd.endWrite();
  ESP_LOGI(TAG, "ST7365P custom registers initialized.");
}

#include <math.h>

// Logarithmic scaling for sensor display bubble movement
// High sensitivity around center, compressed at larger deviations, normalized
// to prevent scale overflow
float log_scale(float val, float sensitivity, float max_val, float max_disp) {
  float abs_val = fabsf(val);
  if (abs_val > max_val)
    abs_val = max_val;
  float scaled = log1pf(sensitivity * abs_val) / log1pf(sensitivity * max_val);
  return (val >= 0.0f ? 1.0f : -1.0f) * scaled * max_disp;
}

// Draw a telemetry bar graph with a center line for negative/positive values
void draw_telemetry_bar(LGFX_Sprite *spr, int x, int y, int w, int h, float val,
                        float max_val, uint32_t color) {
  spr->drawRect(x, y, w, h, TFT_WHITE);
  int mid_x = x + w / 2;
  spr->drawFastVLine(mid_x, y, h, TFT_DARKGRAY); // Center mark

  float pct = val / max_val;
  if (pct > 1.0f)
    pct = 1.0f;
  if (pct < -1.0f)
    pct = -1.0f;

  int bar_w = (int)((w / 2 - 2) * pct);
  if (bar_w > 0) {
    spr->fillRect(mid_x + 1, y + 2, bar_w, h - 4, color);
  } else if (bar_w < 0) {
    spr->fillRect(mid_x + 1 + bar_w, y + 2, -bar_w, h - 4, color);
  }
}

float acc_offset[3] = {0.0f, 0.0f, 0.0f};
float gyro_offset[3] = {0.0f, 0.0f, 0.0f};

void calibrate_sensors(QMI8658 &imu) {
  float sum_acc[3] = {0};
  float sum_gyro[3] = {0};
  int samples = 20;
  int valid_samples = 0;

  ESP_LOGI(TAG,
           "Starting IMU offset calibration (please leave device still)...");
  for (int i = 0; i < samples; i++) {
    float temp_acc[3], temp_gyro[3];
    if (imu.read_sensors(temp_acc, temp_gyro) == ESP_OK) {
      sum_acc[0] += temp_acc[0];
      sum_acc[1] += temp_acc[1];
      sum_acc[2] += temp_acc[2];
      sum_gyro[0] += temp_gyro[0];
      sum_gyro[1] += temp_gyro[1];
      sum_gyro[2] += temp_gyro[2];
      valid_samples++;
    }
    vTaskDelay(pdMS_TO_TICKS(15));
  }
  if (valid_samples > 0) {
    acc_offset[0] = sum_acc[0] / valid_samples;
    acc_offset[1] = sum_acc[1] / valid_samples;
    acc_offset[2] = sum_acc[2] / valid_samples;
    gyro_offset[0] = sum_gyro[0] / valid_samples;
    gyro_offset[1] = sum_gyro[1] / valid_samples;
    gyro_offset[2] = sum_gyro[2] / valid_samples;
    ESP_LOGI(TAG,
             "Calibration completed. Offsets: Acc=[%.3f, %.3f, %.3f], "
             "Gyro=[%.1f, %.1f, %.1f]",
             acc_offset[0], acc_offset[1], acc_offset[2], gyro_offset[0],
             gyro_offset[1], gyro_offset[2]);
  }
}

// Settings values
// Settings values
float g_alarm_threshold = 0.05f; // 0.01G to 0.50G (highly sensitive)
float g_alarm_threshold_dps = 5.0f; // 1.0 to 50.0 dps
float g_gain_factor = 100.0f;    // 10.0 to 500.0
int g_slider3_val = 50; // 0 to 100 (controls oversampling filter integration)
bool g_led_alert_enabled = true;
bool g_buzzer_alert_enabled = false;

// Alarm status variables
bool g_alarm_active = false;
uint64_t g_alarm_last_active_time = 0;

volatile int g_display_mode = 0;

// Seismograph/vibration values & globals for the high-speed sampling task
static QMI8658 imu(0, QMI8658::I2C_ADDR_HIGH);
esp_err_t g_imu_err = ESP_FAIL;
volatile float g_vibration_acc = 0.0f;
volatile float g_vibration_gyro = 0.0f;
volatile float g_calibrated_acc[3] = {0};
volatile float g_calibrated_gyro[3] = {0};

void imu_sampling_task(void *pvParameters) {
  float acc[3] = {0};
  float gyro[3] = {0};
  float ema_acc[3] = {0};
  float ema_gyro[3] = {0};

  // Wait for IMU to be ready
  while (g_imu_err != ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // Initialize EMA
  if (imu.read_sensors(acc, gyro) == ESP_OK) {
    for (int i = 0; i < 3; i++) {
      ema_acc[i] = acc[i] - acc_offset[i];
      ema_gyro[i] = gyro[i] - gyro_offset[i];
    }
  }

  while (true) {
    if (imu.read_sensors(acc, gyro) == ESP_OK) {
      // Apply calibration offsets
      float cal_acc[3] = {acc[0] - acc_offset[0], acc[1] - acc_offset[1],
                          acc[2] - acc_offset[2]};
      float cal_gyro[3] = {gyro[0] - gyro_offset[0], gyro[1] - gyro_offset[1],
                           gyro[2] - gyro_offset[2]};

      // Save globally for visualization (double buffered via volatile)
      for (int i = 0; i < 3; i++) {
        g_calibrated_acc[i] = cal_acc[i];
        g_calibrated_gyro[i] = cal_gyro[i];
      }

      // Alpha filter coefficient controlled by Slider 3 (Reserved Parameter)
      // Range of g_slider3_val is 0..100.
      // Map 0 to 0.001 (very strong damping / high integration)
      // Map 100 to 0.2 (light damping / fast tracking)
      float alpha = 0.001f + ((float)g_slider3_val / 100.0f) * 0.199f;

      float dev_acc_sq = 0.0f;
      float dev_gyro_sq = 0.0f;

      for (int i = 0; i < 3; i++) {
        // Exponential Moving Average to track DC offset (slow drift / gravity)
        ema_acc[i] = alpha * cal_acc[i] + (1.0f - alpha) * ema_acc[i];
        float dev_a = cal_acc[i] - ema_acc[i];
        dev_acc_sq += dev_a * dev_a;

        ema_gyro[i] = alpha * cal_gyro[i] + (1.0f - alpha) * ema_gyro[i];
        float dev_g = cal_gyro[i] - ema_gyro[i];
        dev_gyro_sq += dev_g * dev_g;
      }

      float vibr_acc = sqrtf(dev_acc_sq);
      float vibr_gyro = sqrtf(dev_gyro_sq);

      // Seismograph Envelope detector (peak-hold with exponential decay)
      // Decay rate of 0.992 at 500Hz gives ~250ms decay half-life, perfect for
      // visual peak display
      if (vibr_acc > g_vibration_acc) {
        g_vibration_acc = vibr_acc;
      } else {
        g_vibration_acc = vibr_acc * 0.008f + g_vibration_acc * 0.992f;
      }

      if (vibr_gyro > g_vibration_gyro) {
        g_vibration_gyro = vibr_gyro;
      } else {
        g_vibration_gyro = vibr_gyro * 0.008f + g_vibration_gyro * 0.992f;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2)); // Sample at 500 Hz
  }
}

#define WS2812_PIN 0

#define WS2812_HIGH() REG_WRITE(GPIO_OUT_W1TS_REG, (1 << WS2812_PIN))
#define WS2812_LOW()  REG_WRITE(GPIO_OUT_W1TC_REG, (1 << WS2812_PIN))

static portMUX_TYPE ws2812_mux = portMUX_INITIALIZER_UNLOCKED;

static inline void ws2812_write_byte(uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    if (byte & 0x80) {
      WS2812_HIGH();
      uint32_t start = esp_cpu_get_cycle_count();
      while ((esp_cpu_get_cycle_count() - start) < 210) {}
      WS2812_LOW();
      start = esp_cpu_get_cycle_count();
      while ((esp_cpu_get_cycle_count() - start) < 80) {}
    } else {
      WS2812_HIGH();
      uint32_t start = esp_cpu_get_cycle_count();
      while ((esp_cpu_get_cycle_count() - start) < 80) {}
      WS2812_LOW();
      start = esp_cpu_get_cycle_count();
      while ((esp_cpu_get_cycle_count() - start) < 210) {}
    }
    byte <<= 1;
  }
}

void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b) {
  taskENTER_CRITICAL(&ws2812_mux);
  ws2812_write_byte(g); // WS2812 uses GRB
  ws2812_write_byte(r);
  ws2812_write_byte(b);
  taskEXIT_CRITICAL(&ws2812_mux);
  ets_delay_us(60);
}

#define BUZZER_PIN GPIO_NUM_38
#define BUZZER_LEDC_CHANNEL LEDC_CHANNEL_1
#define BUZZER_LEDC_TIMER LEDC_TIMER_1

void buzzer_init() {
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
  ledc_timer.duty_resolution  = LEDC_TIMER_10_BIT;
  ledc_timer.timer_num        = BUZZER_LEDC_TIMER;
  ledc_timer.freq_hz          = 500; // pleasant low pitch
  ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {};
  ledc_channel.gpio_num       = BUZZER_PIN;
  ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
  ledc_channel.channel        = BUZZER_LEDC_CHANNEL;
  ledc_channel.intr_type      = LEDC_INTR_DISABLE;
  ledc_channel.timer_sel      = BUZZER_LEDC_TIMER;
  ledc_channel.duty           = 0;
  ledc_channel.hpoint         = 0;
  ledc_channel_config(&ledc_channel);
}

void buzzer_beep(bool on) {
  if (on) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, 512); // 50% duty
  } else {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, 0); // off
  }
  ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
}

void draw_checkbox(LGFX_Sprite *spr, int x, int y, const char *label, bool checked) {
  spr->drawRect(x, y, 16, 16, TFT_WHITE);
  if (checked) {
    spr->fillRect(x + 3, y + 3, 10, 10, TFT_BLUE);
  }
  spr->setTextColor(TFT_WHITE);
  spr->setTextSize(1);
  spr->setTextDatum(textdatum_t::middle_left);
  spr->drawString(label, x + 24, y + 8);
}

void draw_slider(LGFX_Sprite *spr, int x, int y, int w, float val,
                 float min_val, float max_val, const char *label,
                 const char *val_str) {
  // Label
  spr->setTextColor(TFT_WHITE);
  spr->setTextSize(2);
  spr->setTextDatum(textdatum_t::middle_left);
  spr->drawString(label, x, y - 20);

  // Value string
  spr->setTextDatum(textdatum_t::middle_right);
  spr->drawString(val_str, x + w, y - 20);

  // Slider track
  spr->drawRoundRect(x, y - 4, w, 8, 4, TFT_DARKGRAY);
  spr->fillRoundRect(x, y - 4, w, 8, 4, TFT_DARKGRAY);

  // Fill track up to the slider thumb
  float pct = (val - min_val) / (max_val - min_val);
  if (pct < 0.0f)
    pct = 0.0f;
  if (pct > 1.0f)
    pct = 1.0f;
  int thumb_x = x + (int)(w * pct);
  spr->fillRoundRect(x, y - 4, thumb_x - x, 8, 4, TFT_BLUE);

  // Slider thumb
  spr->fillCircle(thumb_x, y, 12, TFT_WHITE);
  spr->drawCircle(thumb_x, y, 12, TFT_BLUE);
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Starting HMI Display and IMU Test...");

  // Configure interface mode pins: IM0 (GPIO 47) and IM1 (GPIO 48) as outputs
  // and set both to HIGH (1) for 4-line SPI
  gpio_config_t im_gpio_conf = {};
  im_gpio_conf.intr_type = GPIO_INTR_DISABLE;
  im_gpio_conf.mode = GPIO_MODE_OUTPUT;
  im_gpio_conf.pin_bit_mask = (1ULL << 47) | (1ULL << 48);
  im_gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  im_gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&im_gpio_conf);
  gpio_set_level(GPIO_NUM_47, 1);
  gpio_set_level(GPIO_NUM_48, 1);
  ESP_LOGI(TAG, "Interface mode pins (IM0=47, IM1=48) set to HIGH.");

  // Perform hardware reset sequence on GPIO 2 (Touch RST) to wake up CHSC6540
  gpio_config_t rst_gpio_conf = {};
  rst_gpio_conf.intr_type = GPIO_INTR_DISABLE;
  rst_gpio_conf.mode = GPIO_MODE_OUTPUT;
  rst_gpio_conf.pin_bit_mask = (1ULL << 2);
  rst_gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  rst_gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&rst_gpio_conf);
  gpio_set_level(GPIO_NUM_2, 0); // Active low reset
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(GPIO_NUM_2, 1);
  vTaskDelay(pdMS_TO_TICKS(100)); // Wait for touch IC to fully boot
  ESP_LOGI(TAG, "Touch reset completed (GPIO 2 toggled).");

  // Configure Touch INT (GPIO 4) as input with pullup
  gpio_config_t int_gpio_conf = {};
  int_gpio_conf.intr_type = GPIO_INTR_DISABLE; // No interrupt needed
  int_gpio_conf.mode = GPIO_MODE_INPUT;
  int_gpio_conf.pin_bit_mask = (1ULL << 4);
  int_gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  int_gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&int_gpio_conf);

  // Initialize GPIO 0 for WS2812B
  gpio_config_t led_gpio_conf = {};
  led_gpio_conf.intr_type = GPIO_INTR_DISABLE;
  led_gpio_conf.mode = GPIO_MODE_OUTPUT;
  led_gpio_conf.pin_bit_mask = (1ULL << GPIO_NUM_0);
  led_gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  led_gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&led_gpio_conf);
  ws2812_set_color(0, 255, 0); // Start Green

  // Initialize Buzzer on GPIO 38
  buzzer_init();

  // Initialize LovyanGFX Display
  lcd.init();

  // Execute custom register setup for ST7365P
  init_st7365p();

  // Set backlight brightness to 80% (204 out of 255)
  lcd.setBrightness(204);

  lcd.setRotation(1); // Landscape

  g_display_mode = 0; // 0 = Bubble Level, 1 = Telemetry Graph, 2 = Settings

  // Initialize Screen layout and draw initial header
  lcd.clear(TFT_BLACK);
  lcd.fillRect(0, 0, 480, 40, TFT_BLUE);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.drawString("IMU Bubble Level Mode (Tap to Swap)", 10, 10);

  // Create the sprite for double-buffering (covers 480x280 area below title
  // bar)
  sprite.createSprite(480, 280);

  // Initialize IMU on Port 0 (shared with touch)
  g_imu_err = imu.begin();
  if (g_imu_err != ESP_OK) {
    ESP_LOGW(TAG, "IMU begin failed on 0x6B, trying 0x6A...");
    imu = QMI8658(0, QMI8658::I2C_ADDR_LOW);
    g_imu_err = imu.begin();
  }

  if (g_imu_err == ESP_OK) {
    calibrate_sensors(imu);
    // Start high-speed sampling task on Core 1 (Priority 8)
    xTaskCreatePinnedToCore(imu_sampling_task, "imu_sampling_task", 4096, NULL,
                            8, NULL, 1);
  }

  char buf[128];
  uint32_t last_draw_time = 0;
  bool was_touched = false;
  uint32_t last_tap_time = 0;

  while (true) {
    // Read Touch status by querying LovyanGFX
    uint16_t tx = 0, ty = 0;
    bool is_touched = lcd.getTouch(&tx, &ty);

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool tap_detected = false;

    if (is_touched) {
      if (!was_touched && (now - last_tap_time > 250)) {
        tap_detected = true;
        last_tap_time = now;
      }
      was_touched = true;
    } else {
      was_touched = false;
    }

    if (tap_detected) {
      ESP_LOGI(TAG, "Tap detected: X=%d, Y=%d", tx, ty);

      if (g_display_mode != 2) {
        // Instant toggle for Mode 0 & Mode 1
        g_display_mode = (g_display_mode + 1) % 3;
        lcd.clear(TFT_BLACK);
        if (g_display_mode == 0) {
          lcd.fillRect(0, 0, 480, 40, TFT_BLUE);
          lcd.setTextColor(TFT_WHITE);
          lcd.setTextSize(2);
          lcd.drawString("IMU Bubble Level Mode (Tap to Swap)", 10, 10);
        } else if (g_display_mode == 1) {
          lcd.fillRect(0, 0, 480, 40, TFT_NAVY);
          lcd.setTextColor(TFT_WHITE);
          lcd.setTextSize(2);
          lcd.drawString("IMU Telemetry Graph Mode (Tap to Swap)", 10, 10);
        } else {
          lcd.fillRect(0, 0, 480, 40, TFT_DARKCYAN);
          lcd.setTextColor(TFT_WHITE);
          lcd.setTextSize(2);
          lcd.drawString("Settings (Tap Header to Swap)", 10, 10);
        }
        last_draw_time = 0; // Force immediate redraw
      } else {
        // Settings mode: check if it's a tap in the header (Y < 40)
        if (ty < 40) {
          g_display_mode = (g_display_mode + 1) % 3;
          lcd.clear(TFT_BLACK);
          lcd.fillRect(0, 0, 480, 40, TFT_BLUE);
          lcd.setTextColor(TFT_WHITE);
          lcd.setTextSize(2);
          lcd.drawString("IMU Bubble Level Mode (Tap to Swap)", 10, 10);
          last_draw_time = 0;
        } else {
          // Tap on slider: jump directly (clamped for sticky dragging)
          int slider_w = 300;
          int slider_x = 50;
          int cx = tx;
          if (cx < slider_x)
            cx = slider_x;
          if (cx > (slider_x + slider_w))
            cx = slider_x + slider_w;
          float pct = (float)(cx - slider_x) / slider_w;

          if (ty >= 65 && ty <= 95) { // Slider 1 (80 +- 15px) - 0.01G to 0.50G
            g_alarm_threshold = 0.01f + pct * (0.50f - 0.01f);
          } else if (ty >= 120 && ty <= 150) { // Slider 2 (135 +- 15px) - 1.0 to 50.0 DPS
            g_alarm_threshold_dps = 1.0f + pct * (50.0f - 1.0f);
          } else if (ty >= 175 && ty <= 205) { // Slider 3 (190 +- 15px) - 10.0 to 500.0 GAIN
            g_gain_factor = 10.0f + pct * (500.0f - 10.0f);
          } else if (ty >= 230 && ty <= 260) { // Slider 4 (245 +- 15px) - 0% to 100%
            g_slider3_val = (int)(pct * 100);
          } else if (ty >= 270 && ty <= 305) { // Checkboxes (285 +- 15/20px)
            if (tx >= 50 && tx <= 220) {
              g_led_alert_enabled = !g_led_alert_enabled;
            } else if (tx >= 250 && tx <= 420) {
              g_buzzer_alert_enabled = !g_buzzer_alert_enabled;
            }
          }
        }
      }
    }

    // For smooth slider dragging in Settings mode
    if (g_display_mode == 2 && is_touched && !tap_detected) {
      int slider_w = 300;
      int slider_x = 50;
      int cx = tx;
      if (cx < slider_x)
        cx = slider_x;
      if (cx > (slider_x + slider_w))
        cx = slider_x + slider_w;
      float pct = (float)(cx - slider_x) / slider_w;

      if (ty >= 65 && ty <= 95) {
        g_alarm_threshold = 0.01f + pct * (0.50f - 0.01f);
      } else if (ty >= 120 && ty <= 150) {
        g_alarm_threshold_dps = 1.0f + pct * (50.0f - 1.0f);
      } else if (ty >= 175 && ty <= 205) {
        g_gain_factor = 10.0f + pct * (500.0f - 10.0f);
      } else if (ty >= 230 && ty <= 260) {
        g_slider3_val = (int)(pct * 100);
      }
    }

    now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_draw_time >= 100) {
      last_draw_time = now;

      if (g_imu_err == ESP_OK) {
        // Copy values locally to prevent race conditions during rendering
        float cal_acc[3] = {g_calibrated_acc[0], g_calibrated_acc[1],
                            g_calibrated_acc[2]};
        float cal_gyro[3] = {g_calibrated_gyro[0], g_calibrated_gyro[1],
                             g_calibrated_gyro[2]};

        // Get dynamic vibration magnitudes from the high-speed task
        float dev_acc = g_vibration_acc;
        float dev_gyro = g_vibration_gyro;

        // Trigger alarm if dynamic AC vibration exceeds the threshold
        bool triggered = (dev_acc > g_alarm_threshold) || (dev_gyro > g_alarm_threshold_dps);
        if (triggered) {
          g_alarm_active = true;
          g_alarm_last_active_time = now;
        } else {
          // Hold alarm active for 1000 ms (1 second) after settling
          if (now - g_alarm_last_active_time >= 1000) {
            g_alarm_active = false;
          }
        }

        // Handle physical alarm signals (LED & Buzzer)
        static int last_led_state = -1;
        int target_led_state = 0; // 0 = Off, 1 = Green, 2 = Red
        if (g_led_alert_enabled) {
          target_led_state = g_alarm_active ? 2 : 1;
        }

        if (target_led_state != last_led_state) {
          last_led_state = target_led_state;
          if (target_led_state == 2) {
            ws2812_set_color(255, 0, 0); // Red
          } else if (target_led_state == 1) {
            ws2812_set_color(0, 255, 0); // Green
          } else {
            ws2812_set_color(0, 0, 0);   // Off
          }
        }

        if (g_alarm_active && g_buzzer_alert_enabled) {
          bool beep_on = ((now / 200) % 2) == 0;
          buzzer_beep(beep_on);
        } else {
          buzzer_beep(false);
        }

        sprite
            .clear(); // Clear the off-screen buffer (default black background)

        if (g_display_mode == 0) {
          // Mode 0: Bubble Level Mode

          // Draw Accelerometer Circle
          sprite.drawCircle(120, 110, 80, TFT_WHITE);
          sprite.drawCircle(120, 110, 40, TFT_DARKGRAY);
          sprite.drawFastHLine(30, 110, 180, TFT_DARKGRAY);
          sprite.drawFastVLine(120, 20, 180, TFT_DARKGRAY);

          sprite.setTextColor(TFT_GREEN);
          sprite.setTextSize(2);
          sprite.setTextDatum(textdatum_t::top_center);
          sprite.drawString("Accelerometer", 120, 200);

          // Draw Gyroscope Circle
          sprite.drawCircle(360, 110, 80, TFT_WHITE);
          sprite.drawCircle(360, 110, 40, TFT_DARKGRAY);
          sprite.drawFastHLine(270, 110, 180, TFT_DARKGRAY);
          sprite.drawFastVLine(360, 20, 180, TFT_DARKGRAY);

          sprite.setTextColor(TFT_YELLOW);
          sprite.drawString("Gyroscope", 360, 200);

          // Scale dot offsets logarithmically: High sensitivity for small
          // values, compressed for large Sensitivity: g_gain_factor for
          // Accel, 2.0 for Gyro. Max displacement: 74 px
          float ax_off = log_scale(cal_acc[0], g_gain_factor, 2.0f, 74.0f);
          float ay_off =
              log_scale(-cal_acc[1], g_gain_factor, 2.0f,
                        74.0f); // Invert Y axis for display coordinate system

          float gx_off = log_scale(cal_gyro[0], 2.0f, 16.0f, 74.0f);
          float gy_off =
              log_scale(-cal_gyro[1], 2.0f, 16.0f, 74.0f); // Invert Y

          // Draw Accelerometer bubble dot
          sprite.fillCircle(120 + (int)ax_off, 110 + (int)ay_off, 6, TFT_GREEN);

          // Draw Gyroscope bubble dot
          sprite.fillCircle(360 + (int)gx_off, 110 + (int)gy_off, 6,
                            TFT_YELLOW);

          // Output raw text telemetry values under the bubbles (calibrated
          // values)
          sprite.setTextSize(1);
          sprite.setTextColor(TFT_GREEN);
          snprintf(buf, sizeof(buf), "X: %+.3f  Y: %+.3f  Z: %+.3f G",
                   cal_acc[0], cal_acc[1], cal_acc[2]);
          sprite.drawString(buf, 120, 230);

          sprite.setTextColor(TFT_YELLOW);
          snprintf(buf, sizeof(buf), "X: %+.1f  Y: %+.1f  Z: %+.1f dps",
                   cal_gyro[0], cal_gyro[1], cal_gyro[2]);
          sprite.drawString(buf, 360, 230);

          // Draw vertical ALARM sprite in the center empty space
          uint32_t alarm_color = g_alarm_active ? TFT_RED : TFT_GREEN;
          sprite.fillRoundRect(220, 20, 40, 180, 5, alarm_color);
          sprite.drawRoundRect(220, 20, 40, 180, 5, TFT_WHITE);

          sprite.setTextColor(TFT_WHITE);
          sprite.setTextSize(2);
          sprite.setTextDatum(textdatum_t::top_center);
          sprite.drawString("A", 240, 30);
          sprite.drawString("L", 240, 60);
          sprite.drawString("A", 240, 90);
          sprite.drawString("R", 240, 120);
          sprite.drawString("M", 240, 150);
        } else if (g_display_mode == 1) {
          // Mode 1: Telemetry Graph Mode

          // Accelerometer column on the Left side
          sprite.setTextColor(TFT_GREEN);
          sprite.setTextSize(2);
          sprite.setTextDatum(textdatum_t::top_left);
          sprite.drawString("Accel (G)", 10, 10);

          sprite.drawString("X:", 10, 50);
          draw_telemetry_bar(&sprite, 40, 50, 120, 20, cal_acc[0], 2.0f,
                             TFT_GREEN);
          snprintf(buf, sizeof(buf), "%+.2f", cal_acc[0]);
          sprite.drawString(buf, 170, 50);

          sprite.drawString("Y:", 10, 100);
          draw_telemetry_bar(&sprite, 40, 100, 120, 20, cal_acc[1], 2.0f,
                             TFT_GREEN);
          snprintf(buf, sizeof(buf), "%+.2f", cal_acc[1]);
          sprite.drawString(buf, 170, 100);

          sprite.drawString("Z:", 10, 150);
          draw_telemetry_bar(&sprite, 40, 150, 120, 20, cal_acc[2], 2.0f,
                             TFT_GREEN);
          snprintf(buf, sizeof(buf), "%+.2f", cal_acc[2]);
          sprite.drawString(buf, 170, 150);

          // Gyroscope column on the Right side (scaled to +-16 dps)
          sprite.setTextColor(TFT_YELLOW);
          sprite.drawString("Gyro (dps)", 250, 10);

          sprite.drawString("X:", 250, 50);
          draw_telemetry_bar(&sprite, 280, 50, 120, 20, cal_gyro[0], 16.0f,
                             TFT_YELLOW);
          snprintf(buf, sizeof(buf), "%+.1f", cal_gyro[0]);
          sprite.drawString(buf, 410, 50);

          sprite.drawString("Y:", 250, 100);
          draw_telemetry_bar(&sprite, 280, 100, 120, 20, cal_gyro[1], 16.0f,
                             TFT_YELLOW);
          snprintf(buf, sizeof(buf), "%+.1f", cal_gyro[1]);
          sprite.drawString(buf, 410, 100);

          sprite.drawString("Z:", 250, 150);
          draw_telemetry_bar(&sprite, 280, 150, 120, 20, cal_gyro[2], 16.0f,
                             TFT_YELLOW);
          snprintf(buf, sizeof(buf), "%+.1f", cal_gyro[2]);
          sprite.drawString(buf, 410, 150);

          // Draw horizontal ALARM sprite below the graphs
          uint32_t alarm_color = g_alarm_active ? TFT_RED : TFT_GREEN;
          sprite.fillRoundRect(10, 220, 460, 35, 5, alarm_color);
          sprite.drawRoundRect(10, 220, 460, 35, 5, TFT_WHITE);

          sprite.setTextColor(TFT_WHITE);
          sprite.setTextSize(2);
          sprite.setTextDatum(textdatum_t::middle_center);
          sprite.drawString(g_alarm_active ? "ALARM TRIGGERED" : "SYSTEM OK",
                            240, 237);
        } else {
          // Mode 2: Settings Mode (Interactive Sliders)
          // Slider 1: Alarm Threshold (0.010G to 0.500G)
          char slider1_str[32];
          snprintf(slider1_str, sizeof(slider1_str), "%.3f G",
                   g_alarm_threshold);
          draw_slider(&sprite, 50, 40, 300, g_alarm_threshold, 0.01f, 0.50f,
                      "Alarm Threshold", slider1_str);

          // Slider 2: Alarm Threshold (1.0 to 50.0 DPS)
          char slider2_str[32];
          snprintf(slider2_str, sizeof(slider2_str), "%.1f DPS",
                   g_alarm_threshold_dps);
          draw_slider(&sprite, 50, 95, 300, g_alarm_threshold_dps, 1.0f, 50.0f,
                      "Alarm Threshold", slider2_str);

          // Slider 3: GAIN Factor (10.0 to 500.0)
          char slider3_str[32];
          snprintf(slider3_str, sizeof(slider3_str), "%.1f", g_gain_factor);
          draw_slider(&sprite, 50, 150, 300, g_gain_factor, 10.0f, 500.0f,
                      "Logarithmic GAIN", slider3_str);

          // Slider 4: Seismic Filter Damping (0% to 100%)
          char slider4_str[32];
          snprintf(slider4_str, sizeof(slider4_str), "%d %%", g_slider3_val);
          draw_slider(&sprite, 50, 205, 300, (float)g_slider3_val, 0.0f, 100.0f,
                      "Filter Damping", slider4_str);

          // Draw settings checkboxes
          draw_checkbox(&sprite, 80, 245, "LED Alert", g_led_alert_enabled);
          draw_checkbox(&sprite, 260, 245, "Buzzer Alert", g_buzzer_alert_enabled);

          // Draw vertical ALARM status sprite on the right
          uint32_t alarm_color = g_alarm_active ? TFT_RED : TFT_GREEN;
          sprite.fillRoundRect(400, 40, 40, 180, 5, alarm_color);
          sprite.drawRoundRect(400, 40, 40, 180, 5, TFT_WHITE);
          sprite.setTextColor(TFT_WHITE);
          sprite.setTextSize(2);
          sprite.setTextDatum(textdatum_t::top_center);
          sprite.drawString("A", 420, 50);
          sprite.drawString("L", 420, 80);
          sprite.drawString("A", 420, 110);
          sprite.drawString("R", 420, 140);
          sprite.drawString("M", 420, 170);
        }

        sprite.pushSprite(
            0, 40); // Instantly swap/update screen area below the title bar
      } else {
        sprite.clear();
        sprite.setTextColor(TFT_RED);
        sprite.setTextSize(2);
        sprite.setTextDatum(textdatum_t::top_center);
        snprintf(buf, sizeof(buf), "IMU Init Error: 0x%x", g_imu_err);
        sprite.drawString(buf, 240, 100);
        sprite.drawString("Check IMU wiring (SDA=1,SCL=3)", 240, 135);
        sprite.pushSprite(0, 40);
      }
    }

    vTaskDelay(
        pdMS_TO_TICKS(15)); // Fast loop polling to catch quick tap gestures
  }
}
