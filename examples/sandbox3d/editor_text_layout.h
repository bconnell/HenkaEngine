#ifndef SANDBOX3D_EDITOR_TEXT_LAYOUT_H
#define SANDBOX3D_EDITOR_TEXT_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/result.h>
#include <henka/ui.h>

#define SANDBOX3D_EDITOR_TEXT_MAX_BYTES 4096U

/* These editor operations use the UI context's active font metrics. */
henka_result sandbox3d_editor_text_measure_wrapped_lines(
    const henka_ui_context* ui,
    const char* text,
    float maximum_width,
    size_t* out_line_count);

henka_result sandbox3d_editor_text_wrap(
    const henka_ui_context* ui,
    const char* text,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count);

henka_result sandbox3d_editor_text_limit_wrapped_lines(
    const henka_ui_context* ui,
    const char* wrapped_text,
    size_t maximum_line_count,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count,
    bool* out_truncated);

henka_result sandbox3d_editor_text_fit(
    const henka_ui_context* ui,
    const char* text,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    bool* out_truncated);

/* Hit-test a rendered single-line string, returning its UTF-8 byte offset. */
henka_result sandbox3d_editor_text_byte_offset_at_x(
    const henka_ui_context* ui,
    const char* text,
    float scale,
    float x,
    size_t* out_byte_offset);

/* Return the active font authority's exact x position for a bounded UTF-8
 * byte offset, including kerning with the glyph beginning at that offset. */
henka_result sandbox3d_editor_text_byte_offset_x(
    const henka_ui_context* ui,
    const char* text,
    size_t text_byte_count,
    size_t byte_offset,
    float scale,
    float* out_x);

henka_result sandbox3d_editor_text_format_hidden_row(
    const henka_ui_context* ui,
    const char* object_name,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    bool* out_name_truncated);

#endif
