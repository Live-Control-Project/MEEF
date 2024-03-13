#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "common.h"

#include "../settings.h"
#include "../utils/send_data.h"

static const char *TAG_MQTT = "MQTT";
extern cJSON *sensor_json;

esp_mqtt_client_handle_t client = NULL;
uint32_t MQTT_CONNEECTED = 0;
char mqtt_url[32];
char mqtt_login[32];
char mqtt_pwd[32];

const char *deviceName = sys_settings.wifi.STA_MAC;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG_MQTT, "Last error %s: 0x%x", message, error_code);
    }
}
// публикация списка элементов  в HOMEd
void publis_elements(void)
{
    cJSON *element = cJSON_CreateObject();
    cJSON_AddStringToObject(element, "action", "updateDevice");
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(element, "data", data);
    cJSON_AddBoolToObject(data, "active", true);
    cJSON_AddBoolToObject(data, "cloud", false);
    cJSON_AddBoolToObject(data, "discovery", false);
    cJSON *exposesArray = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "exposes", exposesArray);

    char *id = "";
    char *sensor = "";
    cJSON *item = sensor_json->child;
    while (item != NULL)
    {
        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *pin_ = cJSON_GetObjectItemCaseSensitive(item, "pin");
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
        cJSON *clusters_ = cJSON_GetObjectItemCaseSensitive(item, "clasters");
        if (cJSON_IsString(sensor_) && cJSON_IsString(id_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            id = id_->valuestring;
            sensor = sensor_->valuestring;
            int EP = ep_->valueint;

            if (EP > 1)
            {
                if (strcmp(cluster, "all") == 0)
                {
                    cJSON *expose = clusters_->child;
                    while (expose != NULL)
                    {

                        if (strcmp(cJSON_GetStringValue(expose), "all") != 0)
                        {
                            char result[30];
                            snprintf(result, sizeof(result), "%s_%d", cJSON_GetStringValue(expose), EP);
                            //     printf("Result: %s\n", result);
                            cJSON_AddItemToArray(exposesArray, cJSON_CreateString(result));
                        }
                        expose = expose->next;
                    }
                }
                else
                {
                    char result[30];
                    snprintf(result, sizeof(result), "%s_%d", cluster, EP);
                    cJSON_AddItemToArray(exposesArray, cJSON_CreateString(result));
                }
            }
            else
            {
                if (strcmp(cluster, "all") == 0)
                {
                    cJSON *expose = clusters_->child;
                    while (expose != NULL)
                    {
                        if (strcmp(cJSON_GetStringValue(expose), "all") != 0)
                        {
                            char result[30];
                            snprintf(result, sizeof(result), "%s", cJSON_GetStringValue(expose));
                            //   printf("Result: %s\n", result);
                            cJSON_AddItemToArray(exposesArray, cJSON_CreateString(result));
                        }
                        expose = expose->next;
                    }
                }
                else
                {
                    cJSON_AddItemToArray(exposesArray, cJSON_CreateString(cluster));
                }
            }
        }
        item = item->next;
    }
    cJSON_AddStringToObject(data, "id", sys_settings.wifi.STA_MAC);
    cJSON_AddStringToObject(data, "name", sys_settings.device.devicename);
    cJSON_AddBoolToObject(data, "real", true);
    cJSON_AddStringToObject(element, "device", sys_settings.wifi.STA_MAC);

    // Публикуем все элементы
    const char *mqttPrefx = sys_settings.mqtt.prefx;
    const char *topic = "/command/custom";
    char completeTopic[strlen(mqttPrefx) + strlen(topic) + 1];
    strcpy(completeTopic, mqttPrefx);
    strcat(completeTopic, topic);
    // const char *topic = "/homed/command/custom";
    const char *jsonData = cJSON_PrintUnformatted(element);
    esp_mqtt_client_publish(client, completeTopic, jsonData, 0, 0, 0);

    // Публикуем доступность устойства
    mqttPrefx = sys_settings.mqtt.prefx;
    topic = "/device/custom/";
    const char *deviceName = sys_settings.wifi.STA_MAC;
    char completeTopic2[strlen(mqttPrefx) + strlen(topic) + strlen(deviceName) + 1];
    strcpy(completeTopic2, mqttPrefx);
    strcat(completeTopic2, topic);
    strcat(completeTopic2, deviceName);

    // topic = "/homed/device/custom/";
    // const char *deviceName = sys_settings.wifi.STA_MAC;
    // char completeTopic[strlen(topic) + strlen(deviceName) + 1];
    // strcpy(completeTopic, topic);
    // strcat(completeTopic, deviceName);
    jsonData = "{\"status\":\"online\"}";
    esp_mqtt_client_publish(client, completeTopic2, jsonData, 0, 0, 1);
    cJSON_Delete(element); // Free cJSON object
}

