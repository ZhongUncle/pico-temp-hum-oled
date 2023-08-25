cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(temp_oled)

pico_sdk_init()


if (TARGET tinyusb_device)
	add_executable(temp_oled
        	temp_oled.c
        )


    # pull in common dependencies
    target_link_libraries(temp_oled pico_stdlib hardware_adc hardware_i2c)

    # enable usb output, disable uart output
    pico_enable_stdio_usb(temp_oled 1)
    pico_enable_stdio_uart(temp_oled 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(temp_oled)

elseif(PICO_ON_DEVICE)
    message(WARNING "not building hello_usb because TinyUSB submodule is not initialized in the SDK")
endif()

