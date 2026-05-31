#include "workspace_tools.h"

#include <stdio.h>
#include <string.h>

static sandbox3d_workspace_panel_id* sandbox3d_workspace_get_dock_list(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    size_t** out_count)
{
    if (out_count == NULL || model == NULL)
    {
        return NULL;
    }

    switch (dock_zone)
    {
        case SANDBOX3D_WORKSPACE_DOCK_LEFT:
            *out_count = &model->left_dock_panel_count;
            return model->left_dock_panels;
        case SANDBOX3D_WORKSPACE_DOCK_RIGHT:
            *out_count = &model->right_dock_panel_count;
            return model->right_dock_panels;
        case SANDBOX3D_WORKSPACE_DOCK_FLOATING:
        case SANDBOX3D_WORKSPACE_DOCK_DETACHED:
        default:
            return NULL;
    }
}

static const sandbox3d_workspace_panel_id* sandbox3d_workspace_get_dock_list_const(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    size_t* out_count)
{
    if (out_count == NULL || model == NULL)
    {
        return NULL;
    }

    switch (dock_zone)
    {
        case SANDBOX3D_WORKSPACE_DOCK_LEFT:
            *out_count = model->left_dock_panel_count;
            return model->left_dock_panels;
        case SANDBOX3D_WORKSPACE_DOCK_RIGHT:
            *out_count = model->right_dock_panel_count;
            return model->right_dock_panels;
        case SANDBOX3D_WORKSPACE_DOCK_FLOATING:
        case SANDBOX3D_WORKSPACE_DOCK_DETACHED:
        default:
            return NULL;
    }
}

static bool sandbox3d_workspace_dock_contains_panel(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    sandbox3d_workspace_panel_id panel_id)
{
    const sandbox3d_workspace_panel_id* dock_panels;
    size_t count;
    size_t index;

    dock_panels = sandbox3d_workspace_get_dock_list_const(model, dock_zone, &count);
    if (dock_panels == NULL)
    {
        return false;
    }

    for (index = 0U; index < count; ++index)
    {
        if (dock_panels[index] == panel_id)
        {
            return true;
        }
    }

    return false;
}

static void sandbox3d_workspace_remove_panel_from_docks(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id)
{
    sandbox3d_workspace_dock_zone dock_zone;

    if (model == NULL)
    {
        return;
    }

    for (dock_zone = SANDBOX3D_WORKSPACE_DOCK_LEFT;
         dock_zone <= SANDBOX3D_WORKSPACE_DOCK_RIGHT;
         dock_zone = (sandbox3d_workspace_dock_zone)(dock_zone + 1))
    {
        sandbox3d_workspace_panel_id* dock_panels;
        size_t* count;
        size_t index;

        dock_panels = sandbox3d_workspace_get_dock_list(model, dock_zone, &count);
        if (dock_panels == NULL || count == NULL)
        {
            continue;
        }

        for (index = 0U; index < *count; ++index)
        {
            if (dock_panels[index] != panel_id)
            {
                continue;
            }

            for (; index + 1U < *count; ++index)
            {
                dock_panels[index] = dock_panels[index + 1U];
            }
            *count -= 1U;
            dock_panels[*count] = SANDBOX3D_WORKSPACE_PANEL_NONE;
            break;
        }
    }
}

