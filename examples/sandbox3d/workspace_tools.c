#include "workspace_tools.h"

#include <stdio.h>
#include <string.h>

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

static void sandbox3d_workspace_clamp_floating_rect(
    sandbox3d_workspace_panel* panel,
    int framebuffer_width,
    int framebuffer_height)
{
    const float margin = 8.0f;
    float maximum_height;
    float maximum_width;
    float minimum_height;
    float minimum_width;

    if (panel == NULL || framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return;
    }

    maximum_width = (float)framebuffer_width - margin * 2.0f;
    maximum_height = (float)framebuffer_height - margin * 2.0f;
    minimum_width = panel->minimum_width < maximum_width ? panel->minimum_width : maximum_width;
    minimum_height = panel->minimum_height < maximum_height ? panel->minimum_height : maximum_height;
    panel->floating_rect.width = sandbox3d_workspace_clamp_float(
        panel->floating_rect.width,
        minimum_width,
        maximum_width);
    panel->floating_rect.height = sandbox3d_workspace_clamp_float(
        panel->floating_rect.height,
        minimum_height,
        maximum_height);
    panel->floating_rect.x = sandbox3d_workspace_clamp_float(
        panel->floating_rect.x,
        margin,
        (float)framebuffer_width - panel->floating_rect.width - margin);
    panel->floating_rect.y = sandbox3d_workspace_clamp_float(
        panel->floating_rect.y,
        margin,
        (float)framebuffer_height - panel->floating_rect.height - margin);
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
        {820.0f, 94.0f, 396.0f, 560.0f},
        332.0f,
        672.0f,
        4U
    };
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
    if (panel == NULL || dock_zone == SANDBOX3D_WORKSPACE_DOCK_FLOATING)
    {
        return;
    }

    panel->dock = dock_zone;
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

    panel->dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    panel->floating_rect = current_rect;
    panel->floating_rect.width = panel->floating_rect.width > panel->minimum_width ? panel->floating_rect.width : panel->minimum_width;
    panel->floating_rect.height = panel->floating_rect.height > panel->minimum_height ? panel->floating_rect.height : panel->minimum_height;
    sandbox3d_workspace_clamp_floating_rect(panel, framebuffer_width, framebuffer_height);
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
    sandbox3d_workspace_clamp_floating_rect(panel, framebuffer_width, framebuffer_height);
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
    sandbox3d_workspace_clamp_floating_rect(panel, framebuffer_width, framebuffer_height);
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
        case SANDBOX3D_WORKSPACE_DOCK_FLOATING:
        default:
            return "floating";
    }
}
