# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "spiffs_example_main.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)