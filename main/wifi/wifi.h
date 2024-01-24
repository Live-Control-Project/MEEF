#ifndef MEEF_WIFI_H_
#define MEEF_WIFI_H_

#include "common.h"
#include <esp_wifi.h>

esp_err_t wifi_init();
static esp_err_t init_ap();

#endif /* MEEF_WIFI_H_ */
