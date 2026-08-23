#include "modeling_selection_commands.h"

#include <string.h>

static void sandbox3d_modeling_selection_command_run(
    sandbox3d_authoring_object* object,
    const sandbox3d_authoring_selection_query* query,
    sandbox3d_modeling_selection_command_result* out_result)
{
    out_result->invoked = true;
    out_result->kind = query->kind;
    out_result->result = sandbox3d_authoring_object_select_matching_components(
        object, query);
    out_result->selected_count =
        sandbox3d_authoring_object_get_selected_component_count(object);
}

void sandbox3d_modeling_selection_commands_draw(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    sandbox3d_authoring_object* object,
    sandbox3d_modeling_selection_command_result* out_result)
{
    const float gap = 6.0f;
    const float button_width = (bounds.width - gap * 2.0f) / 3.0f;
    sandbox3d_authoring_selection_query query = {0};
    sandbox3d_authoring_selection_mode mode;

    if (out_result != NULL)
    {
        memset(out_result, 0, sizeof(*out_result));
        out_result->result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (ui == NULL || object == NULL || out_result == NULL ||
        bounds.width < 290.0f || bounds.height < 24.0f)
    {
        return;
    }
    mode = sandbox3d_authoring_object_get_selection_mode(object);
    if (mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        const float edge_button_width = (bounds.width - gap) * 0.5f;
        if (henka_ui_button(ui, "selection_boundary",
                (henka_ui_rect){bounds.x, bounds.y, edge_button_width, 24.0f},
                "Boundary"))
        {
            query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_BOUNDARY;
            sandbox3d_modeling_selection_command_run(object, &query, out_result);
        }
        else if (henka_ui_button(ui, "selection_hard_edges",
                (henka_ui_rect){bounds.x + edge_button_width + gap, bounds.y,
                    edge_button_width, 24.0f},
                "Hard Edges"))
        {
            query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_HARD_EDGE;
            sandbox3d_modeling_selection_command_run(object, &query, out_result);
        }
    }
    else if (mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        if (henka_ui_button(ui, "selection_four_sided",
                (henka_ui_rect){bounds.x, bounds.y, button_width, 24.0f},
                "4-Sided"))
        {
            query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_FACE_SIDE_COUNT;
            query.face_side_count = 4U;
            sandbox3d_modeling_selection_command_run(object, &query, out_result);
        }
        else if (henka_ui_button(ui, "selection_similar_normal",
                (henka_ui_rect){bounds.x + button_width + gap, bounds.y, button_width, 24.0f},
                "Similar Normal"))
        {
            query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_NORMAL;
            query.minimum_normal_dot = 0.98f;
            sandbox3d_modeling_selection_command_run(object, &query, out_result);
        }
        else if (henka_ui_button(ui, "selection_similar_material",
                (henka_ui_rect){bounds.x + (button_width + gap) * 2.0f, bounds.y, button_width, 24.0f},
                "Same Material"))
        {
            query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_MATERIAL_REGION;
            sandbox3d_modeling_selection_command_run(object, &query, out_result);
        }
    }
    else if (henka_ui_button(ui, "selection_similar_vertex_material",
            (henka_ui_rect){bounds.x, bounds.y, bounds.width, 24.0f},
            "Same Material Region"))
    {
        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_MATERIAL_REGION;
        sandbox3d_modeling_selection_command_run(object, &query, out_result);
    }
}
