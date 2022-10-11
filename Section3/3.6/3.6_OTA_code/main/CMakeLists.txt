# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "OTA_test.c"
                    INCLUDE_DIRS ".")
