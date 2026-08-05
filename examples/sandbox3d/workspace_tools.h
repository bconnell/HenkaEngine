#ifndef SANDBOX3D_WORKSPACE_TOOLS_H
#define SANDBOX3D_WORKSPACE_TOOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    SANDBOX3D_WORKSPACE_DOCK_FLOATING,
    SANDBOX3D_WORKSPACE_DOCK_DETACHED
} sandbox3d_workspace_dock_zone;

#define SANDBOX3D_WORKSPACE_DOCK_MASK_LEFT (1U << SANDBOX3D_WORKSPACE_DOCK_LEFT)
#define SANDBOX3D_WORKSPACE_DOCK_MASK_RIGHT (1U << SANDBOX3D_WORKSPACE_DOCK_RIGHT)

#define SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES 16U
#define SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS SANDBOX3D_WORKSPACE_PANEL_COUNT
#define SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH 10.0f
#define SANDBOX3D_WORKSPACE_DIVIDER_CLOSE_THRESHOLD 32.0f
#define SANDBOX3D_WORKSPACE_UI_SCALE_MIN 0.75f
#define SANDBOX3D_WORKSPACE_UI_SCALE_MAX 4.0f
#define SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_NAME_MAX 32U
#define SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX 8U

typedef enum sandbox3d_workspace_split_orientation
{
    SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL = 0,
    SANDBOX3D_WORKSPACE_SPLIT_VERTICAL
} sandbox3d_workspace_split_orientation;

typedef enum sandbox3d_workspace_named_layout
{
    SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT = 0,
    SANDBOX3D_WORKSPACE_LAYOUT_MODELING,
    SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS,
    SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY,
    SANDBOX3D_WORKSPACE_LAYOUT_DEBUGGING,
    SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT,
    SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM,
    SANDBOX3D_WORKSPACE_LAYOUT_COUNT
} sandbox3d_workspace_named_layout;

typedef enum sandbox3d_workspace_topology_node_type
{
    SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED = 0,
    SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT,
    SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION
} sandbox3d_workspace_topology_node_type;

typedef struct sandbox3d_workspace_topology_node
{
    sandbox3d_workspace_topology_node_type type;
    uint16_t parent;
    union
    {
        struct
        {
            uint16_t first_child;
            uint16_t second_child;
            sandbox3d_workspace_split_orientation orientation;
            float ratio;
            float minimum_first;
            float minimum_second;
        } split;
        struct
        {
            sandbox3d_workspace_panel_id section_id;
            sandbox3d_workspace_panel_id tabs[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS];
            uint8_t tab_count;
            uint8_t active_tab;
        } section;
    } data;
} sandbox3d_workspace_topology_node;

