#ifndef LIGHT_H
#define LIGHT_H

#include <stdint.h>

typedef struct light_data_s
{
    uint8_t status;
    uint8_t level;
    uint8_t color_h;
    uint8_t color_s;
    uint16_t color_x;
    uint16_t color_y;
    uint8_t color_mode;
    uint16_t crc;
} light_data_t;

void light_init(light_data_t *data);
void light_update(void);

#endif