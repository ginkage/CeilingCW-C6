#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define HA_ESP_LIGHT_ENDPOINT           10      /* esp light bulb device endpoint, used to process light controlling commands */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */
#define MAX_CHILDREN                    10      /* the max amount of connected devices */

#define OTA_UPGRADE_MANUFACTURER        0x1001                                /* The attribute indicates the file version of the downloaded image on the device*/
#define OTA_UPGRADE_IMAGE_TYPE          0x1011                                /* The attribute indicates the value for the manufacturer of the device */
#define OTA_UPGRADE_FILE_VERSION        0x01010101                            /* The attribute indicates the file version of the running firmware image on the device */
#define OTA_UPGRADE_HW_VERSION          0x0101                                /* The parameter indicates the version of hardware */
#define OTA_UPGRADE_MAX_DATA_SIZE       64                                    /* The parameter indicates the maximum data size of query block image */

#define MANUFACTURER_NAME               "GinKage"
#define MODEL_NAME                      "CeilingCW"
#define FIRMWARE_VERSION                "v1.0"

#define ESP_ZB_ZR_CONFIG()                                      \
    {                                                           \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,               \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,       \
        .nwk_cfg.zczr_cfg = {                                   \
            .max_children = MAX_CHILDREN,                       \
        },                                                      \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                        \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,      \
    }
