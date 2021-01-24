set (SOURCES "azure_main.c")

if (CONFIG_WIFI_PROV_ENABLE)
    list (APPEND SOURCES
    "wifi_provisioning.c")
endif()

idf_component_register(SRCS "${SOURCES}"
					  INCLUDE_DIRS "include")
