#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "driver/gpio.h"
#include "zboss_api.h"

#include "light_driver.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */
#define MAX_CHILDREN                    10      /* the max amount of connected devices */

#define OTA_UPGRADE_MANUFACTURER        0x1001                                /* The attribute indicates the file version of the downloaded image on the device*/
#define OTA_UPGRADE_IMAGE_TYPE          0x1011                                /* The attribute indicates the value for the manufacturer of the device */
#define OTA_UPGRADE_FILE_VERSION        0x01010101                            /* The attribute indicates the file version of the running firmware image on the device */
#define OTA_UPGRADE_HW_VERSION          0x0101                                /* The parameter indicates the version of hardware */
#define OTA_UPGRADE_MAX_DATA_SIZE       64                                    /* The parameter indicates the maximum data size of query block image */

#define MODEL_NAME                      "CeilingCW"
#define MANUFACTURER_NAME               "GinKage"
#define FIRMWARE_VERSION                "v1.0"

static char model_id[16];
static char manufacturer_name[16];
static char firmware_version[16];

esp_timer_handle_t timer_handle;

#define LED_COMMISSION GPIO_NUM_8

static void esp_error_blink(void *pvParameters) {
    for (int i = 0; i < 5; i++) {
        gpio_set_level(LED_COMMISSION, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(LED_COMMISSION, 0);
    }
    esp_restart();
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_leave_params_t *leave_params = NULL;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        leave_params = (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {
            ESP_LOGI(TAG, "Reset device");
            esp_zb_factory_reset();
        }
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        gpio_set_level(LED_COMMISSION, 0);
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new()) {
                light_set_defaults();
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                gpio_set_level(LED_COMMISSION, 1);
                light_load_settings();
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            /* commissioning failed */
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
            xTaskCreate(esp_error_blink, "error task", 4096, NULL, 5, NULL);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            gpio_set_level(LED_COMMISSION, 1);
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
            esp_err_to_name(err_status));
        break;
    }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d), data type(%d)", message->info.dst_endpoint,
        message->info.cluster, message->attribute.id, message->attribute.data.size, message->attribute.data.type);

    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        switch (message->info.cluster) {
        case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
            ESP_LOGI(TAG, "Identify pressed");
            break;

        case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                bool light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
                ESP_LOGI(TAG, "New state: %s", light_state ? "On" : "Off");
                light_set_on_off(light_state);
            }
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM) {
                uint8_t state = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : ZB_ZCL_ON_OFF_START_UP_ON_OFF_IS_ON;
                ESP_LOGI(TAG, "New startup state: %d", (int)state);
                light_set_startup_on_off(state);
            }
            break;

        case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                uint8_t brightness = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : ESP_ZB_ZCL_LEVEL_CONTROL_CURRENT_LEVEL_DEFAULT_VALUE;
                ESP_LOGI(TAG, "New brightness: %d", (int)brightness);
                light_set_level(brightness);
            }
            break;

        case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
                uint16_t temperature = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
                ESP_LOGI(TAG, "New temperature: %d", (int)temperature);
                light_set_temperature(temperature);
            }
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_MIREDS_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
                uint16_t temperature = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : ZB_ZCL_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_USE_PREVIOUS_VALUE;
                ESP_LOGI(TAG, "New startup temperature: %d", (int)temperature);
                light_set_startup_temperature(temperature);
            }
            break;
        }
    }

    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;

    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void set_zcl_string(char *buffer, char *value) {
    buffer[0] = (char) strlen(value);
    memcpy(buffer + 1, value, buffer[0]);
}

