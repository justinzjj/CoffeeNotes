idf_component_register(
    SRCS "main.c" "coffee_model.c" "coffee_store.c"
         "coffee_ui.c" "coffee_network.c" "coffee_form.c"
    INCLUDE_DIRS "." "../assets/fonts"
    REQUIRES bsp esp_event esp_netif esp_timer esp_wifi esp_http_server nvs_flash
)
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/coffee_font_14.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/coffee_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/coffee_font_20.c"
)
