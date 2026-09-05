#include "workspace_persistence.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char* g_setting_key_workspace_topology_version =
    "ui.workspace.topology.version";
static const char* g_setting_key_workspace_left_dock_width =
    "ui.workspace.left_dock_width";
static const char* g_setting_key_workspace_right_dock_width =
    "ui.workspace.right_dock_width";
static const char* g_setting_key_workspace_named_layout =
    "ui.workspace.named_layout";
static const int g_workspace_topology_settings_version = 5;
static const int g_workspace_custom_layout_settings_version = 1;
static const int g_workspace_custom_layout_slots_settings_version = 1;

bool sandbox3d_workspace_persistence_load_panels(
    sandbox3d_workspace_model* model,
    const henka_settings* settings)
{
    sandbox3d_workspace_model candidate;
    size_t index;

    if (model == NULL || settings == NULL)
    {
        return false;
    }
    candidate = *model;
    candidate.left_dock_width = henka_settings_get_float(
        settings, g_setting_key_workspace_left_dock_width, candidate.left_dock_width);
    candidate.right_dock_width = henka_settings_get_float(
        settings, g_setting_key_workspace_right_dock_width, candidate.right_dock_width);
    if (!isfinite(candidate.left_dock_width) ||
        candidate.left_dock_width < 280.0f || candidate.left_dock_width > 720.0f ||
        !isfinite(candidate.right_dock_width) ||
        candidate.right_dock_width < 300.0f || candidate.right_dock_width > 720.0f)
    {
        return false;
    }

    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        char key[96];
        sandbox3d_workspace_panel* panel = &candidate.panels[index];
        int dock;
        int last_docked_zone;
        henka_ui_rect floating_rect;

        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.dock", index);
        dock = henka_settings_get_int(settings, key, (int)panel->dock);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.last_docked", index);
        last_docked_zone = henka_settings_get_int(settings, key, (int)panel->last_docked_zone);
        if (dock < SANDBOX3D_WORKSPACE_DOCK_LEFT || dock > SANDBOX3D_WORKSPACE_DOCK_DETACHED ||
            last_docked_zone < SANDBOX3D_WORKSPACE_DOCK_LEFT ||
            last_docked_zone > SANDBOX3D_WORKSPACE_DOCK_RIGHT)
        {
            return false;
        }
        panel->last_docked_zone = (sandbox3d_workspace_dock_zone)last_docked_zone;
        panel->dock = (sandbox3d_workspace_dock_zone)dock;
        if (panel->dock == SANDBOX3D_WORKSPACE_DOCK_DETACHED)
        {
            panel->dock = panel->last_docked_zone;
        }
        panel->detached_window_id = 0U;

        floating_rect = panel->floating_rect;
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.x", index);
        floating_rect.x = henka_settings_get_float(settings, key, floating_rect.x);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.y", index);
        floating_rect.y = henka_settings_get_float(settings, key, floating_rect.y);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.width", index);
        floating_rect.width = henka_settings_get_float(settings, key, floating_rect.width);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.height", index);
        floating_rect.height = henka_settings_get_float(settings, key, floating_rect.height);
        if (!sandbox3d_workspace_sanitize_floating_rect(
                panel,
                floating_rect,
                &floating_rect))
        {
            return false;
        }
        panel->floating_rect = floating_rect;
    }
    sandbox3d_workspace_rebuild_dock_lists(&candidate);
    *model = candidate;
    return true;
}

void sandbox3d_workspace_persistence_save_panels(
    const sandbox3d_workspace_model* model,
    henka_settings* settings)
{
    size_t index;

    if (model == NULL || settings == NULL)
    {
        return;
    }
    (void)henka_settings_set_float(settings, g_setting_key_workspace_left_dock_width, model->left_dock_width);
    (void)henka_settings_set_float(settings, g_setting_key_workspace_right_dock_width, model->right_dock_width);
    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        char key[96];
        const sandbox3d_workspace_panel* panel = &model->panels[index];
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.dock", index);
        (void)henka_settings_set_int(settings, key, (int)panel->dock);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.last_docked", index);
        (void)henka_settings_set_int(settings, key, (int)panel->last_docked_zone);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.x", index);
        (void)henka_settings_set_float(settings, key, panel->floating_rect.x);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.y", index);
        (void)henka_settings_set_float(settings, key, panel->floating_rect.y);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.width", index);
        (void)henka_settings_set_float(settings, key, panel->floating_rect.width);
        snprintf(key, sizeof(key), "ui.workspace.panel.%zu.floating.height", index);
        (void)henka_settings_set_float(settings, key, panel->floating_rect.height);
    }
}

