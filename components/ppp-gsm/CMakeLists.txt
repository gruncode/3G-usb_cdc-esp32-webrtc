idf_component_register(SRCS "gt_pppos.c"
                       INCLUDE_DIRS "." ${CMAKE_SOURCE_DIR}/main
                       REQUIRES esp_netif esp_modem lwip usb espressif__esp_modem_usb_dte
                       PRIV_INCLUDE_DIRS ${IDF_PATH}/components/lwip/lwip/src/include/netif/ppp
                       )


