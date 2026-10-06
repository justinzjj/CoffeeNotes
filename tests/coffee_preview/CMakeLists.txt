cmake_minimum_required(VERSION 3.16)
project(coffee_preview C)
get_filename_component(COFFEE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${COFFEE_ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(coffee_preview preview.c
    "${COFFEE_ROOT}/main/coffee_model.c"
    "${COFFEE_ROOT}/main/coffee_ui.c"
    "${COFFEE_ROOT}/assets/fonts/coffee_font_14.c"
    "${COFFEE_ROOT}/assets/fonts/coffee_font_16.c"
    "${COFFEE_ROOT}/assets/fonts/coffee_font_20.c")
target_include_directories(coffee_preview PRIVATE "${COFFEE_ROOT}/main" "${COFFEE_ROOT}/assets/fonts")
target_link_libraries(coffee_preview PRIVATE lvgl::lvgl m)
target_compile_options(coffee_preview PRIVATE -Wall -Wextra -Werror)
