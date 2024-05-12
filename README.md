| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

# Ceiling Light based on WT0132C6-S5 module

This code shows how to configure Zigbee router device and use it as HA CW light.

The ESP Zigbee SDK provides more examples and tools for productization:
* [ESP Zigbee SDK Docs](https://docs.espressif.com/projects/esp-zigbee-sdk)
* [ESP Zigbee SDK Repo](https://github.com/espressif/esp-zigbee-sdk)

## Hardware Required

* WT0132C6-S5 module acting as Zigbee router-device (loaded with this firmware)
* A CH340G-based programmer

## Configure the project

Before project configuration and build, make sure to set the correct chip target using `idf.py --preview set-target TARGET` command.

## Erase the NVRAM

Before flash it to the board, it is recommended to erase NVRAM if user doesn't want to keep the previous examples or other projects stored info using `idf.py -p PORT erase-flash`

## Build and Flash

Build the project, flash it to the board, and start the monitor tool to view the serial output by running `idf.py -p PORT flash monitor`.

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

As you run the example, you will see the following log:

```
I (394) main_task: Calling app_main()
I (404) gpio: GPIO[8]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (404) phy_init: phy_version 220,2dbbbe7,Sep 25 2023,20:39:25
I (464) phy: libbtbb version: 90c587c, Sep 25 2023, 20:39:57
I (474) ESP_ZB_COLOR_DIMM_LIGHT: ZDO signal: ZDO Config Ready (0x17), status: ESP_FAIL
I (474) ESP_ZB_COLOR_DIMM_LIGHT: Zigbee stack initialized
I (484) ESP_ZB_COLOR_DIMM_LIGHT: Start network steering
I (484) main_task: Returned from app_main()
I (9614) ESP_ZB_COLOR_DIMM_LIGHT: ZDO signal: NWK Permit Join (0x36), status: ESP_OK
I (9834) ESP_ZB_COLOR_DIMM_LIGHT: ZDO signal: NWK Permit Join (0x36), status: ESP_OK
I (9834) ESP_ZB_COLOR_DIMM_LIGHT: Joined network successfully (Extended PAN ID: 60:55:f9:00:00:f6:07:b4, PAN ID: 0x2a74, Channel:13)
I (32944) ESP_ZB_COLOR_DIMM_LIGHT: Received message: endpoint(10), cluster(0x6), attribute(0x0), data size(1)
I (32944) ESP_ZB_COLOR_DIMM_LIGHT: Light sets to On
I (33984) ESP_ZB_COLOR_DIMM_LIGHT: Received message: endpoint(10), cluster(0x6), attribute(0x0), data size(1)
I (33984) ESP_ZB_COLOR_DIMM_LIGHT: Light sets to Off
I (35304) ESP_ZB_COLOR_DIMM_LIGHT: ZDO signal: NLME Status Indication (0x32), status: ESP_OK
I (35534) ESP_ZB_COLOR_DIMM_LIGHT: Received message: endpoint(10), cluster(0x6), attribute(0x0), data size(1)
I (35534) ESP_ZB_COLOR_DIMM_LIGHT: Light sets to On
```

## Light Control Functions

 * GPIO pins 10 and 5 are used for PWM control of cold and warm white LED strips.

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
