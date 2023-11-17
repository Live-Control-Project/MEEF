FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

idf_component_register(SRCS ${app_sources}
INCLUDE_DIRS "include"
)
spiffs_create_partition_image(storage ../spiffs_data FLASH_IN_PROJECT)
