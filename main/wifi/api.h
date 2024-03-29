#ifndef MEEF_API_H_
#define MEEF_API_H_

#include "../utils/common.h"
#include <esp_http_server.h>

// API Handlers
//
// GET  /api/info                   -> Application name, version, build date etc
// GET  /api/settings/reset         -> reset wifi and led settings to defaults
// GET  /api/settings/wifi          -> return current wifi settings
// POST /api/settings/wifi          -> set wifi settings
// GET  /api/settings/leds          -> return led settings
// POST /api/settings/leds          -> set led settings
// GET  /api/reboot                 -> reboot lamp
// GET  /api/effects                -> get effects list
// GET  /api/effects/reset          -> reset effect settings to defaults
// GET  /api/lamp/state             -> get current lamp state, effect, brightness and FPS
// POST /api/lamp/state             -> set current lamp state, effect, brightness and FPS
// GET  /api/lamp/effect            -> get current effect settings
// POST /api/lamp/effect            -> set current effect settings
// GET  /*                          -> отправка любых файлов
// POST /saveelement                -> сохранение списка элементов
// POST /upload_firmware            -> обновление прошивки OTA

// Сохранение кофига в виде JSON не используется
// POST /saveconfig                 -> сохранение конфигурации

esp_err_t api_init(httpd_handle_t server);

#endif /* MEEF_API_H_ */
