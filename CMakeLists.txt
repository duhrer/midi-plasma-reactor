cmake_minimum_required(VERSION 3.13)

set(NAME midi-plasma-reactor)

set(PICO_PLATFORM rp2350)
set(PICO_BOARD pimoroni_plasma2350)

# Set the default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Specify the compiler to use
if(DEFINED ENV{PICO_TOOLCHAIN_PATH})
    set(CMAKE_C_COMPILER "$ENV{PICO_TOOLCHAIN_PATH}/bin/arm-none-eabi-gcc")
    set(CMAKE_CXX_COMPILER "$ENV{PICO_TOOLCHAIN_PATH}/bin/arm-none-eabi-g++")
endif()

# Set up the Pico SDK
set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/pico-sdk)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Compiler settings
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall)

pico_sdk_init()

add_subdirectory(lib/pico_neopixels build)

set(SOURCE_FILES
    src/reactor.cpp
    src/usb_descriptors.c
)

add_executable(${NAME}
    ${SOURCE_FILES}
)

# Pedantic warnings on for project source code only.
set_source_files_properties(${SOURCE_FILES} PROPERTIES COMPILE_OPTIONS "-Wextra;-Wpedantic")

target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${NAME}
    pico_stdlib
    pico_neopixel
    hardware_pio
    tinyusb_device
    pico_bootsel_via_double_reset
)

target_compile_definitions(${NAME} PRIVATE)

# Disable USB serial and enable UART serial
pico_enable_stdio_usb(${NAME} 0)
pico_enable_stdio_uart(${NAME} 1)

pico_add_extra_outputs(${NAME})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2
    ${CMAKE_CURRENT_LIST_DIR}/README.md
    DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)

# Mark TinyUSB includes as system headers
# to suppress warnings when -Wextra is set.
if(TARGET tinyusb_common_base)
    get_target_property(TINYUSB_INCLUDES tinyusb_common_base INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(tinyusb_common_base SYSTEM INTERFACE ${TINYUSB_INCLUDES})
endif()

if(TARGET tinyusb_device)
    get_target_property(TINYUSB_INCLUDES tinyusb_device INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(tinyusb_device SYSTEM INTERFACE ${TINYUSB_INCLUDES})
endif()

if(TARGET tinyusb_host)
    get_target_property(TINYUSB_INCLUDES tinyusb_host INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(tinyusb_host SYSTEM INTERFACE ${TINYUSB_INCLUDES})
endif()
