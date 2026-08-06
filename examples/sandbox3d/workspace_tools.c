#include "workspace_tools.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static henka_ui_rect sandbox3d_workspace_topology_divider_hit_rect_scaled(
    henka_ui_rect divider_rect,
    sandbox3d_workspace_split_orientation orientation,
    float ui_scale);

int sandbox3d_workspace_clamp_controls_page(int page)
{
    if (page < SANDBOX3D_CONTROLS_PAGE_MAIN)
    {
        return SANDBOX3D_CONTROLS_PAGE_MAIN;
    }
    if (page >= SANDBOX3D_CONTROLS_PAGE_COUNT)
    {
        return SANDBOX3D_CONTROLS_PAGE_QA;
    }
    return page;
}

henka_ui_rect sandbox3d_workspace_controls_page_tab_rect(
    henka_ui_rect panel_bounds,
    size_t page_index)
{
    henka_ui_rect rect = {0};
    const float width = (panel_bounds.width - 36.0f) / 3.0f;

    if (page_index >= SANDBOX3D_WORKSPACE_CONTROLS_PAGE_COUNT ||
        panel_bounds.width <= 36.0f ||
        panel_bounds.height <= 62.0f ||
        width <= 0.0f)
    {
        return rect;
    }

    rect.x = panel_bounds.x + 14.0f +
        (width + 4.0f) * (float)page_index;
    rect.y = panel_bounds.y + 38.0f;
    rect.width = width;
    rect.height = 24.0f;
    return rect;
}

henka_ui_rect sandbox3d_workspace_controls_qa_action_rect(
    henka_ui_rect panel_bounds,
    size_t action_index)
{
    henka_ui_rect rect = {0};
    const float x_left = panel_bounds.x + 14.0f;
    const float width = (panel_bounds.width - 36.0f) / 3.0f;

    if (action_index >= SANDBOX3D_WORKSPACE_CONTROLS_QA_ACTION_COUNT ||
        panel_bounds.width <= 36.0f ||
        panel_bounds.height <= 158.0f ||
        width <= 0.0f)
    {
        return rect;
    }

    rect.y = panel_bounds.y + 94.0f;
    rect.height = 28.0f;

    if (action_index < 3U)
    {
        rect.x = x_left +
            (width + 4.0f) * (float)action_index;
        rect.width = width;
    }
    else
    {
        rect.x = x_left;
        rect.y += 36.0f;
        rect.width = panel_bounds.width - 28.0f;
    }

    return rect;
}

bool sandbox3d_workspace_rect_contains_rect(
    henka_ui_rect outer,
    henka_ui_rect inner)
{
    return outer.width > 0.0f &&
        outer.height > 0.0f &&
        inner.width > 0.0f &&
        inner.height > 0.0f &&
        inner.x >= outer.x &&
        inner.y >= outer.y &&
        inner.x + inner.width <= outer.x + outer.width &&
        inner.y + inner.height <= outer.y + outer.height;
}

bool sandbox3d_workspace_sanitize_floating_rect(
    const sandbox3d_workspace_panel* panel,
    henka_ui_rect candidate,
    henka_ui_rect* out_rect)
{
    if (panel == NULL ||
        out_rect == NULL ||
        !isfinite(candidate.x) ||
        !isfinite(candidate.y) ||
        !isfinite(candidate.width) ||
        !isfinite(candidate.height) ||
        candidate.x < -4096.0f ||
        candidate.x > 16384.0f ||
        candidate.y < -4096.0f ||
        candidate.y > 16384.0f ||
        candidate.width <= 0.0f ||
        candidate.width > 4096.0f ||
        candidate.height <= 0.0f ||
        candidate.height > 4096.0f ||
        !isfinite(panel->minimum_width) ||
        !isfinite(panel->minimum_height) ||
        panel->minimum_width <= 0.0f ||
        panel->minimum_width > 4096.0f ||
        panel->minimum_height <= 0.0f ||
        panel->minimum_height > 4096.0f)
    {
        return false;
    }

    if (candidate.width < panel->minimum_width)
    {
        candidate.width = panel->minimum_width;
    }
    if (candidate.height < panel->minimum_height)
    {
        candidate.height = panel->minimum_height;
    }

    *out_rect = candidate;
    return true;
}

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

void sandbox3d_workspace_set_ui_scale(
    sandbox3d_workspace_model* model,
    float ui_scale)
{
    if (model == NULL || !isfinite(ui_scale))
    {
        return;
    }
    model->ui_scale = sandbox3d_workspace_clamp_float(
        ui_scale,
        SANDBOX3D_WORKSPACE_UI_SCALE_MIN,
        SANDBOX3D_WORKSPACE_UI_SCALE_MAX);
}

float sandbox3d_workspace_get_ui_scale(
    const sandbox3d_workspace_model* model)
{
    if (model == NULL || !isfinite(model->ui_scale) || model->ui_scale <= 0.0f)
    {
        return 1.0f;
    }
    return model->ui_scale;
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
    model->active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
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
    model->section_chooser_selected_index = 0U;
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
            sandbox3d_workspace_topology_divider_hit_rect_scaled(
                visual,
                node->data.split.orientation,
                sandbox3d_workspace_get_ui_scale(model));
        out_layout->divider_node_indices[divider_index] = node_index;
        out_layout->divider_count += 1U;
        return sandbox3d_workspace_topology_layout_node(
                   model, node->data.split.first_child, first, out_layout) &&
            sandbox3d_workspace_topology_layout_node(
                model, node->data.split.second_child, second, out_layout);
    }
}

static bool sandbox3d_workspace_topology_node_has_dock_section(
    const sandbox3d_workspace_model* model,
    uint16_t node_index,
    sandbox3d_workspace_dock_zone dock_zone)
{
    const sandbox3d_workspace_topology_node* node;

    if (model == NULL || node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
    {
        return false;
    }
    node = &model->topology_nodes[node_index];
    if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        const sandbox3d_workspace_panel* panel =
            sandbox3d_workspace_get_panel_const(model, node->data.section.section_id);
        return panel != NULL && panel->dock == dock_zone &&
            !sandbox3d_workspace_section_is_closed(model, node->data.section.section_id);
    }
    if (node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return false;
    }
    return sandbox3d_workspace_topology_node_has_dock_section(
               model, node->data.split.first_child, dock_zone) ||
        sandbox3d_workspace_topology_node_has_dock_section(
            model, node->data.split.second_child, dock_zone);
}

static bool sandbox3d_workspace_topology_layout_dock_node(
    const sandbox3d_workspace_model* model,
    uint16_t node_index,
    sandbox3d_workspace_dock_zone dock_zone,
    henka_ui_rect bounds,
    sandbox3d_workspace_topology_layout* out_layout)
{
    const sandbox3d_workspace_topology_node* node;
    bool first_has;
    bool second_has;

    if (model == NULL || out_layout == NULL ||
        node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return false;
    }
    node = &model->topology_nodes[node_index];
    if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        const sandbox3d_workspace_panel* panel =
            sandbox3d_workspace_get_panel_const(model, node->data.section.section_id);
        if (panel == NULL || panel->dock != dock_zone ||
            sandbox3d_workspace_section_is_closed(model, node->data.section.section_id))
        {
            return false;
        }
        out_layout->section_rects[node->data.section.section_id] = bounds;
        return true;
    }
    if (node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return false;
    }
    first_has = sandbox3d_workspace_topology_node_has_dock_section(
        model, node->data.split.first_child, dock_zone);
    second_has = sandbox3d_workspace_topology_node_has_dock_section(
        model, node->data.split.second_child, dock_zone);
    if (!first_has && !second_has)
    {
        return false;
    }
    if (!first_has)
    {
        return sandbox3d_workspace_topology_layout_dock_node(
            model, node->data.split.second_child, dock_zone, bounds, out_layout);
    }
    if (!second_has)
    {
        return sandbox3d_workspace_topology_layout_dock_node(
            model, node->data.split.first_child, dock_zone, bounds, out_layout);
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
        const size_t divider_index = out_layout->divider_count;
        henka_ui_rect first = bounds;
        henka_ui_rect second = bounds;
        henka_ui_rect visual;

        if (available <= 0.0f || divider_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
        {
            return false;
        }
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
            sandbox3d_workspace_topology_divider_hit_rect_scaled(
                visual,
                node->data.split.orientation,
                sandbox3d_workspace_get_ui_scale(model));
        out_layout->divider_node_indices[divider_index] = node_index;
        ++out_layout->divider_count;
        return sandbox3d_workspace_topology_layout_dock_node(
                   model, node->data.split.first_child, dock_zone, first, out_layout) &&
            sandbox3d_workspace_topology_layout_dock_node(
                model, node->data.split.second_child, dock_zone, second, out_layout);
    }
}

