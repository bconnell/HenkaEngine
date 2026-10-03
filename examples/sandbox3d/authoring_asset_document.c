#include "authoring_asset_document.h"

#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/persistence.h>

enum
{
    SANDBOX3D_AUTHORING_ASSET_NAME_CAPACITY = 64U,
    SANDBOX3D_AUTHORING_ASSET_VERSION = 5
};

#define SANDBOX3D_AUTHORING_ASSET_PATH_CAPACITY 512U

typedef struct sandbox3d_authoring_asset_part
{
    henka_entity entity;
    sandbox3d_authoring_object* object;
    sandbox3d_authoring_primitive_kind kind;
    bool owns_object;
    bool owns_entity;
} sandbox3d_authoring_asset_part;

struct sandbox3d_authoring_asset_document
{
    henka_engine* engine;
    henka_scene* scene;
    char name[SANDBOX3D_AUTHORING_ASSET_NAME_CAPACITY];
    sandbox3d_authoring_provenance provenance;
    uint32_t persisted_revision;
    size_t part_count;
    sandbox3d_authoring_asset_part parts[SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY];
};

static bool sandbox3d_authoring_asset_document_has_part_name(
    const sandbox3d_authoring_asset_document* document,
    const char* name);

static bool sandbox3d_authoring_asset_name_is_valid(const char* name, size_t* out_length)
{
    size_t length;

    if (name == NULL || out_length == NULL)
    {
        return false;
    }

    for (length = 0U; length < SANDBOX3D_AUTHORING_ASSET_NAME_CAPACITY; ++length)
    {
        const unsigned char character = (unsigned char)name[length];

        if (character == '\0')
        {
            *out_length = length;
            return length > 0U;
        }
        if (!isalnum(character) && character != '_' && character != '-')
        {
            return false;
        }
    }

    return false;
}

