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
    henka_ui_frame_desc frame_desc = {0};
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

    {
        henka_ui_scroll_state scroll = {0.0f, 0.0f, 0.0f};
        float thumb_height;
        float thumb_offset;
        HENKA_TEST_ASSERT(
            henka_ui_scroll_state_set_content(
                &scroll,
                400.0f,
                100.0f) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 0.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_scrollbar_thumb_height(
                400.0f,
                100.0f,
                200.0f,
                &thumb_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(thumb_height, 50.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_scrollbar_thumb_offset(
                300.0f,
                400.0f,
                100.0f,
                200.0f,
                thumb_height,
                &thumb_offset) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(thumb_offset, 150.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_scroll_state_set_from_scrollbar(
                &scroll,
                200.0f,
                0.0f,
                200.0f,
                thumb_height,
                thumb_height * 0.5f) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 300.0f, 0.0001f);
        scroll.offset = 0.0f;
        HENKA_TEST_ASSERT(
            henka_ui_scroll_state_apply_delta(
                &scroll,
                40.0f) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 40.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_scroll_state_apply_delta(
                &scroll,
                1000.0f) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 300.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_scroll_state_set_content(
                &scroll,
                80.0f,
                100.0f) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 0.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_scrollbar_thumb_height(
                100.0f,
                100.0f,
                200.0f,
                &thumb_height) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_scroll_state_set_content(
                &scroll,
                100.0f,
                0.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    }

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
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_panel(
        ui,
        (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f},
        "Panel") == HENKA_SUCCESS);
    uppercase_draw_count = henka_ui_get_draw_rect_count(ui) - draw_count_before;
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_panel_with_border_mask(
        ui,
        (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f},
        "Panel",
        HENKA_UI_BORDER_ALL) == HENKA_SUCCESS);
    lowercase_draw_count = henka_ui_get_draw_rect_count(ui) - draw_count_before;
    HENKA_TEST_ASSERT(lowercase_draw_count == uppercase_draw_count);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_panel_with_border_mask(
        ui,
        (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f},
        "Panel",
        HENKA_UI_BORDER_NONE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_get_draw_rect_count(ui) - draw_count_before ==
        uppercase_draw_count - 4U);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == true);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_panel_with_border_mask(
        ui,
        (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f},
        "Panel",
        HENKA_UI_BORDER_ALL & ~HENKA_UI_BORDER_LEFT) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_get_draw_rect_count(ui) - draw_count_before ==
        uppercase_draw_count - 1U);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_panel_with_border_mask(
        ui,
        (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f},
        "Panel",
        HENKA_UI_BORDER_ALL | (1U << 4)) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) == draw_count_before);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_viewport_frame(
        ui,
        (henka_ui_rect){240.0f, 20.0f, 320.0f, 220.0f},
        "Scene View") == HENKA_SUCCESS);
    uppercase_draw_count = henka_ui_get_draw_rect_count(ui) - draw_count_before;
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_viewport_frame_with_border_mask(
        ui,
        (henka_ui_rect){240.0f, 20.0f, 320.0f, 220.0f},
        "Scene View",
        HENKA_UI_BORDER_ALL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_get_draw_rect_count(ui) - draw_count_before ==
        uppercase_draw_count);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    draw_count_before = henka_ui_get_draw_rect_count(ui);
    HENKA_TEST_ASSERT(henka_ui_viewport_frame_with_border_mask(
        ui,
        (henka_ui_rect){240.0f, 20.0f, 320.0f, 220.0f},
        "Scene View",
        HENKA_UI_BORDER_NONE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_get_draw_rect_count(ui) - draw_count_before ==
        uppercase_draw_count - 4U);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

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


    {
        henka_ui_flow_desc flow_desc;
        henka_ui_rect flow_row;
        bool flow_visible;
        float flow_content_height;
        bool expanded;
        bool changed;

        flow_desc.bounds =
            (henka_ui_rect){20.0f, 40.0f, 180.0f, 60.0f};
        flow_desc.scroll_offset = 0.0f;
        flow_desc.row_spacing = 4.0f;
        flow_desc.indent_width = 12.0f;

        HENKA_TEST_ASSERT(
            henka_ui_flow_begin(NULL, &flow_desc) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_flow_begin(ui, &flow_desc) ==
            HENKA_ERROR_INVALID_ARGUMENT);

        frame_desc.mouse_position = (henka_vec2){300.0f, 300.0f};
        frame_desc.mouse_left_down = false;
        frame_desc.mouse_left_pressed = false;
        frame_desc.mouse_left_released = false;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_begin(ui, &flow_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_begin(ui, &flow_desc) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_flow_next_row(
                ui,
                20.0f,
                0U,
                &flow_row,
                &flow_visible) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(flow_visible);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            flow_row.x, 20.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            flow_row.y, 40.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            flow_row.width, 180.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_flow_next_row(
                ui,
                20.0f,
                1U,
                &flow_row,
                &flow_visible) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(flow_visible);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            flow_row.x, 32.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            flow_row.y, 64.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            flow_row.width, 168.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_flow_end(
                ui,
                &flow_content_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            flow_content_height, 44.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_flow_end(
                ui,
                &flow_content_height) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        flow_desc.scroll_offset = 80.0f;
        frame_desc.mouse_position = (henka_vec2){300.0f, 300.0f};
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_begin(ui, &flow_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_next_row(
                ui,
                20.0f,
                0U,
                &flow_row,
                &flow_visible) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!flow_visible);
        HENKA_TEST_ASSERT(
            henka_ui_flow_end(
                ui,
                &flow_content_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        expanded = false;
        changed = true;
        frame_desc.mouse_position = (henka_vec2){60.0f, 60.0f};
        frame_desc.mouse_left_down = true;
        frame_desc.mouse_left_pressed = true;
        frame_desc.mouse_left_released = false;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "test.group",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "Group",
                &expanded,
                &changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!expanded);
        HENKA_TEST_ASSERT(!changed);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        changed = true;
        frame_desc.mouse_left_down = false;
        frame_desc.mouse_left_pressed = false;
        frame_desc.mouse_left_released = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "test.group",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "Group",
                &expanded,
                &changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(expanded);
        HENKA_TEST_ASSERT(changed);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        changed = true;
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "Group",
                &expanded,
                &changed) ==
            HENKA_ERROR_INVALID_ARGUMENT);
    }

    {
        bool duplicate_expanded;
        bool duplicate_changed;
        size_t duplicate_rect_count;
        size_t duplicate_line_count;
        henka_ui_flow_desc hardening_flow_desc;
        henka_ui_rect hardening_row;
        bool hardening_row_visible;
        float hardening_content_height;

        duplicate_expanded = false;
        duplicate_changed = false;
        frame_desc.mouse_position = (henka_vec2){300.0f, 300.0f};
        frame_desc.mouse_left_down = false;
        frame_desc.mouse_left_pressed = false;
        frame_desc.mouse_left_released = false;

        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "duplicate.group",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "First",
                &duplicate_expanded,
                &duplicate_changed) == HENKA_SUCCESS);
        duplicate_rect_count = henka_ui_get_draw_rect_count(ui);
        duplicate_line_count = henka_ui_get_draw_line_count(ui);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "duplicate.group",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "Second",
                &duplicate_expanded,
                &duplicate_changed) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_get_draw_rect_count(ui) == duplicate_rect_count);
        HENKA_TEST_ASSERT(
            henka_ui_get_draw_line_count(ui) == duplicate_line_count);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        hardening_flow_desc.bounds =
            (henka_ui_rect){0.0f, -FLT_MAX, 100.0f, 10.0f};
        hardening_flow_desc.scroll_offset = FLT_MAX;
        hardening_flow_desc.row_spacing = 0.0f;
        hardening_flow_desc.indent_width = 0.0f;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_begin(ui, &hardening_flow_desc) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        hardening_flow_desc.bounds =
            (henka_ui_rect){0.0f, 0.0f, 100.0f, 100.0f};
        hardening_flow_desc.scroll_offset = 0.0f;
        hardening_flow_desc.row_spacing = 0.0f;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_begin(ui, &hardening_flow_desc) ==
            HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_next_row(
                ui,
                FLT_MAX,
                0U,
                &hardening_row,
                &hardening_row_visible) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_flow_next_row(
                ui,
                FLT_MAX,
                0U,
                &hardening_row,
                &hardening_row_visible) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            henka_ui_flow_end(
                ui,
                &hardening_content_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            hardening_content_height,
            FLT_MAX,
            FLT_MAX * 0.0001f);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        duplicate_expanded = false;
        duplicate_changed = false;
        frame_desc.mouse_position = (henka_vec2){60.0f, 60.0f};
        frame_desc.mouse_left_down = true;
        frame_desc.mouse_left_pressed = true;
        frame_desc.mouse_left_released = false;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "drag.away.group",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "Drag Away",
                &duplicate_expanded,
                &duplicate_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        frame_desc.mouse_position = (henka_vec2){300.0f, 300.0f};
        frame_desc.mouse_left_down = false;
        frame_desc.mouse_left_pressed = false;
        frame_desc.mouse_left_released = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "drag.away.group",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "Drag Away",
                &duplicate_expanded,
                &duplicate_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!duplicate_expanded);
        HENKA_TEST_ASSERT(!duplicate_changed);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);
    }

    {
        bool nav_a_expanded;
        bool nav_b_expanded;
        bool nav_c_expanded;
        bool nav_a_changed;
        bool nav_b_changed;
        bool nav_c_changed;
        unsigned int consumed_navigation;

        nav_a_expanded = false;
        nav_b_expanded = false;
        nav_c_expanded = false;
        nav_a_changed = false;
        nav_b_changed = false;
        nav_c_changed = false;

        frame_desc.navigation_up_pressed = false;
        frame_desc.navigation_down_pressed = false;
        frame_desc.navigation_left_pressed = false;
        frame_desc.navigation_right_pressed = false;
        frame_desc.navigation_enter_pressed = false;

        frame_desc.mouse_position = (henka_vec2){60.0f, 60.0f};
        frame_desc.mouse_left_down = true;
        frame_desc.mouse_left_pressed = true;
        frame_desc.mouse_left_released = false;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A",
                &nav_a_expanded,
                &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B",
                &nav_b_expanded,
                &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C",
                &nav_c_expanded,
                &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        frame_desc.mouse_left_down = false;
        frame_desc.mouse_left_pressed = false;
        frame_desc.mouse_left_released = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A",
                &nav_a_expanded,
                &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B",
                &nav_b_expanded,
                &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui,
                "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C",
                &nav_c_expanded,
                &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(nav_a_expanded);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        frame_desc.mouse_position = (henka_vec2){300.0f, 300.0f};
        frame_desc.mouse_left_released = false;
        frame_desc.navigation_down_pressed = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B", &nav_b_expanded, &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);
        consumed_navigation =
            henka_ui_get_consumed_navigation_mask(ui);
        HENKA_TEST_ASSERT(
            (consumed_navigation &
             HENKA_UI_NAVIGATION_DOWN) != 0U);

        frame_desc.navigation_down_pressed = false;
        frame_desc.navigation_enter_pressed = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B", &nav_b_expanded, &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(nav_b_expanded);
        HENKA_TEST_ASSERT(nav_b_changed);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            (henka_ui_get_consumed_navigation_mask(ui) &
             HENKA_UI_NAVIGATION_ENTER) != 0U);

        frame_desc.navigation_enter_pressed = false;
        frame_desc.navigation_left_pressed = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B", &nav_b_expanded, &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!nav_b_expanded);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            (henka_ui_get_consumed_navigation_mask(ui) &
             HENKA_UI_NAVIGATION_LEFT) != 0U);

        frame_desc.navigation_left_pressed = false;
        frame_desc.navigation_right_pressed = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B", &nav_b_expanded, &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(nav_b_expanded);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            (henka_ui_get_consumed_navigation_mask(ui) &
             HENKA_UI_NAVIGATION_RIGHT) != 0U);

        frame_desc.navigation_right_pressed = false;
        frame_desc.navigation_up_pressed = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B", &nav_b_expanded, &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            (henka_ui_get_consumed_navigation_mask(ui) &
             HENKA_UI_NAVIGATION_UP) != 0U);

        frame_desc.navigation_up_pressed = false;
        frame_desc.navigation_down_pressed = true;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.b",
                (henka_ui_rect){40.0f, 76.0f, 160.0f, 24.0f},
                "B", &nav_b_expanded, &nav_b_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        frame_desc.navigation_down_pressed = false;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);

        frame_desc.navigation_enter_pressed = true;
        nav_a_changed = false;
        nav_c_changed = false;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.a",
                (henka_ui_rect){40.0f, 48.0f, 160.0f, 24.0f},
                "A", &nav_a_expanded, &nav_a_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_disclosure_row(
                ui, "nav.c",
                (henka_ui_rect){40.0f, 104.0f, 160.0f, 24.0f},
                "C", &nav_c_expanded, &nav_c_changed) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            henka_ui_end_frame(ui) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!nav_a_changed);
        HENKA_TEST_ASSERT(!nav_c_changed);
        HENKA_TEST_ASSERT(
            (henka_ui_get_consumed_navigation_mask(ui) &
             HENKA_UI_NAVIGATION_ENTER) == 0U);

        frame_desc.navigation_enter_pressed = false;
    }
    henka_ui_destroy(ui);
}
