/* Объявляется структура передаваемых параметров */
typedef struct
{
    int param_pin;
    int param_pin_SCL;
    int param_pin_SDA;
    int param_I2C_GND;
    int param_ep;
    int param_int;
    int param_seveState;
    char *param_cluster;
    char *param_id;
    char *param_sensor_type;
} TaskParameters;
