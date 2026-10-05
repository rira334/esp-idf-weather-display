#include "lcd1602.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_rom_sys.h"

#include "driver/i2c_master.h"

#define I2C_PORT I2C_NUM_0
#define I2C_FREQ_HZ 100000

#define LCD_RS (1 << 0)
#define LCD_RW (1 << 1)
#define LCD_EN (1 << 2)
#define LCD_BACKLIGHT (1 << 3)

static const char *TAG = "lcd1602";

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t lcd_dev;

/* Send one output byte to the PCF8574 backpack over I2C. */
static esp_err_t pcf8574_write(uint8_t data)
{
  return i2c_master_transmit(
      lcd_dev,
      &data,
      1,
      100);
}

static void lcd_pulse_enable(uint8_t data)
{
  /* Set data and RS before raising Enable. */
  ESP_ERROR_CHECK(pcf8574_write(data & ~LCD_EN));
  esp_rom_delay_us(1);
  ESP_ERROR_CHECK(pcf8574_write(data | LCD_EN));
  esp_rom_delay_us(10);

  ESP_ERROR_CHECK(pcf8574_write(data & ~LCD_EN));
  esp_rom_delay_us(100);
}

/* D4-D7 use the upper four bits; RS selects text or commands, RW stays low. */
static void lcd_write_nibble(uint8_t nibble, uint8_t mode)
{
  uint8_t data = nibble & 0xF0;

  data |= LCD_BACKLIGHT;
  data &= ~LCD_RW;

  if (mode)
  {
    data |= LCD_RS;
  }
  else
  {
    data &= ~LCD_RS;
  }

  lcd_pulse_enable(data);
}

/* In 4-bit mode, send the high nibble first, then the low nibble. */
static void lcd_send(uint8_t value, uint8_t rs)
{
  lcd_write_nibble(value & 0xF0, rs);
  lcd_write_nibble((value << 4) & 0xF0, rs);
}

/* Clear and home need a longer wait because this driver does not read busy status. */
static void lcd_command(uint8_t command)
{
  lcd_send(command, 0);

  if (command == 0x01 || command == 0x02)
  {
    /* Millisecond task delays can round to zero at low tick rates. */
    esp_rom_delay_us(3000);
  }
  else
  {
    esp_rom_delay_us(100);
  }
}

void lcd1602_write_char(char c)
{
  lcd_send((uint8_t)c, 1);
  esp_rom_delay_us(100);
}

void lcd1602_print(const char *text)
{
  while (*text)
  {
    lcd1602_write_char(*text++);
  }
}

void lcd1602_clear(void)
{
  lcd_command(0x01);
}

/* The two visible rows start at LCD memory addresses 0x00 and 0x40. */
void lcd1602_set_cursor(uint8_t col, uint8_t row)
{
  uint8_t address;

  if (row == 0)
  {
    address = 0x00;
  }
  else
  {
    address = 0x40;
  }

  address += col;

  lcd_command(0x80 | address);
}

/* Reset the controller, select 4-bit/two-line mode, then enable the display. */
static void lcd_hw_init(void)
{
  ESP_ERROR_CHECK(pcf8574_write(LCD_BACKLIGHT));
  vTaskDelay(pdMS_TO_TICKS(100));

  lcd_write_nibble(0x30, 0);
  vTaskDelay(pdMS_TO_TICKS(10));

  lcd_write_nibble(0x30, 0);
  vTaskDelay(pdMS_TO_TICKS(10));

  lcd_write_nibble(0x30, 0);
  vTaskDelay(pdMS_TO_TICKS(10));

  lcd_write_nibble(0x20, 0);
  vTaskDelay(pdMS_TO_TICKS(10));

  lcd_command(0x28);
  lcd_command(0x08);
  lcd_command(0x01);

  vTaskDelay(pdMS_TO_TICKS(5));

  lcd_command(0x06);
  lcd_command(0x0C);
}

/* Create the I2C bus and probe the two supported backpack addresses. */
esp_err_t lcd1602_init(int sda_gpio, int scl_gpio)
{
  i2c_master_bus_config_t bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_PORT,
      .scl_io_num = scl_gpio,
      .sda_io_num = sda_gpio,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };

  esp_err_t err = i2c_new_master_bus(
      &bus_config,
      &i2c_bus);

  if (err != ESP_OK)
  {
    return err;
  }

  uint16_t address;

  if (i2c_master_probe(i2c_bus, 0x27, 100) == ESP_OK)
  {
    address = 0x27;
  }
  else if (i2c_master_probe(i2c_bus, 0x3F, 100) == ESP_OK)
  {
    address = 0x3F;
  }
  else
  {
    ESP_LOGE(TAG, "LCD not found");

    return ESP_ERR_NOT_FOUND;
  }

  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = I2C_FREQ_HZ,
  };

  err = i2c_master_bus_add_device(
      i2c_bus,
      &dev_config,
      &lcd_dev);

  if (err != ESP_OK)
  {
    return err;
  }

  ESP_LOGI(TAG, "LCD found at 0x%02X", address);

  lcd_hw_init();

  return ESP_OK;
}