static void sandbox3d_workspace_append_panel_to_dock(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    sandbox3d_workspace_panel_id panel_id)
{
    sandbox3d_workspace_panel_id* dock_panels;
    size_t* count;

    dock_panels = sandbox3d_workspace_get_dock_list(model, dock_zone, &count);
    if (dock_panels == NULL || count == NULL || *count >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
    {
        return;
    }

    if (sandbox3d_workspace_dock_contains_panel(model, dock_zone, panel_id))
    {
        return;
    }

    dock_panels[*count] = panel_id;
    *count += 1U;
}

static float sandbox3d_workspace_clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static void sandbox3d_workspace_enforce_minimum_floating_size(
    sandbox3d_workspace_panel* panel)
{
    if (panel == NULL)
    {
        return;
    }

    if (panel->floating_rect.width < panel->minimum_width)
    {
        panel->floating_rect.width = panel->minimum_width;
    }
    if (panel->floating_rect.height < panel->minimum_height)
    {
        panel->floating_rect.height = panel->minimum_height;
    }
}

void sandbox3d_workspace_model_reset(sandbox3d_workspace_model* model)
{
    if (model == NULL)
    {
        return;
    }

    memset(model, 0, sizeof(*model));
    model->panels[SANDBOX3D_WORKSPACE_PANEL_CONTROLS] = (sandbox3d_workspace_panel)
    {
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_DOCK_MASK_LEFT | SANDBOX3D_WORKSPACE_DOCK_MASK_RIGHT,
        0U,
        {28.0f, 32.0f, 328.0f, 500.0f},
        300.0f,
        470.0f,
        1U
    };
    model->panels[SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS] = (sandbox3d_workspace_panel)
    {
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_DOCK_MASK_LEFT | SANDBOX3D_WORKSPACE_DOCK_MASK_RIGHT,
        0U,
        {46.0f, 138.0f, 300.0f, 242.0f},
        260.0f,
        152.0f,
        2U
    };
    model->panels[SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS] = (sandbox3d_workspace_panel)
    {
        SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        SANDBOX3D_WORKSPACE_DOCK_MASK_LEFT | SANDBOX3D_WORKSPACE_DOCK_MASK_RIGHT,
        0U,
        {868.0f, 38.0f, 356.0f, 404.0f},
        344.0f,
        400.0f,
        3U
    };
    model->panels[SANDBOX3D_WORKSPACE_PANEL_UTILITY] = (sandbox3d_workspace_panel)
    {
        SANDBOX3D_WORKSPACE_PANEL_UTILITY,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        SANDBOX3D_WORKSPACE_DOCK_MASK_LEFT | SANDBOX3D_WORKSPACE_DOCK_MASK_RIGHT,
        0U,
        {820.0f, 94.0f, 396.0f, 560.0f},
        332.0f,
        672.0f,
        4U
    };
    model->left_dock_panels[0] = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
    model->left_dock_panels[1] = SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS;
    model->right_dock_panels[0] = SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS;
    model->right_dock_panels[1] = SANDBOX3D_WORKSPACE_PANEL_UTILITY;
    model->left_dock_panel_count = 2U;
    model->right_dock_panel_count = 2U;
    model->left_dock_width = 320.0f;
    model->right_dock_width = 356.0f;
    model->hovered_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_resize_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    model->active_dock_target = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->next_z_order = 5U;
    snprintf(model->last_action, sizeof(model->last_action), "Layout reset");
}

bool sandbox3d_workspace_should_start_panels_visible(bool settings_file_found)
{
    (void)settings_file_found;
    return true;
}

sandbox3d_workspace_panel* sandbox3d_workspace_get_panel(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id)
{
    if (model == NULL || panel_id < 0 || panel_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
    {
        return NULL;
    }
    return &model->panels[panel_id];
}

const sandbox3d_workspace_panel* sandbox3d_workspace_get_panel_const(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id)
{
    if (model == NULL || panel_id < 0 || panel_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
    {
        return NULL;
    }
    return &model->panels[panel_id];
}

bool sandbox3d_workspace_panel_is_floating(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id)
{
    const sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel_const(model, panel_id);
    return panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING;
}

bool sandbox3d_workspace_panel_is_detached(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id)
{
    const sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel_const(model, panel_id);
    return panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_DETACHED;
}

bool sandbox3d_workspace_panel_allows_dock(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone)
{
    const sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel_const(model, panel_id);
    if (panel == NULL || (dock_zone != SANDBOX3D_WORKSPACE_DOCK_LEFT && dock_zone != SANDBOX3D_WORKSPACE_DOCK_RIGHT))
    {
        return false;
    }
    return (panel->allowed_dock_mask & (1U << dock_zone)) != 0U;
}

size_t sandbox3d_workspace_get_dock_panel_count(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone)
{
    size_t count;

    if (sandbox3d_workspace_get_dock_list_const(model, dock_zone, &count) == NULL)
    {
        return 0U;
    }

    return count;
}

sandbox3d_workspace_panel_id sandbox3d_workspace_get_dock_panel_at(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    size_t index)
{
    const sandbox3d_workspace_panel_id* dock_panels;
    size_t count;

    dock_panels = sandbox3d_workspace_get_dock_list_const(model, dock_zone, &count);
    if (dock_panels == NULL || index >= count)
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }

    return dock_panels[index];
}

void sandbox3d_workspace_detach_panel(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    uint32_t detached_window_id)
{
    sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel(model, panel_id);
    if (panel == NULL || detached_window_id == 0U)
    {
        return;
    }
    if (panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT || panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT)
    {
        panel->last_docked_zone = panel->dock;
    }
    sandbox3d_workspace_remove_panel_from_docks(model, panel_id);
    panel->dock = SANDBOX3D_WORKSPACE_DOCK_DETACHED;
    panel->detached_window_id = detached_window_id;
    model->active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    snprintf(model->last_action, sizeof(model->last_action), "%s detached", sandbox3d_workspace_panel_name(panel_id));
}

void sandbox3d_workspace_bring_to_front(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id)
{
    sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel(model, panel_id);
    if (panel == NULL)
    {
        return;
    }
    panel->z_order = model->next_z_order++;
}

void sandbox3d_workspace_dock_panel(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone)
{
    sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel(model, panel_id);
    if (panel == NULL || !sandbox3d_workspace_panel_allows_dock(model, panel_id, dock_zone))
    {
        return;
    }

    sandbox3d_workspace_remove_panel_from_docks(model, panel_id);
    panel->dock = dock_zone;
    panel->last_docked_zone = dock_zone;
    panel->detached_window_id = 0U;
    sandbox3d_workspace_append_panel_to_dock(model, dock_zone, panel_id);
    model->active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_resize_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s docked %s",
        sandbox3d_workspace_panel_name(panel_id),
        sandbox3d_workspace_dock_name(dock_zone));
}

henka_ui_rect sandbox3d_workspace_title_drag_rect(henka_ui_rect panel_rect)
{
    return (henka_ui_rect){panel_rect.x + 4.0f, panel_rect.y + 2.0f, panel_rect.width - 170.0f, 26.0f};
}

henka_ui_rect sandbox3d_workspace_docked_title_drag_rect(henka_ui_rect panel_rect)
{
    return (henka_ui_rect){panel_rect.x + 4.0f, panel_rect.y + 2.0f, panel_rect.width - 8.0f, 26.0f};
}

henka_ui_rect sandbox3d_workspace_resize_rect(henka_ui_rect panel_rect)
{
    return (henka_ui_rect){panel_rect.x + panel_rect.width - 14.0f, panel_rect.y + panel_rect.height - 14.0f, 14.0f, 14.0f};
}

henka_ui_rect sandbox3d_workspace_left_splitter_rect(henka_ui_rect left_dock, henka_ui_rect scene_frame)
{
    const float x = left_dock.x + left_dock.width + (scene_frame.x - (left_dock.x + left_dock.width) - 6.0f) * 0.5f;
    return (henka_ui_rect){x, scene_frame.y, 6.0f, scene_frame.height};
}

henka_ui_rect sandbox3d_workspace_right_splitter_rect(henka_ui_rect scene_frame, henka_ui_rect right_dock)
{
    const float x = scene_frame.x + scene_frame.width + (right_dock.x - (scene_frame.x + scene_frame.width) - 6.0f) * 0.5f;
    return (henka_ui_rect){x, scene_frame.y, 6.0f, scene_frame.height};
}

void sandbox3d_workspace_begin_panel_drag(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    henka_vec2 pointer)
{
    sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel(model, panel_id);
    if (panel == NULL || panel->dock != SANDBOX3D_WORKSPACE_DOCK_FLOATING)
    {
        return;
    }

    sandbox3d_workspace_bring_to_front(model, panel_id);
    model->active_drag_panel = panel_id;
    model->drag_offset.x = pointer.x - panel->floating_rect.x;
    model->drag_offset.y = pointer.y - panel->floating_rect.y;
    snprintf(model->last_action, sizeof(model->last_action), "Moving %s", sandbox3d_workspace_panel_name(panel_id));
}

void sandbox3d_workspace_begin_docked_panel_drag(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    henka_ui_rect current_rect,
    henka_vec2 pointer,
    int framebuffer_width,
    int framebuffer_height)
{
    sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel(model, panel_id);
    if (panel == NULL || panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING)
    {
        return;
    }

    panel->floating_rect = current_rect;
    (void)framebuffer_width;
    (void)framebuffer_height;
    sandbox3d_workspace_enforce_minimum_floating_size(panel);
    sandbox3d_workspace_remove_panel_from_docks(model, panel_id);
    panel->dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    sandbox3d_workspace_bring_to_front(model, panel_id);
    model->active_drag_panel = panel_id;
    model->drag_offset.x = pointer.x - panel->floating_rect.x;
    model->drag_offset.y = pointer.y - panel->floating_rect.y;
    snprintf(model->last_action, sizeof(model->last_action), "Dragging %s from dock", sandbox3d_workspace_panel_name(panel_id));
}

void sandbox3d_workspace_update_panel_drag(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    int framebuffer_width,
    int framebuffer_height)
{
    sandbox3d_workspace_panel* panel;
    if (model == NULL || model->active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_NONE)
    {
        return;
    }

    panel = sandbox3d_workspace_get_panel(model, model->active_drag_panel);
    if (panel == NULL)
    {
        return;
    }
    panel->floating_rect.x = pointer.x - model->drag_offset.x;
    panel->floating_rect.y = pointer.y - model->drag_offset.y;
    (void)framebuffer_width;
    (void)framebuffer_height;
    sandbox3d_workspace_enforce_minimum_floating_size(panel);
}

void sandbox3d_workspace_begin_panel_resize(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    henka_vec2 pointer)
{
    sandbox3d_workspace_panel* panel = sandbox3d_workspace_get_panel(model, panel_id);
    if (panel == NULL || panel->dock != SANDBOX3D_WORKSPACE_DOCK_FLOATING)
    {
        return;
    }

    sandbox3d_workspace_bring_to_front(model, panel_id);
    model->active_resize_panel = panel_id;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_FLOATING_PANEL;
    model->resize_start_mouse = pointer;
    model->resize_start_rect = panel->floating_rect;
    snprintf(model->last_action, sizeof(model->last_action), "Resizing %s", sandbox3d_workspace_panel_name(panel_id));
}

void sandbox3d_workspace_update_panel_resize(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    int framebuffer_width,
    int framebuffer_height)
{
    sandbox3d_workspace_panel* panel;
    if (model == NULL ||
        model->resize_target != SANDBOX3D_WORKSPACE_RESIZE_FLOATING_PANEL ||
        model->active_resize_panel == SANDBOX3D_WORKSPACE_PANEL_NONE)
    {
        return;
    }
    panel = sandbox3d_workspace_get_panel(model, model->active_resize_panel);
    if (panel == NULL)
    {
        return;
    }
    panel->floating_rect.width = model->resize_start_rect.width + pointer.x - model->resize_start_mouse.x;
    panel->floating_rect.height = model->resize_start_rect.height + pointer.y - model->resize_start_mouse.y;
    (void)framebuffer_width;
    (void)framebuffer_height;
    sandbox3d_workspace_enforce_minimum_floating_size(panel);
}

void sandbox3d_workspace_begin_dock_resize(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_resize_target target,
    henka_vec2 pointer)
{
    if (model == NULL ||
        (target != SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK &&
         target != SANDBOX3D_WORKSPACE_RESIZE_RIGHT_DOCK))
    {
        return;
    }
    model->resize_target = target;
    model->resize_start_mouse = pointer;
    model->resize_start_width = target == SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK
        ? model->left_dock_width
        : model->right_dock_width;
    snprintf(model->last_action, sizeof(model->last_action), "%s dock resizing", target == SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK ? "Left" : "Right");
}

void sandbox3d_workspace_update_dock_resize(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    int framebuffer_width,
    float minimum_scene_width,
    float minimum_dock_width,
    float reserved_other_dock_width)
{
    const float maximum_dock_width = (float)framebuffer_width - minimum_scene_width - reserved_other_dock_width - 70.0f;
    float delta;
    if (model == NULL || maximum_dock_width < minimum_dock_width)
    {
        return;
    }

    delta = pointer.x - model->resize_start_mouse.x;
    if (model->resize_target == SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK)
    {
        model->left_dock_width = sandbox3d_workspace_clamp_float(model->resize_start_width + delta, minimum_dock_width, maximum_dock_width);
    }
    else if (model->resize_target == SANDBOX3D_WORKSPACE_RESIZE_RIGHT_DOCK)
    {
        model->right_dock_width = sandbox3d_workspace_clamp_float(model->resize_start_width - delta, minimum_dock_width, maximum_dock_width);
    }
}

void sandbox3d_workspace_end_interaction(sandbox3d_workspace_model* model)
{
    if (model == NULL)
    {
        return;
    }
    model->active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_resize_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    model->active_dock_target = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
}

sandbox3d_workspace_dock_zone sandbox3d_workspace_evaluate_dock_zone(
    henka_vec2 pointer,
    henka_ui_rect left_dock,
    henka_ui_rect scene_frame,
    henka_ui_rect right_dock,
    float dock_margin)
{
    (void)scene_frame;

    if (left_dock.width > 0.0f &&
        pointer.x >= left_dock.x - dock_margin &&
        pointer.x < left_dock.x + left_dock.width + dock_margin &&
        pointer.y >= left_dock.y &&
        pointer.y < left_dock.y + left_dock.height)
    {
        return SANDBOX3D_WORKSPACE_DOCK_LEFT;
    }

    if (right_dock.width > 0.0f &&
        pointer.x >= right_dock.x - dock_margin &&
        pointer.x < right_dock.x + right_dock.width + dock_margin &&
        pointer.y >= right_dock.y &&
        pointer.y < right_dock.y + right_dock.height)
    {
        return SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    }

    return SANDBOX3D_WORKSPACE_DOCK_FLOATING;
}

const char* sandbox3d_workspace_panel_name(sandbox3d_workspace_panel_id panel_id)
{
    switch (panel_id)
    {
        case SANDBOX3D_WORKSPACE_PANEL_CONTROLS:
            return "Controls";
        case SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS:
            return "Scene Objects";
        case SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS:
            return "Object Details";
        case SANDBOX3D_WORKSPACE_PANEL_UTILITY:
            return "Utility";
        case SANDBOX3D_WORKSPACE_PANEL_NONE:
        default:
            return "None";
    }
}

const char* sandbox3d_workspace_dock_name(sandbox3d_workspace_dock_zone dock_zone)
{
    switch (dock_zone)
    {
        case SANDBOX3D_WORKSPACE_DOCK_LEFT:
            return "left";
        case SANDBOX3D_WORKSPACE_DOCK_RIGHT:
            return "right";
        case SANDBOX3D_WORKSPACE_DOCK_DETACHED:
            return "detached";
        case SANDBOX3D_WORKSPACE_DOCK_FLOATING:
        default:
            return "floating";
    }
}
