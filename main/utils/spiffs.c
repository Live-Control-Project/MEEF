#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "spiffs.h"
#define STORAGE_NAMESPACE "nvs_storage"

static const char *TAG_spiffs = "storage";
/* Function to initialize SPIFFS for web_data */
esp_err_t mount_storage(const char *base_path)
{
	ESP_LOGI(TAG_spiffs, "Initializing SPIFFS");
	esp_vfs_spiffs_conf_t conf = {
		.base_path = base_path,
		.partition_label = NULL,
		.max_files = 5, // This sets the maximum number of files that can be open at the same time
		.format_if_mount_failed = true};

	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK)
	{
		if (ret == ESP_FAIL)
		{
			ESP_LOGE(TAG_spiffs, "Failed to mount or format filesystem");
		}
		else if (ret == ESP_ERR_NOT_FOUND)
		{
			ESP_LOGE(TAG_spiffs, "Failed to find SPIFFS partition");
		}
		else
		{
			ESP_LOGE(TAG_spiffs, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG_spiffs, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
		return ret;
	}

	ESP_LOGI(TAG_spiffs, "Partition size: total: %dKb, used: %dKb", total / 1024, used / 1024);
	return ESP_OK;
}

// Запись в файл
static int writeFile(char *fname, char *mode, char *buf)
{
	FILE *fd = fopen(fname, mode);
	if (fd == NULL)
	{
		ESP_LOGE("[write]", "fopen failed");
		return -1;
	}
	int len = strlen(buf);
	int res = fwrite(buf, 1, len, fd);
	if (res != len)
	{
		ESP_LOGE("[write]", "fwrite failed: %d <> %d ", res, len);
		res = fclose(fd);
		if (res)
		{
			ESP_LOGE("[write]", "fclose failed: %d", res);
			return -2;
		}
		return -3;
	}
	res = fclose(fd);
	if (res)
	{
		ESP_LOGE("[write]", "fclose failed: %d", res);
		return -4;
	}
	return 0;
}

void writeJSONtoFile(char fileName, char *json_data)
{
	cJSON *body_json = cJSON_Parse(json_data);
	char *string = cJSON_Print(body_json);
	const char *base_path = "/spiffs_storage/";
	strcat(base_path, fileName);
	writeFile(base_path, "w", string);
	free(json_data);
}
void deleteFile(const char *fileName)
{
	if (remove(fileName) != 0)
	{
		perror("Error deleting file");
	}
	else
	{
		ESP_LOGI(TAG_spiffs, "File deleted successfully: %s\n", fileName);
	}
}