static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,
        .nwk_cfg.zczr_cfg = {
            .max_children = MAX_CHILDREN,
        }
    };
    esp_zb_init(&zb_nwk_cfg);

    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    light_cfg.basic_cfg.power_source = ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    light_cfg.color_cfg.color_mode = ZB_ZCL_COLOR_CONTROL_COLOR_MODE_TEMPERATURE;
    light_cfg.color_cfg.color_capabilities = ZB_ZCL_COLOR_CONTROL_CAPABILITIES_COLOR_TEMP;

    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&light_cfg.basic_cfg);
    set_zcl_string(model_id, MODEL_NAME);
    set_zcl_string(manufacturer_name, MANUFACTURER_NAME);
    set_zcl_string(firmware_version, FIRMWARE_VERSION);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model_id);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer_name);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, firmware_version);

    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&light_cfg.identify_cfg);
    esp_zb_attribute_list_t *esp_zb_ep_groups_cluster = esp_zb_groups_cluster_create(&light_cfg.groups_cfg);
    esp_zb_attribute_list_t *esp_zb_ep_scenes_cluster = esp_zb_scenes_cluster_create(&light_cfg.scenes_cfg);

    uint8_t default_on_off = ZB_ZCL_ON_OFF_START_UP_ON_OFF_IS_ON;
    esp_zb_attribute_list_t *esp_zb_ep_on_off_cluster = esp_zb_on_off_cluster_create(&light_cfg.on_off_cfg);
    esp_zb_on_off_cluster_add_attr(esp_zb_ep_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF, &default_on_off);

    esp_zb_attribute_list_t *esp_zb_ep_level_cluster = esp_zb_level_cluster_create(&light_cfg.level_cfg);

    uint16_t default_color_temp = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
    uint16_t min_color_temp = 150;
    uint16_t max_color_temp = 500;
    esp_zb_attribute_list_t *esp_zb_ep_color_cluster = esp_zb_color_control_cluster_create(&light_cfg.color_cfg);
    esp_zb_color_control_cluster_add_attr(esp_zb_ep_color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &default_color_temp);
    esp_zb_color_control_cluster_add_attr(esp_zb_ep_color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_MIREDS_ID, &default_color_temp);
    esp_zb_color_control_cluster_add_attr(esp_zb_ep_color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, &min_color_temp);
    esp_zb_color_control_cluster_add_attr(esp_zb_ep_color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, &max_color_temp);

    /** Create ota client cluster with attributes.
     *  Manufacturer code, image type and file version should match with configured values for server.
     *  If the client values do not match with configured values then it shall discard the command and
     *  no further processing shall continue.
     */
    esp_zb_ota_cluster_cfg_t ota_cluster_cfg = {
        .ota_upgrade_downloaded_file_ver = OTA_UPGRADE_FILE_VERSION,
        .ota_upgrade_manufacturer = OTA_UPGRADE_MANUFACTURER,
        .ota_upgrade_image_type = OTA_UPGRADE_IMAGE_TYPE,
    };
    esp_zb_attribute_list_t *esp_zb_ota_client_cluster = esp_zb_ota_cluster_create(&ota_cluster_cfg);

    /** add client parameters to ota client cluster */
    esp_zb_zcl_ota_upgrade_client_variable_t ota_variable_config  = {
        .timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF,    /* time interval for query next image request command */
        .hw_version = OTA_UPGRADE_HW_VERSION,                           /* version of hardware */
        .max_data_size = OTA_UPGRADE_MAX_DATA_SIZE,                     /* maximum data size of query block image */
    };
    esp_zb_ota_cluster_add_attr(esp_zb_ota_client_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, &ota_variable_config);

    esp_zb_cluster_list_t *esp_zb_zcl_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_zcl_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_zcl_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(esp_zb_zcl_cluster_list, esp_zb_ep_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(esp_zb_zcl_cluster_list, esp_zb_ep_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_zcl_cluster_list, esp_zb_ep_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(esp_zb_zcl_cluster_list, esp_zb_ep_level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(esp_zb_zcl_cluster_list, esp_zb_ep_color_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_ota_cluster(esp_zb_zcl_cluster_list, esp_zb_ota_client_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID
    };

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_zcl_cluster_list, endpoint_config);

    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Successful boot, reset counter");
    ESP_ERROR_CHECK(esp_timer_delete(timer_handle));

    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "reboot", 0));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);
}

void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = { .radio_mode = ZB_RADIO_MODE_NATIVE },
        .host_config = { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE },
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

	gpio_config_t gpio_output_config = {
        .pin_bit_mask = (1ULL << LED_COMMISSION),
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .mode = GPIO_MODE_OUTPUT
	};
	ESP_ERROR_CHECK(gpio_config(&gpio_output_config));
	gpio_set_level(LED_COMMISSION, 1);

    light_init();

    esp_timer_create_args_t timer_cfg = {
        .callback = timer_callback,
        .arg = &timer_handle,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "FR timer",
        .skip_unhandled_events = false
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_cfg, &timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_once(timer_handle, 5000000));

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