void sandbox3d_workspace_build_dock_topology_layout(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock_zone,
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
        out_layout->divider_node_indices[index] = UINT16_MAX;
    }
    if (model == NULL || !sandbox3d_workspace_topology_is_valid(model) ||
        (dock_zone != SANDBOX3D_WORKSPACE_DOCK_LEFT &&
         dock_zone != SANDBOX3D_WORKSPACE_DOCK_RIGHT) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return;
    }
    if (model->maximized_section != SANDBOX3D_WORKSPACE_PANEL_NONE &&
        !sandbox3d_workspace_section_is_closed(model, model->maximized_section))
    {
        const sandbox3d_workspace_panel* panel =
            sandbox3d_workspace_get_panel_const(model, model->maximized_section);
        if (panel != NULL && panel->dock == dock_zone)
        {
            out_layout->section_rects[model->maximized_section] = bounds;
            return;
        }
    }
    (void)sandbox3d_workspace_topology_layout_dock_node(
        model, model->topology_root, dock_zone, bounds, out_layout);
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

const char* sandbox3d_workspace_named_layout_label(
    sandbox3d_workspace_named_layout layout)
{
    switch (layout)
    {
        case SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT:
            return "Default";
        case SANDBOX3D_WORKSPACE_LAYOUT_MODELING:
            return "Modeling";
        case SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS:
            return "Materials";
        case SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY:
            return "Scene Assembly";
        case SANDBOX3D_WORKSPACE_LAYOUT_DEBUGGING:
            return "Debugging";
        case SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT:
            return "Minimal Viewport";
        case SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM:
            return "Custom";
        case SANDBOX3D_WORKSPACE_LAYOUT_COUNT:
        default:
            return "Unknown";
    }
}

const char* sandbox3d_workspace_named_layout_setting_value(
    sandbox3d_workspace_named_layout layout)
{
    switch (layout)
    {
        case SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT:
            return "default";
        case SANDBOX3D_WORKSPACE_LAYOUT_MODELING:
            return "modeling";
        case SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS:
            return "materials";
        case SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY:
            return "scene_assembly";
        case SANDBOX3D_WORKSPACE_LAYOUT_DEBUGGING:
            return "debugging";
        case SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT:
            return "minimal_viewport";
        case SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM:
            return "custom";
        case SANDBOX3D_WORKSPACE_LAYOUT_COUNT:
        default:
            return "custom";
    }
}

sandbox3d_workspace_named_layout sandbox3d_workspace_parse_named_layout(
    const char* value)
{
    if (value == NULL)
    {
        return SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
    }
    if (strcmp(value, "default") == 0)
    {
        return SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT;
    }
    if (strcmp(value, "modeling") == 0)
    {
        return SANDBOX3D_WORKSPACE_LAYOUT_MODELING;
    }
    if (strcmp(value, "materials") == 0)
    {
        return SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS;
    }
    if (strcmp(value, "scene_assembly") == 0)
    {
        return SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY;
    }
    if (strcmp(value, "debugging") == 0)
    {
        return SANDBOX3D_WORKSPACE_LAYOUT_DEBUGGING;
    }
    if (strcmp(value, "minimal_viewport") == 0)
    {
        return SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT;
    }
    return SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
}

sandbox3d_workspace_named_layout sandbox3d_workspace_get_named_layout(
    const sandbox3d_workspace_model* model)
{
    return model != NULL ? model->named_layout : SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
}

static void sandbox3d_workspace_named_layout_set_dock(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone)
{
    sandbox3d_workspace_panel* panel =
        sandbox3d_workspace_get_panel(model, panel_id);
    if (panel == NULL)
    {
        return;
    }
    panel->dock = dock_zone;
    panel->last_docked_zone = dock_zone;
}

