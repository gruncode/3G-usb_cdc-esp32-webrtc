set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components)

idf_component_register(SRCS
  "app_main.c" "camera.c" 
  INCLUDE_DIRS "."
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