typedef struct sandbox3d_workspace_topology_layout
{
    henka_ui_rect section_rects[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    henka_ui_rect divider_visual_rects[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    henka_ui_rect divider_hit_rects[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t divider_node_indices[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    size_t divider_count;
} sandbox3d_workspace_topology_layout;

typedef enum sandbox3d_workspace_context_command
{
    SANDBOX3D_WORKSPACE_CONTEXT_OPEN_HORIZONTAL = 0,
    SANDBOX3D_WORKSPACE_CONTEXT_OPEN_VERTICAL,
    SANDBOX3D_WORKSPACE_CONTEXT_CLOSE_SECTION,
    SANDBOX3D_WORKSPACE_CONTEXT_MERGE_ADJACENT,
    SANDBOX3D_WORKSPACE_CONTEXT_EQUALIZE,
    SANDBOX3D_WORKSPACE_CONTEXT_MAXIMIZE,
    SANDBOX3D_WORKSPACE_CONTEXT_DETACH,
    SANDBOX3D_WORKSPACE_CONTEXT_MOVE_TO_TAB_GROUP,
    SANDBOX3D_WORKSPACE_CONTEXT_RESTORE_LAST_CLOSED,
    SANDBOX3D_WORKSPACE_CONTEXT_COMMAND_COUNT
} sandbox3d_workspace_context_command;

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
    sandbox3d_workspace_dock_zone last_docked_zone;
    unsigned int allowed_dock_mask;
    uint32_t detached_window_id;
    henka_ui_rect floating_rect;
    float minimum_width;
    float minimum_height;
    unsigned int z_order;
} sandbox3d_workspace_panel;

typedef struct sandbox3d_workspace_layout_history_state
{
    sandbox3d_workspace_panel panels[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    sandbox3d_workspace_panel_id left_dock_panels[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    sandbox3d_workspace_panel_id right_dock_panels[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    size_t left_dock_panel_count;
    size_t right_dock_panel_count;
    float left_dock_width;
    float right_dock_width;
    float ui_scale;
    sandbox3d_workspace_topology_node topology_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t topology_root;
    sandbox3d_workspace_named_layout named_layout;
    uint32_t closed_sections_mask;
    sandbox3d_workspace_panel_id maximized_section;
    bool closed_snapshot_valid;
    sandbox3d_workspace_topology_node closed_snapshot_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t closed_snapshot_root;
    uint32_t closed_snapshot_mask;
} sandbox3d_workspace_layout_history_state;

typedef struct sandbox3d_workspace_model
{
    sandbox3d_workspace_panel panels[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    sandbox3d_workspace_panel_id left_dock_panels[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    sandbox3d_workspace_panel_id right_dock_panels[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    size_t left_dock_panel_count;
    size_t right_dock_panel_count;
    float left_dock_width;
    float right_dock_width;
    float ui_scale;
    sandbox3d_workspace_panel_id hovered_panel;
    sandbox3d_workspace_panel_id keyboard_focus_panel;
    sandbox3d_workspace_panel_id active_drag_panel;
    sandbox3d_workspace_panel_id drag_start_section;
    sandbox3d_workspace_dock_zone drag_start_dock;
    sandbox3d_workspace_panel_id tab_drop_target;
    bool drag_origin_valid;
    sandbox3d_workspace_panel_id active_tab_drag_section;
    sandbox3d_workspace_panel_id active_tab_drag_tab;
    size_t active_tab_drag_target_index;
    bool tab_drag_active;
    sandbox3d_workspace_panel_id active_resize_panel;
    sandbox3d_workspace_resize_target resize_target;
    sandbox3d_workspace_dock_zone active_dock_target;
    henka_vec2 drag_offset;
    henka_vec2 resize_start_mouse;
    henka_ui_rect resize_start_rect;
    float resize_start_width;
    unsigned int next_z_order;
    uint16_t topology_root;
    sandbox3d_workspace_topology_node topology_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    bool topology_transaction_active;
    sandbox3d_workspace_topology_node topology_transaction_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t topology_transaction_root;
    sandbox3d_workspace_named_layout named_layout;
    sandbox3d_workspace_named_layout topology_transaction_named_layout;
    sandbox3d_workspace_named_layout topology_transaction_result_named_layout;
    sandbox3d_workspace_layout_history_state topology_transaction_state;
    sandbox3d_workspace_layout_history_state undo_history[SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX];
    sandbox3d_workspace_layout_history_state undo_after_history[SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX];
    sandbox3d_workspace_layout_history_state redo_history[SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX];
    size_t undo_history_count;
    size_t redo_history_count;
    bool custom_layout_valid;
    char custom_layout_name[SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_NAME_MAX];
    sandbox3d_workspace_topology_node custom_layout_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t custom_layout_root;
    uint32_t custom_layout_closed_sections_mask;
    sandbox3d_workspace_panel_id custom_layout_maximized_section;
    sandbox3d_workspace_dock_zone custom_layout_docks[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    sandbox3d_workspace_dock_zone custom_layout_last_docked_zones[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    float custom_layout_left_dock_width;
    float custom_layout_right_dock_width;
    float custom_layout_ui_scale;
    uint16_t active_divider_node;
    sandbox3d_workspace_dock_zone active_divider_dock;
    float active_divider_start_ratio;
    henka_vec2 active_divider_start_pointer;
    bool divider_close_preview;
    sandbox3d_workspace_panel_id divider_close_section;
    uint32_t closed_sections_mask;
    sandbox3d_workspace_panel_id maximized_section;
    bool closed_snapshot_valid;
    sandbox3d_workspace_topology_node closed_snapshot_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t closed_snapshot_root;
    uint32_t closed_snapshot_mask;
    bool context_menu_open;
    sandbox3d_workspace_panel_id context_menu_section;
    size_t context_menu_selected_command;
    henka_ui_rect context_menu_rect;
    bool section_chooser_open;
    sandbox3d_workspace_panel_id section_chooser_source;
    sandbox3d_workspace_split_orientation section_chooser_orientation;
    henka_ui_rect section_chooser_rect;
    char last_action[128];
} sandbox3d_workspace_model;

void sandbox3d_workspace_model_reset(sandbox3d_workspace_model* model);
void sandbox3d_workspace_reset_layout(sandbox3d_workspace_model* model);
void sandbox3d_workspace_set_ui_scale(
    sandbox3d_workspace_model* model,
    float ui_scale);
float sandbox3d_workspace_get_ui_scale(
    const sandbox3d_workspace_model* model);
const char* sandbox3d_workspace_named_layout_label(
    sandbox3d_workspace_named_layout layout);
const char* sandbox3d_workspace_named_layout_setting_value(
    sandbox3d_workspace_named_layout layout);
sandbox3d_workspace_named_layout sandbox3d_workspace_parse_named_layout(
    const char* value);
sandbox3d_workspace_named_layout sandbox3d_workspace_get_named_layout(
    const sandbox3d_workspace_model* model);
bool sandbox3d_workspace_apply_named_layout(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_named_layout layout);
bool sandbox3d_workspace_save_custom_layout(
    sandbox3d_workspace_model* model,
    const char* name);
bool sandbox3d_workspace_has_custom_layout(
    const sandbox3d_workspace_model* model);
const char* sandbox3d_workspace_custom_layout_name(
    const sandbox3d_workspace_model* model);
bool sandbox3d_workspace_apply_custom_layout(
    sandbox3d_workspace_model* model);
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
bool sandbox3d_workspace_panel_is_detached(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id);
bool sandbox3d_workspace_panel_allows_dock(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone);
size_t sandbox3d_workspace_get_dock_panel_count(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone);
sandbox3d_workspace_panel_id sandbox3d_workspace_get_dock_panel_at(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    size_t index);
void sandbox3d_workspace_detach_panel(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    uint32_t detached_window_id);
void sandbox3d_workspace_bring_to_front(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id);
void sandbox3d_workspace_dock_panel(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone);
void sandbox3d_workspace_rebuild_dock_lists(sandbox3d_workspace_model* model);
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
void sandbox3d_workspace_cancel_panel_drag(sandbox3d_workspace_model* model);
sandbox3d_workspace_topology_node* sandbox3d_workspace_topology_get_node(
    sandbox3d_workspace_model* model,
    uint16_t node_index);
const sandbox3d_workspace_topology_node* sandbox3d_workspace_topology_get_node_const(
    const sandbox3d_workspace_model* model,
    uint16_t node_index);
bool sandbox3d_workspace_topology_is_valid(const sandbox3d_workspace_model* model);
void sandbox3d_workspace_build_topology_layout(
    const sandbox3d_workspace_model* model,
    henka_ui_rect bounds,
    sandbox3d_workspace_topology_layout* out_layout);
void sandbox3d_workspace_build_dock_topology_layout(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    henka_ui_rect bounds,
    sandbox3d_workspace_topology_layout* out_layout);
henka_ui_rect sandbox3d_workspace_topology_divider_hit_rect(
    henka_ui_rect divider_rect,
    sandbox3d_workspace_split_orientation orientation);
void sandbox3d_workspace_begin_divider_drag(
    sandbox3d_workspace_model* model,
    uint16_t node_index,
    henka_vec2 pointer);
void sandbox3d_workspace_begin_dock_divider_drag(
    sandbox3d_workspace_model* model,
    uint16_t node_index,
    henka_vec2 pointer,
    sandbox3d_workspace_dock_zone dock_zone);
void sandbox3d_workspace_update_divider_drag(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    henka_ui_rect bounds);
bool sandbox3d_workspace_divider_close_preview(
    const sandbox3d_workspace_model* model);
sandbox3d_workspace_panel_id sandbox3d_workspace_divider_close_section(
    const sandbox3d_workspace_model* model);
void sandbox3d_workspace_begin_topology_transaction(sandbox3d_workspace_model* model);
void sandbox3d_workspace_commit_topology_transaction(sandbox3d_workspace_model* model);
void sandbox3d_workspace_rollback_topology_transaction(sandbox3d_workspace_model* model);
bool sandbox3d_workspace_can_undo(const sandbox3d_workspace_model* model);
bool sandbox3d_workspace_can_redo(const sandbox3d_workspace_model* model);
bool sandbox3d_workspace_undo(sandbox3d_workspace_model* model);
bool sandbox3d_workspace_redo(sandbox3d_workspace_model* model);
bool sandbox3d_workspace_topology_section_has_tab(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id);
size_t sandbox3d_workspace_get_topology_dock_section_count(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone);
sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_dock_section_at(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    size_t index);
sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_section_for_tab(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id tab_id);
size_t sandbox3d_workspace_get_topology_section_tab_count(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id);
sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_section_tab_at(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    size_t index);
sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_section_active_tab(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id);
bool sandbox3d_workspace_set_topology_section_active_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id);
bool sandbox3d_workspace_cycle_topology_section_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    int direction);
bool sandbox3d_workspace_close_section(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id);

/* Closes the active tab; closing the final tab removes the containing section. */
bool sandbox3d_workspace_close_active_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id);
bool sandbox3d_workspace_restore_last_closed_section(sandbox3d_workspace_model* model);
bool sandbox3d_workspace_merge_sections(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id target_section,
    sandbox3d_workspace_panel_id source_section);
bool sandbox3d_workspace_move_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id target_section,
    sandbox3d_workspace_panel_id tab_id);
bool sandbox3d_workspace_begin_tab_drag(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id);
void sandbox3d_workspace_update_tab_drag(
    sandbox3d_workspace_model* model,
    size_t target_index);
bool sandbox3d_workspace_commit_tab_drag(sandbox3d_workspace_model* model);
void sandbox3d_workspace_cancel_tab_drag(sandbox3d_workspace_model* model);
bool sandbox3d_workspace_reorder_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id,
    size_t target_index);
bool sandbox3d_workspace_split_section(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id target_section,
    sandbox3d_workspace_split_orientation orientation,
    sandbox3d_workspace_panel_id new_section);
void sandbox3d_workspace_equalize_sections(sandbox3d_workspace_model* model);
void sandbox3d_workspace_set_maximized_section(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id);
void sandbox3d_workspace_restore_maximized_section(sandbox3d_workspace_model* model);
bool sandbox3d_workspace_section_is_closed(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id);
const char* sandbox3d_workspace_context_command_label(
    sandbox3d_workspace_context_command command);
sandbox3d_workspace_dock_zone sandbox3d_workspace_evaluate_dock_zone(
    henka_vec2 pointer,
    henka_ui_rect left_dock,
    henka_ui_rect scene_frame,
    henka_ui_rect right_dock,
    float dock_margin);
const char* sandbox3d_workspace_panel_name(sandbox3d_workspace_panel_id panel_id);
const char* sandbox3d_workspace_dock_name(sandbox3d_workspace_dock_zone dock_zone);

#endif
