cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico-usb-midi-interface C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/midi_uart_lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pio_midi_uart_lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/usb_midi_device_multistream)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/cdc_stdio_lib)
set(EMBEDDED_CLI_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/embedded-cli/lib/)

add_executable(pico-usb-midi-interface
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
  ${EMBEDDED_CLI_PATH}/src/embedded_cli.c
)

target_include_directories(pico-usb-midi-interface PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/preprocessor/include
  ${EMBEDDED_CLI_PATH}/include
)

target_link_options(pico-usb-midi-interface PRIVATE -Xlinker --print-memory-usage)
target_compile_options(pico-usb-midi-interface PRIVATE -Wall -Wextra)
target_link_libraries(pico-usb-midi-interface pio_midi_uart_lib midi_uart_lib tinyusb_device tinyusb_board
                      pico_stdlib usb_midi_device_multistream cdc_stdio_lib)

pico_enable_stdio_uart(pico-usb-midi-interface 0)
pico_add_extra_outputs(pico-usb-midi-interface)
