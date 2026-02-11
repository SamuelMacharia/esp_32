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
#include "system.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define STREAM_READY_BIT BIT2


#define FRAME_QUEUE_LEN 3

#define CAMERA_MODEL_AI_THINKER


static const char* TAG = "AI_THINKER";


TaskHandle_t xCaptureTaskHandle;
TaskHandle_t xStreamingTaskHandle;
TaskHandle_t xWebServerHandle;
TaskHandle_t xMqttTaskHandle;
TaskHandle_t xPSTaskHandle;
TaskHandle_t handle_detect_face;
TaskHandle_t handle_face_recognition;




static void handle_stream(void* args){
    httpd_req_t* req = (httpd_req_t*)args;
    camera_fb_t* recieved_fb = NULL;
    esp_err_t res = ESP_OK;

    static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
    static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
    
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Cache-control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin","*");
    for(;;){
        if(xQueueReceive(xFrameQueue, &recieved_fb, portMAX_DELAY)==pdTRUE){          
            if(res != ESP_OK){
                ESP_LOGE(TAG, "Something happened %s", res);
                break;
            }

            char header[64];
            size_t hlen = snprintf(header, sizeof(header), _STREAM_PART, recieved_fb->len);
            res= httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));

            if(res==ESP_OK){
                httpd_resp_send_chunk(req, header, hlen);
            }
            if(res==ESP_OK){
                httpd_resp_send_chunk(req, (const char*)recieved_fb->buf, recieved_fb->len);
            }
            esp_camera_fb_return(recieved_fb);
            recieved_fb=NULL;
        }
    }
    httpd_req_async_handler_complete(req);
    xEventGroupClearBits(s_wifi_event_group, STREAM_READY_BIT);
    vTaskDelete(NULL);
}

esp_err_t start_stream_handler(httpd_req_t* req){
    httpd_req_t *copy = NULL;

    //create a persistent copy safe for other tasks
    esp_err_t res = httpd_req_async_handler_begin(req, &copy);

    if(res==ESP_OK){
        xTaskCreatePinnedToCore(handle_stream, "handle_stream", 4096, (void*)copy, 3, &xStreamingTaskHandle, 0);
        xEventGroupSetBits(s_wifi_event_group, STREAM_READY_BIT);
    }else{
        xEventGroupClearBits(s_wifi_event_group, STREAM_READY_BIT);
    }
    return res;
}

httpd_uri_t stream_uri={
        .uri ="/stream",
        .method = HTTP_GET,
        .handler= start_stream_handler,
        .user_ctx = NULL
    };
static httpd_handle_t server = NULL;


static esp_err_t startWebServer(){
    ESP_LOGI(TAG, "Starting http server");

    if(server != NULL){
        ESP_LOGW(TAG, "Web Server already running");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    esp_err_t err = httpd_start(&server, &config);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to start server %s", esp_err_to_name(err));
        server= NULL;
        return err;
    }
    httpd_register_uri_handler(server, &stream_uri);
    ESP_LOGI(TAG, "Web Server Started Successfully");
    return ESP_OK;
}

void handle_webServer(void *args){
    xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );
    startWebServer();
    vTaskDelete(NULL);
}

void app_main(void)
{
    
    

    esp_err_t ret = nvs_flash_init();
    if (ret==ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret=nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xFrameQueue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(camera_fb_t *));

    
    xTaskCreatePinnedToCore(
        handle_webServer,
        "handle_webServer",
        4096,
        NULL,
        2,
        &xWebServerHandle,
        0
    ); 
    
    
    xTaskCreatePinnedToCore(
        handle_capture, //Task Function
        "handle_capture", //Task Name
        4096,  //Stack Size
        NULL, //parameters
        2, // Priority
        &xCaptureTaskHandle, //Task Handle
        1 // core
    );

    power_init();



    esp_err_t wifi_status = init_wifi();
    if(wifi_status==ESP_OK){
        ESP_LOGI(TAG, "CONNECTED TO %s", CONFIG_WIFI_SSID);
    }else{
        ESP_LOGE(TAG, "FAILED TO CONNECT TO %s", CONFIG_WIFI_SSID);
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    mqtt_start();

    init_camera();
    vTaskDelay(pdMS_TO_TICKS(1500));

    //startWebServer();



    
}
