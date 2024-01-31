#include "wifi.h"

#include <string.h>
#include <esp_netif.h>
#include <lwip/ip_addr.h>
#include "sdkconfig.h"
#include "settings.h"
#include "utils/bus.h"

static esp_netif_t *iface = NULL;
static int s_retry_num = 0;
static const char *TAG_wifi = "WiFi";
TaskHandle_t myTaskHandle = NULL;
static void log_ip_info()
{
    if (!iface)
        return;

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(iface, &ip_info);

    ESP_LOGI(TAG_wifi, "--------------------------------------------------");
    ESP_LOGI(TAG_wifi, "IP address: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG_wifi, "Netmask:    " IPSTR, IP2STR(&ip_info.netmask));
    ESP_LOGI(TAG_wifi, "Gateway:    " IPSTR, IP2STR(&ip_info.gw));
    ESP_LOGI(TAG_wifi, "--------------------------------------------------");
    sprintf(sys_settings.wifi.ip.staip, IPSTR, IP2STR(&ip_info.ip));

    esp_netif_dns_info_t dns;
    for (esp_netif_dns_type_t t = ESP_NETIF_DNS_MAIN; t < ESP_NETIF_DNS_MAX; t++)
    {
        esp_netif_get_dns_info(iface, t, &dns);
        ESP_LOGI(TAG_wifi, "DNS %d:      " IPSTR, t, IP2STR(&dns.ip.u_addr.ip4));
    }
    ESP_LOGI(TAG_wifi, "--------------------------------------------------");
}

static void set_ip_info()
{
    esp_err_t res;

    if (!sys_settings.wifi.ip.dhcp || sys_settings.wifi.mode == WIFI_MODE_APSTA)
    {
        if (sys_settings.wifi.mode == WIFI_MODE_APSTA)
            esp_netif_dhcps_stop(iface);
        else
            esp_netif_dhcpc_stop(iface);

        esp_netif_ip_info_t ip_info;
        ip_info.ip.addr = ipaddr_addr(sys_settings.wifi.ip.ip);
        ip_info.netmask.addr = ipaddr_addr(sys_settings.wifi.ip.netmask);
        ip_info.gw.addr = ipaddr_addr(sys_settings.wifi.ip.gateway);
        res = esp_netif_set_ip_info(iface, &ip_info);
        if (res != ESP_OK)
            ESP_LOGW(TAG_wifi, "Error setting IP address %d (%s)", res, esp_err_to_name(res));

        if (sys_settings.wifi.mode == WIFI_MODE_APSTA)
            esp_netif_dhcps_start(iface);
    }

    esp_netif_dns_info_t dns;
    dns.ip.type = IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ipaddr_addr(sys_settings.wifi.ip.dns);
    res = esp_netif_set_dns_info(iface,
                                 sys_settings.wifi.mode != WIFI_MODE_APSTA && sys_settings.wifi.ip.dhcp
                                     ? ESP_NETIF_DNS_FALLBACK
                                     : ESP_NETIF_DNS_MAIN,
                                 &dns);
    if (res != ESP_OK)
        ESP_LOGW(TAG_wifi, "Error setting DNS address %d (%s)", res, esp_err_to_name(res));
}

static void wifi_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_err_t res = ESP_OK;
    switch (event_id)
    {
    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG_wifi, "WiFi started in access point mode");
        set_ip_info();
        log_ip_info();
        bus_send_event(EVENT_NETWORK_UP, NULL, 0);
        break;
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG_wifi, "WiFi started in station mode, connecting...");
        if ((res = esp_wifi_connect()) != ESP_OK)
            ESP_LOGE(TAG_wifi, "WiFi error %d [%s]", res, esp_err_to_name(res));
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG_wifi, "WiFi connected to '%s'", sys_settings.wifi.sta.ssid);
        set_ip_info();
        vTaskSuspend(myTaskHandle);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        if (s_retry_num < 2 && event_base == WIFI_EVENT)
        {
            s_retry_num++;
            ESP_LOGI(TAG_wifi, "WiFi disconnected, reconnecting...");
            bus_send_event(EVENT_NETWORK_DOWN, NULL, 0);
            if ((res = esp_wifi_connect()) != ESP_OK)
            {
                ESP_LOGE(TAG_wifi, "WiFi error %d [%s]", res, esp_err_to_name(res));
            }
        }
        else
        {
            vTaskResume(myTaskHandle);

            ESP_LOGI(TAG_wifi, "Starting AP");
            //  ESP_ERROR_CHECK(esp_wifi_stop());
            //  ESP_ERROR_CHECK(esp_wifi_deinit());
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
            ESP_ERROR_CHECK(esp_wifi_start());
        }
        break;
    default:
        break;
    }
}