bool sandbox3d_workspace_persistence_load_topology(
    sandbox3d_workspace_model* model,
    const henka_settings* settings)
{
    sandbox3d_workspace_model candidate;
    char key[96];
    size_t index;
    int value;
    int stored_version;

    if (model == NULL || settings == NULL ||
        !henka_settings_has_key(settings, g_setting_key_workspace_topology_version))
    {
        return true;
    }
    stored_version = henka_settings_get_int(
        settings, g_setting_key_workspace_topology_version, 0);
    if (stored_version != g_workspace_topology_settings_version)
    {
        return false;
    }

    candidate = *model;
    candidate.named_layout = henka_settings_has_key(
            settings, g_setting_key_workspace_named_layout)
        ? sandbox3d_workspace_parse_named_layout(henka_settings_get_string(
            settings, g_setting_key_workspace_named_layout, "custom"))
        : SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM;
    value = henka_settings_get_int(
        settings,
        "ui.workspace.topology.root",
        (int)candidate.topology_root);
    if (value < 0 || value >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
    {
        return false;
    }
    candidate.topology_root = (uint16_t)value;
    value = henka_settings_get_int(
        settings,
        "ui.workspace.topology.closed_mask",
        (int)candidate.closed_sections_mask);
    if (value < 0 || value >= (1 << SANDBOX3D_WORKSPACE_PANEL_COUNT))
    {
        return false;
    }
    candidate.closed_sections_mask = (uint32_t)value;
    value = henka_settings_get_int(
        settings,
        "ui.workspace.topology.maximized_section",
        (int)candidate.maximized_section);
    if (value < SANDBOX3D_WORKSPACE_PANEL_NONE || value >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
    {
        return false;
    }
    candidate.maximized_section = (sandbox3d_workspace_panel_id)value;
    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++index)
    {
        sandbox3d_workspace_topology_node* node = &candidate.topology_nodes[index];
        int parent;
        int type;

        snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.type", index);
        type = henka_settings_get_int(settings, key, (int)node->type);
        snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.parent", index);
        parent = henka_settings_get_int(settings, key, (int)node->parent);
        if (type < SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED ||
            type > SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
            (parent != (int)UINT16_MAX &&
             (parent < 0 || parent >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)))
        {
            return false;
        }
        node->type = (sandbox3d_workspace_topology_node_type)type;
        node->parent = (uint16_t)parent;
        if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
        {
            int first_child;
            int second_child;
            int orientation;

            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.first", index);
            first_child = henka_settings_get_int(settings, key, (int)node->data.split.first_child);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.second", index);
            second_child = henka_settings_get_int(settings, key, (int)node->data.split.second_child);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.orientation", index);
            orientation = henka_settings_get_int(settings, key, (int)node->data.split.orientation);
            if (first_child < 0 || first_child >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
                second_child < 0 || second_child >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES ||
                orientation < SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL ||
                orientation > SANDBOX3D_WORKSPACE_SPLIT_VERTICAL)
            {
                return false;
            }
            node->data.split.first_child = (uint16_t)first_child;
            node->data.split.second_child = (uint16_t)second_child;
            node->data.split.orientation = (sandbox3d_workspace_split_orientation)orientation;
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.ratio", index);
            node->data.split.ratio = henka_settings_get_float(settings, key, node->data.split.ratio);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.minimum_first", index);
            node->data.split.minimum_first = henka_settings_get_float(settings, key, node->data.split.minimum_first);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.minimum_second", index);
            node->data.split.minimum_second = henka_settings_get_float(settings, key, node->data.split.minimum_second);
        }
        else if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
        {
            int section_id;
            int tab_count;
            int active_tab;
            size_t tab_index;

            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.section", index);
            section_id = henka_settings_get_int(settings, key, (int)node->data.section.section_id);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.tab_count", index);
            tab_count = henka_settings_get_int(settings, key, (int)node->data.section.tab_count);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.active_tab", index);
            active_tab = henka_settings_get_int(settings, key, (int)node->data.section.active_tab);
            if (section_id < 0 || section_id >= SANDBOX3D_WORKSPACE_PANEL_COUNT ||
                tab_count <= 0 || tab_count > (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS ||
                active_tab < 0 || active_tab >= tab_count)
            {
                return false;
            }
            node->data.section.section_id = (sandbox3d_workspace_panel_id)section_id;
            node->data.section.tab_count = (uint8_t)tab_count;
            node->data.section.active_tab = (uint8_t)active_tab;
            for (tab_index = 0U;
                 tab_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS;
                 ++tab_index)
            {
                if (tab_index >= (size_t)tab_count)
                {
                    node->data.section.tabs[tab_index] =
                        SANDBOX3D_WORKSPACE_PANEL_NONE;
                    continue;
                }

                snprintf(
                    key,
                    sizeof(key),
                    "ui.workspace.topology.node.%zu.tab.%zu",
                    index,
                    tab_index);
                value = henka_settings_get_int(
                    settings,
                    key,
                    (int)node->data.section.tabs[tab_index]);
                if (value < 0 ||
                    value >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
                {
                    return false;
                }
                node->data.section.tabs[tab_index] =
                    (sandbox3d_workspace_panel_id)value;
            }
        }
    }
    candidate.topology_transaction_active = false;
    candidate.topology_transaction_root = UINT16_MAX;
    candidate.active_divider_node = UINT16_MAX;
    candidate.active_divider_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    candidate.divider_close_preview = false;
    candidate.divider_close_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.closed_snapshot_valid = false;
    candidate.drag_start_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.drag_start_dock = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
    candidate.tab_drop_target = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.drag_origin_valid = false;
    candidate.active_tab_drag_section = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.active_tab_drag_tab = SANDBOX3D_WORKSPACE_PANEL_NONE;
    candidate.active_tab_drag_target_index = 0U;
    candidate.tab_drag_active = false;
    if (!sandbox3d_workspace_topology_is_valid(&candidate))
    {
        return false;
    }
    memcpy(model->topology_nodes, candidate.topology_nodes, sizeof(model->topology_nodes));
    model->topology_root = candidate.topology_root;
    model->closed_sections_mask = candidate.closed_sections_mask;
    model->maximized_section = candidate.maximized_section;
    model->named_layout = candidate.named_layout;
    snprintf(
        model->last_action,
        sizeof(model->last_action),
        stored_version < g_workspace_topology_settings_version
            ? "Saved workspace topology migrated"
            : "Saved workspace topology restored");
    return true;
}

void sandbox3d_workspace_persistence_save_topology(
    const sandbox3d_workspace_model* model,
    henka_settings* settings)
{
    char key[96];
    size_t index;

    if (model == NULL || settings == NULL || !sandbox3d_workspace_topology_is_valid(model))
    {
        return;
    }
    (void)henka_settings_set_int(
        settings,
        g_setting_key_workspace_topology_version,
        g_workspace_topology_settings_version);
    (void)henka_settings_set_string(
        settings,
        g_setting_key_workspace_named_layout,
        sandbox3d_workspace_named_layout_setting_value(model->named_layout));
    (void)henka_settings_set_int(settings, "ui.workspace.topology.root", (int)model->topology_root);
    (void)henka_settings_set_int(settings, "ui.workspace.topology.closed_mask", (int)model->closed_sections_mask);
    (void)henka_settings_set_int(settings, "ui.workspace.topology.maximized_section", (int)model->maximized_section);
    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++index)
    {
        const sandbox3d_workspace_topology_node* node = &model->topology_nodes[index];
        snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.type", index);
        (void)henka_settings_set_int(settings, key, (int)node->type);
        snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.parent", index);
        (void)henka_settings_set_int(settings, key, (int)node->parent);
        if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
        {
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.first", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.split.first_child);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.second", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.split.second_child);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.orientation", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.split.orientation);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.ratio", index);
            (void)henka_settings_set_float(settings, key, node->data.split.ratio);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.minimum_first", index);
            (void)henka_settings_set_float(settings, key, node->data.split.minimum_first);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.minimum_second", index);
            (void)henka_settings_set_float(settings, key, node->data.split.minimum_second);
        }
        else if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
        {
            size_t tab_index;
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.section", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.section.section_id);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.tab_count", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.section.tab_count);
            snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.active_tab", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.section.active_tab);
            for (tab_index = 0U; tab_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS; ++tab_index)
            {
                snprintf(key, sizeof(key), "ui.workspace.topology.node.%zu.tab.%zu", index, tab_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.section.tabs[tab_index]);
            }
        }
    }
}

