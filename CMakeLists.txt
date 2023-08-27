cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(pico-temp-hum)

pico_sdk_init()


if (TARGET tinyusb_device)
	add_executable(pico-temp-hum
        	pico-temp-hum.c
        )


    # pull in common dependencies
    target_link_libraries(pico-temp-hum pico_stdlib hardware_adc hardware_i2c)

    # enable usb output, disable uart output
    pico_enable_stdio_usb(pico-temp-hum 1)
    pico_enable_stdio_uart(pico-temp-hum 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(pico-temp-hum)

elseif(PICO_ON_DEVICE)
    message(WARNING "not building hello_usb because TinyUSB submodule is not initialized in the SDK")
endif()