// Публикация статуса датчика
void publis_status_mqtt(const char *topic, int EP, const char *deviceData)
{
    if (EP > 1)
    {
        char completeTopic[150];
        snprintf(completeTopic, sizeof(completeTopic), "%s/%d", topic, EP);
        esp_mqtt_client_publish(client, topic, deviceData, 0, 0, 0);
        printf("completeTopic: %s\n", completeTopic);
    }
    else
    {
        esp_mqtt_client_publish(client, topic, deviceData, 0, 0, 0);
        printf("completeTopic: %s\n", topic);
        printf("deviceData: %s\n", deviceData);
    }
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    int msg_id;

    const char *mqttPrefxIN = sys_settings.mqtt.prefx;
    const char *topicIN = "/td/custom/#";
    char completeTopicIN[strlen(mqttPrefxIN) + strlen(deviceName) + strlen(topicIN) + 2];
    strcpy(completeTopicIN, mqttPrefxIN);
    strcat(completeTopicIN, "/");
    strcat(completeTopicIN, deviceName);
    strcat(completeTopicIN, topicIN);

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
        MQTT_CONNEECTED = 1;
        msg_id = esp_mqtt_client_subscribe(client, completeTopicIN, 0);
        ESP_LOGI(TAG_MQTT, "subscribe successful to %s, msg_id=%d", completeTopicIN, msg_id);
        sys_settings.mqtt.mqtt_conected = true;
        publis_elements();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        MQTT_CONNEECTED = 0;
        sys_settings.mqtt.mqtt_conected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        // Сюда добавить обработчик команд поступающих из MQTT для переключения реле
        printf("Обработка команд из MQTT пока не реализована");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG_MQTT, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG_MQTT, "Other event id:%d", event->event_id);
        break;
    }
}

void Publisher_Task(void *params)
{
    while (true)
    {
        if (MQTT_CONNEECTED)
        {
            //  esp_mqtt_client_publish(client, "/topic/test3", "Helllo World", 0, 0, 0);
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

esp_err_t mqtt_app_start(void)
{
    // Публикуем доступность устойства
    const char *mqttPrefx = sys_settings.mqtt.prefx;
    const char *topic = "/device/custom/";

    char completeTopiclwt[strlen(mqttPrefx) + strlen(topic) + strlen(deviceName) + 1];
    strcpy(completeTopiclwt, mqttPrefx);
    strcat(completeTopiclwt, topic);
    strcat(completeTopiclwt, deviceName);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = sys_settings.mqtt.server,
        // .broker.address.port = sys_settings.mqtt.port,
        // .broker.address.path = sys_settings.mqtt.path,
        .credentials.username = sys_settings.mqtt.user,
        .credentials.authentication.password = sys_settings.mqtt.password,
        .session.last_will.msg = "{\"status\":\"offline\"}",
        .session.last_will.topic = completeTopiclwt, // Set your last will topic
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    xTaskCreate(Publisher_Task, "Publisher_Task", 1024 * 5, NULL, 5, NULL);

    return ESP_OK;
}
