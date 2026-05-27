#ifndef SANDBOX3D_WORKSPACE_TOOLS_H
#define SANDBOX3D_WORKSPACE_TOOLS_H

#include <stdbool.h>

#include <henka/math.h>
#include <henka/ui.h>

typedef enum sandbox3d_workspace_panel_id
{
    SANDBOX3D_WORKSPACE_PANEL_NONE = -1,
    SANDBOX3D_WORKSPACE_PANEL_CONTROLS = 0,
    SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS,
    SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS,
    SANDBOX3D_WORKSPACE_PANEL_UTILITY,
    SANDBOX3D_WORKSPACE_PANEL_COUNT
} sandbox3d_workspace_panel_id;

typedef enum sandbox3d_workspace_dock_zone
{
    SANDBOX3D_WORKSPACE_DOCK_LEFT = 0,
    SANDBOX3D_WORKSPACE_DOCK_RIGHT,
    SANDBOX3D_WORKSPACE_DOCK_FLOATING
} sandbox3d_workspace_dock_zone;

typedef enum sandbox3d_workspace_resize_target
{
    SANDBOX3D_WORKSPACE_RESIZE_NONE = 0,
    SANDBOX3D_WORKSPACE_RESIZE_FLOATING_PANEL,
    SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK,
    SANDBOX3D_WORKSPACE_RESIZE_RIGHT_DOCK
} sandbox3d_workspace_resize_target;

typedef struct sandbox3d_workspace_panel
{
    sandbox3d_workspace_panel_id id;
    sandbox3d_workspace_dock_zone default_dock;
    sandbox3d_workspace_dock_zone dock;
    henka_ui_rect floating_rect;
    float minimum_width;
    float minimum_height;
    unsigned int z_order;
} sandbox3d_workspace_panel;

typedef struct sandbox3d_workspace_model
{
    sandbox3d_workspace_panel panels[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    float left_dock_width;
    float right_dock_width;
    sandbox3d_workspace_panel_id hovered_panel;
    sandbox3d_workspace_panel_id active_drag_panel;
    sandbox3d_workspace_panel_id active_resize_panel;
    sandbox3d_workspace_resize_target resize_target;
    sandbox3d_workspace_dock_zone active_dock_target;
    henka_vec2 drag_offset;
    henka_vec2 resize_start_mouse;
    henka_ui_rect resize_start_rect;
    float resize_start_width;
    unsigned int next_z_order;
    char last_action[128];
} sandbox3d_workspace_model;

void sandbox3d_workspace_model_reset(sandbox3d_workspace_model* model);
bool sandbox3d_workspace_should_start_panels_visible(bool settings_file_found);
sandbox3d_workspace_panel* sandbox3d_workspace_get_panel(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id);
const sandbox3d_workspace_panel* sandbox3d_workspace_get_panel_const(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id);
bool sandbox3d_workspace_panel_is_floating(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id);
void sandbox3d_workspace_bring_to_front(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id);
void sandbox3d_workspace_dock_panel(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone);
henka_ui_rect sandbox3d_workspace_docked_title_drag_rect(henka_ui_rect panel_rect);
henka_ui_rect sandbox3d_workspace_title_drag_rect(henka_ui_rect panel_rect);
henka_ui_rect sandbox3d_workspace_resize_rect(henka_ui_rect panel_rect);
henka_ui_rect sandbox3d_workspace_left_splitter_rect(henka_ui_rect left_dock, henka_ui_rect scene_frame);
henka_ui_rect sandbox3d_workspace_right_splitter_rect(henka_ui_rect scene_frame, henka_ui_rect right_dock);
void sandbox3d_workspace_begin_panel_drag(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    henka_vec2 pointer);
void sandbox3d_workspace_begin_docked_panel_drag(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    henka_ui_rect current_rect,
    henka_vec2 pointer,
    int framebuffer_width,
    int framebuffer_height);
void sandbox3d_workspace_update_panel_drag(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    int framebuffer_width,
    int framebuffer_height);
void sandbox3d_workspace_begin_panel_resize(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    henka_vec2 pointer);
void sandbox3d_workspace_update_panel_resize(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    int framebuffer_width,
    int framebuffer_height);
void sandbox3d_workspace_begin_dock_resize(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_resize_target target,
    henka_vec2 pointer);
void sandbox3d_workspace_update_dock_resize(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    int framebuffer_width,
    float minimum_scene_width,
    float minimum_dock_width,
    float reserved_other_dock_width);
void sandbox3d_workspace_end_interaction(sandbox3d_workspace_model* model);
sandbox3d_workspace_dock_zone sandbox3d_workspace_evaluate_dock_zone(
    henka_vec2 pointer,
    henka_ui_rect left_dock,
    henka_ui_rect scene_frame,
    henka_ui_rect right_dock,
    float dock_margin);
const char* sandbox3d_workspace_panel_name(sandbox3d_workspace_panel_id panel_id);
const char* sandbox3d_workspace_dock_name(sandbox3d_workspace_dock_zone dock_zone);

#endif
