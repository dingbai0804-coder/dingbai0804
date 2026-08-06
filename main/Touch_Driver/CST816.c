#include "CST816.h"
#include "TCA9554PWR.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define CST816_PORT I2C_NUM_1
#define CST816_ADDR 0x15
#define CST816_SDA  GPIO_NUM_1
#define CST816_SCL  GPIO_NUM_3
#define CST816_INT  GPIO_NUM_4

static const char *TAG = "CST816";
static bool ready;

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(CST816_PORT, CST816_ADDR, &reg, 1,
                                        data, len, pdMS_TO_TICKS(20));
}

esp_err_t CST816_Init(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CST816_SDA,
        .scl_io_num = CST816_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(CST816_PORT, &cfg));
    esp_err_t err = i2c_driver_install(CST816_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    gpio_set_direction(CST816_INT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CST816_INT, GPIO_PULLUP_ONLY);
    Set_EXIO(TCA9554_EXIO1, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    Set_EXIO(TCA9554_EXIO1, true);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t chip_id = 0;
    err = read_regs(0xA7, &chip_id, 1);
    ready = err == ESP_OK;
    if (ready) ESP_LOGI(TAG, "touch ready, chip id=0x%02x", chip_id);
    else ESP_LOGE(TAG, "touch controller not found: %s", esp_err_to_name(err));
    return err;
}

bool CST816_Read(uint16_t *x, uint16_t *y)
{
    static uint16_t stable_x, stable_y;
    static uint8_t stable_count;
    static bool active;
    if (!ready) return false;
    uint8_t data[5];
    if (read_regs(0x02, data, sizeof(data)) != ESP_OK || (data[0] & 0x0f) == 0) {
        stable_count = 0;
        active = false;
        return false;
    }
    uint16_t raw_x = ((uint16_t)(data[1] & 0x0f) << 8) | data[2];
    uint16_t raw_y = ((uint16_t)(data[3] & 0x0f) << 8) | data[4];
    if (raw_x >= 360 || raw_y >= 360) return false;
    if (!active) {
        int dx = (int)raw_x - (int)stable_x;
        int dy = (int)raw_y - (int)stable_y;
        if (stable_count == 0 || dx > 14 || dx < -14 || dy > 14 || dy < -14) {
            stable_x = raw_x;
            stable_y = raw_y;
            stable_count = 1;
            return false;
        }
        if (++stable_count < 2) return false;
        active = true;
    } else {
        /* Suppress the small coordinate jitter produced while a finger rests. */
        stable_x = (stable_x * 3 + raw_x) / 4;
        stable_y = (stable_y * 3 + raw_y) / 4;
    }
    /* Native panel and CST816 coordinates use the same orientation. */
    *x = stable_x;
    *y = stable_y;
    return true;
}
