#pragma once

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_camera.h"


QueueHandle_t xFrameQueue;
EventGroupHandle_t s_wifi_event_group;

void system_init(void);
