#ifndef SANDBOX3D_WORKSPACE_PERSISTENCE_H
#define SANDBOX3D_WORKSPACE_PERSISTENCE_H

#include <stdbool.h>

#include <henka/persistence.h>

#include "workspace_tools.h"

bool sandbox3d_workspace_persistence_load_panels(
    sandbox3d_workspace_model* model,
    const henka_settings* settings);
void sandbox3d_workspace_persistence_save_panels(
    const sandbox3d_workspace_model* model,
    henka_settings* settings);
bool sandbox3d_workspace_persistence_load_topology(
    sandbox3d_workspace_model* model,
    const henka_settings* settings);
void sandbox3d_workspace_persistence_save_topology(
    const sandbox3d_workspace_model* model,
    henka_settings* settings);
bool sandbox3d_workspace_persistence_load_custom_layout(
    sandbox3d_workspace_model* model,
    const henka_settings* settings);
void sandbox3d_workspace_persistence_save_custom_layout(
    const sandbox3d_workspace_model* model,
    henka_settings* settings);
bool sandbox3d_workspace_persistence_load_custom_layout_slots(
    sandbox3d_workspace_model* model,
    const henka_settings* settings);
void sandbox3d_workspace_persistence_save_custom_layout_slots(
    const sandbox3d_workspace_model* model,
    henka_settings* settings);

#endif
