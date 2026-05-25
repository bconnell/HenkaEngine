#include "test_suite.h"

#include <henka/ui.h>

void henka_test_ui(void)
{
    bool toggle_value;
    henka_result result;
    henka_ui_context* ui;
    henka_ui_frame_desc frame_desc;
    int text_height;
    int text_width;

    ui = NULL;
    HENKA_TEST_ASSERT(henka_ui_create(NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ui != NULL);
    HENKA_TEST_ASSERT(henka_ui_is_visible(ui) == false);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == false);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) == 0U);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_end_frame(NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_ui_measure_text(NULL, 1.0f, &text_width, &text_height) == HENKA_ERROR_INVALID_ARGUMENT);
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
    HENKA_TEST_ASSERT(henka_ui_panel(ui, (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f}, "Panel") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_label(ui, 28.0f, 44.0f, 1.0f, "Status") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == false);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) == 0U);
    HENKA_TEST_ASSERT(henka_ui_button(ui, "hidden_button", (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f}, "Click") == false);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    henka_ui_set_visible(ui, true);
    HENKA_TEST_ASSERT(henka_ui_is_visible(ui) == true);

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_panel(ui, (henka_ui_rect){20.0f, 20.0f, 200.0f, 100.0f}, "Panel") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui) == true);
    HENKA_TEST_ASSERT(henka_ui_button(ui, "button", (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f}, "Click") == true);
    HENKA_TEST_ASSERT(henka_ui_selectable(ui, "selected", (henka_ui_rect){40.0f, 72.0f, 120.0f, 28.0f}, "Cube", true) == false);
    HENKA_TEST_ASSERT(henka_ui_get_draw_rect_count(ui) > 0U);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_button(ui, "button", (henka_ui_rect){40.0f, 40.0f, 120.0f, 28.0f}, "Click") == false);
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
        &toggle_value) == true);
    HENKA_TEST_ASSERT(toggle_value == true);
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
        &toggle_value) == false);
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
    result = henka_ui_label(ui, 0.0f, 0.0f, 1.0f, NULL);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);

    henka_ui_destroy(ui);
}
