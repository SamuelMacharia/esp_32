#include "system.h"

#define CAMERA_MODEL_AI_THINKER
#define FRAME_QUEUE_LEN 3


QueueHandle_t xFrameQueue = NULL;
EventGroupHandle_t s_wifi_event_group = NULL;

void system_init(void){
    s_wifi_event_group = xEventGroupCreate();
    xFrameQueue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(camera_fb_t *));


}