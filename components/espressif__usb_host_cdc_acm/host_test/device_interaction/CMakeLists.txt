cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)

list(APPEND EXTRA_COMPONENT_DIRS
     "$ENV{IDF_PATH}/tools/mocks/usb/"
     #"$ENV{IDF_PATH}/tools/mocks/freertos/"    We are using freertos as real component
    )

add_definitions("-DCMOCK_MEM_DYNAMIC")
project(host_test_usb_cdc)
