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
TaskHandle_t xCaptureTaskHandle;
TaskHandle_t xStreamingTaskHandle;
TaskHandle_t xWebServerHandle;
TaskHandle_t xMqttTaskHandle;
TaskHandle_t xPSTaskHandle;
TaskHandle_t handle_detect_face;
TaskHandle_t handle_face_recognition;

void system_init(void);
