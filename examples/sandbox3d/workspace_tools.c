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

static void sandbox3d_workspace_topology_clear(
    sandbox3d_workspace_model* model)
{
    size_t index;

    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++index)
    {
        model->topology_nodes[index].type = SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED;
        model->topology_nodes[index].parent = UINT16_MAX;
    }
    model->topology_root = UINT16_MAX;
    model->topology_transaction_active = false;
    model->topology_transaction_root = UINT16_MAX;
    model->active_divider_node = UINT16_MAX;
    model->active_divider_start_ratio = 0.0f;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->closed_sections_mask = 0U;
    model->maximized_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->closed_snapshot_valid = false;
    model->closed_snapshot_root = UINT16_MAX;
    model->closed_snapshot_mask = 0U;
    model->context_menu_open = false;
    model->context_menu_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->context_menu_rect = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    model->section_chooser_open = false;
    model->section_chooser_source = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->section_chooser_orientation = SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL;
    model->section_chooser_rect = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
}

static void sandbox3d_workspace_topology_make_section(
    sandbox3d_workspace_topology_node* node,
    sandbox3d_workspace_panel_id section_id)
{
    size_t index;

    memset(node, 0, sizeof(*node));
    node->type = SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION;
    node->parent = UINT16_MAX;
    node->data.section.section_id = section_id;
    node->data.section.tab_count = 1U;
    node->data.section.active_tab = 0U;
    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS; ++index)
    {
        node->data.section.tabs[index] = SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    node->data.section.tabs[0] = section_id;
}

static void sandbox3d_workspace_topology_make_split(
    sandbox3d_workspace_topology_node* node,
    uint16_t parent,
    uint16_t first_child,
    uint16_t second_child,
    sandbox3d_workspace_split_orientation orientation,
    float ratio)
{
    memset(node, 0, sizeof(*node));
    node->type = SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT;
    node->parent = parent;
    node->data.split.first_child = first_child;
    node->data.split.second_child = second_child;
    node->data.split.orientation = orientation;
    node->data.split.ratio = ratio;
    node->data.split.minimum_first = 180.0f;
    node->data.split.minimum_second = 180.0f;
}

