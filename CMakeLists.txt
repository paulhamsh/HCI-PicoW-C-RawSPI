cmake_minimum_required(VERSION 3.13)

include(./pico_sdk_import.cmake)

project(my_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico_w)
set(FAMILY pico_w)

pico_sdk_init()

add_compile_definitions(CYW43_ENABLE_BLUETOOTH=1)

message("Not using btstack")
add_executable(pico_cyw pico_cyw.c )
add_compile_definitions(CYW43_DISABLE_BT_INIT=1)   # when set will disable btstack init
target_link_libraries(pico_cyw pico_stdlib pico_cyw43_arch_none )

target_include_directories(pico_cyw PRIVATE ${CMAKE_CURRENT_LIST_DIR}) # bt stack config
pico_enable_stdio_usb(pico_cyw 1)
pico_enable_stdio_uart(pico_cyw 0)

pico_add_extra_outputs(pico_cyw)
