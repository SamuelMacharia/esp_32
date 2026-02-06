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
#include <esp_http_server.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define STREAM_READY_BIT BIT2

#define MAXIMUM_RETRY_NUM 10
#define FRAME_QUEUE_LEN 3

#define CAMERA_MODEL_AI_THINKER


static const char* TAG = "AI_THINKER";


TaskHandle_t xCaptureTaskHandle;
TaskHandle_t xStreamingTaskHandle;
TaskHandle_t xWebServerHandle;
TaskHandle_t xMqttTaskHandle;
TaskHandle_t handle_detect_face;
TaskHandle_t handle_face_recognition;

QueueHandle_t xFrameQueue;

static EventGroupHandle_t s_wifi_event_group;

static int num_of_retries = 0;

camera_config_t config;
#include "pins.h"

static esp_err_t init_camera(){
    
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;

    esp_err_t err = esp_camera_init(&config);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Camera Initialization failed");
        return err;
    }

    //flip the camera to the right orientation
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s,1);
    s->set_hmirror(s,0);

    ESP_LOGI(TAG, "Camera Initialization successful");
    return ESP_OK;
}
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
    void* event_data){
        if(event_base==WIFI_EVENT && event_id==WIFI_EVENT_STA_START){
            esp_wifi_connect();
        }else if(event_base==WIFI_EVENT && event_id ==WIFI_EVENT_STA_DISCONNECTED){
            if(num_of_retries < MAXIMUM_RETRY_NUM){
                esp_wifi_connect();
                num_of_retries++;
                ESP_LOGI(TAG, "RETRYING TO CONNECT TO AP");
            }else{
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
            }
            ESP_LOGI(TAG, "Failed to connect to AP");
        }else if(event_base==IP_EVENT && event_id==IP_EVENT_STA_GOT_IP){
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "Got IP: %s", IPSTR, IP2STR(&event->ip_info.ip));
            num_of_retries = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
static esp_err_t init_wifi(){
    //create an event group
    s_wifi_event_group = xEventGroupCreate();

    //initialize TCP/IP Stack
    ESP_ERROR_CHECK(esp_netif_init());

    //create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //create default network interface for station mode
    esp_netif_create_default_wifi_sta();

    //initialize wifi with default  configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID, 
        &event_handler,
        NULL,
        &instance_any_id
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP, 
        &event_handler,
        NULL,
        &instance_got_ip
    ));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid= CONFIG_WIFI_SSID,
            .password= CONFIG_WIFI_PASSWORD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi station initialization successful");


    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to WiFi SSID:%s ", CONFIG_WIFI_SSID);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        esp_wifi_set_max_tx_power(52);
        return ESP_OK;
    }else if(bits & WIFI_FAILED_BIT){
        ESP_LOGI(TAG, "Failed to connect to WiFi SSID:%s ", CONFIG_WIFI_SSID);
        return ESP_FAIL;
    }else{
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
    
    
}

static void handle_capture(void* args){
    camera_fb_t* fb = NULL;
    
    for(;;){
        xEventGroupWaitBits(
            s_wifi_event_group,
            STREAM_READY_BIT,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );
       fb = esp_camera_fb_get();
       if(!fb){
        ESP_LOGE(TAG, "Camera Capture Failed");
        
        break;
       }else{
        if(xQueueSend(xFrameQueue, &fb, portMAX_DELAY) != pdPASS){
            esp_camera_fb_return(fb);
        }
       }
       UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
       ESP_LOGI(TAG, "Remaining Stack size %u words", (unsigned int)high_water_mark);
            
       vTaskDelay(pdMS_TO_TICKS(10));
    }
}

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

    init_camera();
    vTaskDelay(pdMS_TO_TICKS(1500));

    esp_err_t wifi_status = init_wifi();
    if(wifi_status==ESP_OK){
        ESP_LOGI(TAG, "CONNECTED TO %s", CONFIG_WIFI_SSID);
    }else{
        ESP_LOGE(TAG, "FAILED TO CONNECT TO %s", CONFIG_WIFI_SSID);
    }

    //startWebServer();

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

    
}
