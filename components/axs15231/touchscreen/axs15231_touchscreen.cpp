#include "axs15231_touchscreen.h"
#include "axs15231_defines.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"  // Include I2C header

namespace esphome {
namespace axs15231 {

namespace {
  constexpr static const char *const TAG = "axs15231.touchscreen";
  constexpr static const uint8_t AXS_READ_TOUCHPAD[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x8};
} // anonymous namespace

// Manufacturer-provided functions
bool WriteC8D8(uint8_t c, uint8_t d, esphome::i2c::I2CDevice *device) {
  if (device->write_byte(c) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "->Write(c=0x%02X) fail", c);
    return false;
  }
  if (device->write_byte(d) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "->Write(d=0x%02X) fail", d);
    return false;
  }
  return true;
}

bool IIC_WriteC8D8(uint8_t device_address, uint8_t c, uint8_t d, esphome::i2c::I2CDevice *device) {
  device->set_address(device_address);
  if (!WriteC8D8(c, d, device)) {
    ESP_LOGE(TAG, "->WriteC8D8(c=0x%02X, d=0x%02X) fail", c, d);
    return false;
  }
  return true;
}

void AXS15231Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AXS15231 Touchscreen...");

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(2);
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(2);
  }

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  this->x_raw_max_ = this->display_->get_width();
  this->y_raw_max_ = this->display_->get_height();

  // Set the I2C device address
  this->set_address(0x6A);

  // Disable ILIM
  if (!IIC_WriteC8D8(0x6A, 0x00, 0b00111111, this)) {
    ESP_LOGE(TAG, "Failed to disable ILIM and set current limit");
  } else {
    ESP_LOGI(TAG, "Successfully disabled ILIM and set current limit");
  }

  // Disable BATFET
  if (!IIC_WriteC8D8(0x6A, 0x09, 0b01100100, this)) {
    ESP_LOGE(TAG, "Failed to turn off BATFET");
  } else {
    ESP_LOGI(TAG, "Successfully turned off BATFET");
  }

  ESP_LOGCONFIG(TAG, "AXS15231 Touchscreen setup complete");
}

void AXS15231Touchscreen::update_touches() {
  i2c::ErrorCode err;
  uint8_t buff[AXS_TOUCH_DATA_SIZE];
  uint16_t x, y;
  uint16_t w;

  err = this->write(AXS_READ_TOUCHPAD, sizeof(AXS_READ_TOUCHPAD), false);
  I2C_ERROR_CHECK(err);

  err = this->read(buff, AXS_TOUCH_DATA_SIZE);
  I2C_ERROR_CHECK(err);

  x = AXS_GET_POINT_X(buff, 0);
  y = AXS_GET_POINT_Y(buff, 0);
  w = AXS_GET_WEIGHT(buff);

  if ((x == 0 && y == 0) || AXS_GET_GESTURE_TYPE(buff) != 0) {
    return;
  } else {
    ESP_LOGI(TAG, "Touch Weight: %d", w);
  }
  if (w > 130) { // attempt to filter touch events below weight threshold.
    return;
  }
  this->add_raw_touch_position_(0, x, y);
}

void AXS15231Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "AXS15231 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN(" Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  X min: %d", this->x_raw_min_);
  ESP_LOGCONFIG(TAG, "  X max: %d", this->x_raw_max_);
  ESP_LOGCONFIG(TAG, "  Y min: %d", this->y_raw_min_);
  ESP_LOGCONFIG(TAG, "  Y max: %d", this->y_raw_max_);

  ESP_LOGCONFIG(TAG, "  Swap X/Y: %s", YESNO(this->swap_x_y_));
  ESP_LOGCONFIG(TAG, "  Invert X: %s", YESNO(this->invert_x_));
  ESP_LOGCONFIG(TAG, "  Invert Y: %s", YESNO(this->invert_y_));
}

}  // namespace axs15231
}  // namespace esphome
