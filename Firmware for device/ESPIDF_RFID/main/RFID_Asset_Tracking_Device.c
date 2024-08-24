#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/spi_master.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "pn532.h"

#define WIFI_SSID "xxxxxxx"
#define WIFI_PASS "xxxxxxx"

#define LED_PIN GPIO_NUM_13
#define BUZZER_PIN GPIO_NUM_14

#define TAG "RFID_TRACKER"

// Global variables
pn532_t nfc;
char details[50];
char detailsLong[250];
char idcard[20];
char idcardOld[20];

void Indicate()
{
    gpio_set_level(LED_PIN, 1);
    gpio_set_level(BUZZER_PIN, 1);
    vTaskDelay(400 / portTICK_PERIOD_MS);
    gpio_set_level(LED_PIN, 0);
    gpio_set_level(BUZZER_PIN, 0);
}

void readPn532()
{
    uint8_t uid[7];
    uint8_t uidLength;

    // Corrected function call with proper parameters
    if (pn532_readPassiveTargetID(&nfc, 0, uid, &uidLength, 1000))
    { // Added cardbaudrate and timeout parameters
        snprintf(idcardOld, sizeof(idcardOld), "%s", idcard);
        memset(idcard, 0, sizeof(idcard));

        for (int i = 0; i < uidLength; i++)
        {
            snprintf(idcard + strlen(idcard), sizeof(idcard) - strlen(idcard), "%02X", uid[i]);
        }

        if (strcmp(idcard, idcardOld) != 0)
        {
            snprintf(details, sizeof(details), "%s", idcard);
            // esp_mqtt_client_publish(mqttClient, "CardDetails", details, 0, 1, 0);
            Indicate();
        }
    }
}

void setupPn532()
{
    pn532_begin(&nfc); // Assuming pn532_initialize should be pn532_begin
    uint32_t versiondata = pn532_getFirmwareVersion(&nfc);

    if (versiondata == 0)
    {
        // ESP_LOGE(TAG, "Error: PN532 not detected. Restarting in 1 second...");
        printf("Error: PN532 not detected. Restarting in 1 second...\n");

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    ESP_LOGI(TAG, "Found chip PN5%02lX", (unsigned long)((versiondata >> 24) & 0xFF));
    ESP_LOGI(TAG, "Firmware ver. %lu.%lu", (unsigned long)((versiondata >> 16) & 0xFF), (unsigned long)((versiondata >> 8) & 0xFF));

    pn532_setPassiveActivationRetries(&nfc, 0xFF);
    Indicate();
}

////////////

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

void wifi_connection()
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS}};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_connect();
}

///////////////

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "SERVER_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://mqtt.eclipseprojects.io",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

/////////////////

void app_main()
{
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_PIN, 0);    // Set LED_PIN to LOW
    gpio_set_level(BUZZER_PIN, 0); // Set BUZZER_PIN to LOW

    wifi_connection();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    mqtt_app_start();


    setupPn532();
}