bool sandbox3d_workspace_apply_named_layout(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_named_layout layout)
{
    sandbox3d_workspace_model candidate;
    sandbox3d_workspace_panel_id first_left;
    sandbox3d_workspace_panel_id second_left;
    sandbox3d_workspace_panel_id first_right;
    sandbox3d_workspace_panel_id second_right;

    if (model == NULL || layout < SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT ||
        layout >= SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM ||
        model->active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE ||
        model->resize_target != SANDBOX3D_WORKSPACE_RESIZE_NONE ||
        model->topology_transaction_active || model->tab_drag_active)
    {
        return false;
    }

    candidate = *model;
    sandbox3d_workspace_topology_initialize(&candidate);
    candidate.closed_sections_mask = 0U;
    candidate.maximized_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.closed_snapshot_valid = false;
    candidate.named_layout = layout;

    first_left = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
    second_left = SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS;
    first_right = SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS;
    second_right = SANDBOX3D_WORKSPACE_PANEL_UTILITY;
    switch (layout)
    {
        case SANDBOX3D_WORKSPACE_LAYOUT_MODELING:
            first_left = SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS;
            second_left = SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS;
            first_right = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
            break;
        case SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS:
            first_left = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
            second_left = SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS;
            first_right = SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS;
            break;
        case SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY:
            first_left = SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS;
            second_left = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
            first_right = SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS;
            second_right = SANDBOX3D_WORKSPACE_PANEL_UTILITY;
            break;
        case SANDBOX3D_WORKSPACE_LAYOUT_DEBUGGING:
            first_left = SANDBOX3D_WORKSPACE_PANEL_UTILITY;
            second_left = SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS;
            first_right = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
            second_right = SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS;
            break;
        case SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT:
            first_left = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
            second_left = SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS;
            first_right = SANDBOX3D_WORKSPACE_PANEL_UTILITY;
            second_right = SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS;
            break;
        case SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT:
        case SANDBOX3D_WORKSPACE_LAYOUT_COUNT:
        case SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM:
        default:
            break;
    }

    sandbox3d_workspace_topology_make_section(
        &candidate.topology_nodes[2], first_left);
    sandbox3d_workspace_topology_make_section(
        &candidate.topology_nodes[3], second_left);
    sandbox3d_workspace_topology_make_section(
        &candidate.topology_nodes[5], first_right);
    sandbox3d_workspace_topology_make_section(
        &candidate.topology_nodes[6], second_right);
    candidate.topology_nodes[2].parent = 1U;
    candidate.topology_nodes[3].parent = 1U;
    candidate.topology_nodes[5].parent = 4U;
    candidate.topology_nodes[6].parent = 4U;
    candidate.topology_nodes[0].data.split.ratio =
        layout == SANDBOX3D_WORKSPACE_LAYOUT_MODELING ? 0.60f :
        layout == SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS ? 0.46f :
        layout == SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY ? 0.42f :
        layout == SANDBOX3D_WORKSPACE_LAYOUT_DEBUGGING ? 0.54f : 0.50f;
    candidate.topology_nodes[1].data.split.ratio =
        layout == SANDBOX3D_WORKSPACE_LAYOUT_DEBUGGING ? 0.62f :
        layout == SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT ? 0.38f : 0.50f;
    candidate.topology_nodes[4].data.split.ratio =
        layout == SANDBOX3D_WORKSPACE_LAYOUT_MODELING ? 0.42f :
        layout == SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS ? 0.58f : 0.50f;
    if (layout == SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT)
    {
        candidate.left_dock_width = 260.0f;
        candidate.right_dock_width = 280.0f;
    }

    sandbox3d_workspace_named_layout_set_dock(
        &candidate, first_left, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    sandbox3d_workspace_named_layout_set_dock(
        &candidate, second_left, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    sandbox3d_workspace_named_layout_set_dock(
        &candidate, first_right, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    sandbox3d_workspace_named_layout_set_dock(
        &candidate, second_right, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    sandbox3d_workspace_rebuild_dock_lists(&candidate);
    if (!sandbox3d_workspace_topology_is_valid(&candidate))
    {
        return false;
    }

    sandbox3d_workspace_begin_topology_transaction(model);
    model->topology_transaction_result_named_layout = layout;
    memcpy(model->panels, candidate.panels, sizeof(model->panels));
    memcpy(model->left_dock_panels, candidate.left_dock_panels, sizeof(model->left_dock_panels));
    memcpy(model->right_dock_panels, candidate.right_dock_panels, sizeof(model->right_dock_panels));
    model->left_dock_panel_count = candidate.left_dock_panel_count;
    model->right_dock_panel_count = candidate.right_dock_panel_count;
    memcpy(model->topology_nodes, candidate.topology_nodes, sizeof(model->topology_nodes));
    model->topology_root = candidate.topology_root;
    model->closed_sections_mask = candidate.closed_sections_mask;
    model->maximized_section = candidate.maximized_section;
    model->closed_snapshot_valid = candidate.closed_snapshot_valid;
    model->named_layout = candidate.named_layout;
    sandbox3d_workspace_commit_topology_transaction(model);
    model->active_divider_node = UINT16_MAX;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s workspace restored",
        sandbox3d_workspace_named_layout_label(layout));
    return true;
}

static bool sandbox3d_workspace_custom_layout_name_is_valid(const char* name)
{
    size_t length;
    size_t index;

    if (name == NULL)
    {
        return false;
    }
    length = strlen(name);
    if (length == 0U || length >= SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_NAME_MAX)
    {
        return false;
    }
    for (index = 0U; index < length; ++index)
    {
        const unsigned char character = (unsigned char)name[index];
        if (character < 0x20U || character == 0x7fU)
        {
            return false;
        }
    }
    return true;
}

bool sandbox3d_workspace_save_custom_layout(
    sandbox3d_workspace_model* model,
    const char* name)
{
    size_t panel_index;

    if (model == NULL || !sandbox3d_workspace_custom_layout_name_is_valid(name) ||
        model->active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE ||
        model->resize_target != SANDBOX3D_WORKSPACE_RESIZE_NONE ||
        model->topology_transaction_active || model->tab_drag_active ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }

    memcpy(
        model->custom_layout_nodes,
        model->topology_nodes,
        sizeof(model->custom_layout_nodes));
    model->custom_layout_root = model->topology_root;
    model->custom_layout_closed_sections_mask = model->closed_sections_mask;
    model->custom_layout_maximized_section = model->maximized_section;
    for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
    {
        model->custom_layout_docks[panel_index] = model->panels[panel_index].dock;
        model->custom_layout_last_docked_zones[panel_index] = model->panels[panel_index].last_docked_zone;
    }
    model->custom_layout_left_dock_width = model->left_dock_width;
    model->custom_layout_right_dock_width = model->right_dock_width;
    model->custom_layout_ui_scale = model->ui_scale;
    snprintf(model->custom_layout_name, sizeof(model->custom_layout_name), "%s", name);
    model->custom_layout_valid = true;
    snprintf(model->last_action, sizeof(model->last_action), "Custom workspace saved: %s", model->custom_layout_name);
    return true;
}

bool sandbox3d_workspace_save_custom_layout_slot(
    sandbox3d_workspace_model* model,
    size_t slot_index,
    const char* name)
{
    sandbox3d_workspace_custom_layout_slot* slot;
    size_t panel_index;

    if (slot_index == 0U)
    {
        return sandbox3d_workspace_save_custom_layout(model, name);
    }
    if (model == NULL || slot_index >= SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_SLOT_COUNT ||
        !sandbox3d_workspace_custom_layout_name_is_valid(name) ||
        model->active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE ||
        model->resize_target != SANDBOX3D_WORKSPACE_RESIZE_NONE ||
        model->topology_transaction_active || model->tab_drag_active ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }

    slot = &model->custom_layout_slots[slot_index - 1U];
    memcpy(slot->nodes, model->topology_nodes, sizeof(slot->nodes));
    slot->root = model->topology_root;
    slot->closed_sections_mask = model->closed_sections_mask;
    slot->maximized_section = model->maximized_section;
    for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
    {
        slot->docks[panel_index] = model->panels[panel_index].dock;
        slot->last_docked_zones[panel_index] = model->panels[panel_index].last_docked_zone;
    }
    slot->left_dock_width = model->left_dock_width;
    slot->right_dock_width = model->right_dock_width;
    slot->ui_scale = model->ui_scale;
    snprintf(slot->name, sizeof(slot->name), "%s", name);
    slot->valid = true;
    snprintf(model->last_action, sizeof(model->last_action), "Workspace slot %zu saved: %s", slot_index, slot->name);
    return true;
}

bool sandbox3d_workspace_has_custom_layout(
    const sandbox3d_workspace_model* model)
{
    return model != NULL && model->custom_layout_valid;
}

bool sandbox3d_workspace_has_custom_layout_slot(
    const sandbox3d_workspace_model* model,
    size_t slot_index)
{
    if (model == NULL || slot_index >= SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_SLOT_COUNT)
    {
        return false;
    }
    return slot_index == 0U
        ? model->custom_layout_valid
        : model->custom_layout_slots[slot_index - 1U].valid;
}

const char* sandbox3d_workspace_custom_layout_name(
    const sandbox3d_workspace_model* model)
{
    return model != NULL && model->custom_layout_valid && model->custom_layout_name[0] != '\0'
        ? model->custom_layout_name
        : "Custom";
}

const char* sandbox3d_workspace_custom_layout_slot_name(
    const sandbox3d_workspace_model* model,
    size_t slot_index)
{
    if (!sandbox3d_workspace_has_custom_layout_slot(model, slot_index))
    {
        return "Empty";
    }
    return slot_index == 0U
        ? sandbox3d_workspace_custom_layout_name(model)
        : model->custom_layout_slots[slot_index - 1U].name;
}

bool sandbox3d_workspace_apply_custom_layout(
    sandbox3d_workspace_model* model)
{
    sandbox3d_workspace_model candidate;
    size_t panel_index;

    if (model == NULL || !model->custom_layout_valid ||
        model->active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE ||
        model->resize_target != SANDBOX3D_WORKSPACE_RESIZE_NONE ||
        model->topology_transaction_active || model->tab_drag_active)
    {
        return false;
    }

    candidate = *model;
    memcpy(candidate.topology_nodes, model->custom_layout_nodes, sizeof(candidate.topology_nodes));
    candidate.topology_root = model->custom_layout_root;
    candidate.closed_sections_mask = model->custom_layout_closed_sections_mask;
    candidate.maximized_section = model->custom_layout_maximized_section;
    candidate.left_dock_width = model->custom_layout_left_dock_width;
    candidate.right_dock_width = model->custom_layout_right_dock_width;
    candidate.ui_scale = model->custom_layout_ui_scale;
    for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
    {
        candidate.panels[panel_index].dock = model->custom_layout_docks[panel_index];
        candidate.panels[panel_index].last_docked_zone = model->custom_layout_last_docked_zones[panel_index];
        candidate.panels[panel_index].detached_window_id = 0U;
    }
    candidate.named_layout = SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
    candidate.topology_transaction_active = false;
    candidate.topology_transaction_root = UINT16_MAX;
    candidate.active_divider_node = UINT16_MAX;
    candidate.active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    candidate.divider_close_preview = false;
    candidate.divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.closed_snapshot_valid = false;
    candidate.active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    candidate.tab_drag_active = false;
    sandbox3d_workspace_rebuild_dock_lists(&candidate);
    if (!sandbox3d_workspace_topology_is_valid(&candidate))
    {
        return false;
    }

    sandbox3d_workspace_begin_topology_transaction(model);
    memcpy(model->panels, candidate.panels, sizeof(model->panels));
    memcpy(model->left_dock_panels, candidate.left_dock_panels, sizeof(model->left_dock_panels));
    memcpy(model->right_dock_panels, candidate.right_dock_panels, sizeof(model->right_dock_panels));
    model->left_dock_panel_count = candidate.left_dock_panel_count;
    model->right_dock_panel_count = candidate.right_dock_panel_count;
    memcpy(model->topology_nodes, candidate.topology_nodes, sizeof(model->topology_nodes));
    model->topology_root = candidate.topology_root;
    model->closed_sections_mask = candidate.closed_sections_mask;
    model->maximized_section = candidate.maximized_section;
    model->left_dock_width = candidate.left_dock_width;
    model->right_dock_width = candidate.right_dock_width;
    model->ui_scale = candidate.ui_scale;
    model->named_layout = candidate.named_layout;
    sandbox3d_workspace_commit_topology_transaction(model);
    model->active_divider_node = UINT16_MAX;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->closed_snapshot_valid = false;
    snprintf(model->last_action, sizeof(model->last_action), "Custom workspace restored: %s", model->custom_layout_name);
    return true;
}

bool sandbox3d_workspace_apply_custom_layout_slot(
    sandbox3d_workspace_model* model,
    size_t slot_index)
{
    sandbox3d_workspace_custom_layout_slot slot;
    sandbox3d_workspace_model candidate;
    size_t panel_index;

    if (slot_index == 0U)
    {
        return sandbox3d_workspace_apply_custom_layout(model);
    }
    if (model == NULL || slot_index >= SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_SLOT_COUNT ||
        !sandbox3d_workspace_has_custom_layout_slot(model, slot_index) ||
        model->active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE ||
        model->resize_target != SANDBOX3D_WORKSPACE_RESIZE_NONE ||
        model->topology_transaction_active || model->tab_drag_active)
    {
        return false;
    }

    slot = model->custom_layout_slots[slot_index - 1U];
    candidate = *model;
    memcpy(candidate.topology_nodes, slot.nodes, sizeof(candidate.topology_nodes));
    candidate.topology_root = slot.root;
    candidate.closed_sections_mask = slot.closed_sections_mask;
    candidate.maximized_section = slot.maximized_section;
    candidate.left_dock_width = slot.left_dock_width;
    candidate.right_dock_width = slot.right_dock_width;
    candidate.ui_scale = slot.ui_scale;
    for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
    {
        candidate.panels[panel_index].dock = slot.docks[panel_index];
        candidate.panels[panel_index].last_docked_zone = slot.last_docked_zones[panel_index];
        candidate.panels[panel_index].detached_window_id = 0U;
    }
    candidate.named_layout = SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
    candidate.topology_transaction_active = false;
    candidate.topology_transaction_root = UINT16_MAX;
    candidate.active_divider_node = UINT16_MAX;
    candidate.active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    candidate.divider_close_preview = false;
    candidate.divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.closed_snapshot_valid = false;
    candidate.active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    candidate.tab_drag_active = false;
    sandbox3d_workspace_rebuild_dock_lists(&candidate);
    if (!sandbox3d_workspace_topology_is_valid(&candidate))
    {
        return false;
    }

    sandbox3d_workspace_begin_topology_transaction(model);
    memcpy(model->panels, candidate.panels, sizeof(model->panels));
    memcpy(model->left_dock_panels, candidate.left_dock_panels, sizeof(model->left_dock_panels));
    memcpy(model->right_dock_panels, candidate.right_dock_panels, sizeof(model->right_dock_panels));
    model->left_dock_panel_count = candidate.left_dock_panel_count;
    model->right_dock_panel_count = candidate.right_dock_panel_count;
    memcpy(model->topology_nodes, candidate.topology_nodes, sizeof(model->topology_nodes));
    model->topology_root = candidate.topology_root;
    model->closed_sections_mask = candidate.closed_sections_mask;
    model->maximized_section = candidate.maximized_section;
    model->left_dock_width = candidate.left_dock_width;
    model->right_dock_width = candidate.right_dock_width;
    model->ui_scale = candidate.ui_scale;
    model->named_layout = candidate.named_layout;
    sandbox3d_workspace_commit_topology_transaction(model);
    model->active_divider_node = UINT16_MAX;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->closed_snapshot_valid = false;
    snprintf(model->last_action, sizeof(model->last_action), "Workspace slot restored: %s", slot.name);
    return true;
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
        {820.0f, 94.0f, 396.0f, 672.0f},
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
    model->ui_scale = 1.0f;
    model->hovered_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->keyboard_focus_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->drag_start_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->drag_start_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->tab_drop_target = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->drag_origin_valid = false;
    model->active_tab_drag_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_tab = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_target_index = 0U;
    model->tab_drag_active = false;
    model->active_resize_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    model->active_dock_target = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->active_divider_node = UINT16_MAX;
    model->active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->last_divider_click_node = UINT16_MAX;
    model->last_divider_click_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->last_divider_click_time = -1.0;
    model->named_layout = SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT;
    model->topology_transaction_named_layout = SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT;
    model->context_menu_selected_command = 0U;
    model->next_z_order = 5U;
    sandbox3d_workspace_topology_initialize(model);
    snprintf(model->last_action, sizeof(model->last_action), "Layout reset");
}

void sandbox3d_workspace_reset_layout(sandbox3d_workspace_model* model)
{
    bool saved_custom_layout_valid;
    char saved_custom_layout_name[SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_NAME_MAX];
    sandbox3d_workspace_topology_node saved_custom_layout_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t saved_custom_layout_root;
    uint32_t saved_custom_layout_closed_sections_mask;
    sandbox3d_workspace_panel_id saved_custom_layout_maximized_section;
    sandbox3d_workspace_dock_zone saved_custom_layout_docks[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    sandbox3d_workspace_dock_zone saved_custom_layout_last_docked_zones[SANDBOX3D_WORKSPACE_PANEL_COUNT];
    float saved_custom_layout_left_dock_width;
    float saved_custom_layout_right_dock_width;
    float saved_custom_layout_ui_scale;
    sandbox3d_workspace_custom_layout_slot saved_custom_layout_slots[SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_SLOT_COUNT - 1U];

    if (model == NULL)
    {
        return;
    }
    saved_custom_layout_valid = model->custom_layout_valid;
    memcpy(saved_custom_layout_name, model->custom_layout_name, sizeof(saved_custom_layout_name));
    memcpy(saved_custom_layout_nodes, model->custom_layout_nodes, sizeof(saved_custom_layout_nodes));
    saved_custom_layout_root = model->custom_layout_root;
    saved_custom_layout_closed_sections_mask = model->custom_layout_closed_sections_mask;
    saved_custom_layout_maximized_section = model->custom_layout_maximized_section;
    memcpy(saved_custom_layout_docks, model->custom_layout_docks, sizeof(saved_custom_layout_docks));
    memcpy(
        saved_custom_layout_last_docked_zones,
        model->custom_layout_last_docked_zones,
        sizeof(saved_custom_layout_last_docked_zones));
    saved_custom_layout_left_dock_width = model->custom_layout_left_dock_width;
    saved_custom_layout_right_dock_width = model->custom_layout_right_dock_width;
    saved_custom_layout_ui_scale = model->custom_layout_ui_scale;
    memcpy(saved_custom_layout_slots, model->custom_layout_slots, sizeof(saved_custom_layout_slots));

    sandbox3d_workspace_model_reset(model);
    memcpy(model->custom_layout_slots, saved_custom_layout_slots, sizeof(model->custom_layout_slots));
    if (!saved_custom_layout_valid)
    {
        return;
    }
    model->custom_layout_valid = true;
    memcpy(model->custom_layout_name, saved_custom_layout_name, sizeof(model->custom_layout_name));
    memcpy(model->custom_layout_nodes, saved_custom_layout_nodes, sizeof(model->custom_layout_nodes));
    model->custom_layout_root = saved_custom_layout_root;
    model->custom_layout_closed_sections_mask = saved_custom_layout_closed_sections_mask;
    model->custom_layout_maximized_section = saved_custom_layout_maximized_section;
    memcpy(model->custom_layout_docks, saved_custom_layout_docks, sizeof(model->custom_layout_docks));
    memcpy(
        model->custom_layout_last_docked_zones,
        saved_custom_layout_last_docked_zones,
        sizeof(model->custom_layout_last_docked_zones));
    model->custom_layout_left_dock_width = saved_custom_layout_left_dock_width;
    model->custom_layout_right_dock_width = saved_custom_layout_right_dock_width;
    model->custom_layout_ui_scale = saved_custom_layout_ui_scale;
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
    model->drag_start_section = sandbox3d_workspace_get_topology_section_for_tab(model, panel_id);
    model->drag_start_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->tab_drop_target = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->drag_origin_valid = true;
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
    sandbox3d_workspace_dock_zone original_dock;
    if (panel == NULL || panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING)
    {
        return;
    }

    original_dock = panel->dock;
    panel->floating_rect = current_rect;
    (void)framebuffer_width;
    (void)framebuffer_height;
    sandbox3d_workspace_enforce_minimum_floating_size(panel);
    sandbox3d_workspace_remove_panel_from_docks(model, panel_id);
    panel->dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    sandbox3d_workspace_bring_to_front(model, panel_id);
    model->active_drag_panel = panel_id;
    model->drag_start_section = sandbox3d_workspace_get_topology_section_for_tab(model, panel_id);
    model->drag_start_dock = original_dock;
    model->tab_drop_target = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->drag_origin_valid = true;
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
    model->drag_start_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->drag_start_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->tab_drop_target = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->drag_origin_valid = false;
    model->active_tab_drag_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_tab = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_target_index = 0U;
    model->tab_drag_active = false;
    model->active_resize_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    model->active_dock_target = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->active_divider_node = UINT16_MAX;
    model->active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->context_menu_open = false;
    model->context_menu_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->section_chooser_open = false;
    model->section_chooser_source = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->section_chooser_selected_index = 0U;
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

void sandbox3d_workspace_cancel_panel_drag(sandbox3d_workspace_model* model)
{
    sandbox3d_workspace_panel* panel;

    if (model == NULL || model->active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_NONE ||
        !model->drag_origin_valid)
    {
        return;
    }
    panel = sandbox3d_workspace_get_panel(model, model->active_drag_panel);
    if (panel != NULL &&
        (model->drag_start_dock == SANDBOX3D_WORKSPACE_DOCK_LEFT ||
         model->drag_start_dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT))
    {
        sandbox3d_workspace_dock_panel(model, model->active_drag_panel, model->drag_start_dock);
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

static henka_ui_rect sandbox3d_workspace_topology_divider_hit_rect_scaled(
    henka_ui_rect divider_rect,
    sandbox3d_workspace_split_orientation orientation,
    float ui_scale)
{
    const float hit_width = SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH *
        sandbox3d_workspace_clamp_float(
            ui_scale,
            SANDBOX3D_WORKSPACE_UI_SCALE_MIN,
            SANDBOX3D_WORKSPACE_UI_SCALE_MAX);
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

henka_ui_rect sandbox3d_workspace_topology_divider_hit_rect(
    henka_ui_rect divider_rect,
    sandbox3d_workspace_split_orientation orientation)
{
    return sandbox3d_workspace_topology_divider_hit_rect_scaled(
        divider_rect, orientation, 1.0f);
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
        out_layout->divider_node_indices[index] = UINT16_MAX;
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

static void sandbox3d_workspace_capture_history_state(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_layout_history_state* state)
{
    if (model == NULL || state == NULL)
    {
        return;
    }
    *state = (sandbox3d_workspace_layout_history_state){0};
    memcpy(state->panels, model->panels, sizeof(model->panels));
    memcpy(state->left_dock_panels, model->left_dock_panels, sizeof(model->left_dock_panels));
    memcpy(state->right_dock_panels, model->right_dock_panels, sizeof(model->right_dock_panels));
    state->left_dock_panel_count = model->left_dock_panel_count;
    state->right_dock_panel_count = model->right_dock_panel_count;
    state->left_dock_width = model->left_dock_width;
    state->right_dock_width = model->right_dock_width;
    state->ui_scale = model->ui_scale;
    memcpy(state->topology_nodes, model->topology_nodes, sizeof(model->topology_nodes));
    state->topology_root = model->topology_root;
    state->named_layout = model->named_layout;
    state->closed_sections_mask = model->closed_sections_mask;
    state->maximized_section = model->maximized_section;
    state->closed_snapshot_valid = model->closed_snapshot_valid;
    memcpy(state->closed_snapshot_nodes, model->closed_snapshot_nodes, sizeof(model->closed_snapshot_nodes));
    state->closed_snapshot_root = model->closed_snapshot_root;
    state->closed_snapshot_mask = model->closed_snapshot_mask;
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
    sandbox3d_workspace_capture_history_state(model, &model->topology_transaction_state);
    model->topology_transaction_root = model->topology_root;
    model->topology_transaction_named_layout = model->named_layout;
    model->topology_transaction_result_named_layout = SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
    model->topology_transaction_active = true;
}

void sandbox3d_workspace_commit_topology_transaction(sandbox3d_workspace_model* model)
{
    if (model == NULL)
    {
        return;
    }
    if (model->topology_transaction_active)
    {
        model->named_layout = model->topology_transaction_result_named_layout;
        if (model->undo_history_count >= SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX)
        {
            memmove(
                &model->undo_history[0],
                &model->undo_history[1],
                (SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX - 1U) * sizeof(model->undo_history[0]));
            memmove(
                &model->undo_after_history[0],
                &model->undo_after_history[1],
                (SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX - 1U) * sizeof(model->undo_after_history[0]));
            model->undo_history_count = SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX - 1U;
        }
        model->undo_history[model->undo_history_count] = model->topology_transaction_state;
        sandbox3d_workspace_capture_history_state(
            model, &model->undo_after_history[model->undo_history_count]);
        ++model->undo_history_count;
        model->redo_history_count = 0U;
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
    memcpy(model->panels, model->topology_transaction_state.panels, sizeof(model->panels));
    memcpy(
        model->left_dock_panels,
        model->topology_transaction_state.left_dock_panels,
        sizeof(model->left_dock_panels));
    memcpy(
        model->right_dock_panels,
        model->topology_transaction_state.right_dock_panels,
        sizeof(model->right_dock_panels));
    model->left_dock_panel_count = model->topology_transaction_state.left_dock_panel_count;
    model->right_dock_panel_count = model->topology_transaction_state.right_dock_panel_count;
    model->left_dock_width = model->topology_transaction_state.left_dock_width;
    model->right_dock_width = model->topology_transaction_state.right_dock_width;
    model->ui_scale = model->topology_transaction_state.ui_scale;
    memcpy(model->topology_nodes, model->topology_transaction_state.topology_nodes, sizeof(model->topology_nodes));
    model->topology_root = model->topology_transaction_state.topology_root;
    model->named_layout = model->topology_transaction_state.named_layout;
    model->closed_sections_mask = model->topology_transaction_state.closed_sections_mask;
    model->maximized_section = model->topology_transaction_state.maximized_section;
    model->closed_snapshot_valid = model->topology_transaction_state.closed_snapshot_valid;
    memcpy(model->closed_snapshot_nodes, model->topology_transaction_state.closed_snapshot_nodes, sizeof(model->closed_snapshot_nodes));
    model->closed_snapshot_root = model->topology_transaction_state.closed_snapshot_root;
    model->closed_snapshot_mask = model->topology_transaction_state.closed_snapshot_mask;
    model->topology_transaction_active = false;
    model->topology_transaction_root = UINT16_MAX;
    model->topology_transaction_result_named_layout = SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
    model->active_divider_node = UINT16_MAX;
    model->active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
}

static void sandbox3d_workspace_restore_history_state(
    sandbox3d_workspace_model* model,
    const sandbox3d_workspace_layout_history_state* state)
{
    if (model == NULL || state == NULL)
    {
        return;
    }
    memcpy(model->panels, state->panels, sizeof(model->panels));
    memcpy(model->left_dock_panels, state->left_dock_panels, sizeof(model->left_dock_panels));
    memcpy(model->right_dock_panels, state->right_dock_panels, sizeof(model->right_dock_panels));
    model->left_dock_panel_count = state->left_dock_panel_count;
    model->right_dock_panel_count = state->right_dock_panel_count;
    model->left_dock_width = state->left_dock_width;
    model->right_dock_width = state->right_dock_width;
    model->ui_scale = state->ui_scale;
    memcpy(model->topology_nodes, state->topology_nodes, sizeof(model->topology_nodes));
    model->topology_root = state->topology_root;
    model->named_layout = state->named_layout;
    model->closed_sections_mask = state->closed_sections_mask;
    model->maximized_section = state->maximized_section;
    model->closed_snapshot_valid = state->closed_snapshot_valid;
    memcpy(model->closed_snapshot_nodes, state->closed_snapshot_nodes, sizeof(model->closed_snapshot_nodes));
    model->closed_snapshot_root = state->closed_snapshot_root;
    model->closed_snapshot_mask = state->closed_snapshot_mask;
    model->topology_transaction_active = false;
    model->topology_transaction_root = UINT16_MAX;
    model->active_drag_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_tab = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->tab_drag_active = false;
    model->active_resize_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->resize_target = SANDBOX3D_WORKSPACE_RESIZE_NONE;
    model->active_divider_node = UINT16_MAX;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
}

bool sandbox3d_workspace_can_undo(const sandbox3d_workspace_model* model)
{
    return model != NULL && model->undo_history_count > 0U;
}

bool sandbox3d_workspace_can_redo(const sandbox3d_workspace_model* model)
{
    return model != NULL && model->redo_history_count > 0U;
}

bool sandbox3d_workspace_undo(sandbox3d_workspace_model* model)
{
    sandbox3d_workspace_layout_history_state post_state;
    if (model == NULL || model->undo_history_count == 0U ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    post_state = model->undo_after_history[model->undo_history_count - 1U];
    sandbox3d_workspace_restore_history_state(model, &model->undo_history[model->undo_history_count - 1U]);
    --model->undo_history_count;
    if (model->redo_history_count >= SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX)
    {
        memmove(&model->redo_history[0], &model->redo_history[1],
            (SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX - 1U) * sizeof(model->redo_history[0]));
        model->redo_history_count = SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX - 1U;
    }
    model->redo_history[model->redo_history_count++] = post_state;
    snprintf(model->last_action, sizeof(model->last_action), "Layout undo");
    return sandbox3d_workspace_topology_is_valid(model);
}

bool sandbox3d_workspace_redo(sandbox3d_workspace_model* model)
{
    sandbox3d_workspace_layout_history_state current;
    sandbox3d_workspace_layout_history_state post_state;
    if (model == NULL || model->redo_history_count == 0U ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    sandbox3d_workspace_capture_history_state(model, &current);
    post_state = model->redo_history[model->redo_history_count - 1U];
    sandbox3d_workspace_restore_history_state(model, &post_state);
    --model->redo_history_count;
    if (model->undo_history_count >= SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX)
    {
        memmove(&model->undo_history[0], &model->undo_history[1],
            (SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX - 1U) * sizeof(model->undo_history[0]));
        model->undo_history_count = SANDBOX3D_WORKSPACE_LAYOUT_HISTORY_MAX - 1U;
    }
    model->undo_history[model->undo_history_count++] = current;
    model->undo_after_history[model->undo_history_count - 1U] = post_state;
    snprintf(model->last_action, sizeof(model->last_action), "Layout redo");
    return sandbox3d_workspace_topology_is_valid(model);
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
    model->active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    model->active_divider_start_ratio = node->data.split.ratio;
    model->active_divider_start_pointer = pointer;
    model->divider_close_preview = false;
    model->divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    snprintf(model->last_action, sizeof(model->last_action), "Divider %u moving", (unsigned int)node_index);
}

void sandbox3d_workspace_begin_dock_divider_drag(
    sandbox3d_workspace_model* model,
    uint16_t node_index,
    henka_vec2 pointer,
    sandbox3d_workspace_dock_zone dock_zone)
{
    if (dock_zone != SANDBOX3D_WORKSPACE_DOCK_LEFT &&
        dock_zone != SANDBOX3D_WORKSPACE_DOCK_RIGHT)
    {
        return;
    }
    sandbox3d_workspace_begin_divider_drag(model, node_index, pointer);
    if (model != NULL && model->active_divider_node == node_index)
    {
        model->active_divider_dock = dock_zone;
    }
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

static bool sandbox3d_workspace_topology_find_dock_node_rect(
    const sandbox3d_workspace_model* model,
    uint16_t node_index,
    sandbox3d_workspace_dock_zone dock_zone,
    henka_ui_rect bounds,
    uint16_t target,
    henka_ui_rect* out_rect)
{
    const sandbox3d_workspace_topology_node* node;
    bool first_has;
    bool second_has;

    if (model == NULL || out_rect == NULL ||
        node_index >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
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
    first_has = sandbox3d_workspace_topology_node_has_dock_section(
        model, node->data.split.first_child, dock_zone);
    second_has = sandbox3d_workspace_topology_node_has_dock_section(
        model, node->data.split.second_child, dock_zone);
    if (!first_has && !second_has)
    {
        return false;
    }
    if (!first_has)
    {
        return sandbox3d_workspace_topology_find_dock_node_rect(
            model, node->data.split.second_child, dock_zone, bounds, target, out_rect);
    }
    if (!second_has)
    {
        return sandbox3d_workspace_topology_find_dock_node_rect(
            model, node->data.split.first_child, dock_zone, bounds, target, out_rect);
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
        if (available <= 0.0f)
        {
            return false;
        }
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
        return sandbox3d_workspace_topology_find_dock_node_rect(
                   model, node->data.split.first_child, dock_zone, first, target, out_rect) ||
            sandbox3d_workspace_topology_find_dock_node_rect(
                model, node->data.split.second_child, dock_zone, second, target, out_rect);
    }
}

void sandbox3d_workspace_update_divider_drag(
    sandbox3d_workspace_model* model,
    henka_vec2 pointer,
    henka_ui_rect bounds)
{
    sandbox3d_workspace_topology_node* node;
    henka_ui_rect node_rect = {0.0f, 0.0f, 0.0f, 0.0f};
    float available;
    float delta;
    float ratio;
    float raw_ratio;
    const sandbox3d_workspace_topology_node* first_child;
    const sandbox3d_workspace_topology_node* second_child;

    if (model == NULL || model->active_divider_node == UINT16_MAX ||
        (model->active_divider_dock != SANDBOX3D_WORKSPACE_DOCK_LEFT &&
         model->active_divider_dock != SANDBOX3D_WORKSPACE_DOCK_RIGHT &&
         !sandbox3d_workspace_topology_find_node_rect(
             model, model->topology_root, bounds, model->active_divider_node, &node_rect)) ||
        ((model->active_divider_dock == SANDBOX3D_WORKSPACE_DOCK_LEFT ||
          model->active_divider_dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT) &&
         !sandbox3d_workspace_topology_find_dock_node_rect(
             model,
             model->topology_root,
             model->active_divider_dock,
             bounds,
             model->active_divider_node,
             &node_rect)))
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

bool sandbox3d_workspace_cycle_topology_section_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    int direction)
{
    const int node_index = sandbox3d_workspace_find_section_node(model, section_id);
    sandbox3d_workspace_topology_node* node;
    int next_index;

    if (model == NULL || node_index < 0 || (direction != -1 && direction != 1) ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    node = sandbox3d_workspace_topology_get_node(model, (uint16_t)node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
        node->data.section.tab_count < 2U)
    {
        return false;
    }
    next_index = (int)node->data.section.active_tab + direction;
    if (next_index < 0)
    {
        next_index = (int)node->data.section.tab_count - 1;
    }
    else if (next_index >= (int)node->data.section.tab_count)
    {
        next_index = 0;
    }
    node->data.section.active_tab = (uint8_t)next_index;
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s tab selected",
        sandbox3d_workspace_panel_name(node->data.section.tabs[next_index]));
    return true;
}

bool sandbox3d_workspace_section_is_closed(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    return model != NULL && section_id >= 0 && section_id < SANDBOX3D_WORKSPACE_PANEL_COUNT &&
        (model->closed_sections_mask & (1U << section_id)) != 0U;
}

size_t sandbox3d_workspace_get_closed_section_count(
    const sandbox3d_workspace_model* model)
{
    size_t count = 0U;
    sandbox3d_workspace_panel_id section_id;

    if (model == NULL)
    {
        return 0U;
    }
    for (section_id = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
         section_id < SANDBOX3D_WORKSPACE_PANEL_COUNT;
         section_id = (sandbox3d_workspace_panel_id)(section_id + 1))
    {
        if (sandbox3d_workspace_section_is_closed(model, section_id))
        {
            count += 1U;
        }
    }
    return count;
}

sandbox3d_workspace_panel_id sandbox3d_workspace_get_closed_section_at(
    const sandbox3d_workspace_model* model,
    size_t closed_index)
{
    sandbox3d_workspace_panel_id section_id;

    if (model == NULL)
    {
        return SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    for (section_id = SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
         section_id < SANDBOX3D_WORKSPACE_PANEL_COUNT;
         section_id = (sandbox3d_workspace_panel_id)(section_id + 1))
    {
        if (sandbox3d_workspace_section_is_closed(model, section_id))
        {
            if (closed_index == 0U)
            {
                return section_id;
            }
            closed_index -= 1U;
        }
    }
    return SANDBOX3D_WORKSPACE_PANEL_NONE;
}

bool sandbox3d_workspace_cycle_section_chooser_selection(
    sandbox3d_workspace_model* model,
    int direction)
{
    const size_t count = sandbox3d_workspace_get_closed_section_count(model);

    if (model == NULL || count == 0U || direction == 0)
    {
        return false;
    }
    if (model->section_chooser_selected_index >= count)
    {
        model->section_chooser_selected_index = 0U;
    }
    if (direction < 0)
    {
        model->section_chooser_selected_index =
            model->section_chooser_selected_index == 0U
                ? count - 1U
                : model->section_chooser_selected_index - 1U;
    }
    else
    {
        model->section_chooser_selected_index =
            (model->section_chooser_selected_index + 1U) % count;
    }
    return true;
}

bool sandbox3d_workspace_close_section(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    const int section_index = sandbox3d_workspace_find_section_node(model, section_id);
    const sandbox3d_workspace_topology_node* section_node = section_index >= 0
        ? sandbox3d_workspace_topology_get_node_const(model, (uint16_t)section_index)
        : NULL;
    size_t tab_index;
    if (model == NULL || section_id < 0 || section_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
        sandbox3d_workspace_section_is_closed(model, section_id) ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    sandbox3d_workspace_begin_topology_transaction(model);
    sandbox3d_workspace_copy_topology(
        model->closed_snapshot_nodes,
        &model->closed_snapshot_root,
        model->topology_nodes,
        model->topology_root);
    model->closed_snapshot_mask = model->closed_sections_mask;
    model->closed_snapshot_valid = true;
    if (section_node != NULL && section_node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        for (tab_index = 0U; tab_index < section_node->data.section.tab_count; ++tab_index)
        {
            model->closed_sections_mask |= 1U << section_node->data.section.tabs[tab_index];
        }
    }
    else
    {
        model->closed_sections_mask |= 1U << section_id;
    }
    if (!sandbox3d_workspace_remove_section_node(model, section_id))
    {
        sandbox3d_workspace_rollback_topology_transaction(model);
        return false;
    }
    if (model->maximized_section == section_id)
    {
        model->maximized_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    }
    snprintf(model->last_action, sizeof(model->last_action), "%s closed", sandbox3d_workspace_panel_name(section_id));
    if (!sandbox3d_workspace_topology_is_valid(model))
    {
        sandbox3d_workspace_rollback_topology_transaction(model);
        return false;
    }
    sandbox3d_workspace_commit_topology_transaction(model);
    return true;
}

bool sandbox3d_workspace_close_active_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id)
{
    const int section_index = sandbox3d_workspace_find_section_node(model, section_id);
    sandbox3d_workspace_topology_node* section;
    sandbox3d_workspace_panel_id closed_tab;
    size_t active_tab;

    if (model == NULL || section_index < 0 || !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    section = sandbox3d_workspace_topology_get_node(model, (uint16_t)section_index);
    if (section == NULL || section->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
        section->data.section.tab_count == 0U)
    {
        return false;
    }
    sandbox3d_workspace_begin_topology_transaction(model);
    if (section->data.section.tab_count == 1U)
    {
        return sandbox3d_workspace_close_section(model, section_id);
    }

    sandbox3d_workspace_copy_topology(
        model->closed_snapshot_nodes,
        &model->closed_snapshot_root,
        model->topology_nodes,
        model->topology_root);
    model->closed_snapshot_mask = model->closed_sections_mask;
    model->closed_snapshot_valid = true;
    active_tab = section->data.section.active_tab;
    closed_tab = section->data.section.tabs[active_tab];
    memmove(
        &section->data.section.tabs[active_tab],
        &section->data.section.tabs[active_tab + 1U],
        (section->data.section.tab_count - active_tab - 1U) * sizeof(section->data.section.tabs[0]));
    --section->data.section.tab_count;
    if (active_tab >= section->data.section.tab_count)
    {
        active_tab = section->data.section.tab_count - 1U;
    }
    section->data.section.active_tab = (uint8_t)active_tab;
    if (!sandbox3d_workspace_topology_is_valid(model))
    {
        sandbox3d_workspace_rollback_topology_transaction(model);
        return false;
    }
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s tab closed; %u tab%s remain",
        sandbox3d_workspace_panel_name(closed_tab),
        (unsigned int)section->data.section.tab_count,
        section->data.section.tab_count == 1U ? "" : "s");
    sandbox3d_workspace_commit_topology_transaction(model);
    return true;
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

bool sandbox3d_workspace_move_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id target_section,
    sandbox3d_workspace_panel_id tab_id)
{
    sandbox3d_workspace_panel_id source_section;
    int source_index;
    int target_index;
    sandbox3d_workspace_topology_node* source;
    sandbox3d_workspace_topology_node* target;
    sandbox3d_workspace_topology_node old_closed_snapshot_nodes[SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES];
    uint16_t old_closed_snapshot_root;
    uint32_t old_closed_sections_mask;
    uint32_t old_closed_snapshot_mask;
    bool old_closed_snapshot_valid;
    size_t tab_index;
    size_t old_active_tab;
    bool found = false;

    if (model == NULL || target_section < 0 ||
        target_section >= SANDBOX3D_WORKSPACE_PANEL_COUNT || tab_id < 0 ||
        tab_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
    {
        return false;
    }
    source_section = sandbox3d_workspace_get_topology_section_for_tab(model, tab_id);
    source_index = sandbox3d_workspace_find_section_node(model, source_section);
    target_index = sandbox3d_workspace_find_section_node(model, target_section);
    if (source_section == SANDBOX3D_WORKSPACE_PANEL_NONE ||
        source_section == target_section || target_section < 0 ||
        target_section >= SANDBOX3D_WORKSPACE_PANEL_COUNT || tab_id < 0 ||
        tab_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT || source_index < 0 ||
        target_index < 0 || !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    source = &model->topology_nodes[source_index];
    target = &model->topology_nodes[target_index];
    if (source->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
        target->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
        target->data.section.tab_count >= SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS)
    {
        return false;
    }
    for (tab_index = 0U; tab_index < source->data.section.tab_count; ++tab_index)
    {
        if (source->data.section.tabs[tab_index] == tab_id)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        return false;
    }

    memcpy(old_closed_snapshot_nodes, model->closed_snapshot_nodes, sizeof(old_closed_snapshot_nodes));
    old_closed_snapshot_root = model->closed_snapshot_root;
    old_closed_sections_mask = model->closed_sections_mask;
    old_closed_snapshot_mask = model->closed_snapshot_mask;
    old_closed_snapshot_valid = model->closed_snapshot_valid;
    sandbox3d_workspace_begin_topology_transaction(model);
    source = &model->topology_nodes[source_index];
    target = &model->topology_nodes[target_index];
    old_active_tab = source->data.section.active_tab;
    if (source->data.section.tab_count == 1U)
    {
        sandbox3d_workspace_copy_topology(
            model->closed_snapshot_nodes,
            &model->closed_snapshot_root,
            model->topology_nodes,
            model->topology_root);
        model->closed_snapshot_mask = model->closed_sections_mask;
        model->closed_snapshot_valid = true;
        if (!sandbox3d_workspace_remove_section_node(model, source_section))
        {
            sandbox3d_workspace_rollback_topology_transaction(model);
            memcpy(model->closed_snapshot_nodes, old_closed_snapshot_nodes, sizeof(old_closed_snapshot_nodes));
            model->closed_snapshot_root = old_closed_snapshot_root;
            model->closed_sections_mask = old_closed_sections_mask;
            model->closed_snapshot_mask = old_closed_snapshot_mask;
            model->closed_snapshot_valid = old_closed_snapshot_valid;
            return false;
        }
        model->closed_sections_mask |= 1U << source_section;
    }
    else
    {
        memmove(
            &source->data.section.tabs[tab_index],
            &source->data.section.tabs[tab_index + 1U],
            sizeof(source->data.section.tabs[0]) * (source->data.section.tab_count - tab_index - 1U));
        --source->data.section.tab_count;
        if (old_active_tab > tab_index)
        {
            --old_active_tab;
        }
        if (old_active_tab >= source->data.section.tab_count)
        {
            old_active_tab = source->data.section.tab_count - 1U;
        }
        source->data.section.active_tab = (uint8_t)old_active_tab;
    }
    target = &model->topology_nodes[target_index];
    target->data.section.tabs[target->data.section.tab_count] = tab_id;
    target->data.section.active_tab = target->data.section.tab_count;
    ++target->data.section.tab_count;
    if (!sandbox3d_workspace_topology_is_valid(model))
    {
        sandbox3d_workspace_rollback_topology_transaction(model);
        memcpy(model->closed_snapshot_nodes, old_closed_snapshot_nodes, sizeof(old_closed_snapshot_nodes));
        model->closed_snapshot_root = old_closed_snapshot_root;
        model->closed_sections_mask = old_closed_sections_mask;
        model->closed_snapshot_mask = old_closed_snapshot_mask;
        model->closed_snapshot_valid = old_closed_snapshot_valid;
        return false;
    }
    sandbox3d_workspace_commit_topology_transaction(model);
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s moved into %s tab group",
        sandbox3d_workspace_panel_name(tab_id),
        sandbox3d_workspace_panel_name(target_section));
    return true;
}

bool sandbox3d_workspace_begin_tab_drag(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id)
{
    const int node_index = sandbox3d_workspace_find_section_node(model, section_id);
    const sandbox3d_workspace_topology_node* node;
    size_t tab_index;

    if (model == NULL || model->tab_drag_active || node_index < 0 ||
        !sandbox3d_workspace_topology_is_valid(model))
    {
        return false;
    }
    node = sandbox3d_workspace_topology_get_node_const(model, (uint16_t)node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
    {
        return false;
    }
    for (tab_index = 0U; tab_index < node->data.section.tab_count; ++tab_index)
    {
        if (node->data.section.tabs[tab_index] == tab_id)
        {
            sandbox3d_workspace_begin_topology_transaction(model);
            model->active_tab_drag_section = section_id;
            model->active_tab_drag_tab = tab_id;
            model->active_tab_drag_target_index = tab_index;
            model->tab_drag_active = true;
            snprintf(
                model->last_action,
                sizeof(model->last_action),
                "%s tab moving",
                sandbox3d_workspace_panel_name(tab_id));
            return true;
        }
    }
    return false;
}

void sandbox3d_workspace_update_tab_drag(
    sandbox3d_workspace_model* model,
    size_t target_index)
{
    const size_t tab_count = model != NULL
        ? sandbox3d_workspace_get_topology_section_tab_count(
            model, model->active_tab_drag_section)
        : 0U;
    if (model == NULL || !model->tab_drag_active || tab_count == 0U)
    {
        return;
    }
    model->active_tab_drag_target_index = target_index < tab_count
        ? target_index
        : tab_count - 1U;
}

bool sandbox3d_workspace_reorder_tab(
    sandbox3d_workspace_model* model,
    sandbox3d_workspace_panel_id section_id,
    sandbox3d_workspace_panel_id tab_id,
    size_t target_index)
{
    const int node_index = sandbox3d_workspace_find_section_node(model, section_id);
    sandbox3d_workspace_topology_node* node;
    sandbox3d_workspace_panel_id active_tab;
    size_t source_index = 0U;
    size_t tab_index;
    bool found = false;
    bool owns_transaction;

    if (model == NULL || node_index < 0 ||
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
            source_index = tab_index;
            found = true;
            break;
        }
    }
    if (!found || node->data.section.tab_count == 0U)
    {
        return false;
    }
    if (target_index >= node->data.section.tab_count)
    {
        target_index = node->data.section.tab_count - 1U;
    }
    active_tab = node->data.section.tabs[node->data.section.active_tab];
    owns_transaction = !model->topology_transaction_active;
    if (owns_transaction)
    {
        sandbox3d_workspace_begin_topology_transaction(model);
    }
    if (source_index != target_index)
    {
        const sandbox3d_workspace_panel_id moved_tab = node->data.section.tabs[source_index];
        if (source_index < target_index)
        {
            memmove(
                &node->data.section.tabs[source_index],
                &node->data.section.tabs[source_index + 1U],
                sizeof(node->data.section.tabs[0]) * (target_index - source_index));
        }
        else
        {
            memmove(
                &node->data.section.tabs[target_index + 1U],
                &node->data.section.tabs[target_index],
                sizeof(node->data.section.tabs[0]) * (source_index - target_index));
        }
        node->data.section.tabs[target_index] = moved_tab;
    }
    for (tab_index = 0U; tab_index < node->data.section.tab_count; ++tab_index)
    {
        if (node->data.section.tabs[tab_index] == active_tab)
        {
            node->data.section.active_tab = (uint8_t)tab_index;
            break;
        }
    }
    if (!sandbox3d_workspace_topology_is_valid(model))
    {
        if (owns_transaction)
        {
            sandbox3d_workspace_rollback_topology_transaction(model);
        }
        return false;
    }
    if (owns_transaction)
    {
        sandbox3d_workspace_commit_topology_transaction(model);
    }
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        "%s tab moved to position %u",
        sandbox3d_workspace_panel_name(tab_id),
        (unsigned int)(target_index + 1U));
    return true;
}

bool sandbox3d_workspace_commit_tab_drag(sandbox3d_workspace_model* model)
{
    if (model == NULL || !model->tab_drag_active)
    {
        return false;
    }
    if (!sandbox3d_workspace_reorder_tab(
            model,
            model->active_tab_drag_section,
            model->active_tab_drag_tab,
            model->active_tab_drag_target_index))
    {
        sandbox3d_workspace_rollback_topology_transaction(model);
        return false;
    }
    (void)sandbox3d_workspace_set_topology_section_active_tab(
        model,
        model->active_tab_drag_section,
        model->active_tab_drag_tab);
    sandbox3d_workspace_commit_topology_transaction(model);
    return true;
}

void sandbox3d_workspace_cancel_tab_drag(sandbox3d_workspace_model* model)
{
    if (model == NULL || !model->tab_drag_active)
    {
        return;
    }
    sandbox3d_workspace_rollback_topology_transaction(model);
    model->active_tab_drag_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_tab = SANDBOX3D_WORKSPACE_PANEL_NONE;
    model->active_tab_drag_target_index = 0U;
    model->tab_drag_active = false;
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

void sandbox3d_workspace_equalize_divider(
    sandbox3d_workspace_model* model,
    uint16_t node_index)
{
    sandbox3d_workspace_topology_node* node;

    if (model == NULL || !sandbox3d_workspace_topology_is_valid(model))
    {
        return;
    }
    node = sandbox3d_workspace_topology_get_node(model, node_index);
    if (node == NULL || node->type != SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
    {
        return;
    }
    node->data.split.ratio = 0.5f;
    snprintf(model->last_action, sizeof(model->last_action), "Divider %u equalized", (unsigned int)node_index);
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
