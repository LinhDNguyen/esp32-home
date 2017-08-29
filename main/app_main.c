#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "mqtt.h"

#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_OUTPUT_IO_2    23
#define GPIO_OUTPUT_IO_3    25
#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_0) | (1<<GPIO_OUTPUT_IO_1) | (1<<GPIO_OUTPUT_IO_2) | (1<<GPIO_OUTPUT_IO_3))
#define RED_TOPIC "home/light/red"
#define GREEN_TOPIC "home/light/green"

const char *MQTT_TAG = "MQTT_SAMPLE";

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

void connected_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    ESP_LOGI(MQTT_TAG, "[APP] Connected!!!");
    mqtt_subscribe(client, RED_TOPIC, 0);
    mqtt_subscribe(client, GREEN_TOPIC, 0);
}
void disconnected_cb(void *self, void *params)
{
    ESP_LOGI(MQTT_TAG, "[APP] Disconnected!!!");
}
void reconnect_cb(void *self, void *params)
{
    ESP_LOGI(MQTT_TAG, "[APP] Reconnect!!!");
}
void subscribe_cb(void *self, void *params)
{
    // mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;
    // ESP_LOGI(MQTT_TAG, "[APP] Subscribed %s", event_data->topic);
}

void publish_cb(void *self, void *params)
{
    // mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;
    // ESP_LOGI(MQTT_TAG, "[APP] Published %s", event_data->topic);
}
void data_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;

    ESP_LOGI(MQTT_TAG, "[APP] data received: %s -> %s:%d", event_data->topic, event_data->data, event_data->data_offset);
    if(event_data->data_offset == 0) {
        int gpio = -1;
        int gpio2 = -1;
        if (memcmp(event_data->topic, RED_TOPIC, event_data->topic_length) == 0) {
            // RED LED
            gpio = GPIO_OUTPUT_IO_0;
            gpio2 = GPIO_OUTPUT_IO_2;
        } else if (memcmp(event_data->topic, GREEN_TOPIC, event_data->topic_length) == 0) {
            // GREEN LED
            gpio = GPIO_OUTPUT_IO_1;
            gpio2 = GPIO_OUTPUT_IO_3;
        } else {
            ESP_LOGE(MQTT_TAG, "Topic %s is wrong", event_data->topic);
            return;
        }

        int val = 0;
        if (memcmp(event_data->data, "ON", event_data->data_length) == 0) {
            val = 1;
        }

        ESP_LOGI(MQTT_TAG, "[APP] gpio %d -> %d", gpio, val);
        gpio_set_level(gpio, val);
        gpio_set_level(gpio2, val);
    }
}

mqtt_settings settings = {
    .host = "192.168.1.196",
    .port = 8883,
    .client_id = "esp32_client",
    .username = "ha",
    .password = "danhlinh",
    .clean_session = 0,
    .keepalive = 120,
    .lwt_topic = "/lwt",
    .lwt_msg = "offline",
    .lwt_qos = 0,
    .lwt_retain = 0,
    .connected_cb = connected_cb,
    .disconnected_cb = disconnected_cb,
    .subscribe_cb = subscribe_cb,
    .publish_cb = publish_cb,
    .data_cb = data_cb,
    .auto_reconnect = 1,
};

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            mqtt_start(&settings);
            //init app here
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
               auto-reassociate. */
            esp_wifi_connect();
            mqtt_stop();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_conn_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(MQTT_TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_WIFI_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main()
{
    ESP_LOGI(MQTT_TAG, "[APP] Startup..MQTT LED");
    ESP_LOGI(MQTT_TAG, "[APP] Free memory: %d bytes", system_get_free_heap_size());
    ESP_LOGI(MQTT_TAG, "[APP] SDK version: %s, Build time: %s", system_get_sdk_version(), BUID_TIME);

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(GPIO_OUTPUT_IO_0, 1);
    gpio_set_level(GPIO_OUTPUT_IO_1, 1);
    gpio_set_level(GPIO_OUTPUT_IO_2, 1);
    gpio_set_level(GPIO_OUTPUT_IO_3, 1);

    nvs_flash_init();
    wifi_conn_init();
}