static void sandbox3d_workspace_topology_initialize(
    sandbox3d_workspace_model* model)
{
    sandbox3d_workspace_topology_clear(model);
    model->topology_root = 0U;
    sandbox3d_workspace_topology_make_split(
        &model->topology_nodes[0],
        UINT16_MAX,
        1U,
        4U,
        SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL,
        0.5f);
    sandbox3d_workspace_topology_make_split(
        &model->topology_nodes[1],
        0U,
        2U,
        3U,
        SANDBOX3D_WORKSPACE_SPLIT_VERTICAL,
        0.5f);
    sandbox3d_workspace_topology_make_section(
        &model->topology_nodes[2], SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    sandbox3d_workspace_topology_make_section(
        &model->topology_nodes[3], SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    sandbox3d_workspace_topology_make_split(
        &model->topology_nodes[4],
        0U,
        5U,
        6U,
        SANDBOX3D_WORKSPACE_SPLIT_VERTICAL,
        0.5f);
    sandbox3d_workspace_topology_make_section(
        &model->topology_nodes[5], SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
    sandbox3d_workspace_topology_make_section(
        &model->topology_nodes[6], SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    model->topology_nodes[2].parent = 1U;
    model->topology_nodes[3].parent = 1U;
    model->topology_nodes[4].parent = 0U;
    model->topology_nodes[5].parent = 4U;
    model->topology_nodes[6].parent = 4U;
}

static bool sandbox3d_workspace_topology_visit(
    const sandbox3d_workspace_model* model,
    uint16_t node_index,
    uint16_t parent,
    bool* section_seen,
    bool* node_seen,
    bool* tab_seen,
    size_t* visited_count)
{
    const sandbox3d_workspace_topology_node* node;

    if (node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        *visited_count >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        node_seen[node_index])
    {
        return false;
    }
    node = &model->topology_nodes[node_index];
    if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED || node->parent != parent)
    {
        return false;
    }
    *visited_count += 1U;
    node_seen[node_index] = true;
    if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        size_t tab_index;
        if (node->data.section.section_id < 0 ||
            node->data.section.section_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
            node->data.section.tab_count == 0U ||
            node->data.section.tab_count > SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS ||
            node->data.section.active_tab >= node->data.section.tab_count)
        {
            return false;
        }
        section_seen[node->data.section.section_id] = true;
        for (tab_index = 0U; tab_index < node->data.section.tab_count; ++tab_index)
        {
            const sandbox3d_workspace_panel_id tab = node->data.section.tabs[tab_index];
            if (tab < 0 || tab >= SANDBOX3D_WORKSPACE_PANEL_COUNT || tab_seen[tab])
            {
                return false;
            }
            tab_seen[tab] = true;
        }
        return true;
    }
    if (node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT ||
        node->data.split.first_child >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        node->data.split.second_child >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        node->data.split.first_child == node->data.split.second_child ||
        node->data.split.ratio < 0.0f || node->data.split.ratio > 1.0f ||
        node->data.split.minimum_first < 0.0f || node->data.split.minimum_second < 0.0f)
    {
        return false;
    }
    return sandbox3d_workspace_topology_visit(
               model, node->data.split.first_child, node_index, section_seen, node_seen, tab_seen, visited_count) &&
        sandbox3d_workspace_topology_visit(
            model, node->data.split.second_child, node_index, section_seen, node_seen, tab_seen, visited_count);
}

static henka_ui_rect sandbox3d_workspace_topology_zero_rect(void)
{
    return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
}

static bool sandbox3d_workspace_topology_layout_node(
    const sandbox3d_workspace_model* model,
    uint16_t node_index,
    henka_ui_rect bounds,
    sandbox3d_workspace_topology_layout* out_layout)
{
    const sandbox3d_workspace_topology_node* node;

    if (node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
    {
        return false;
    }
    node = &model->topology_nodes[node_index];
    if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        out_layout->section_rects[node->data.section.section_id] = bounds;
        return true;
    }
    if (node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return false;
    }

    {
        const float divider = 1.0f;
        const float available = (node->data.split.orientation == SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL
            ? bounds.width
            : bounds.height) - divider;
        float first_extent;
        henka_ui_rect first = bounds;
        henka_ui_rect second = bounds;
        henka_ui_rect visual;
        const size_t divider_index = out_layout->divider_count;
        if (available <= 0.0f || divider_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
        {
            return false;
        }
        first_extent = sandbox3d_workspace_clamp_float(
            available * node->data.split.ratio,
            node->data.split.minimum_first,
            available - node->data.split.minimum_second);
        if (node->data.split.orientation == SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL)
        {
            first.width = first_extent;
            second.x += first_extent + divider;
            second.width -= first_extent + divider;
            visual = (henka_ui_rect){second.x - divider, bounds.y, divider, bounds.height};
        }
        else
        {
            first.height = first_extent;
            second.y += first_extent + divider;
            second.height -= first_extent + divider;
            visual = (henka_ui_rect){bounds.x, second.y - divider, bounds.width, divider};
        }
        out_layout->divider_visual_rects[divider_index] = visual;
        out_layout->divider_hit_rects[divider_index] =
            sandbox3d_workspace_topology_divider_hit_rect(visual, node->data.split.orientation);
        out_layout->divider_count += 1U;
        return sandbox3d_workspace_topology_layout_node(
                   model, node->data.split.first_child, first, out_layout) &&
            sandbox3d_workspace_topology_layout_node(
                model, node->data.split.second_child, second, out_layout);
    }
}

static bool sandbox3d_workspace_topology_find_node_rect(
    const sandbox3d_workspace_model* model,
    uint16_t node_index,
    henka_ui_rect bounds,
    uint16_t target,
    henka_ui_rect* out_rect)
{
    const sandbox3d_workspace_topology_node* node;
    if (node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
    {
        return false;
    }
    if (node_index == target)
    {
        *out_rect = bounds;
        return true;
    }
    node = &model->topology_nodes[node_index];
    if (node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return false;
    }
    {
        const float divider = 1.0f;
        const float available = (node->data.split.orientation == SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL
            ? bounds.width
            : bounds.height) - divider;
        const float first_extent = sandbox3d_workspace_clamp_float(
            available * node->data.split.ratio,
            node->data.split.minimum_first,
            available - node->data.split.minimum_second);
        henka_ui_rect first = bounds;
        henka_ui_rect second = bounds;
        if (node->data.split.orientation == SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL)
        {
            first.width = first_extent;
            second.x += first_extent + divider;
            second.width -= first_extent + divider;
        }
        else
        {
            first.height = first_extent;
            second.y += first_extent + divider;
            second.height -= first_extent + divider;
        }
        return sandbox3d_workspace_topology_find_node_rect(
                   model, node->data.split.first_child, first, target, out_rect) ||
            sandbox3d_workspace_topology_find_node_rect(
                model, node->data.split.second_child, second, target, out_rect);
    }
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
    sandbox3d_workspace_topology_initialize(model);
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

void sandbox3d_workspace_rebuild_dock_lists(sandbox3d_workspace_model* model)
{
    size_t index;

    if (model == NULL)
    {
        return;
    }
    model->left_dock_panel_count = 0U;
    model->right_dock_panel_count = 0U;
    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        const sandbox3d_workspace_dock_zone dock = model->panels[index].dock;
        if (dock == SANDBOX3D_WORKSPACE_DOCK_LEFT || dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT)
        {
            sandbox3d_workspace_append_panel_to_dock(
                model, dock, (sandbox3d_workspace_panel_id)index);
        }
    }
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
    const float x = left_dock.x + left_dock.width + (scene_frame.x - (left_dock.x + left_dock.width) - SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH) * 0.5f;
    return (henka_ui_rect){x, scene_frame.y, SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH, scene_frame.height};
}

henka_ui_rect sandbox3d_workspace_right_splitter_rect(henka_ui_rect scene_frame, henka_ui_rect right_dock)
{
    const float x = scene_frame.x + scene_frame.width + (right_dock.x - (scene_frame.x + scene_frame.width) - SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH) * 0.5f;
    return (henka_ui_rect){x, scene_frame.y, SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH, scene_frame.height};
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
    const sandbox3d_workspace_panel_id close_section =
        model != NULL ? model->divider_close_section : SANDBOX3D_WORKSPACE_PANEL_NONE;
    const bool close_preview = model != NULL && model->divider_close_preview;

    if (model == NULL)
    {
        return;
    }
    model->active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_resize_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    model->active_dock_target = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->active_divider_node = UINT16_MAX;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->context_menu_open = false;
    model->context_menu_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->section_chooser_open = false;
    model->section_chooser_source = SANDBOX3D_WORKSPACE_PANEL_NONE;
    if (model->topology_transaction_active)
    {
        if (close_preview && close_section != SANDBOX3D_WORKSPACE_PANEL_NONE)
        {
            if (sandbox3d_workspace_close_section(model, close_section))
            {
                sandbox3d_workspace_commit_topology_transaction(model);
            }
            else
            {
                sandbox3d_workspace_rollback_topology_transaction(model);
            }
        }
        else
        {
            sandbox3d_workspace_commit_topology_transaction(model);
        }
    }
}

static int sandbox3d_workspace_find_section_node(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    size_t node_index;
    if (model == NULL)
    {
        return -1;
    }
    for (node_index = 0U; node_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++node_index)
    {
        const sandbox3d_workspace_topology_node* node =
            sandbox3d_workspace_topology_get_node_const(model, (uint16_t)node_index);
        if (node != NULL && node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION &&
            node->data.section.section_id == section_id)
        {
            return (int)node_index;
        }
    }
    return -1;
}

static bool sandbox3d_workspace_remove_section_node(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    const int leaf_index = sandbox3d_workspace_find_section_node(model, section_id);
    sandbox3d_workspace_topology_node* leaf;
    sandbox3d_workspace_topology_node* parent;
    uint16_t sibling_index;
    uint16_t grandparent_index;

    if (leaf_index < 0)
    {
        return false;
    }
    leaf = &model->topology_nodes[leaf_index];
    if (leaf->parent == UINT16_MAX)
    {
        return false;
    }
    parent = &model->topology_nodes[leaf->parent];
    if (parent->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return false;
    }
    sibling_index = parent->data.split.first_child == (uint16_t)leaf_index
        ? parent->data.split.second_child
        : parent->data.split.first_child;
    grandparent_index = parent->parent;
    if (grandparent_index == UINT16_MAX)
    {
        model->topology_root = sibling_index;
        model->topology_nodes[sibling_index].parent = UINT16_MAX;
    }
    else
    {
        sandbox3d_workspace_topology_node* grandparent = &model->topology_nodes[grandparent_index];
        if (grandparent->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
        {
            return false;
        }
        if (grandparent->data.split.first_child == (uint16_t)(leaf->parent))
        {
            grandparent->data.split.first_child = sibling_index;
        }
        else if (grandparent->data.split.second_child == (uint16_t)(leaf->parent))
        {
            grandparent->data.split.second_child = sibling_index;
        }
        else
        {
            return false;
        }
        model->topology_nodes[sibling_index].parent = grandparent_index;
    }
    memset(leaf, 0, sizeof(*leaf));
    leaf->type = SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED;
    leaf->parent = UINT16_MAX;
    memset(parent, 0, sizeof(*parent));
    parent->type = SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED;
    parent->parent = UINT16_MAX;
    return true;
}

static void sandbox3d_workspace_copy_topology(
    sandbox3d_workspace_topology_node* destination,
    uint16_t* destination_root,
    const sandbox3d_workspace_topology_node* source,
    uint16_t source_root)
{
    memcpy(
        destination,
        source,
        sizeof(sandbox3d_workspace_topology_node) * SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES);
    *destination_root = source_root;
}

static void sandbox3d_workspace_equalize_node(
    sandbox3d_workspace_model* model,
    uint16_t node_index)
{
    sandbox3d_workspace_topology_node* node;
    if (node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
    {
        return;
    }
    node = &model->topology_nodes[node_index];
    if (node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return;
    }
    node->data.split.ratio = 0.5f;
    sandbox3d_workspace_equalize_node(model, node->data.split.first_child);
    sandbox3d_workspace_equalize_node(model, node->data.split.second_child);
}

sandbox3d_workspace_topology_node* sandbox3d_workspace_topology_get_node(
    sandbox3d_workspace_model* model,
    uint16_t node_index)
{
    if (model == NULL || node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        model->topology_nodes[node_index].type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED)
    {
        return NULL;
    }
    return &model->topology_nodes[node_index];
}

const sandbox3d_workspace_topology_node* sandbox3d_workspace_topology_get_node_const(
    const sandbox3d_workspace_model* model,
    uint16_t node_index)
{
    if (model == NULL || node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        model->topology_nodes[node_index].type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED)
    {
        return NULL;
    }
    return &model->topology_nodes[node_index];
}

bool sandbox3d_workspace_topology_is_valid(const sandbox3d_workspace_model* model)
{
    bool section_seen[SANDBOX3D_WORKSPACE_PANEL_COUNT] = {false};
    bool node_seen[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES] = {false};
    bool tab_seen[SANDBOX3D_WORKSPACE_PANEL_COUNT] = {false};
    size_t visited_count = 0U;
    size_t index;

    if (model == NULL || model->topology_root >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        !sandbox3d_workspace_topology_visit(
            model, model->topology_root, UINT16_MAX, section_seen, node_seen, tab_seen, &visited_count) ||
        visited_count == 0U)
    {
        return false;
    }
    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        if (!section_seen[index] &&
            (model->closed_sections_mask & (1U << index)) == 0U)
        {
            return false;
        }
    }
    return true;
}

henka_ui_rect sandbox3d_workspace_topology_divider_hit_rect(
    henka_ui_rect divider_rect,
    sandbox3d_workspace_split_orientation orientation)
{
    const float hit_width = SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH;
    if (orientation == SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL)
    {
        return (henka_ui_rect){
            divider_rect.x - (hit_width - divider_rect.width) * 0.5f,
            divider_rect.y,
            hit_width,
            divider_rect.height};
    }
    return (henka_ui_rect){
        divider_rect.x,
        divider_rect.y - (hit_width - divider_rect.height) * 0.5f,
        divider_rect.width,
        hit_width};
}

void sandbox3d_workspace_build_topology_layout(
    const sandbox3d_workspace_model* model,
    henka_ui_rect bounds,
    sandbox3d_workspace_topology_layout* out_layout)
{
    size_t index;

    if (out_layout == NULL)
    {
        return;
    }
    memset(out_layout, 0, sizeof(*out_layout));
    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        out_layout->section_rects[index] = sandbox3d_workspace_topology_zero_rect();
    }
    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++index)
    {
        out_layout->divider_visual_rects[index] = sandbox3d_workspace_topology_zero_rect();
        out_layout->divider_hit_rects[index] = sandbox3d_workspace_topology_zero_rect();
    }
    if (model == NULL || !sandbox3d_workspace_topology_is_valid(model) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return;
    }
    if (model->maximized_section != SANDBOX3D_WORKSPACE_PANEL_NONE &&
        !sandbox3d_workspace_section_is_closed(model, model->maximized_section))
    {
        out_layout->section_rects[model->maximized_section] = bounds;
        return;
    }
    (void)sandbox3d_workspace_topology_layout_node(model, model->topology_root, bounds, out_layout);
    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        if (sandbox3d_workspace_section_is_closed(model, (sandbox3d_workspace_panel_id)index))
        {
            out_layout->section_rects[index] = sandbox3d_workspace_topology_zero_rect();
        }
    }
}

void sandbox3d_workspace_begin_topology_transaction(sandbox3d_workspace_model* model)
{
    if (model == NULL || model->topology_transaction_active)
    {
        return;
    }
    memcpy(
        model->topology_transaction_nodes,
        model->topology_nodes,
        sizeof(model->topology_nodes));
    model->topology_transaction_root = model->topology_root;
    model->topology_transaction_active = true;
}

void sandbox3d_workspace_commit_topology_transaction(sandbox3d_workspace_model* model)
{
    if (model == NULL)
    {
        return;
    }
    model->topology_transaction_active = false;
    model->topology_transaction_root = UINT16_MAX;
}

void sandbox3d_workspace_rollback_topology_transaction(sandbox3d_workspace_model* model)
{
    if (model == NULL || !model->topology_transaction_active)
    {
        return;
    }
    memcpy(
        model->topology_nodes,
        model->topology_transaction_nodes,
        sizeof(model->topology_nodes));
    model->topology_root = model->topology_transaction_root;
    sandbox3d_workspace_commit_topology_transaction(model);
    model->active_divider_node = UINT16_MAX;
}

void sandbox3d_workspace_begin_divider_drag(
    sandbox3d_workspace_model* model,
    uint16_t node_index,
    henka_vec2 pointer)
{
    sandbox3d_workspace_topology_node* node =
        sandbox3d_workspace_topology_get_node(model, node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return;
    }
    sandbox3d_workspace_begin_topology_transaction(model);
    model->active_divider_node = node_index;
    model->active_divider_start_ratio = node->data.split.ratio;
    model->active_divider_start_pointer = pointer;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    snprintf(model->last_action, sizeof(model->last_action), "Divider %u moving", (unsigned int)node_index);
}

static sandbox3d_workspace_panel_id sandbox3d_workspace_direct_section_child(
    const sandbox3d_workspace_model* model,
    uint16_t node_index)
{
    const sandbox3d_workspace_topology_node* node =
        sandbox3d_workspace_topology_get_node_const(model, node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    return node->data.section.section_id;
}

void sandbox3d_workspace_update_divider_drag(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    henka_ui_rect bounds)
{
    sandbox3d_workspace_topology_node* node;
    henka_ui_rect node_rect;
    float available;
    float delta;
    float ratio;
    float raw_ratio;
    const sandbox3d_workspace_topology_node* first_child;
    const sandbox3d_workspace_topology_node* second_child;

    if (model == NULL || model->active_divider_node == UINT16_MAX ||
        !sandbox3d_workspace_topology_find_node_rect(
            model, model->topology_root, bounds, model->active_divider_node, &node_rect))
    {
        return;
    }
    node = sandbox3d_workspace_topology_get_node(model, model->active_divider_node);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return;
    }
    available = (node->data.split.orientation == SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL
        ? node_rect.width
        : node_rect.height) - 1.0f;
    if (available <= 0.0f)
    {
        return;
    }
    delta = (node->data.split.orientation == SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL
        ? pointer.x - model->active_divider_start_pointer.x
        : pointer.y - model->active_divider_start_pointer.y);
    ratio = model->active_divider_start_ratio + delta / available;
    raw_ratio = ratio;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    first_child = sandbox3d_workspace_topology_get_node_const(
        model,
        node->data.split.first_child);
    second_child = sandbox3d_workspace_topology_get_node_const(
        model,
        node->data.split.second_child);
    if (first_child != NULL && first_child->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION &&
        raw_ratio < (node->data.split.minimum_first - SANDBOX3D_WORKSPACE_DIVIDER_CLOSE_THRESHOLD) / available)
    {
        model->divider_close_preview = true;
        model->divider_close_section = sandbox3d_workspace_direct_section_child(
            model,
            node->data.split.first_child);
        snprintf(
            model->last_action,
            sizeof(model->last_action),
            "Release to close %s",
            sandbox3d_workspace_panel_name(model->divider_close_section));
    }
    else if (second_child != NULL && second_child->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION &&
        raw_ratio > 1.0f - (node->data.split.minimum_second - SANDBOX3D_WORKSPACE_DIVIDER_CLOSE_THRESHOLD) / available)
    {
        model->divider_close_preview = true;
        model->divider_close_section = sandbox3d_workspace_direct_section_child(
            model,
            node->data.split.second_child);
        snprintf(
            model->last_action,
            sizeof(model->last_action),
            "Release to close %s",
            sandbox3d_workspace_panel_name(model->divider_close_section));
    }
    ratio = sandbox3d_workspace_clamp_float(
        ratio,
        node->data.split.minimum_first / available,
        1.0f - node->data.split.minimum_second / available);
    node->data.split.ratio = ratio;
}

bool sandbox3d_workspace_divider_close_preview(
    const sandbox3d_workspace_model* model)
{
    return model != NULL && model->divider_close_preview &&
        model->divider_close_section != SANDBOX3D_WORKSPACE_PANEL_NONE;
}

sandbox3d_workspace_panel_id sandbox3d_workspace_divider_close_section(
    const sandbox3d_workspace_model* model)
{
    return sandbox3d_workspace_divider_close_preview(model)
        ? model->divider_close_section
        : SANDBOX3D_WORKSPACE_PANEL_NONE;
}

bool sandbox3d_workspace_topology_section_has_tab(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id)
{
    size_t index;
    size_t node_index;
    if (model == NULL || section_id < 0 || section_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
        tab_id < 0 || tab_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
    {
        return false;
    }
    for (node_index = 0U; node_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++node_index)
    {
        const sandbox3d_workspace_topology_node* node =
            sandbox3d_workspace_topology_get_node_const(model, (uint16_t)node_index);
        if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
            node->data.section.section_id != section_id)
        {
            continue;
        }
        for (index = 0U; index < node->data.section.tab_count; ++index)
        {
            if (node->data.section.tabs[index] == tab_id)
            {
                return true;
            }
        }
    }
    return false;
}

static void sandbox3d_workspace_collect_topology_dock_sections(
    const sandbox3d_workspace_model* model,
    uint16_t node_index,
    sandbox3d_workspace_dock_zone dock_zone,
    sandbox3d_workspace_panel_id* out_sections,
    size_t* in_out_count)
{
    const sandbox3d_workspace_topology_node* node;

    if (model == NULL || out_sections == NULL || in_out_count == NULL ||
        *in_out_count >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
        node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
    {
        return;
    }
    node = sandbox3d_workspace_topology_get_node_const(model, node_index);
    if (node == NULL)
    {
        return;
    }
    if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        const sandbox3d_workspace_panel* panel =
            sandbox3d_workspace_get_panel_const(model, node->data.section.section_id);
        if (panel != NULL && panel->dock == dock_zone)
        {
            out_sections[*in_out_count] = node->data.section.section_id;
            *in_out_count += 1U;
        }
        return;
    }
    if (node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return;
    }
    sandbox3d_workspace_collect_topology_dock_sections(
        model, node->data.split.first_child, dock_zone, out_sections, in_out_count);
    sandbox3d_workspace_collect_topology_dock_sections(
        model, node->data.split.second_child, dock_zone, out_sections, in_out_count);
}

size_t sandbox3d_workspace_get_topology_dock_section_count(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone)
{
    sandbox3d_workspace_panel_id sections[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    size_t count = 0U;

    if (model == NULL ||
        (dock_zone != SANDBOX3D_WORKSPACE_DOCK_LEFT &&
         dock_zone != SANDBOX3D_WORKSPACE_DOCK_RIGHT) ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return 0U;
    }
    sandbox3d_workspace_collect_topology_dock_sections(
        model, model->topology_root, dock_zone, sections, &count);
    return count;
}

sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_dock_section_at(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
    size_t index)
{
    sandbox3d_workspace_panel_id sections[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    size_t count = 0U;

    if (model == NULL ||
        (dock_zone != SANDBOX3D_WORKSPACE_DOCK_LEFT &&
         dock_zone != SANDBOX3D_WORKSPACE_DOCK_RIGHT) ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    sandbox3d_workspace_collect_topology_dock_sections(
        model, model->topology_root, dock_zone, sections, &count);
    return index < count ? sections[index] : SANDBOX3D_WORKSPACE_PANEL_NONE;
}

sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_section_for_tab(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id tab_id)
{
    size_t node_index;

    if (model == NULL || tab_id < 0 || tab_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    for (node_index = 0U; node_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++node_index)
    {
        const sandbox3d_workspace_topology_node* node =
            sandbox3d_workspace_topology_get_node_const(model, (uint16_t)node_index);
        size_t tab_index;
        if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
        {
            continue;
        }
        for (tab_index = 0U; tab_index < node->data.section.tab_count; ++tab_index)
        {
            if (node->data.section.tabs[tab_index] == tab_id)
            {
                return node->data.section.section_id;
            }
        }
    }
    return SANDBOX3D_WORKSPACE_PANEL_NONE;
}

size_t sandbox3d_workspace_get_topology_section_tab_count(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    const int node_index = sandbox3d_workspace_find_section_node(model, section_id);
    const sandbox3d_workspace_topology_node* node;

    if (node_index < 0 || !sandbox3d_workspace_topology_is_valid(model))
    {
        return 0U;
    }
    node = sandbox3d_workspace_topology_get_node_const(model, (uint16_t)node_index);
    return node != NULL && node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION
        ? node->data.section.tab_count
        : 0U;
}

sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_section_tab_at(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    size_t index)
{
    const int node_index = sandbox3d_workspace_find_section_node(model, section_id);
    const sandbox3d_workspace_topology_node* node;

    if (node_index < 0 || !sandbox3d_workspace_topology_is_valid(model))
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    node = sandbox3d_workspace_topology_get_node_const(model, (uint16_t)node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
        index >= node->data.section.tab_count)
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    return node->data.section.tabs[index];
}

sandbox3d_workspace_panel_id sandbox3d_workspace_get_topology_section_active_tab(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    const int node_index = sandbox3d_workspace_find_section_node(model, section_id);
    const sandbox3d_workspace_topology_node* node;

    if (node_index < 0 || !sandbox3d_workspace_topology_is_valid(model))
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    node = sandbox3d_workspace_topology_get_node_const(model, (uint16_t)node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
        node->data.section.active_tab >= node->data.section.tab_count)
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    return node->data.section.tabs[node->data.section.active_tab];
}

bool sandbox3d_workspace_set_topology_section_active_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id)
{
    const int node_index = sandbox3d_workspace_find_section_node(model, section_id);
    sandbox3d_workspace_topology_node* node;
    size_t tab_index;

    if (node_index < 0 || model == NULL ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    node = sandbox3d_workspace_topology_get_node(model, (uint16_t)node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        return false;
    }
    for (tab_index = 0U; tab_index < node->data.section.tab_count; ++tab_index)
    {
        if (node->data.section.tabs[tab_index] == tab_id)
        {
            node->data.section.active_tab = (uint8_t)tab_index;
            snprintf(
                model->last_action,
                sizeof(model->last_action),
                "%s tab selected",
                sandbox3d_workspace_panel_name(tab_id));
            return true;
        }
    }
    return false;
}

bool sandbox3d_workspace_section_is_closed(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    return model != NULL && section_id >= 0 && section_id < SANDBOX3D_WORKSPACE_PANEL_COUNT &&
        (model->closed_sections_mask & (1U << section_id)) != 0U;
}

bool sandbox3d_workspace_close_section(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    if (model == NULL || section_id < 0 || section_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
        sandbox3d_workspace_section_is_closed(model, section_id) ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    sandbox3d_workspace_copy_topology(
        model->closed_snapshot_nodes,
        &model->closed_snapshot_root,
        model->topology_nodes,
        model->topology_root);
    model->closed_snapshot_mask = model->closed_sections_mask;
    model->closed_snapshot_valid = true;
    if (!sandbox3d_workspace_remove_section_node(model, section_id))
    {
        model->closed_snapshot_valid = false;
        return false;
    }
    model->closed_sections_mask |= 1U << section_id;
    if (model->maximized_section == section_id)
    {
        model->maximized_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    snprintf(model->last_action, sizeof(model->last_action), "%s closed", sandbox3d_workspace_panel_name(section_id));
    return sandbox3d_workspace_topology_is_valid(model);
}

bool sandbox3d_workspace_restore_last_closed_section(sandbox3d_workspace_model* model)
{
    if (model == NULL || !model->closed_snapshot_valid)
    {
        return false;
    }
    sandbox3d_workspace_copy_topology(
        model->topology_nodes,
        &model->topology_root,
        model->closed_snapshot_nodes,
        model->closed_snapshot_root);
    model->closed_sections_mask = model->closed_snapshot_mask;
    model->closed_snapshot_valid = false;
    snprintf(model->last_action, sizeof(model->last_action), "Closed section restored");
    return sandbox3d_workspace_topology_is_valid(model);
}

bool sandbox3d_workspace_merge_sections(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id target_section,
    sandbox3d_workspace_panel_id source_section)
{
    const int target_index = sandbox3d_workspace_find_section_node(model, target_section);
    const int source_index = sandbox3d_workspace_find_section_node(model, source_section);
    sandbox3d_workspace_topology_node* target;
    const sandbox3d_workspace_topology_node* source;
    size_t tab_index;

    if (model == NULL || target_section == source_section || target_index < 0 || source_index < 0 ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    target = &model->topology_nodes[target_index];
    source = &model->topology_nodes[source_index];
    if (target->data.section.tab_count + source->data.section.tab_count >
        SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS)
    {
        return false;
    }
    sandbox3d_workspace_begin_topology_transaction(model);
    for (tab_index = 0U; tab_index < source->data.section.tab_count; ++tab_index)
    {
        target->data.section.tabs[target->data.section.tab_count++] = source->data.section.tabs[tab_index];
    }
    if (!sandbox3d_workspace_remove_section_node(model, source_section))
    {
        sandbox3d_workspace_rollback_topology_transaction(model);
        return false;
    }
    model->closed_sections_mask |= 1U << source_section;
    sandbox3d_workspace_commit_topology_transaction(model);
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s merged into %s",
        sandbox3d_workspace_panel_name(source_section),
        sandbox3d_workspace_panel_name(target_section));
    return sandbox3d_workspace_topology_is_valid(model);
}

static int sandbox3d_workspace_find_unused_node(const sandbox3d_workspace_model* model)
{
    size_t index;
    if (model == NULL)
    {
        return -1;
    }
    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++index)
    {
        if (model->topology_nodes[index].type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED)
        {
            return (int)index;
        }
    }
    return -1;
}

bool sandbox3d_workspace_split_section(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id target_section,
    sandbox3d_workspace_split_orientation orientation,
    sandbox3d_workspace_panel_id new_section)
{
    const int target_index = sandbox3d_workspace_find_section_node(model, target_section);
    const int target_leaf_index = sandbox3d_workspace_find_unused_node(model);
    int new_leaf_index;
    sandbox3d_workspace_topology_node target_leaf;
    sandbox3d_workspace_topology_node* split;
    sandbox3d_workspace_dock_zone target_dock;

    if (model == NULL || target_index < 0 || target_section == new_section ||
        new_section < 0 || new_section >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
        (orientation != SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL &&
         orientation != SANDBOX3D_WORKSPACE_SPLIT_VERTICAL) ||
        sandbox3d_workspace_find_section_node(model, new_section) >= 0 ||
        (model->closed_sections_mask & (1U << new_section)) == 0U ||
        target_leaf_index < 0 || !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    new_leaf_index = -1;
    {
        size_t node_index;
        for (node_index = 0U; node_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++node_index)
        {
            if ((int)node_index != target_leaf_index &&
                model->topology_nodes[node_index].type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED)
            {
                new_leaf_index = (int)node_index;
                break;
            }
        }
    }
    if (new_leaf_index < 0)
    {
        return false;
    }
    target_leaf = model->topology_nodes[target_index];
    if (target_leaf.type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        return false;
    }
    {
        const sandbox3d_workspace_panel* new_panel =
            sandbox3d_workspace_get_panel_const(model, new_section);
        const sandbox3d_workspace_panel* target_panel =
            sandbox3d_workspace_get_panel_const(model, target_section);
        if (new_panel == NULL || target_panel == NULL ||
            (target_panel->dock != SANDBOX3D_WORKSPACE_DOCK_LEFT &&
             target_panel->dock != SANDBOX3D_WORKSPACE_DOCK_RIGHT))
        {
            return false;
        }
        target_dock = target_panel->dock;
    }
    sandbox3d_workspace_begin_topology_transaction(model);
    model->topology_nodes[target_leaf_index] = target_leaf;
    model->topology_nodes[target_leaf_index].parent = (uint16_t)target_index;
    sandbox3d_workspace_topology_make_section(
        &model->topology_nodes[new_leaf_index],
        new_section);
    model->topology_nodes[new_leaf_index].parent = (uint16_t)target_index;
    split = &model->topology_nodes[target_index];
    memset(split, 0, sizeof(*split));
    split->type = SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT;
    split->parent = target_leaf.parent;
    split->data.split.first_child = (uint16_t)target_leaf_index;
    split->data.split.second_child = (uint16_t)new_leaf_index;
    split->data.split.orientation = orientation;
    split->data.split.ratio = 0.5f;
    split->data.split.minimum_first = 180.0f;
    split->data.split.minimum_second = 180.0f;
    model->closed_sections_mask &= ~(1U << new_section);
    model->closed_snapshot_valid = false;
    if (!sandbox3d_workspace_topology_is_valid(model))
    {
        sandbox3d_workspace_rollback_topology_transaction(model);
        return false;
    }
    sandbox3d_workspace_commit_topology_transaction(model);
    {
        sandbox3d_workspace_panel* new_panel = sandbox3d_workspace_get_panel(model, new_section);
        sandbox3d_workspace_remove_panel_from_docks(model, new_section);
        new_panel->dock = target_dock;
        new_panel->last_docked_zone = target_dock;
        new_panel->detached_window_id = 0U;
        sandbox3d_workspace_append_panel_to_dock(model, target_dock, new_section);
    }
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s split %s with %s",
        sandbox3d_workspace_panel_name(target_section),
        orientation == SANDBOX3D_WORKSPACE_SPLIT_VERTICAL ? "horizontal" : "vertical",
        sandbox3d_workspace_panel_name(new_section));
    return true;
}

void sandbox3d_workspace_equalize_sections(sandbox3d_workspace_model* model)
{
    if (model == NULL || !sandbox3d_workspace_topology_is_valid(model))
    {
        return;
    }
    sandbox3d_workspace_equalize_node(model, model->topology_root);
    snprintf(model->last_action, sizeof(model->last_action), "Sections equalized");
}

void sandbox3d_workspace_set_maximized_section(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    if (model == NULL || section_id < 0 || section_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
        sandbox3d_workspace_section_is_closed(model, section_id) ||
        sandbox3d_workspace_find_section_node(model, section_id) < 0)
    {
        return;
    }
    model->maximized_section = section_id;
    snprintf(model->last_action, sizeof(model->last_action), "%s maximized", sandbox3d_workspace_panel_name(section_id));
}

void sandbox3d_workspace_restore_maximized_section(sandbox3d_workspace_model* model)
{
    if (model == NULL)
    {
        return;
    }
    model->maximized_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    snprintf(model->last_action, sizeof(model->last_action), "Section maximization restored");
}

const char* sandbox3d_workspace_context_command_label(
    sandbox3d_workspace_context_command command)
{
    static const char* labels[SANDBOX3D_WORKSPACE_CONTEXT_COMMAND_COUNT] =
    {
        "Open a horizontal window",
        "Open a vertical window",
        "Close this section",
        "Merge with adjacent section",
        "Equalize sections",
        "Maximize / Restore section",
        "Detach section",
        "Move to tab group",
        "Restore last closed section"
    };
    if (command < 0 || command >= SANDBOX3D_WORKSPACE_CONTEXT_COMMAND_COUNT)
    {
        return "Unknown command";
    }
    return labels[command];
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
