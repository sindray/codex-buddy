if(PASSPORT_WRECK)
    idf_component_register(
        SRCS "wreck_main.c" "wreck_game.c" "wreck_render.c"
        INCLUDE_DIRS "." "../assets/fonts"
        REQUIRES bsp nvs_flash esp_timer esp_lcd
        PRIV_REQUIRES esp_app_format
    )
    target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
else()
idf_component_register(
    SRCS "buddy_caption.c" "buddy_ble.c" "buddy_transport.c" "main.c" "buddy_state.c" "buddy_ui.c" "buddy_sound.c" "buddy_audio.c" "buddy_pet.c" "buddy_volume.c" "buddy_expression.c"
         "../assets/fonts/buddy_font_16.c" "../assets/fonts/buddy_font_20.c" "../assets/fonts/buddy_font_24.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash bsp esp_timer esp_driver_usb_serial_jtag json
    PRIV_REQUIRES esp_app_format
)
endif()
