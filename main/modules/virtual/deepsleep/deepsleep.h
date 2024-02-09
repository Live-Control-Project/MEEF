#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 4000 /* 4000 millisecond */
void deep_sleep(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams);
