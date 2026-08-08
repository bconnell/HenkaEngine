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

#endif
