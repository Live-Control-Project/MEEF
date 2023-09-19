FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
idf_component_register(SRCS ${app_sources}
                    INCLUDE_DIRS "include")
                    set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
                    spiffs_create_partition_image(storage ../spiffs_data FLASH_IN_PROJECT)