#include "transform_inspector.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool sandbox3d_transform_inspector_finite(float value)
{
    return isfinite(value) != 0;
}

static bool sandbox3d_transform_inspector_transform_finite(
    henka_transform transform)
{
    return sandbox3d_transform_inspector_finite(transform.position.x) &&
        sandbox3d_transform_inspector_finite(transform.position.y) &&
        sandbox3d_transform_inspector_finite(transform.position.z) &&
        sandbox3d_transform_inspector_finite(transform.rotation.x) &&
        sandbox3d_transform_inspector_finite(transform.rotation.y) &&
        sandbox3d_transform_inspector_finite(transform.rotation.z) &&
        sandbox3d_transform_inspector_finite(transform.rotation.w) &&
        sandbox3d_transform_inspector_finite(transform.scale.x) &&
        sandbox3d_transform_inspector_finite(transform.scale.y) &&
        sandbox3d_transform_inspector_finite(transform.scale.z);
}

static bool sandbox3d_transform_inspector_same_transform(
    henka_transform left,
    henka_transform right)
{
    henka_quat left_rotation;
    henka_quat right_rotation;
    float rotation_dot;

    if (!sandbox3d_transform_inspector_transform_finite(left) ||
        !sandbox3d_transform_inspector_transform_finite(right))
    {
        return false;
    }
    left_rotation = henka_quat_normalize(left.rotation);
    right_rotation = henka_quat_normalize(right.rotation);
    rotation_dot =
        left_rotation.x * right_rotation.x +
        left_rotation.y * right_rotation.y +
        left_rotation.z * right_rotation.z +
        left_rotation.w * right_rotation.w;
    return left.position.x == right.position.x &&
        left.position.y == right.position.y &&
        left.position.z == right.position.z &&
        left.scale.x == right.scale.x &&
        left.scale.y == right.scale.y &&
        left.scale.z == right.scale.z &&
        fabsf(rotation_dot) >= 0.9999999f;
}

