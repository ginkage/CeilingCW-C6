#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/ledc.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zboss_api.h"

#include "light_driver.h"

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_CW          GPIO_NUM_10
#define LEDC_OUTPUT_WW          GPIO_NUM_5
#define LEDC_CHANNEL_CW         LEDC_CHANNEL_0
#define LEDC_CHANNEL_WW         LEDC_CHANNEL_1
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2, ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */

static const uint32_t max_duty = 8192;

static bool current_power;
static uint8_t current_level;
static uint16_t current_temperature;
static enum zb_zcl_on_off_start_up_on_off_e start_power;
static uint16_t start_temperature;
static uint8_t reboot_count;

static void set_duty(ledc_channel_t channel, uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, channel));
}

static void update_duty() {
    if (current_power) {
        uint32_t cw, ww;
        if (current_temperature >= 370) {
            ww = max_duty;
            if (current_temperature >= 454)
                cw = (500 - current_temperature) * max_duty / 92;
            else
                cw = (538 - current_temperature) * max_duty / 168;
        }
        else {
            cw = max_duty;
            if (current_temperature >= 250)
                ww = (current_temperature - 130) * max_duty / 240;
            else
                ww = (current_temperature - 150) * max_duty / 200;
        }
        cw = cw * current_level / 255;
        ww = ww * current_level / 255;

        set_duty(LEDC_CHANNEL_CW, cw);
        set_duty(LEDC_CHANNEL_WW, ww);
    } else {
        set_duty(LEDC_CHANNEL_CW, 0);
        set_duty(LEDC_CHANNEL_WW, 0);
    }
}

void light_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel_cw = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_CW,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_CW,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_cw));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel_ww = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_WW,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_WW,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_ww));
}

void light_set_on_off(bool power)
{
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "power", power));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    current_power = power;
    update_duty();
}

void light_set_startup_on_off(uint8_t startup)
{
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "start_power", startup));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);
}

void light_set_level(uint8_t level)
{
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "level", level));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    current_level = level;
    update_duty();
}

void light_set_temperature(uint16_t temperature)
{
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "temp", temperature));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    current_temperature = temperature;
    update_duty();
}

void light_set_startup_temperature(uint16_t startup)
{
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "start_temp", startup));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);
}

void light_set_defaults()
{
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "power", ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "start_power", ZB_ZCL_ON_OFF_START_UP_ON_OFF_IS_ON));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "level", ESP_ZB_ZCL_LEVEL_CONTROL_CURRENT_LEVEL_DEFAULT_VALUE));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "temp", ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "start_temp", ZB_ZCL_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_USE_PREVIOUS_VALUE));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "reboot", 0));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    light_load_settings();
}

void light_load_settings()
{
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READONLY, &my_handle));
    ESP_ERROR_CHECK(nvs_get_u8(my_handle, "power", (uint8_t*)&current_power));
    ESP_ERROR_CHECK(nvs_get_u8(my_handle, "start_power", (uint8_t*)&start_power));
    ESP_ERROR_CHECK(nvs_get_u8(my_handle, "level", &current_level));
    ESP_ERROR_CHECK(nvs_get_u16(my_handle, "temp", &current_temperature));
    ESP_ERROR_CHECK(nvs_get_u16(my_handle, "start_temp", &start_temperature));
    ESP_ERROR_CHECK(nvs_get_u8(my_handle, "reboot", &reboot_count));
    nvs_close(my_handle);

    reboot_count++;
    ESP_LOGI(TAG, "Boot attempt number %d", (int)reboot_count);

    if (reboot_count > 5) {
        ESP_LOGI(TAG, "Too many reboots, reset device");
        esp_zb_factory_reset();
        return;
    }

    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "reboot", reboot_count));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    if (start_temperature != ZB_ZCL_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_USE_PREVIOUS_VALUE)
        current_temperature = start_temperature;

    switch (start_power)
    {
    case ZB_ZCL_ON_OFF_START_UP_ON_OFF_IS_OFF:
        current_power = false;
        break;
    case ZB_ZCL_ON_OFF_START_UP_ON_OFF_IS_ON:
        current_power = true;
        break;
    case ZB_ZCL_ON_OFF_START_UP_ON_OFF_IS_TOGGLE:
        current_power = !current_power;
        break;
    case ZB_ZCL_ON_OFF_START_UP_ON_OFF_IS_PREVIOUS:
    default:
        break;
    }

    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &current_power, false);
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF, &start_power, false);
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, &current_level, false);
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &current_temperature, false);
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_MIREDS_ID, &start_temperature, false);

    update_duty();
}