henka_result sandbox3d_authoring_asset_document_create(
    henka_engine* engine,
    henka_scene* scene,
    const char* name,
    sandbox3d_authoring_asset_document** out_document)
{
    sandbox3d_authoring_asset_document* document;
    size_t name_length;

    if (out_document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_document = NULL;

    if (engine == NULL || scene == NULL ||
        !sandbox3d_authoring_asset_name_is_valid(name, &name_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    document = henka_calloc(1U, sizeof(*document));
    if (document == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    document->engine = engine;
    document->scene = scene;
    memcpy(document->name, name, name_length);
    document->name[name_length] = '\0';
    document->provenance = SANDBOX3D_AUTHORING_PROVENANCE_PRODUCT_NATIVE_AUTHORED;

    *out_document = document;
    return HENKA_SUCCESS;
}

void sandbox3d_authoring_asset_document_destroy(
    sandbox3d_authoring_asset_document* document)
{
    size_t part_index;

    if (document == NULL)
    {
        return;
    }

    for (part_index = 0U; part_index < document->part_count; ++part_index)
    {
        sandbox3d_authoring_asset_part* part = &document->parts[part_index];

        if (part->owns_object && part->object != NULL)
        {
            sandbox3d_authoring_object_destroy(part->object);
        }
        if (part->owns_entity && document->scene != NULL &&
            henka_scene_is_entity_valid(document->scene, part->entity))
        {
            henka_scene_destroy_entity(document->scene, part->entity);
        }
    }
    henka_free(document);
}

sandbox3d_authoring_provenance
sandbox3d_authoring_asset_document_get_provenance(
    const sandbox3d_authoring_asset_document* document)
{
    return document == NULL
        ? SANDBOX3D_AUTHORING_PROVENANCE_UNKNOWN
        : document->provenance;
}

const char* sandbox3d_authoring_asset_document_get_name(
    const sandbox3d_authoring_asset_document* document)
{
    return document == NULL ? NULL : document->name;
}

bool sandbox3d_authoring_asset_document_name_is_valid(const char* name)
{
    size_t name_length;

    return sandbox3d_authoring_asset_name_is_valid(name, &name_length);
}

size_t sandbox3d_authoring_asset_document_get_part_count(
    const sandbox3d_authoring_asset_document* document)
{
    return document == NULL ? 0U : document->part_count;
}

static bool sandbox3d_authoring_primitive_desc_is_finite(
    const sandbox3d_authoring_primitive_desc* desc)
{
    return desc != NULL && isfinite(desc->width) && isfinite(desc->height) &&
        isfinite(desc->depth) && isfinite(desc->radius);
}

static bool sandbox3d_authoring_asset_vec3_is_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static const float g_authoring_asset_visual_staging_clearance = 2.0f;

static bool sandbox3d_authoring_asset_bounds_overlap(
    henka_bounds left,
    henka_bounds right)
{
    const float physical_clearance = 0.01f;
    /* A part can be physically disjoint yet still be hidden by a nearby
     * scene object from the editor camera.  Keep a bounded visual staging
     * margin in the horizontal plane so authoring evidence shows the real
     * surface instead of a coincident gallery silhouette. */
    return fabsf(left.center.x - right.center.x) <
            left.extents.x + right.extents.x + physical_clearance +
                g_authoring_asset_visual_staging_clearance &&
        fabsf(left.center.y - right.center.y) <
            left.extents.y + right.extents.y + physical_clearance &&
        fabsf(left.center.z - right.center.z) <
            left.extents.z + right.extents.z + physical_clearance +
                g_authoring_asset_visual_staging_clearance;
}

static void sandbox3d_authoring_asset_stage_grid_coordinate(
    size_t candidate_index,
    int* out_x,
    int* out_z)
{
    /* Favor diagonal cells first so a new authoring part is not placed
     * directly behind one of the default center-gallery objects from the
     * camera's normal approach direction.  The later ring search remains
     * deterministic and scene-agnostic when those cells are occupied. */
    static const int initial_coordinates[][2] =
    {
        {-1, -1},
        {1, -1},
        {-1, 1},
        {1, 1},
        {0, -1},
        {-1, 0},
        {1, 0},
        {0, 1}
    };
    size_t remaining;
    size_t ring;
    size_t ring_offset;
    size_t ring_length;

    if (out_x == NULL || out_z == NULL)
    {
        return;
    }
    if (candidate_index < sizeof(initial_coordinates) /
        sizeof(initial_coordinates[0]))
    {
        *out_x = initial_coordinates[candidate_index][0];
        *out_z = initial_coordinates[candidate_index][1];
        return;
    }

    remaining = candidate_index -
        (sizeof(initial_coordinates) / sizeof(initial_coordinates[0]));
    ring = 2U;
    for (;;)
    {
        ring_length = 8U * ring;
        if (remaining < ring_length)
        {
            break;
        }
        remaining -= ring_length;
        ++ring;
    }
    ring_offset = remaining;
    if (ring_offset < 2U * ring)
    {
        *out_x = -(int)ring + (int)ring_offset;
        *out_z = -(int)ring;
    }
    else if (ring_offset < 4U * ring)
    {
        *out_x = (int)ring;
        *out_z = -(int)ring + (int)(ring_offset - 2U * ring);
    }
    else if (ring_offset < 6U * ring)
    {
        *out_x = (int)ring - (int)(ring_offset - 4U * ring);
        *out_z = (int)ring;
    }
    else
    {
        *out_x = -(int)ring;
        *out_z = (int)ring - (int)(ring_offset - 6U * ring);
    }
}

static henka_result sandbox3d_authoring_asset_stage_part(
    const sandbox3d_authoring_asset_document* document,
    henka_entity entity)
{
    enum { stage_candidate_capacity = 128 };
    henka_bounds local_bounds;
    henka_transform previous_transform;
    size_t candidate_index;
    float spacing;

    if (document == NULL || document->scene == NULL || entity == HENKA_INVALID_ENTITY ||
        henka_scene_get_entity_local_bounds(document->scene, entity, &local_bounds) != HENKA_SUCCESS ||
        henka_scene_get_entity_transform(document->scene, entity, &previous_transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    spacing = fmaxf(
        2.5f,
        2.0f * fmaxf(
            fmaxf(local_bounds.extents.x, local_bounds.extents.y),
            local_bounds.extents.z) +
            0.1f +
            g_authoring_asset_visual_staging_clearance);
    for (candidate_index = 0U;
         candidate_index < stage_candidate_capacity;
         ++candidate_index)
    {
        int grid_x;
        int grid_z;
        henka_transform candidate = henka_transform_identity();
        henka_bounds candidate_bounds;
        bool occupied = false;
        size_t entity_index;

        sandbox3d_authoring_asset_stage_grid_coordinate(
            candidate_index, &grid_x, &grid_z);
        candidate.position.x = (float)grid_x * spacing;
        candidate.position.z = (float)grid_z * spacing;
        /* Leave a visible and depth-stable gap above floor-like surfaces. */
        candidate.position.y = -local_bounds.center.y + local_bounds.extents.y + 0.25f;
        if (henka_scene_set_entity_transform(document->scene, entity, candidate) != HENKA_SUCCESS ||
            henka_scene_get_entity_world_bounds(document->scene, entity, &candidate_bounds) != HENKA_SUCCESS)
        {
            (void)henka_scene_set_entity_transform(document->scene, entity, previous_transform);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }

        for (entity_index = 0U; entity_index < henka_scene_get_entity_count(document->scene); ++entity_index)
        {
            henka_entity other = henka_scene_get_entity_at_index(document->scene, entity_index);
            henka_bounds other_bounds;

            if (other == HENKA_INVALID_ENTITY || other == entity ||
                henka_scene_is_entity_helper(document->scene, other) ||
                henka_scene_get_entity_world_bounds(document->scene, other, &other_bounds) != HENKA_SUCCESS)
            {
                continue;
            }
            if (sandbox3d_authoring_asset_bounds_overlap(candidate_bounds, other_bounds))
            {
                occupied = true;
                break;
            }
        }
        if (!occupied)
        {
            return HENKA_SUCCESS;
        }
    }

    (void)henka_scene_set_entity_transform(document->scene, entity, previous_transform);
    return HENKA_ERROR_LIMIT;
}

henka_result sandbox3d_authoring_asset_document_add_primitive(
    sandbox3d_authoring_asset_document* document,
    const char* part_name,
    sandbox3d_authoring_primitive_kind kind,
    const sandbox3d_authoring_primitive_desc* desc,
    size_t history_steps,
    size_t* out_part_index)
{
    henka_authoring_mesh_desc mesh_desc;
    henka_authoring_mesh* mesh = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_entity entity;
    henka_result result;
    size_t part_name_length;

    if (out_part_index == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_part_index = SIZE_MAX;

    if (document == NULL || document->engine == NULL || document->scene == NULL ||
        history_steps == 0U || !sandbox3d_authoring_asset_name_is_valid(part_name, &part_name_length) ||
        !sandbox3d_authoring_primitive_desc_is_finite(desc))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (document->part_count >= SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY)
    {
        return HENKA_ERROR_LIMIT;
    }

    mesh_desc = henka_authoring_mesh_desc_default();
    /* Cylinders and cones encode their cap as one polygon. Keep the per-face
     * descriptor limit aligned with the requested segment count; the constructor
     * then enforces the mesh's configured resource capacities. */
    if ((kind == SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER ||
         kind == SANDBOX3D_AUTHORING_PRIMITIVE_CONE) &&
        desc->segments > mesh_desc.max_face_corners)
    {
        mesh_desc.max_face_corners = desc->segments;
    }
    switch (kind)
    {
        case SANDBOX3D_AUTHORING_PRIMITIVE_BOX:
            result = henka_authoring_mesh_create_box(
                &mesh_desc, desc->width, desc->height, desc->depth, &mesh);
            break;
        case SANDBOX3D_AUTHORING_PRIMITIVE_PLANE:
            result = henka_authoring_mesh_create_plane(
                &mesh_desc, desc->width, desc->depth, &mesh);
            break;
        case SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER:
            result = henka_authoring_mesh_create_cylinder(
                &mesh_desc, desc->radius, desc->height, desc->segments, &mesh);
            break;
        case SANDBOX3D_AUTHORING_PRIMITIVE_CONE:
            result = henka_authoring_mesh_create_cone(
                &mesh_desc, desc->radius, desc->height, desc->segments, &mesh);
            break;
        case SANDBOX3D_AUTHORING_PRIMITIVE_UV_SPHERE:
            result = henka_authoring_mesh_create_uv_sphere(
                &mesh_desc, desc->radius, desc->segments, desc->latitude_segments, &mesh);
            break;
        case SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE:
            result = henka_authoring_mesh_create_quad_sphere(
                &mesh_desc, desc->radius, desc->subdivisions, &mesh);
            break;
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    entity = henka_scene_create_entity_named(document->scene, part_name);
    if (entity == HENKA_INVALID_ENTITY)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    result = sandbox3d_authoring_object_create_from_mesh(
        document->engine, document->scene, entity, mesh, history_steps, &object);
    henka_authoring_mesh_destroy(mesh);
    if (result != HENKA_SUCCESS)
    {
        henka_scene_destroy_entity(document->scene, entity);
        return result;
    }
    result = sandbox3d_authoring_asset_stage_part(document, entity);
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_authoring_object_destroy(object);
        henka_scene_destroy_entity(document->scene, entity);
        return result;
    }

    document->parts[document->part_count] =
        (sandbox3d_authoring_asset_part){entity, object, kind, true, true};
    *out_part_index = document->part_count;
    ++document->part_count;
    return HENKA_SUCCESS;
}

sandbox3d_authoring_object* sandbox3d_authoring_asset_document_get_part(
    sandbox3d_authoring_asset_document* document,
    size_t part_index)
{
    if (document == NULL || part_index >= document->part_count)
    {
        return NULL;
    }
    return document->parts[part_index].object;
}

bool sandbox3d_authoring_asset_document_find_part(
    const sandbox3d_authoring_asset_document* document,
    const sandbox3d_authoring_object* part,
    size_t* out_part_index)
{
    size_t part_index;

    if (out_part_index == NULL)
    {
        return false;
    }
    *out_part_index = SIZE_MAX;
    if (document == NULL || part == NULL)
    {
        return false;
    }
    for (part_index = 0U; part_index < document->part_count; ++part_index)
    {
        if (document->parts[part_index].object == part)
        {
            *out_part_index = part_index;
            return true;
        }
    }
    return false;
}

henka_result sandbox3d_authoring_asset_document_get_part_kind(
    const sandbox3d_authoring_asset_document* document,
    const sandbox3d_authoring_object* part,
    sandbox3d_authoring_primitive_kind* out_kind)
{
    size_t part_index;

    if (out_kind == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_kind = SANDBOX3D_AUTHORING_PRIMITIVE_BOX;
    if (!sandbox3d_authoring_asset_document_find_part(
            document, part, &part_index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_kind = document->parts[part_index].kind;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_asset_document_adopt_part(
    sandbox3d_authoring_asset_document* document,
    sandbox3d_authoring_object* part,
    sandbox3d_authoring_primitive_kind kind,
    size_t* out_part_index)
{
    henka_entity entity;
    const char* name;
    size_t name_length;
    size_t existing_index;

    if (out_part_index == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_part_index = SIZE_MAX;
    if (document == NULL || part == NULL ||
        document->part_count >= SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY)
    {
        return document != NULL && document->part_count >= SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY
            ? HENKA_ERROR_LIMIT : HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind > SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE ||
        sandbox3d_authoring_asset_document_find_part(document, part, &existing_index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    entity = sandbox3d_authoring_object_get_entity(part);
    name = document->scene == NULL ? NULL :
        henka_scene_get_entity_name(document->scene, entity);
    if (document->scene == NULL || entity == HENKA_INVALID_ENTITY ||
        !henka_scene_is_entity_valid(document->scene, entity) ||
        !sandbox3d_authoring_asset_name_is_valid(name, &name_length) ||
        sandbox3d_authoring_asset_document_has_part_name(document, name))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document->parts[document->part_count] =
        (sandbox3d_authoring_asset_part){entity, part, kind, true, true};
    *out_part_index = document->part_count;
    ++document->part_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_asset_document_release_part_ownership(
    sandbox3d_authoring_asset_document* document,
    size_t part_index,
    sandbox3d_authoring_object** out_part)
{
    sandbox3d_authoring_asset_part* part;

    if (out_part == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_part = NULL;

    if (document == NULL || part_index >= document->part_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    part = &document->parts[part_index];
    if (part->object == NULL || !part->owns_object)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    part->owns_object = false;
    *out_part = part->object;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_asset_document_discard_part(
    sandbox3d_authoring_asset_document* document,
    size_t part_index)
{
    sandbox3d_authoring_asset_part* part;

    if (document == NULL || part_index >= document->part_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    part = &document->parts[part_index];
    if (!part->owns_object || part->object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    sandbox3d_authoring_object_destroy(part->object);
    if (part->owns_entity && document->scene != NULL &&
        henka_scene_is_entity_valid(document->scene, part->entity))
    {
        henka_scene_destroy_entity(document->scene, part->entity);
    }

    if (part_index + 1U < document->part_count)
    {
        memmove(
            part,
            part + 1U,
            (document->part_count - part_index - 1U) * sizeof(*part));
    }
    --document->part_count;
    memset(&document->parts[document->part_count], 0, sizeof(document->parts[0]));
    return HENKA_SUCCESS;
}

bool sandbox3d_authoring_asset_document_forget_released_part(
    sandbox3d_authoring_asset_document* document,
    const sandbox3d_authoring_object* part)
{
    size_t part_index;

    if (document == NULL || part == NULL)
    {
        return false;
    }

    for (part_index = 0U; part_index < document->part_count; ++part_index)
    {
        sandbox3d_authoring_asset_part* candidate = &document->parts[part_index];
        if (candidate->object != part)
        {
            continue;
        }
        if (candidate->owns_object)
        {
            return false;
        }
        if (part_index + 1U < document->part_count)
        {
            memmove(
                candidate,
                candidate + 1U,
                (document->part_count - part_index - 1U) * sizeof(*candidate));
        }
        --document->part_count;
        memset(
            &document->parts[document->part_count],
            0,
            sizeof(document->parts[0]));
        return true;
    }
    return false;
}

static bool sandbox3d_authoring_asset_key(char* key, size_t capacity, size_t index, const char* suffix)
{
    const int length = snprintf(key, capacity, "part.%zu.%s", index, suffix);
    return length > 0 && (size_t)length < capacity;
}

static henka_result sandbox3d_authoring_asset_source_relative(
    const char* asset_name,
    const char* part_name,
    uint32_t revision,
    char* output,
    size_t output_capacity)
{
    const int length = snprintf(
        output, output_capacity, "authored_assets/%s/rev%u/%s.hams",
        asset_name, (unsigned int)revision, part_name);
    return length > 0 && (size_t)length < output_capacity ? HENKA_SUCCESS : HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_authoring_asset_material_relative(
    const char* asset_name,
    const char* part_name,
    uint32_t revision,
    char* output,
    size_t output_capacity)
{
    const int length = snprintf(
        output, output_capacity, "authored_assets/%s/rev%u/%s.material",
        asset_name, (unsigned int)revision, part_name);
    return length > 0 && (size_t)length < output_capacity ? HENKA_SUCCESS : HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_authoring_asset_set_part_value(
    henka_settings* settings, size_t index, const char* suffix, float value)
{
    char key[64];
    return sandbox3d_authoring_asset_key(key, sizeof(key), index, suffix)
        ? henka_settings_set_float(settings, key, value) : HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_authoring_asset_set_part_flag(
    henka_settings* settings, size_t index, const char* suffix, bool value)
{
    char key[64];
    return sandbox3d_authoring_asset_key(key, sizeof(key), index, suffix)
        ? henka_settings_set_bool(settings, key, value) : HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_authoring_asset_set_part_int(
    henka_settings* settings, size_t index, const char* suffix, int value)
{
    char key[64];
    return sandbox3d_authoring_asset_key(key, sizeof(key), index, suffix)
        ? henka_settings_set_int(settings, key, value) : HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_authoring_asset_set_part_string(
    henka_settings* settings, size_t index, const char* suffix, const char* value)
{
    char key[64];
    return value != NULL && sandbox3d_authoring_asset_key(key, sizeof(key), index, suffix)
        ? henka_settings_set_string(settings, key, value) : HENKA_ERROR_LIMIT;
}

static bool sandbox3d_authoring_asset_get_part_value(
    const henka_settings* settings, size_t index, const char* suffix, float* out_value)
{
    char key[64];
    if (out_value == NULL || !sandbox3d_authoring_asset_key(key, sizeof(key), index, suffix) ||
        !henka_settings_has_key(settings, key)) return false;
    *out_value = henka_settings_get_float(settings, key, NAN);
    return isfinite(*out_value);
}

static bool sandbox3d_authoring_asset_get_part_flag(
    const henka_settings* settings, size_t index, const char* suffix, bool* out_value)
{
    char key[64];
    const char* value;
    if (out_value == NULL || !sandbox3d_authoring_asset_key(key, sizeof(key), index, suffix)) return false;
    value = henka_settings_get_string(settings, key, NULL);
    if (value == NULL) return false;
    if (strcmp(value, "true") == 0) { *out_value = true; return true; }
    if (strcmp(value, "false") == 0) { *out_value = false; return true; }
    return false;
}

static bool sandbox3d_authoring_asset_get_part_int(
    const henka_settings* settings, size_t index, const char* suffix, int* out_value)
{
    char key[64];

    if (out_value == NULL || !sandbox3d_authoring_asset_key(key, sizeof(key), index, suffix) ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    *out_value = henka_settings_get_int(settings, key, INT_MIN);
    return *out_value != INT_MIN;
}

static henka_result sandbox3d_authoring_asset_save_material_file(
    const sandbox3d_authoring_asset_document* document,
    const char* project_root,
    const char* relative_material_path,
    const henka_material* material)
{
    if (document == NULL || project_root == NULL || relative_material_path == NULL ||
        material == NULL || document->engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_assets_save_native_material_file(
        henka_engine_get_asset_manager(document->engine),
        project_root,
        relative_material_path,
        material);
}

static henka_result sandbox3d_authoring_asset_load_material_file(
    henka_engine* engine,
    const char* project_root,
    const char* relative_material_path,
    const henka_material* material_template,
    henka_material_asset** out_asset,
    henka_material* out_material)
{
    henka_material default_material_template;
    henka_shader* default_shader = NULL;
    henka_asset_manager* assets;
    henka_result result;

    if (engine == NULL || project_root == NULL || relative_material_path == NULL ||
        out_asset == NULL || out_material == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    assets = henka_engine_get_asset_manager(engine);
    if (assets == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (material_template == NULL)
    {
        result = henka_assets_load_shader(
            assets,
            "assets/shaders/basic_lit.vert",
            "assets/shaders/basic_lit.frag",
            &default_shader);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        default_material_template = henka_material_default();
        default_material_template.shader = default_shader;
        material_template = &default_material_template;
    }
    if (material_template->shader == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_asset = NULL;
    result = henka_assets_load_native_material_asset(
        assets,
        project_root,
        relative_material_path,
        material_template->shader,
        out_asset);
    if (result == HENKA_SUCCESS)
    {
        result = henka_assets_get_material_asset_material(*out_asset, out_material);
    }
    return result;
}

static henka_result sandbox3d_authoring_asset_validate_material(
    const sandbox3d_authoring_asset_document* document,
    const henka_material* material)
{
    const henka_texture* textures[7];
    henka_asset_manager* assets;
    size_t index;

    if (document == NULL || document->engine == NULL || material == NULL ||
        material->terrain_layers_enabled || henka_material_validate(material) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    assets = henka_engine_get_asset_manager(document->engine);
    textures[0] = material->base_color_texture;
    textures[1] = material->normal_texture;
    textures[2] = material->metallic_roughness_texture;
    textures[3] = material->occlusion_texture;
    textures[4] = material->emissive_texture;
    textures[5] = material->transmission_texture;
    textures[6] = material->thickness_texture;
    for (index = 0U; index < 7U; ++index)
    {
        henka_asset_metadata metadata;
        char* confined_source = NULL;
        henka_result result;

        if (textures[index] == NULL)
        {
            continue;
        }
        memset(&metadata, 0, sizeof(metadata));
        result = henka_assets_get_texture_metadata(assets, textures[index], &metadata);
        if (result != HENKA_SUCCESS || metadata.source_path == NULL ||
            metadata.source_path[0] == '\0' || !metadata.reload_supported || metadata.fallback)
        {
            return HENKA_ERROR_ASSET_SOURCE;
        }
        result = henka_path_resolve_confined("", metadata.source_path, &confined_source);
        henka_free(confined_source);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_asset_validate_part_for_save(
    const sandbox3d_authoring_asset_document* document,
    const char* project_root,
    size_t part_index,
    uint32_t revision)
{
    const sandbox3d_authoring_asset_part* part;
    const char* part_name;
    henka_transform transform;
    henka_material material;
    char relative_source[SANDBOX3D_AUTHORING_ASSET_PATH_CAPACITY];
    char relative_material[SANDBOX3D_AUTHORING_ASSET_PATH_CAPACITY];
    char key[64];
    char* resolved_path = NULL;
    size_t part_name_length;
    henka_result result;

    if (document == NULL || project_root == NULL || project_root[0] == '\0' ||
        document->scene == NULL ||
        part_index >= document->part_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    part = &document->parts[part_index];
    part_name = henka_scene_get_entity_name(document->scene, part->entity);
    if (part->object == NULL || part_name == NULL ||
        !henka_scene_is_entity_valid(document->scene, part->entity) ||
        !sandbox3d_authoring_asset_name_is_valid(part_name, &part_name_length) ||
        henka_scene_get_entity_transform(document->scene, part->entity, &transform) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(document->scene, part->entity, &material) != HENKA_SUCCESS ||
        !sandbox3d_authoring_asset_vec3_is_finite(transform.position) ||
        !sandbox3d_authoring_asset_vec3_is_finite(transform.scale) ||
        transform.scale.x <= 0.0f || transform.scale.y <= 0.0f ||
        transform.scale.z <= 0.0f || !isfinite(transform.rotation.x) ||
        !isfinite(transform.rotation.y) || !isfinite(transform.rotation.z) ||
        !isfinite(transform.rotation.w))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_authoring_asset_source_relative(
        document->name, part_name, revision,
        relative_source, sizeof(relative_source));
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_asset_material_relative(
            document->name, part_name, revision,
            relative_material, sizeof(relative_material));
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_asset_key(
            key, sizeof(key), part_index, "name") &&
            sandbox3d_authoring_asset_key(
                key, sizeof(key), part_index, "source_path") &&
            sandbox3d_authoring_asset_key(
                key, sizeof(key), part_index, "material_path") &&
            sandbox3d_authoring_asset_key(
                key, sizeof(key), part_index, "primitive")
            ? HENKA_SUCCESS
            : HENKA_ERROR_LIMIT;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_path_resolve_confined(
            project_root, relative_source, &resolved_path);
        henka_free(resolved_path);
        resolved_path = NULL;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_path_resolve_confined(
            project_root, relative_material, &resolved_path);
        henka_free(resolved_path);
        resolved_path = NULL;
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_asset_validate_material(
            document, &material);
    }
    return result;
}

henka_result sandbox3d_authoring_asset_document_save(
    sandbox3d_authoring_asset_document* document,
    const char* project_root,
    const char* relative_manifest_path)
{
    henka_settings* settings = NULL;
    char* manifest_path = NULL;
    henka_result result;
    size_t part_index;
    uint32_t revision;

    if (document == NULL || project_root == NULL || project_root[0] == '\0' ||
        relative_manifest_path == NULL || relative_manifest_path[0] == '\0') return HENKA_ERROR_INVALID_ARGUMENT;
    if (document->persisted_revision >= (uint32_t)INT_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }
    revision = document->persisted_revision + 1U;
    result = henka_path_resolve_confined(project_root, relative_manifest_path, &manifest_path);
    if (result == HENKA_SUCCESS) result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS) result = henka_settings_set_int(
        settings, "asset.version", SANDBOX3D_AUTHORING_ASSET_VERSION);
    if (result == HENKA_SUCCESS) result = henka_settings_set_int(settings, "asset.revision", (int)revision);
    if (result == HENKA_SUCCESS) result = henka_settings_set_string(settings, "asset.name", document->name);
    if (result == HENKA_SUCCESS) result = henka_settings_set_string(
        settings, "asset.provenance", SANDBOX3D_AUTHORING_PROVENANCE_LABEL);
    if (result == HENKA_SUCCESS) result = henka_settings_set_int(settings, "asset.part_count", (int)document->part_count);
    for (part_index = 0U; result == HENKA_SUCCESS && part_index < document->part_count; ++part_index)
    {
        result = sandbox3d_authoring_asset_validate_part_for_save(
            document, project_root, part_index, revision);
    }
    for (part_index = 0U; result == HENKA_SUCCESS && part_index < document->part_count; ++part_index)
    {
        const sandbox3d_authoring_asset_part* part = &document->parts[part_index];
        const char* part_name = henka_scene_get_entity_name(document->scene, part->entity);
        henka_transform transform;
        henka_material material;
        char relative_source[SANDBOX3D_AUTHORING_ASSET_PATH_CAPACITY];
        char relative_material[SANDBOX3D_AUTHORING_ASSET_PATH_CAPACITY];
        char* source_path = NULL;
        char key[64];
        size_t part_name_length;
        if (part->object == NULL || part_name == NULL ||
            !sandbox3d_authoring_asset_name_is_valid(part_name, &part_name_length) ||
            henka_scene_get_entity_transform(document->scene, part->entity, &transform) != HENKA_SUCCESS ||
            henka_scene_get_entity_material(document->scene, part->entity, &material) != HENKA_SUCCESS ||
            !sandbox3d_authoring_asset_vec3_is_finite(transform.position) || !sandbox3d_authoring_asset_vec3_is_finite(transform.scale) ||
            !isfinite(transform.rotation.x) || !isfinite(transform.rotation.y) || !isfinite(transform.rotation.z) || !isfinite(transform.rotation.w))
        { result = HENKA_ERROR_INVALID_ARGUMENT; break; }
        result = sandbox3d_authoring_asset_source_relative(
            document->name, part_name, revision, relative_source, sizeof(relative_source));
        if (result == HENKA_SUCCESS) result = sandbox3d_authoring_asset_material_relative(
            document->name, part_name, revision, relative_material, sizeof(relative_material));
        if (result == HENKA_SUCCESS) result = henka_path_resolve_confined(project_root, relative_source, &source_path);
        if (result == HENKA_SUCCESS) result = henka_path_ensure_parent_directory(source_path);
        if (result == HENKA_SUCCESS) result = sandbox3d_authoring_object_save_source(part->object, source_path);
        henka_free(source_path);
        if (result != HENKA_SUCCESS || !sandbox3d_authoring_asset_key(key, sizeof(key), part_index, "name")) { if (result == HENKA_SUCCESS) result = HENKA_ERROR_LIMIT; break; }
        result = henka_settings_set_string(settings, key, part_name);
        if (result == HENKA_SUCCESS && sandbox3d_authoring_asset_key(key, sizeof(key), part_index, "source_path")) result = henka_settings_set_string(settings, key, relative_source);
        if (result == HENKA_SUCCESS && sandbox3d_authoring_asset_key(key, sizeof(key), part_index, "material_path")) result = henka_settings_set_string(settings, key, relative_material);
        if (result == HENKA_SUCCESS && sandbox3d_authoring_asset_key(key, sizeof(key), part_index, "primitive")) result = henka_settings_set_int(settings, key, (int)part->kind);
#define ASSET_VALUE(field, value) do { if (result == HENKA_SUCCESS) result = sandbox3d_authoring_asset_set_part_value(settings, part_index, field, value); } while (0)
        ASSET_VALUE("position.x", transform.position.x); ASSET_VALUE("position.y", transform.position.y); ASSET_VALUE("position.z", transform.position.z);
        ASSET_VALUE("rotation.x", transform.rotation.x); ASSET_VALUE("rotation.y", transform.rotation.y); ASSET_VALUE("rotation.z", transform.rotation.z); ASSET_VALUE("rotation.w", transform.rotation.w);
        ASSET_VALUE("scale.x", transform.scale.x); ASSET_VALUE("scale.y", transform.scale.y); ASSET_VALUE("scale.z", transform.scale.z);
#undef ASSET_VALUE
        if (result == HENKA_SUCCESS) result = sandbox3d_authoring_asset_set_part_flag(settings, part_index, "visible", henka_scene_is_entity_visible(document->scene, part->entity));
        if (result == HENKA_SUCCESS) result = sandbox3d_authoring_asset_save_material_file(
            document, project_root, relative_material, &material);
    }
    if (result == HENKA_SUCCESS) result = henka_settings_save_file(settings, manifest_path);
    if (result == HENKA_SUCCESS) document->persisted_revision = revision;
    henka_settings_destroy(settings);
    henka_free(manifest_path);
    return result;
}

static bool sandbox3d_authoring_asset_document_has_part_name(
    const sandbox3d_authoring_asset_document* document, const char* name)
{
    size_t index;
    for (index = 0U; document != NULL && index < document->part_count; ++index)
    {
        const char* candidate = henka_scene_get_entity_name(document->scene, document->parts[index].entity);
        if (candidate != NULL && strcmp(candidate, name) == 0) return true;
    }
    return false;
}

static henka_result sandbox3d_authoring_asset_document_add_loaded_part(
    sandbox3d_authoring_asset_document* document,
    const char* name,
    sandbox3d_authoring_primitive_kind kind,
    const char* source_path,
    henka_transform transform,
    bool visible,
    henka_material material,
    bool apply_material,
    size_t history_steps)
{
    henka_authoring_mesh* mesh = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_entity entity;
    henka_result result;
    size_t name_length;
    if (document == NULL || document->part_count >= SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY ||
        !sandbox3d_authoring_asset_name_is_valid(name, &name_length) ||
        kind > SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE || history_steps == 0U ||
        source_path == NULL || !sandbox3d_authoring_asset_vec3_is_finite(transform.position) ||
        !sandbox3d_authoring_asset_vec3_is_finite(transform.scale) || transform.scale.x <= 0.0f ||
        transform.scale.y <= 0.0f || transform.scale.z <= 0.0f || !isfinite(transform.rotation.x) ||
        !isfinite(transform.rotation.y) || !isfinite(transform.rotation.z) || !isfinite(transform.rotation.w)) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_authoring_mesh_load_file_new(source_path, &mesh);
    if (result != HENKA_SUCCESS) return result;
    entity = henka_scene_create_entity_named(document->scene, name);
    if (entity == HENKA_INVALID_ENTITY) { henka_authoring_mesh_destroy(mesh); return HENKA_ERROR_OUT_OF_MEMORY; }
    result = sandbox3d_authoring_object_create_from_mesh(document->engine, document->scene, entity, mesh, history_steps, &object);
    henka_authoring_mesh_destroy(mesh);
    if (result == HENKA_SUCCESS) result = henka_scene_set_entity_transform(document->scene, entity, transform);
    if (result == HENKA_SUCCESS) result = henka_scene_set_entity_visible(document->scene, entity, visible);
    if (result == HENKA_SUCCESS && apply_material)
        result = henka_scene_set_entity_material(document->scene, entity, material);
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_authoring_object_destroy(object);
        henka_scene_destroy_entity(document->scene, entity);
        return result;
    }
    document->parts[document->part_count++] =
        (sandbox3d_authoring_asset_part){entity, object, kind, true, true};
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_asset_document_load(
    henka_engine* engine,
    henka_scene* scene,
    const char* project_root,
    const char* relative_manifest_path,
    size_t history_steps,
    const henka_material* material_template,
    sandbox3d_authoring_asset_document** out_document)
{
    henka_settings* settings = NULL;
    sandbox3d_authoring_asset_document* candidate = NULL;
    char* manifest_path = NULL;
    const char* asset_name;
    const char* asset_provenance;
    int part_count;
    int asset_revision;
    henka_result result;
    size_t index;
    if (out_document == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    *out_document = NULL;
    if (engine == NULL || scene == NULL || project_root == NULL || project_root[0] == '\0' ||
        relative_manifest_path == NULL || relative_manifest_path[0] == '\0' || history_steps == 0U) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_path_resolve_confined(project_root, relative_manifest_path, &manifest_path);
    if (result == HENKA_SUCCESS) result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS) result = henka_settings_load_file(settings, manifest_path);
    asset_name = result == HENKA_SUCCESS ? henka_settings_get_string(settings, "asset.name", NULL) : NULL;
    asset_provenance = result == HENKA_SUCCESS ? henka_settings_get_string(settings, "asset.provenance", NULL) : NULL;
    part_count = result == HENKA_SUCCESS ? henka_settings_get_int(settings, "asset.part_count", -1) : -1;
    asset_revision = result == HENKA_SUCCESS ? henka_settings_get_int(settings, "asset.revision", 0) : 0;
    if (result == HENKA_SUCCESS && (!henka_settings_has_key(settings, "asset.version") ||
        henka_settings_get_int(settings, "asset.version", 0) != SANDBOX3D_AUTHORING_ASSET_VERSION || asset_name == NULL ||
        asset_provenance == NULL || strcmp(asset_provenance, SANDBOX3D_AUTHORING_PROVENANCE_LABEL) != 0 ||
        asset_revision <= 0 ||
        part_count < 0 || (size_t)part_count > SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY)) result = HENKA_ERROR_ASSET_SOURCE;
    if (result == HENKA_SUCCESS) result = sandbox3d_authoring_asset_document_create(engine, scene, asset_name, &candidate);
    if (result == HENKA_SUCCESS) candidate->persisted_revision = (uint32_t)asset_revision;
    for (index = 0U; result == HENKA_SUCCESS && index < (size_t)part_count; ++index)
    {
        char key[64];
        char expected_relative[SANDBOX3D_AUTHORING_ASSET_PATH_CAPACITY];
        char expected_material_relative[SANDBOX3D_AUTHORING_ASSET_PATH_CAPACITY];
        char* source_path = NULL;
        const char* name;
        const char* relative_source;
        const char* relative_material;
        henka_material_asset* material_asset = NULL;
        henka_asset_manager* assets = henka_engine_get_asset_manager(engine);
        bool apply_material = false;
        int primitive;
        bool visible;
        henka_transform transform = henka_transform_identity();
        henka_material material = henka_material_default();
        if (!sandbox3d_authoring_asset_key(key, sizeof(key), index, "name")) { result = HENKA_ERROR_LIMIT; break; }
        name = henka_settings_get_string(settings, key, NULL);
        if (!sandbox3d_authoring_asset_key(key, sizeof(key), index, "source_path")) { result = HENKA_ERROR_LIMIT; break; }
        relative_source = henka_settings_get_string(settings, key, NULL);
        if (!sandbox3d_authoring_asset_key(key, sizeof(key), index, "material_path")) { result = HENKA_ERROR_LIMIT; break; }
        relative_material = henka_settings_get_string(settings, key, NULL);
        if (!sandbox3d_authoring_asset_key(key, sizeof(key), index, "primitive") || !henka_settings_has_key(settings, key)) { result = HENKA_ERROR_ASSET_SOURCE; break; }
        primitive = henka_settings_get_int(settings, key, -1);
        if (name == NULL || sandbox3d_authoring_asset_document_has_part_name(candidate, name) || primitive < 0 || primitive > (int)SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE ||
            sandbox3d_authoring_asset_source_relative(asset_name, name, (uint32_t)asset_revision, expected_relative, sizeof(expected_relative)) != HENKA_SUCCESS ||
            sandbox3d_authoring_asset_material_relative(asset_name, name, (uint32_t)asset_revision, expected_material_relative, sizeof(expected_material_relative)) != HENKA_SUCCESS ||
            relative_source == NULL || strcmp(relative_source, expected_relative) != 0 ||
            relative_material == NULL || strcmp(relative_material, expected_material_relative) != 0) { result = HENKA_ERROR_ASSET_SOURCE; break; }
#define ASSET_READ(field, target) if (!sandbox3d_authoring_asset_get_part_value(settings, index, field, &target)) { result = HENKA_ERROR_ASSET_SOURCE; break; }
        ASSET_READ("position.x", transform.position.x); ASSET_READ("position.y", transform.position.y); ASSET_READ("position.z", transform.position.z);
        ASSET_READ("rotation.x", transform.rotation.x); ASSET_READ("rotation.y", transform.rotation.y); ASSET_READ("rotation.z", transform.rotation.z); ASSET_READ("rotation.w", transform.rotation.w);
        ASSET_READ("scale.x", transform.scale.x); ASSET_READ("scale.y", transform.scale.y); ASSET_READ("scale.z", transform.scale.z);
#undef ASSET_READ
        if (result != HENKA_SUCCESS || !sandbox3d_authoring_asset_get_part_flag(settings, index, "visible", &visible))
        {
            if (result == HENKA_SUCCESS) result = HENKA_ERROR_ASSET_SOURCE;
            break;
        }
        result = assets == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_authoring_asset_load_material_file(
                engine,
                project_root,
                expected_material_relative,
                material_template,
                &material_asset,
                &material);
        apply_material = result == HENKA_SUCCESS;
        if (result != HENKA_SUCCESS) break;
        result = henka_path_resolve_confined(project_root, expected_relative, &source_path);
        if (result == HENKA_SUCCESS) result = sandbox3d_authoring_asset_document_add_loaded_part(candidate, name, (sandbox3d_authoring_primitive_kind)primitive, source_path, transform, visible, material, apply_material, history_steps);
        henka_free(source_path);
        if (result == HENKA_SUCCESS && material_asset != NULL)
        {
            const sandbox3d_authoring_asset_part* loaded_part =
                &candidate->parts[candidate->part_count - 1U];
            result = henka_scene_set_entity_material_asset(
                scene, loaded_part->entity, material_asset);
        }
        else if (result == HENKA_SUCCESS && apply_material)
        {
            result = HENKA_ERROR_ASSET_SOURCE;
        }
    }
    henka_settings_destroy(settings);
    henka_free(manifest_path);
    if (result != HENKA_SUCCESS) { sandbox3d_authoring_asset_document_destroy(candidate); return result; }
    *out_document = candidate;
    return HENKA_SUCCESS;
}
