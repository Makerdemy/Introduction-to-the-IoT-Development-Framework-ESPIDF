#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

static const char *TAG = "Capstone Project";

#define SSID "ESP32AP"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

char *str;
char*  buf;
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;
size_t buf_len;


void wifi_init_softap();
void write_to_nvs(char* ssid_write, char* pass_write);
void wifi_init_sta(char* ssid_connect, char* pass_connect);
httpd_handle_t start_webserver();


esp_err_t get_handler(httpd_req_t *req)
{

    FILE *f = fopen("/spiffs/ConnectWifi.html", "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open ConnectWifi.html");
        return;
    }

    char buf[1024];
    memset(buf, 0, sizeof(buf));
    fread(buf, 1, sizeof(buf), f);
    fclose(f);

    httpd_resp_set_type(req, "text/html");

    esp_err_t err = httpd_resp_send(req, buf, sizeof(buf));
 
    if(err == ESP_OK)
        printf("\n On successfully sending the response packet");
    else if(err == ESP_ERR_INVALID_ARG)
        printf("\n Null request pointer");
    else if(err == ESP_ERR_HTTPD_RESP_HDR)
        printf("\n Essential headers are too large for internal buffer");
    else if(err == ESP_ERR_HTTPD_RESP_SEND)
        printf("\n Error in raw send");
    else
        printf("\n Invalid request");

    return ESP_OK;
}

esp_err_t post_handler(httpd_req_t *req)
{
    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content));
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    httpd_resp_send(req, content, HTTPD_RESP_USE_STRLEN);
    
    cJSON *root2 = cJSON_Parse(content);
    char *ssidnvs = cJSON_GetObjectItem(root2, "SSID")->valuestring;
    char *passwordnvs = cJSON_GetObjectItem(root2, "Password")->valuestring;
    ESP_LOGI(TAG, "SSID=%s", ssidnvs);
    ESP_LOGI(TAG, "password=%s", passwordnvs);

    write_to_nvs(ssidnvs,passwordnvs);

    wifi_init_sta(ssidnvs,passwordnvs);
    
    return ESP_OK;
}

httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_handler,
        .user_ctx = NULL};

httpd_uri_t get_wificred = {
        .uri = "/uri",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL};

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    printf("\n Starting HTTP Server \n");

    if (httpd_start(&server, &config) == ESP_OK)
    {
        printf("\n Web server started \n");
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &get_wificred);
    }
   
    return server;
}

void stop_webserver(httpd_handle_t server)
{
    if (server)
    {
        httpd_stop(server);
    }
}

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        start_webserver();
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    printf("\n event_base = %s", event_base);
    printf("\n event_id = %d", event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < 5)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(char* ssid_connect, char* pass_connect)
{
    printf("\n Station mode \n");
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    esp_err_t err = esp_event_loop_create_default();
    if(err == ESP_OK)
    {
        printf("\n Success \n");
    }
    else if(err == ESP_ERR_NO_MEM)
    {
        printf("\n Cannot allocate memory for event loops list \n");
    }
     if(err == ESP_FAIL)
    {
        printf("\n Failed to create task loop \n");
    }
    else
    {
        printf("\n Fail \n");
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    
    wifi_config_t wifi_config = {0}; // Zero initialize all struct memberes
    strcpy((char *)wifi_config.sta.ssid, ssid_connect);
    strcpy((char *)wifi_config.sta.password, pass_connect);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ssid_connect, pass_connect);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ssid_connect, pass_connect);
        wifi_init_softap();
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);
}

void read_from_nvs()
{
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf("Done\n");
        size_t len;
        err = nvs_get_str(my_handle, "WifiCred", NULL, &len);
        str = (char *)malloc(len);
        err = nvs_get_str(my_handle, "WifiCred", str, &len);
        switch (err)
        {
        case ESP_OK:
            printf("Done\n");
            printf("Wifi Credentials = %s\n", str);
            printf("\n cred printed");
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
        nvs_close(my_handle);
        cJSON *root2 = cJSON_Parse(str);
        char *ssidnvs = cJSON_GetObjectItem(root2, "SSID")->valuestring;
        char *passwordnvs = cJSON_GetObjectItem(root2, "Password")->valuestring;
        ESP_LOGI(TAG, "SSID=%s", ssidnvs);
        ESP_LOGI(TAG, "password=%s", passwordnvs);
        wifi_init_sta(ssidnvs, passwordnvs);
    }
}

void write_to_nvs(char* ssid_write, char* pass_write)
{
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf("Done\n");
        printf("Updating SSID and pasword to NVS ... ");
        cJSON *root;
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "SSID", ssid_write);
        cJSON_AddStringToObject(root, "Password", pass_write);
        const char *my_json_string = cJSON_Print(root);
        err = nvs_set_str(my_handle, "WifiCred", my_json_string);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        cJSON_Delete(root);
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        nvs_close(my_handle);
    }
}

void wifi_init_softap()
{
    tcpip_adapter_init();
    esp_err_t err = esp_event_loop_create_default();
    if (err == ESP_OK)
    {
        printf("\n sucess \n");
    }
    wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifiInitializationConfig);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_AP);
    wifi_config_t ap_config = {
        .ap = {
            .ssid = SSID,
            .channel = 0,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = 20,
            .beacon_interval = 100}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s", SSID);

}

void app_main()
{
    nvs_flash_init();
    ESP_ERROR_CHECK(init_fs());
    // write_to_nvs("admin", "admin123");
    read_from_nvs();  
}