bool sandbox3d_workspace_persistence_load_custom_layout(
    sandbox3d_workspace_model* model,
    const henka_settings* settings)
{
    sandbox3d_workspace_model candidate;
    char key[128];
    size_t index;
    int value;
    int version;

    if (model == NULL || settings == NULL ||
        !henka_settings_has_key(settings, "ui.workspace.custom_layout.version"))
    {
        return true;
    }
    version = henka_settings_get_int(settings, "ui.workspace.custom_layout.version", 0);
    if (version != g_workspace_custom_layout_settings_version)
    {
        return false;
    }

    candidate = *model;
    candidate.custom_layout_valid = false;
    snprintf(
        candidate.custom_layout_name,
        sizeof(candidate.custom_layout_name),
        "%s",
        henka_settings_get_string(settings, "ui.workspace.custom_layout.name", "Custom"));
    if (!sandbox3d_workspace_save_custom_layout(&candidate, candidate.custom_layout_name))
    {
        return false;
    }

    value = henka_settings_get_int(settings, "ui.workspace.custom_layout.root", (int)candidate.custom_layout_root);
    if (value < 0 || value >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
    {
        return false;
    }
    candidate.custom_layout_root = (uint16_t)value;
    value = henka_settings_get_int(
        settings,
        "ui.workspace.custom_layout.closed_mask",
        (int)candidate.custom_layout_closed_sections_mask);
    if (value < 0 || value >= (1 << SANDBOX3D_WORKSPACE_PANEL_COUNT))
    {
        return false;
    }
    candidate.custom_layout_closed_sections_mask = (uint32_t)value;
    value = henka_settings_get_int(
        settings,
        "ui.workspace.custom_layout.maximized_section",
        (int)candidate.custom_layout_maximized_section);
    if (value < SANDBOX3D_WORKSPACE_PANEL_NONE || value >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
    {
        return false;
    }
    candidate.custom_layout_maximized_section = (sandbox3d_workspace_panel_id)value;
    candidate.custom_layout_left_dock_width = henka_settings_get_float(
        settings,
        "ui.workspace.custom_layout.left_dock_width",
        candidate.custom_layout_left_dock_width);
    candidate.custom_layout_right_dock_width = henka_settings_get_float(
        settings,
        "ui.workspace.custom_layout.right_dock_width",
        candidate.custom_layout_right_dock_width);
    candidate.custom_layout_ui_scale = henka_settings_get_float(
        settings,
        "ui.workspace.custom_layout.ui_scale",
        candidate.custom_layout_ui_scale);
    if (!isfinite(candidate.custom_layout_left_dock_width) ||
        candidate.custom_layout_left_dock_width < 280.0f || candidate.custom_layout_left_dock_width > 720.0f ||
        !isfinite(candidate.custom_layout_right_dock_width) ||
        candidate.custom_layout_right_dock_width < 300.0f || candidate.custom_layout_right_dock_width > 720.0f ||
        !isfinite(candidate.custom_layout_ui_scale) ||
        candidate.custom_layout_ui_scale < SANDBOX3D_WORKSPACE_UI_SCALE_MIN ||
        candidate.custom_layout_ui_scale > SANDBOX3D_WORKSPACE_UI_SCALE_MAX)
    {
        return false;
    }
    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        snprintf(key, sizeof(key), "ui.workspace.custom_layout.panel.%zu.dock", index);
        value = henka_settings_get_int(settings, key, (int)candidate.custom_layout_docks[index]);
        if (value < SANDBOX3D_WORKSPACE_DOCK_LEFT || value > SANDBOX3D_WORKSPACE_DOCK_DETACHED)
        {
            return false;
        }
        candidate.custom_layout_docks[index] = (sandbox3d_workspace_dock_zone)value;
        snprintf(key, sizeof(key), "ui.workspace.custom_layout.panel.%zu.last_docked", index);
        value = henka_settings_get_int(settings, key, (int)candidate.custom_layout_last_docked_zones[index]);
        if (value < SANDBOX3D_WORKSPACE_DOCK_LEFT || value > SANDBOX3D_WORKSPACE_DOCK_RIGHT)
        {
            return false;
        }
        candidate.custom_layout_last_docked_zones[index] = (sandbox3d_workspace_dock_zone)value;
    }
    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++index)
    {
        sandbox3d_workspace_topology_node* node = &candidate.custom_layout_nodes[index];
        int parent;
        int type;

        snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.type", index);
        type = henka_settings_get_int(settings, key, (int)node->type);
        snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.parent", index);
        parent = henka_settings_get_int(settings, key, (int)node->parent);
        if (type < SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED ||
            type > SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
            (parent != (int)UINT16_MAX &&
             (parent < 0 || parent >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)))
        {
            return false;
        }
        node->type = (sandbox3d_workspace_topology_node_type)type;
        node->parent = (uint16_t)parent;
        if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
        {
            int orientation;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.first", index);
            value = henka_settings_get_int(settings, key, (int)node->data.split.first_child);
            if (value < 0 || value >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES) return false;
            node->data.split.first_child = (uint16_t)value;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.second", index);
            value = henka_settings_get_int(settings, key, (int)node->data.split.second_child);
            if (value < 0 || value >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES) return false;
            node->data.split.second_child = (uint16_t)value;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.orientation", index);
            orientation = henka_settings_get_int(settings, key, (int)node->data.split.orientation);
            if (orientation < SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL || orientation > SANDBOX3D_WORKSPACE_SPLIT_VERTICAL) return false;
            node->data.split.orientation = (sandbox3d_workspace_split_orientation)orientation;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.ratio", index);
            node->data.split.ratio = henka_settings_get_float(settings, key, node->data.split.ratio);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.minimum_first", index);
            node->data.split.minimum_first = henka_settings_get_float(settings, key, node->data.split.minimum_first);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.minimum_second", index);
            node->data.split.minimum_second = henka_settings_get_float(settings, key, node->data.split.minimum_second);
        }
        else if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
        {
            size_t tab_index;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.section", index);
            value = henka_settings_get_int(settings, key, (int)node->data.section.section_id);
            if (value < 0 || value >= SANDBOX3D_WORKSPACE_PANEL_COUNT) return false;
            node->data.section.section_id = (sandbox3d_workspace_panel_id)value;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.tab_count", index);
            value = henka_settings_get_int(settings, key, (int)node->data.section.tab_count);
            if (value <= 0 || value > (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS) return false;
            node->data.section.tab_count = (uint8_t)value;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.active_tab", index);
            value = henka_settings_get_int(settings, key, (int)node->data.section.active_tab);
            if (value < 0 || value >= (int)node->data.section.tab_count) return false;
            node->data.section.active_tab = (uint8_t)value;
            for (tab_index = 0U;
                 tab_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS;
                 ++tab_index)
            {
                if (tab_index >= (size_t)node->data.section.tab_count)
                {
                    node->data.section.tabs[tab_index] =
                        SANDBOX3D_WORKSPACE_PANEL_NONE;
                    continue;
                }

                snprintf(
                    key,
                    sizeof(key),
                    "ui.workspace.custom_layout.node.%zu.tab.%zu",
                    index,
                    tab_index);
                value = henka_settings_get_int(
                    settings,
                    key,
                    (int)node->data.section.tabs[tab_index]);
                if (value < 0 ||
                    value >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
                {
                    return false;
                }
                node->data.section.tabs[tab_index] =
                    (sandbox3d_workspace_panel_id)value;
            }
        }
    }
    candidate.custom_layout_valid = true;
    candidate.custom_layout_name[sizeof(candidate.custom_layout_name) - 1U] = '\0';
    memcpy(candidate.topology_nodes, candidate.custom_layout_nodes, sizeof(candidate.topology_nodes));
    candidate.topology_root = candidate.custom_layout_root;
    candidate.closed_sections_mask = candidate.custom_layout_closed_sections_mask;
    candidate.maximized_section = candidate.custom_layout_maximized_section;
    if (!sandbox3d_workspace_topology_is_valid(&candidate))
    {
        return false;
    }
    *model = candidate;
    return true;
}

void sandbox3d_workspace_persistence_save_custom_layout(
    const sandbox3d_workspace_model* model,
    henka_settings* settings)
{
    char key[128];
    size_t index;

    if (model == NULL || settings == NULL || !sandbox3d_workspace_has_custom_layout(model))
    {
        return;
    }
    (void)henka_settings_set_int(settings, "ui.workspace.custom_layout.version", g_workspace_custom_layout_settings_version);
    (void)henka_settings_set_string(settings, "ui.workspace.custom_layout.name", sandbox3d_workspace_custom_layout_name(model));
    (void)henka_settings_set_int(settings, "ui.workspace.custom_layout.root", (int)model->custom_layout_root);
    (void)henka_settings_set_int(settings, "ui.workspace.custom_layout.closed_mask", (int)model->custom_layout_closed_sections_mask);
    (void)henka_settings_set_int(settings, "ui.workspace.custom_layout.maximized_section", (int)model->custom_layout_maximized_section);
    (void)henka_settings_set_float(settings, "ui.workspace.custom_layout.left_dock_width", model->custom_layout_left_dock_width);
    (void)henka_settings_set_float(settings, "ui.workspace.custom_layout.right_dock_width", model->custom_layout_right_dock_width);
    (void)henka_settings_set_float(settings, "ui.workspace.custom_layout.ui_scale", model->custom_layout_ui_scale);
    for (index = 0U; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        snprintf(key, sizeof(key), "ui.workspace.custom_layout.panel.%zu.dock", index);
        (void)henka_settings_set_int(settings, key, (int)model->custom_layout_docks[index]);
        snprintf(key, sizeof(key), "ui.workspace.custom_layout.panel.%zu.last_docked", index);
        (void)henka_settings_set_int(settings, key, (int)model->custom_layout_last_docked_zones[index]);
    }
    for (index = 0U; index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++index)
    {
        const sandbox3d_workspace_topology_node* node = &model->custom_layout_nodes[index];
        snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.type", index);
        (void)henka_settings_set_int(settings, key, (int)node->type);
        snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.parent", index);
        (void)henka_settings_set_int(settings, key, (int)node->parent);
        if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
        {
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.first", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.split.first_child);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.second", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.split.second_child);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.orientation", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.split.orientation);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.ratio", index);
            (void)henka_settings_set_float(settings, key, node->data.split.ratio);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.minimum_first", index);
            (void)henka_settings_set_float(settings, key, node->data.split.minimum_first);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.minimum_second", index);
            (void)henka_settings_set_float(settings, key, node->data.split.minimum_second);
        }
        else if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
        {
            size_t tab_index;
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.section", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.section.section_id);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.tab_count", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.section.tab_count);
            snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.active_tab", index);
            (void)henka_settings_set_int(settings, key, (int)node->data.section.active_tab);
            for (tab_index = 0U; tab_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS; ++tab_index)
            {
                snprintf(key, sizeof(key), "ui.workspace.custom_layout.node.%zu.tab.%zu", index, tab_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.section.tabs[tab_index]);
            }
        }
    }
}