static void ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_err_t res = ESP_OK;
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG_wifi, "WiFi got IP address");
        log_ip_info();
        bus_send_event(EVENT_NETWORK_UP, NULL, 0);
        break;
    case IP_EVENT_STA_LOST_IP:
        ESP_LOGI(TAG_wifi, "WiFi lost IP address, reconnecting");
        bus_send_event(EVENT_NETWORK_DOWN, NULL, 0);
        if ((res = esp_wifi_connect()) != ESP_OK)
            ESP_LOGE(TAG_wifi, "WiFi error %d [%s]", res, esp_err_to_name(res));
        break;
    default:
        break;
    }
}

static esp_err_t init_ap()
{
    ESP_LOGI(TAG_wifi, "Starting WiFi in access point mode");

    // create default interface
    iface = esp_netif_create_default_wifi_ap();
    CHECK(esp_netif_set_hostname(iface, APP_NAME));

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    CHECK(esp_wifi_init(&init_cfg));

    CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {0};
    memcpy(wifi_cfg.ap.ssid, sys_settings.wifi.ap.ssid, sizeof(wifi_cfg.ap.ssid));
    wifi_cfg.ap.ssid_len = strlen((const char *)sys_settings.wifi.ap.ssid);
    memcpy(wifi_cfg.ap.password, sys_settings.wifi.ap.password, sizeof(wifi_cfg.ap.password));
    wifi_cfg.ap.max_connection = sys_settings.wifi.ap.max_connection;
    wifi_cfg.ap.authmode = sys_settings.wifi.ap.authmode;
    wifi_cfg.ap.channel = sys_settings.wifi.ap.channel;

    ESP_LOGI(TAG_wifi, "WiFi access point settings:");
    ESP_LOGI(TAG_wifi, "--------------------------------------------------");
    ESP_LOGI(TAG_wifi, "SSID: %s", wifi_cfg.ap.ssid);
    ESP_LOGI(TAG_wifi, "Channel: %d", wifi_cfg.ap.channel);
    ESP_LOGI(TAG_wifi, "Auth mode: %s", wifi_cfg.ap.authmode == WIFI_AUTH_OPEN ? "Open" : "WPA2/PSK");
    ESP_LOGI(TAG_wifi, "--------------------------------------------------");

    CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
    CHECK(esp_wifi_start());

    return ESP_OK;
}

static esp_err_t init_sta()
{
    ESP_LOGI(TAG_wifi, "Starting WiFi in station mode, connecting to '%s'", sys_settings.wifi.sta.ssid);

    iface = esp_netif_create_default_wifi_sta();
    CHECK(esp_netif_set_hostname(iface, APP_NAME));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    CHECK(esp_wifi_init(&cfg));

    CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL, NULL));
    CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_handler, NULL, NULL));
    CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &ip_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {0};
    memcpy(wifi_cfg.sta.ssid, sys_settings.wifi.sta.ssid, sizeof(wifi_cfg.sta.ssid));
    memcpy(wifi_cfg.sta.password, sys_settings.wifi.sta.password, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = sys_settings.wifi.sta.threshold.authmode;

    ESP_LOGI(TAG_wifi, "WiFi station settings:");
    ESP_LOGI(TAG_wifi, "--------------------------------------------------");
    ESP_LOGI(TAG_wifi, "SSID: %s", wifi_cfg.sta.ssid);
    ESP_LOGI(TAG_wifi, "--------------------------------------------------");

    CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    CHECK(esp_wifi_start());

    return ESP_OK;
}
void wifi_AP_task(void *arg)
{
    esp_err_t res;
    while (1)
    {
        vTaskDelay(180000 / portTICK_PERIOD_MS);
        s_retry_num = 0;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        s_retry_num = 0;
        if ((res = esp_wifi_connect()) != ESP_OK)
            ESP_LOGE(TAG_wifi, "WiFi error %d [%s]", res, esp_err_to_name(res));
    }
}
esp_err_t wifi_init()
{
    CHECK(esp_netif_init());
    CHECK(esp_event_loop_create_default());

    if (sys_settings.wifi.mode == WIFI_MODE_AP)
        CHECK(init_ap());
    else
        CHECK(init_sta());

    xTaskCreate(wifi_AP_task, "wifi_AP_task", 4096, NULL, 5, &myTaskHandle);
    vTaskSuspend(myTaskHandle);
    return ESP_OK;
}
