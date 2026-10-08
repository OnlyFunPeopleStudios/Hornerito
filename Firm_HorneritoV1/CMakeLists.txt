# HORNERITO V2 - proyecto ESP-IDF independiente
# Base reutilizada: 78/xiaozhi-esp32 (MIT) adaptada a la placa
# Waveshare ESP32-C6 Touch AMOLED 2.16".
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# Versión de la build (también visible en esp_app_desc y en la UI de estado).
set(PROJECT_VER "2.3.0")
project(hornerito)
