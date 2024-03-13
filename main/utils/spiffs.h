#include "common.h"
esp_err_t mount_storage(const char *base_path);
void writeJSONtoFile(char fileName, char *json_data);
void deleteFile(const char *fileName);