#ifndef SANDBOX3D_AUTHORING_ASSET_COMMANDS_H
#define SANDBOX3D_AUTHORING_ASSET_COMMANDS_H

#include "authoring_asset_ui.h"

/*
 * Editor command policy for the generic native authoring document.
 *
 * This module owns request construction and the user-data persistence
 * convention.  It deliberately does not register scene entities, bind
 * physics, or change selection; those responsibilities remain with the
 * editor orchestration layer that owns the corresponding runtime state.
 */
henka_result sandbox3d_authoring_asset_commands_new_asset(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name);

henka_result sandbox3d_authoring_asset_commands_add_primitive(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name,
    sandbox3d_authoring_asset_ui_action action,
    const char* part_prefix,
    size_t* out_part_index);

henka_result sandbox3d_authoring_asset_commands_save(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine);

henka_result sandbox3d_authoring_asset_commands_get_manifest_path(
    const char* asset_name,
    char* output,
    size_t output_capacity);

#endif
