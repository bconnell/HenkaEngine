#include "test_suite.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include <henka/ui.h>

void henka_test_ui(void)
{
    bool toggle_value;
    henka_result result;
    henka_ui_context* ui;
    henka_ui_frame_desc frame_desc;
    char mutable_id[32];
    int text_height;
    int text_width;
    size_t draw_count_before;
    size_t lowercase_draw_count;
    size_t uppercase_draw_count;

    ui = NULL;
    HENKA_TEST_ASSERT(henka_ui_create(NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ui != NULL);
    HENKA_TEST_ASSERT(henka_ui_is_visible(ui) == false);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == false);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) == 0U);
    HENKA_TEST_ASSERT(henka_ui_get_draw_line_count(ui) == 0U);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_end_frame(NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_panel(
        ui,
        (henka_ui_rect){0.0f, 0.0f, 100.0f, 60.0f},
        "Outside") == HENKA_ERROR_INVALID_ARGUMENT);
    toggle_value = false;
    HENKA_TEST_ASSERT(!henka_ui_toggle(
        ui,
        "outside_toggle",
        (henka_ui_rect){0.0f, 0.0f, 100.0f, 30.0f},
        "Outside",
        &toggle_value));
    HENKA_TEST_ASSERT(toggle_value == false);
    text_width = 123;
    text_height = 456;
    HENKA_TEST_ASSERT(henka_ui_measure_text(
        NULL,
        1.0f,
        &text_width,
        &text_height) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(text_width == 0);
    HENKA_TEST_ASSERT(text_height == 0);
    HENKA_TEST_ASSERT(henka_ui_measure_text("Henka", 0.0f, &text_width, &text_height) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_measure_text("Henka", 1.0f, &text_width, &text_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(text_width > 0);
    HENKA_TEST_ASSERT(text_height > 0);

    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        (henka_ui_rect){10.0f, 10.0f, 20.0f, 20.0f},
        (henka_vec2){15.0f, 18.0f}) == true);
    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        (henka_ui_rect){10.0f, 10.0f, 20.0f, 20.0f},
        (henka_vec2){35.0f, 18.0f}) == false);

    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    frame_desc.mouse_position = (henka_vec2){48.0f, 52.0f};
    frame_desc.mouse_left_down = true;
    frame_desc.mouse_left_pressed = true;
    frame_desc.mouse_left_released = false;

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_overlay_rect(ui, (henka_ui_rect){960.0f, 24.0f, 18.0f, 18.0f}, (henka_vec4){1.0f, 0.0f, 0.0f, 1.0f}) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) == draw_count_before);
    HENKA_TEST_ASSERT(henka_ui_overlay_line(ui, (henka_vec2){980.0f, 40.0f}, (henka_vec2){1012.0f, 54.0f}, 3.0f, (henka_vec4){0.0f, 1.0f, 0.0f, 1.0f}) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_line_count(ui);
    HENKA_TEST_ASSERT(henka_ui_overlay_polyline(
        ui,
        (henka_vec2[]){{1020.0f, 24.0f}, {1040.0f, 36.0f}, {NAN, 54.0f}},
        3U,
        2.0f,
        (henka_vec4){0.0f, 0.5f, 1.0f, 1.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_get_draw_line_count(ui) == draw_count_before);
    HENKA_TEST_ASSERT(henka_ui_overlay_line(
        ui,
        (henka_vec2){FLT_MAX, 0.0f},
        (henka_vec2){-FLT_MAX, 0.0f},
        1.0f,
        (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_get_draw_line_count(ui) == draw_count_before + 1U);
    HENKA_TEST_ASSERT(henka_ui_overlay_polyline(
        ui,
        (henka_vec2[]){{1020.0f, 24.0f}, {1040.0f, 36.0f}, {1050.0f, 54.0f}},
        3U,
        2.0f,
        (henka_vec4){0.0f, 0.5f, 1.0f, 1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_panel(ui, (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f}, "Panel") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_label(ui, 28.0f, 44.0f, 1.0f, "Status") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_overlay_hint(ui, (henka_ui_rect){980.0f, 650.0f, 180.0f, 44.0f}, "F4 Panels", "F5 Layout") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == false);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) > 0U);
    HENKA_TEST_ASSERT(henka_ui_get_draw_line_count(ui) >= 3U);
    HENKA_TEST_ASSERT(henka_ui_button(ui, "hidden_button", (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f}, "Click") == false);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_panel(
        ui,
        (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f},
        "After") == HENKA_ERROR_INVALID_ARGUMENT);

    henka_ui_set_visible(ui, true);
    HENKA_TEST_ASSERT(henka_ui_is_visible(ui) == true);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_label(ui, 8.0f, 8.0f, 1.0f, "A") ==
        HENKA_SUCCESS);
    uppercase_draw_count = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_label(ui, 8.0f, 8.0f, 1.0f, "a") ==
        HENKA_SUCCESS);
    lowercase_draw_count = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(lowercase_draw_count > 0U);
    HENKA_TEST_ASSERT(lowercase_draw_count != uppercase_draw_count);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_panel(ui, (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f}, "Panel") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_viewport_frame(ui, (henka_ui_rect){240.0f, 20.0f, 320.0f, 220.0f}, "Scene View") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_heading(ui, 28.0f, 60.0f, 1.0f, "Section") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_value_row(ui, (henka_ui_rect){28.0f, 76.0f, 160.0f, 22.0f}, "Label", "Value") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_status_chip(ui, (henka_ui_rect){28.0f, 102.0f, 80.0f, 20.0f}, "Status", false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == true);
    HENKA_TEST_ASSERT(henka_ui_button(ui, "button", (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f}, "Click") == false);
    HENKA_TEST_ASSERT(henka_ui_primary_button(ui, "primary", (henka_ui_rect){40.0f, 132.0f, 120.0f, 28.0f}, "Apply") == false);
    HENKA_TEST_ASSERT(henka_ui_selectable(ui, "selected", (henka_ui_rect){40.0f, 72.0f, 120.0f, 28.0f}, "Cube", true) == false);
    HENKA_TEST_ASSERT(henka_ui_tab(ui, "tab", (henka_ui_rect){40.0f, 164.0f, 120.0f, 24.0f}, "Utility", true) == false);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) > 0U);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_button(ui, "button", (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f}, "Click") == true);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    strcpy_s(mutable_id, sizeof(mutable_id), "mutable_button");
    frame_desc.mouse_left_down = true;
    frame_desc.mouse_left_pressed = true;
    frame_desc.mouse_left_released = false;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_button(
        ui,
        mutable_id,
        (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f},
        "Mutable") == false);
    strcpy_s(mutable_id, sizeof(mutable_id), "changed_button");
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_button(
        ui,
        "mutable_button",
        (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f},
        "Mutable") == true);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    toggle_value = false;
    frame_desc.mouse_position = (henka_vec2){48.0f, 92.0f};
    frame_desc.mouse_left_down = true;
    frame_desc.mouse_left_pressed = true;
    frame_desc.mouse_left_released = false;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_selectable(
        ui,
        "selectable",
        (henka_ui_rect){40.0f, 80.0f, 140.0f, 30.0f},
        "Ground",
        false) == false);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_selectable(
        ui,
        "selectable",
        (henka_ui_rect){40.0f, 80.0f, 140.0f, 30.0f},
        "Ground",
        false) == true);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_position = (henka_vec2){48.0f, 132.0f};
    frame_desc.mouse_left_down = true;
    frame_desc.mouse_left_pressed = true;
    frame_desc.mouse_left_released = false;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_toggle(
        ui,
        "toggle",
        (henka_ui_rect){40.0f, 120.0f, 140.0f, 30.0f},
        "Grid",
        &toggle_value) == false);
    HENKA_TEST_ASSERT(toggle_value == false);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_toggle(
        ui,
        "toggle",
        (henka_ui_rect){40.0f, 120.0f, 140.0f, 30.0f},
        "Grid",
        &toggle_value) == true);
    HENKA_TEST_ASSERT(toggle_value == true);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_position = (henka_vec2){300.0f, 300.0f};
    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = false;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_panel(ui, (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f}, "Panel") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == false);
    HENKA_TEST_ASSERT(henka_ui_button(ui, "button", (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f}, "Click") == false);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    result = henka_ui_panel(NULL, (henka_ui_rect){0.0f, 0.0f, 10.0f, 10.0f}, "Bad");
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    result = henka_ui_heading(ui, 0.0f, 0.0f, 1.0f, NULL);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    result = henka_ui_label(ui, 0.0f, 0.0f, 1.0f, NULL);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    result = henka_ui_viewport_frame(ui, (henka_ui_rect){0.0f, 0.0f, 100.0f, 80.0f}, NULL);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    result = henka_ui_value_row(ui, (henka_ui_rect){0.0f, 0.0f, 100.0f, 20.0f}, "Label", NULL);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    result = henka_ui_overlay_hint(ui, (henka_ui_rect){0.0f, 0.0f, 100.0f, 20.0f}, NULL, "Layout");
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    result = henka_ui_status_chip(ui, (henka_ui_rect){0.0f, 0.0f, 100.0f, 20.0f}, NULL, false);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        (henka_ui_rect){0.0f, 0.0f, NAN, 20.0f},
        (henka_vec2){1.0f, 1.0f}) == false);
    text_width = 123;
    text_height = 456;
    HENKA_TEST_ASSERT(henka_ui_measure_text(
        "Henka",
        NAN,
        &text_width,
        &text_height) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(text_width == 0);
    HENKA_TEST_ASSERT(text_height == 0);

    frame_desc.mouse_position = (henka_vec2){NAN, 0.0f};
    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = false;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_ERROR_INVALID_ARGUMENT);

    frame_desc.mouse_position = (henka_vec2){0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_overlay_rect(
        ui,
        (henka_ui_rect){0.0f, 0.0f, NAN, 10.0f},
        (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_overlay_line(
        ui,
        (henka_vec2){0.0f, 0.0f},
        (henka_vec2){NAN, 1.0f},
        1.0f,
        (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    henka_ui_destroy(ui);
}
