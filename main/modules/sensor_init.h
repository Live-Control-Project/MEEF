/* Объявляется структура передаваемых параметров */
typedef struct
{
    int param_pin;
    int param_pin_SCL;
    int param_pin_SDA;
    int param_I2C_GND;
    char *param_I2C_ADDRESS;
    int param_ep;
    int param_int;
    int param_index;
    char *param_addr;
    int param_saveState;
    char *param_cluster;
    char *param_id;
    char *param_sensor_type;
    int param_before_sleep;
    int param_sleep_length;
    int param_before_long_sleep;
    int param_long_sleep_length;
} TaskParameters;
void sensor_init(void);
