#pragma once

#include <esp_log.h>
#include "sdkconfig.h"
#include "event_groups.h"
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_camera.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "wifi.h"
#include "power.h"
#include "mqtt.h"
#include "capture.h"
#include "server.h"
#include "system.h"


void handle_webServer(void *args);