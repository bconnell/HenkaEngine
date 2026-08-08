#ifndef SANDBOX3D_OBJECT_DETAILS_TOOLS_H
#define SANDBOX3D_OBJECT_DETAILS_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/assets.h>
#include <henka/result.h>
#include <henka/scene.h>

typedef enum sandbox3d_material_access
{
    SANDBOX3D_MATERIAL_ACCESS_NONE = 0,
    SANDBOX3D_MATERIAL_ACCESS_READ_ONLY,
    SANDBOX3D_MATERIAL_ACCESS_EDITABLE_INSTANCE
} sandbox3d_material_access;

typedef struct sandbox3d_material_editor_binding
{
    henka_entity entity;
    henka_material_instance* instance;
    henka_material_asset* asset;
    bool valid;
} sandbox3d_material_editor_binding;

typedef struct sandbox3d_selected_material_view
{
    sandbox3d_material_access access;
    henka_material material;
    sandbox3d_material_editor_binding* editor_binding;
} sandbox3d_selected_material_view;

henka_result sandbox3d_resolve_selected_material(
    const henka_scene* scene,
    henka_entity entity,
    sandbox3d_material_editor_binding* bindings,
    size_t binding_count,
    sandbox3d_selected_material_view* out_view);


typedef struct sandbox3d_selected_material_display
{
    char material_slot[24];
    char name[64];
    char base_color[64];
    char metallic[24];
    char roughness[24];
    char normal_map[24];
    char emissive[64];
    char alpha_mode[24];
    char double_sided[16];
    char ior[24];
    char transmission[24];
    char mode[32];
} sandbox3d_selected_material_display;

henka_result sandbox3d_format_selected_material_view(
    const sandbox3d_selected_material_view* view,
    sandbox3d_selected_material_display* out_display);
#endif