bool sandbox3d_workspace_persistence_load_custom_layout_slots(
    sandbox3d_workspace_model* model,
    const henka_settings* settings)
{
    size_t slot_index;

    if (model == NULL || settings == NULL)
    {
        return false;
    }
    for (slot_index = 1U; slot_index < SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_SLOT_COUNT; ++slot_index)
    {
        sandbox3d_workspace_custom_layout_slot slot;
        sandbox3d_workspace_model candidate;
        char prefix[96];
        char key[160];
        const char* name;
        size_t name_length;
        size_t panel_index;
        size_t node_index;
        int value;
        int version;
        bool name_valid;

        snprintf(prefix, sizeof(prefix), "ui.workspace.custom_layout.slot.%zu", slot_index);
        snprintf(key, sizeof(key), "%s.version", prefix);
        if (!henka_settings_has_key(settings, key))
        {
            continue;
        }
        version = henka_settings_get_int(settings, key, 0);
        if (version != g_workspace_custom_layout_slots_settings_version)
        {
            continue;
        }

        memset(&slot, 0, sizeof(slot));
        snprintf(key, sizeof(key), "%s.name", prefix);
        name = henka_settings_get_string(settings, key, "Slot");
        name_length = name != NULL ? strlen(name) : 0U;
        name_valid = name != NULL && name_length > 0U &&
            name_length < SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_NAME_MAX;
        for (size_t name_index = 0U; name_valid && name_index < name_length; ++name_index)
        {
            const unsigned char character = (unsigned char)name[name_index];
            if (character < 0x20U || character == 0x7fU)
            {
                name_valid = false;
            }
        }
        if (!name_valid)
        {
            continue;
        }
        snprintf(slot.name, sizeof(slot.name), "%s", name);
        slot.root = 0U;
        snprintf(key, sizeof(key), "%s.root", prefix);
        value = henka_settings_get_int(settings, key, (int)slot.root);
        if (value < 0 || value >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)
        {
            continue;
        }
        slot.root = (uint16_t)value;
        snprintf(key, sizeof(key), "%s.closed_mask", prefix);
        value = henka_settings_get_int(settings, key, 0);
        if (value < 0 || value >= (1 << SANDBOX3D_WORKSPACE_PANEL_COUNT))
        {
            continue;
        }
        slot.closed_sections_mask = (uint32_t)value;
        snprintf(key, sizeof(key), "%s.maximized_section", prefix);
        value = henka_settings_get_int(settings, key, SANDBOX3D_WORKSPACE_PANEL_NONE);
        if (value < SANDBOX3D_WORKSPACE_PANEL_NONE || value >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
        {
            continue;
        }
        slot.maximized_section = (sandbox3d_workspace_panel_id)value;
        snprintf(key, sizeof(key), "%s.left_dock_width", prefix);
        slot.left_dock_width = henka_settings_get_float(settings, key, 320.0f);
        snprintf(key, sizeof(key), "%s.right_dock_width", prefix);
        slot.right_dock_width = henka_settings_get_float(settings, key, 356.0f);
        snprintf(key, sizeof(key), "%s.ui_scale", prefix);
        slot.ui_scale = henka_settings_get_float(settings, key, 1.0f);
        if (!isfinite(slot.left_dock_width) || slot.left_dock_width < 280.0f || slot.left_dock_width > 720.0f ||
            !isfinite(slot.right_dock_width) || slot.right_dock_width < 300.0f || slot.right_dock_width > 720.0f ||
            !isfinite(slot.ui_scale) || slot.ui_scale < SANDBOX3D_WORKSPACE_UI_SCALE_MIN ||
            slot.ui_scale > SANDBOX3D_WORKSPACE_UI_SCALE_MAX)
        {
            continue;
        }
        for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
        {
            snprintf(key, sizeof(key), "%s.panel.%zu.dock", prefix, panel_index);
            value = henka_settings_get_int(settings, key, SANDBOX3D_WORKSPACE_DOCK_LEFT);
            if (value < SANDBOX3D_WORKSPACE_DOCK_LEFT || value > SANDBOX3D_WORKSPACE_DOCK_DETACHED)
            {
                name_valid = false;
                break;
            }
            slot.docks[panel_index] = (sandbox3d_workspace_dock_zone)value;
            snprintf(key, sizeof(key), "%s.panel.%zu.last_docked", prefix, panel_index);
            value = henka_settings_get_int(settings, key, SANDBOX3D_WORKSPACE_DOCK_LEFT);
            if (value < SANDBOX3D_WORKSPACE_DOCK_LEFT || value > SANDBOX3D_WORKSPACE_DOCK_RIGHT)
            {
                name_valid = false;
                break;
            }
            slot.last_docked_zones[panel_index] = (sandbox3d_workspace_dock_zone)value;
        }
        if (!name_valid)
        {
            continue;
        }
        for (node_index = 0U; node_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++node_index)
        {
            sandbox3d_workspace_topology_node* node = &slot.nodes[node_index];
            int parent;
            int type;

            snprintf(key, sizeof(key), "%s.node.%zu.type", prefix, node_index);
            type = henka_settings_get_int(settings, key, SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED);
            snprintf(key, sizeof(key), "%s.node.%zu.parent", prefix, node_index);
            parent = henka_settings_get_int(settings, key, (int)UINT16_MAX);
            if (type < SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_UNUSED ||
                type > SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION ||
                (parent != (int)UINT16_MAX && (parent < 0 || parent >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES)))
            {
                name_valid = false;
                break;
            }
            node->type = (sandbox3d_workspace_topology_node_type)type;
            node->parent = (uint16_t)parent;
            if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
            {
                int orientation;
                snprintf(key, sizeof(key), "%s.node.%zu.first", prefix, node_index);
                value = henka_settings_get_int(settings, key, 0);
                if (value < 0 || value >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES) { name_valid = false; break; }
                node->data.split.first_child = (uint16_t)value;
                snprintf(key, sizeof(key), "%s.node.%zu.second", prefix, node_index);
                value = henka_settings_get_int(settings, key, 0);
                if (value < 0 || value >= (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES) { name_valid = false; break; }
                node->data.split.second_child = (uint16_t)value;
                snprintf(key, sizeof(key), "%s.node.%zu.orientation", prefix, node_index);
                orientation = henka_settings_get_int(settings, key, 0);
                if (orientation < SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL || orientation > SANDBOX3D_WORKSPACE_SPLIT_VERTICAL) { name_valid = false; break; }
                node->data.split.orientation = (sandbox3d_workspace_split_orientation)orientation;
                snprintf(key, sizeof(key), "%s.node.%zu.ratio", prefix, node_index);
                node->data.split.ratio = henka_settings_get_float(settings, key, 0.5f);
                snprintf(key, sizeof(key), "%s.node.%zu.minimum_first", prefix, node_index);
                node->data.split.minimum_first = henka_settings_get_float(settings, key, 1.0f);
                snprintf(key, sizeof(key), "%s.node.%zu.minimum_second", prefix, node_index);
                node->data.split.minimum_second = henka_settings_get_float(settings, key, 1.0f);
            }
            else if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
            {
                size_t tab_index;
                snprintf(key, sizeof(key), "%s.node.%zu.section", prefix, node_index);
                value = henka_settings_get_int(settings, key, SANDBOX3D_WORKSPACE_PANEL_NONE);
                if (value < 0 || value >= SANDBOX3D_WORKSPACE_PANEL_COUNT) { name_valid = false; break; }
                node->data.section.section_id = (sandbox3d_workspace_panel_id)value;
                snprintf(key, sizeof(key), "%s.node.%zu.tab_count", prefix, node_index);
                value = henka_settings_get_int(settings, key, 0);
                if (value <= 0 || value > (int)SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS) { name_valid = false; break; }
                node->data.section.tab_count = (uint8_t)value;
                snprintf(key, sizeof(key), "%s.node.%zu.active_tab", prefix, node_index);
                value = henka_settings_get_int(settings, key, 0);
                if (value < 0 || value >= (int)node->data.section.tab_count) { name_valid = false; break; }
                node->data.section.active_tab = (uint8_t)value;
                for (tab_index = 0U;
                     tab_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS;
                     ++tab_index)
                {
                    if (tab_index >=
                        (size_t)node->data.section.tab_count)
                    {
                        node->data.section.tabs[tab_index] =
                            SANDBOX3D_WORKSPACE_PANEL_NONE;
                        continue;
                    }

                    snprintf(
                        key,
                        sizeof(key),
                        "%s.node.%zu.tab.%zu",
                        prefix,
                        node_index,
                        tab_index);
                    value = henka_settings_get_int(
                        settings,
                        key,
                        SANDBOX3D_WORKSPACE_PANEL_NONE);
                    if (value < 0 ||
                        value >= SANDBOX3D_WORKSPACE_PANEL_COUNT)
                    {
                        name_valid = false;
                        break;
                    }
                    node->data.section.tabs[tab_index] =
                        (sandbox3d_workspace_panel_id)value;
                }
                if (!name_valid) { break; }
            }
        }
        if (!name_valid)
        {
            continue;
        }
        slot.valid = true;
        candidate = *model;
        candidate.custom_layout_slots[slot_index - 1U] = slot;
        if (sandbox3d_workspace_apply_custom_layout_slot(&candidate, slot_index))
        {
            model->custom_layout_slots[slot_index - 1U] = slot;
        }
    }
    return true;
}

void sandbox3d_workspace_persistence_save_custom_layout_slots(
    const sandbox3d_workspace_model* model,
    henka_settings* settings)
{
    size_t slot_index;

    if (model == NULL || settings == NULL)
    {
        return;
    }
    for (slot_index = 1U; slot_index < SANDBOX3D_WORKSPACE_CUSTOM_LAYOUT_SLOT_COUNT; ++slot_index)
    {
        const sandbox3d_workspace_custom_layout_slot* slot = &model->custom_layout_slots[slot_index - 1U];
        char prefix[96];
        char key[160];
        size_t panel_index;
        size_t node_index;

        if (!slot->valid)
        {
            continue;
        }
        snprintf(prefix, sizeof(prefix), "ui.workspace.custom_layout.slot.%zu", slot_index);
        snprintf(key, sizeof(key), "%s.version", prefix);
        (void)henka_settings_set_int(settings, key, g_workspace_custom_layout_slots_settings_version);
        snprintf(key, sizeof(key), "%s.name", prefix);
        (void)henka_settings_set_string(settings, key, slot->name);
        snprintf(key, sizeof(key), "%s.root", prefix);
        (void)henka_settings_set_int(settings, key, (int)slot->root);
        snprintf(key, sizeof(key), "%s.closed_mask", prefix);
        (void)henka_settings_set_int(settings, key, (int)slot->closed_sections_mask);
        snprintf(key, sizeof(key), "%s.maximized_section", prefix);
        (void)henka_settings_set_int(settings, key, (int)slot->maximized_section);
        snprintf(key, sizeof(key), "%s.left_dock_width", prefix);
        (void)henka_settings_set_float(settings, key, slot->left_dock_width);
        snprintf(key, sizeof(key), "%s.right_dock_width", prefix);
        (void)henka_settings_set_float(settings, key, slot->right_dock_width);
        snprintf(key, sizeof(key), "%s.ui_scale", prefix);
        (void)henka_settings_set_float(settings, key, slot->ui_scale);
        for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
        {
            snprintf(key, sizeof(key), "%s.panel.%zu.dock", prefix, panel_index);
            (void)henka_settings_set_int(settings, key, (int)slot->docks[panel_index]);
            snprintf(key, sizeof(key), "%s.panel.%zu.last_docked", prefix, panel_index);
            (void)henka_settings_set_int(settings, key, (int)slot->last_docked_zones[panel_index]);
        }
        for (node_index = 0U; node_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_NODES; ++node_index)
        {
            const sandbox3d_workspace_topology_node* node = &slot->nodes[node_index];
            snprintf(key, sizeof(key), "%s.node.%zu.type", prefix, node_index);
            (void)henka_settings_set_int(settings, key, (int)node->type);
            snprintf(key, sizeof(key), "%s.node.%zu.parent", prefix, node_index);
            (void)henka_settings_set_int(settings, key, (int)node->parent);
            if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT)
            {
                snprintf(key, sizeof(key), "%s.node.%zu.first", prefix, node_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.split.first_child);
                snprintf(key, sizeof(key), "%s.node.%zu.second", prefix, node_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.split.second_child);
                snprintf(key, sizeof(key), "%s.node.%zu.orientation", prefix, node_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.split.orientation);
                snprintf(key, sizeof(key), "%s.node.%zu.ratio", prefix, node_index);
                (void)henka_settings_set_float(settings, key, node->data.split.ratio);
                snprintf(key, sizeof(key), "%s.node.%zu.minimum_first", prefix, node_index);
                (void)henka_settings_set_float(settings, key, node->data.split.minimum_first);
                snprintf(key, sizeof(key), "%s.node.%zu.minimum_second", prefix, node_index);
                (void)henka_settings_set_float(settings, key, node->data.split.minimum_second);
            }
            else if (node->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SECTION)
            {
                size_t tab_index;
                snprintf(key, sizeof(key), "%s.node.%zu.section", prefix, node_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.section.section_id);
                snprintf(key, sizeof(key), "%s.node.%zu.tab_count", prefix, node_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.section.tab_count);
                snprintf(key, sizeof(key), "%s.node.%zu.active_tab", prefix, node_index);
                (void)henka_settings_set_int(settings, key, (int)node->data.section.active_tab);
                for (tab_index = 0U; tab_index < SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS; ++tab_index)
                {
                    snprintf(key, sizeof(key), "%s.node.%zu.tab.%zu", prefix, node_index, tab_index);
                    (void)henka_settings_set_int(settings, key, (int)node->data.section.tabs[tab_index]);
                }
            }
        }
    }
}