static henka_result sandbox3d_transform_inspector_format(
    sandbox3d_transform_inspector* inspector,
    henka_transform transform)
{
    float sine_y;
    float rotation_x;
    float rotation_y;
    float rotation_z;
    float quaternion_length_squared;
    henka_quat rotation;

    if (inspector == NULL ||
        !sandbox3d_transform_inspector_transform_finite(transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    quaternion_length_squared =
        transform.rotation.x * transform.rotation.x +
        transform.rotation.y * transform.rotation.y +
        transform.rotation.z * transform.rotation.z +
        transform.rotation.w * transform.rotation.w;
    if (!sandbox3d_transform_inspector_finite(quaternion_length_squared) ||
        quaternion_length_squared <= 0.000001f)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    rotation = henka_quat_normalize(transform.rotation);
    sine_y = 2.0f *
        (rotation.w * rotation.y - rotation.z * rotation.x);
    if (sine_y > 1.0f) sine_y = 1.0f;
    if (sine_y < -1.0f) sine_y = -1.0f;
    rotation_y = asinf(sine_y);
    if (fabsf(sine_y) < 0.999999f)
    {
        rotation_x = atan2f(
            2.0f * (rotation.w * rotation.x + rotation.y * rotation.z),
            1.0f - 2.0f *
                (rotation.x * rotation.x + rotation.y * rotation.y));
        rotation_z = atan2f(
            2.0f * (rotation.w * rotation.z + rotation.x * rotation.y),
            1.0f - 2.0f *
                (rotation.y * rotation.y + rotation.z * rotation.z));
    }
    else
    {
        /* At gimbal lock, choose Z = 0 and preserve the represented rotation. */
        rotation_x = atan2f(
            2.0f * (rotation.w * rotation.x - rotation.y * rotation.z),
            1.0f - 2.0f *
                (rotation.x * rotation.x + rotation.z * rotation.z));
        rotation_z = 0.0f;
    }

    snprintf(inspector->fields[0], sizeof(inspector->fields[0]), "%.6g", transform.position.x);
    snprintf(inspector->fields[1], sizeof(inspector->fields[1]), "%.6g", transform.position.y);
    snprintf(inspector->fields[2], sizeof(inspector->fields[2]), "%.6g", transform.position.z);
    snprintf(inspector->fields[3], sizeof(inspector->fields[3]), "%.6g", rotation_x * (180.0f / 3.14159265358979323846f));
    snprintf(inspector->fields[4], sizeof(inspector->fields[4]), "%.6g", rotation_y * (180.0f / 3.14159265358979323846f));
    snprintf(inspector->fields[5], sizeof(inspector->fields[5]), "%.6g", rotation_z * (180.0f / 3.14159265358979323846f));
    snprintf(inspector->fields[6], sizeof(inspector->fields[6]), "%.6g", transform.scale.x);
    snprintf(inspector->fields[7], sizeof(inspector->fields[7]), "%.6g", transform.scale.y);
    snprintf(inspector->fields[8], sizeof(inspector->fields[8]), "%.6g", transform.scale.z);
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_transform_inspector_parse(
    const char* text,
    float* out_value)
{
    char* end;
    float value;

    if (text == NULL || out_value == NULL || text[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    errno = 0;
    end = NULL;
    value = strtof(text, &end);
    if (end == text || errno == ERANGE ||
        !sandbox3d_transform_inspector_finite(value))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_value = value;
    return HENKA_SUCCESS;
}

void sandbox3d_transform_inspector_init(
    sandbox3d_transform_inspector* inspector)
{
    if (inspector != NULL)
    {
        memset(inspector, 0, sizeof(*inspector));
        inspector->entity = HENKA_INVALID_ENTITY;
    }
}

henka_result sandbox3d_transform_inspector_sync(
    sandbox3d_transform_inspector* inspector,
    henka_entity entity,
    henka_transform authoritative,
    bool* out_pending_edits_refreshed)
{
    bool changed_authority;
    henka_result result;

    if (out_pending_edits_refreshed != NULL)
    {
        *out_pending_edits_refreshed = false;
    }
    if (inspector == NULL || entity == HENKA_INVALID_ENTITY ||
        !sandbox3d_transform_inspector_transform_finite(authoritative))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    changed_authority = inspector->initialized && inspector->entity == entity &&
        !sandbox3d_transform_inspector_same_transform(
            inspector->baseline,
            authoritative);
    if (!inspector->initialized || inspector->entity != entity || changed_authority)
    {
        const bool discarded_pending = inspector->dirty;
        result = sandbox3d_transform_inspector_format(inspector, authoritative);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        inspector->entity = entity;
        inspector->baseline = authoritative;
        inspector->initialized = true;
        inspector->dirty = false;
        if (out_pending_edits_refreshed != NULL)
        {
            *out_pending_edits_refreshed = discarded_pending;
        }
    }
    return HENKA_SUCCESS;
}

void sandbox3d_transform_inspector_discard(
    sandbox3d_transform_inspector* inspector)
{
    if (inspector != NULL && inspector->initialized)
    {
        (void)sandbox3d_transform_inspector_format(
            inspector,
            inspector->baseline);
        inspector->dirty = false;
    }
}

bool sandbox3d_transform_inspector_has_pending_edits(
    const sandbox3d_transform_inspector* inspector)
{
    return inspector != NULL && inspector->initialized && inspector->dirty;
}

bool sandbox3d_transform_inspector_candidate_changes(
    const sandbox3d_transform_inspector* inspector,
    henka_transform candidate)
{
    return inspector != NULL && inspector->initialized &&
        !sandbox3d_transform_inspector_same_transform(
            inspector->baseline,
            candidate);
}

henka_result sandbox3d_transform_inspector_build_candidate(
    const sandbox3d_transform_inspector* inspector,
    henka_transform* out_candidate)
{
    float values[SANDBOX3D_TRANSFORM_INSPECTOR_FIELD_COUNT];
    sandbox3d_transform_inspector formatted_baseline = {0};
    bool rotation_changed = false;
    float radians_x;
    float radians_y;
    float radians_z;
    size_t index;

    if (out_candidate != NULL)
    {
        *out_candidate = henka_transform_identity();
    }
    if (inspector == NULL || !inspector->initialized || out_candidate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (sandbox3d_transform_inspector_format(
            &formatted_baseline,
            inspector->baseline) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    for (index = 0U; index < SANDBOX3D_TRANSFORM_INSPECTOR_FIELD_COUNT; ++index)
    {
        const henka_result result = sandbox3d_transform_inspector_parse(
            inspector->fields[index],
            &values[index]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    for (index = 0U; index < 3U; ++index)
    {
        if (strcmp(inspector->fields[index], formatted_baseline.fields[index]) == 0)
        {
            const henka_vec3 original = inspector->baseline.position;
            values[index] = index == 0U ? original.x :
                (index == 1U ? original.y : original.z);
        }
        if (strcmp(
                inspector->fields[index + 3U],
                formatted_baseline.fields[index + 3U]) != 0)
        {
            rotation_changed = true;
        }
        if (strcmp(
                inspector->fields[index + 6U],
                formatted_baseline.fields[index + 6U]) == 0)
        {
            const henka_vec3 original = inspector->baseline.scale;
            values[index + 6U] = index == 0U ? original.x :
                (index == 1U ? original.y : original.z);
        }
    }
    radians_x = remainderf(values[3], 360.0f) * (3.14159265358979323846f / 180.0f);
    radians_y = remainderf(values[4], 360.0f) * (3.14159265358979323846f / 180.0f);
    radians_z = remainderf(values[5], 360.0f) * (3.14159265358979323846f / 180.0f);
    *out_candidate = (henka_transform){
        .position = {values[0], values[1], values[2]},
        .rotation = rotation_changed
            ? henka_quat_from_euler(radians_y, radians_z, radians_x)
            : inspector->baseline.rotation,
        .scale = {values[6], values[7], values[8]}};
    return sandbox3d_transform_inspector_transform_finite(*out_candidate)
        ? HENKA_SUCCESS
        : HENKA_ERROR_NUMERIC_RANGE;
}

henka_result sandbox3d_transform_inspector_layout_row(
    henka_ui_rect bounds,
    henka_ui_rect* out_label,
    henka_ui_rect out_fields[3])
{
    const float label_width = 72.0f;
    const float label_gap = 6.0f;
    const float field_gap = 4.0f;
    float field_width;
    size_t index;

    if (out_label != NULL)
    {
        *out_label = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    if (out_fields != NULL)
    {
        memset(out_fields, 0, sizeof(*out_fields) * 3U);
    }
    if (out_label == NULL || out_fields == NULL ||
        !sandbox3d_transform_inspector_finite(bounds.x) ||
        !sandbox3d_transform_inspector_finite(bounds.y) ||
        !sandbox3d_transform_inspector_finite(bounds.width) ||
        !sandbox3d_transform_inspector_finite(bounds.height) ||
        bounds.width < 242.0f || bounds.height < 24.0f)
    {
        return HENKA_ERROR_LIMIT;
    }
    field_width = (bounds.width - label_width - label_gap - 2.0f * field_gap) / 3.0f;
    if (field_width < 52.0f)
    {
        return HENKA_ERROR_LIMIT;
    }
    *out_label = (henka_ui_rect){bounds.x, bounds.y, label_width, bounds.height};
    for (index = 0U; index < 3U; ++index)
    {
        out_fields[index] = (henka_ui_rect){
            bounds.x + label_width + label_gap +
                (float)index * (field_width + field_gap),
            bounds.y,
            field_width,
            bounds.height};
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_transform_inspector_draw_axis_header(
    henka_ui_context* ui,
    henka_ui_rect bounds)
{
    static const char* const axes[3] = {"X", "Y", "Z"};
    henka_ui_rect label;
    henka_ui_rect fields[3];
    size_t index;
    henka_result result;

    if (ui == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_transform_inspector_layout_row(bounds, &label, fields);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (index = 0U; index < 3U; ++index)
    {
        (void)henka_ui_label(
            ui,
            fields[index].x + fields[index].width * 0.5f - 4.0f,
            bounds.y,
            0.75f,
            axes[index]);
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_transform_inspector_draw_row(
    sandbox3d_transform_inspector* inspector,
    henka_ui_context* ui,
    sandbox3d_transform_inspector_row row,
    henka_ui_rect bounds,
    bool editable)
{
    static const char* const labels[3] = {
        "Position", "Rotation (°)", "Scale"};
    static const char* const row_ids[3] = {
        "position", "rotation", "scale"};
    static const char* const axes[3] = {"x", "y", "z"};
    henka_ui_rect label_bounds;
    henka_ui_rect fields[3];
    size_t first_index;
    size_t axis;
    henka_result result;

    if (inspector == NULL || ui == NULL ||
        row < SANDBOX3D_TRANSFORM_INSPECTOR_POSITION ||
        row > SANDBOX3D_TRANSFORM_INSPECTOR_SCALE ||
        !inspector->initialized)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_transform_inspector_layout_row(
        bounds,
        &label_bounds,
        fields);
    if (result != HENKA_SUCCESS)
    {
        (void)henka_ui_label(
            ui,
            bounds.x,
            bounds.y + 4.0f,
            0.8f,
            "Widen Details to edit transforms.");
        return result;
    }
    (void)henka_ui_label(
        ui,
        label_bounds.x,
        label_bounds.y + 6.0f,
        0.8f,
        labels[(size_t)row]);
    first_index = (size_t)row * 3U;
    for (axis = 0U; axis < 3U; ++axis)
    {
        char field_id[80];
        bool changed = false;
        snprintf(
            field_id,
            sizeof(field_id),
            "transform_inspector_%llu_%s_%s",
            (unsigned long long)inspector->entity,
            row_ids[(size_t)row],
            axes[axis]);
        if (editable)
        {
            result = henka_ui_text_field(
                ui,
                field_id,
                fields[axis],
                inspector->fields[first_index + axis],
                sizeof(inspector->fields[first_index + axis]),
                &changed);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            inspector->dirty = inspector->dirty || changed;
        }
        else
        {
            (void)henka_ui_label(
                ui,
                fields[axis].x + 4.0f,
                fields[axis].y + 5.0f,
                0.8f,
                inspector->fields[first_index + axis]);
        }
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_transform_inspector_layout_actions(
    henka_ui_rect bounds,
    bool editable,
    henka_ui_rect* out_label,
    henka_ui_rect* out_apply,
    henka_ui_rect* out_revert)
{
    const float button_width = 64.0f;
    const float button_gap = 8.0f;

    if (out_label != NULL)
    {
        *out_label = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    if (out_apply != NULL)
    {
        *out_apply = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    if (out_revert != NULL)
    {
        *out_revert = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    if (out_label == NULL || out_apply == NULL || out_revert == NULL ||
        !sandbox3d_transform_inspector_finite(bounds.x) ||
        !sandbox3d_transform_inspector_finite(bounds.y) ||
        !sandbox3d_transform_inspector_finite(bounds.width) ||
        !sandbox3d_transform_inspector_finite(bounds.height) ||
        bounds.width <= 0.0f || bounds.height < 24.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_label = (henka_ui_rect){bounds.x, bounds.y, bounds.width, 24.0f};
    if (!editable)
    {
        return HENKA_SUCCESS;
    }
    if (bounds.width < 242.0f)
    {
        return HENKA_ERROR_LIMIT;
    }
    out_label->width = bounds.width - (2.0f * button_width + button_gap * 2.0f);
    *out_apply = (henka_ui_rect){
        bounds.x + bounds.width - 2.0f * button_width - button_gap,
        bounds.y,
        button_width,
        24.0f};
    *out_revert = (henka_ui_rect){
        bounds.x + bounds.width - button_width,
        bounds.y,
        button_width,
        24.0f};
    return HENKA_SUCCESS;
}

henka_result sandbox3d_transform_inspector_draw_actions(
    const sandbox3d_transform_inspector* inspector,
    henka_ui_context* ui,
    henka_ui_rect bounds,
    bool editable,
    bool* out_apply,
    bool* out_discard)
{
    henka_ui_rect label_bounds;
    henka_ui_rect apply_bounds;
    henka_ui_rect revert_bounds;
    henka_result result;

    if (out_apply != NULL) *out_apply = false;
    if (out_discard != NULL) *out_discard = false;
    if (inspector == NULL || ui == NULL || out_apply == NULL || out_discard == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_transform_inspector_layout_actions(
        bounds,
        editable,
        &label_bounds,
        &apply_bounds,
        &revert_bounds);
    if (result == HENKA_ERROR_INVALID_ARGUMENT)
    {
        return result;
    }
    (void)henka_ui_label(
        ui,
        label_bounds.x,
        bounds.y + 6.0f,
        0.75f,
        inspector->dirty ? "Pending edits" : "World transform");
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (!editable)
    {
        return HENKA_SUCCESS;
    }
    *out_apply = henka_ui_button(
        ui,
        "transform_inspector_apply",
        apply_bounds,
        "Apply");
    *out_discard = henka_ui_button(
        ui,
        "transform_inspector_discard",
        revert_bounds,
        "Revert");
    return HENKA_SUCCESS;
}
