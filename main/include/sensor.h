/* Объявляется структура передаваемых параметров */
typedef struct
{
    int param_pin;
    int param_ep;
    int param_int;
    char *param_cluster;
    char *param_id;
    char *param_sensor_type;
} TaskParameters;