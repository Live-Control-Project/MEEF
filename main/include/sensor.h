/* Объявляется структура передаваемых параметров */
typedef struct
{
    char *param_id;
    int param_pin;
    char *param_cluster;
    int param_ep;
    int param_int;
    char *param_sensor_type;
} TaskParameters;