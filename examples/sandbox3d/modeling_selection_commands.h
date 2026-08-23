#ifndef SANDBOX3D_MODELING_SELECTION_COMMANDS_H
#define SANDBOX3D_MODELING_SELECTION_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/ui.h>

#include "object_authoring_tools.h"

typedef struct sandbox3d_modeling_selection_command_result
{
    bool invoked;
    henka_result result;
    sandbox3d_authoring_selection_query_kind kind;
    size_t selected_count;
} sandbox3d_modeling_selection_command_result;

void sandbox3d_modeling_selection_commands_draw(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    sandbox3d_authoring_object* object,
    sandbox3d_modeling_selection_command_result* out_result);

#endif
