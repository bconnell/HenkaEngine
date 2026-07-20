#include <henka/action.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include "checked.h"

typedef struct henka_action_default_transform_record
{
    henka_entity entity;
    henka_transform transform;
} henka_action_default_transform_record;

struct henka_action_context
{
    henka_scene* scene;
    henka_camera* camera;
    henka_entity selected_entity;
    henka_action_default_transform_record* default_transforms;
    size_t default_transform_count;
    size_t default_transform_capacity;
};

static const float g_henka_action_minimum_scale_magnitude = 0.01f;

static bool henka_action_is_finite_float(float value)
{
    return isfinite(value) != 0;
}

static bool henka_action_scale_component_is_valid(float value)
{
    return henka_action_is_finite_float(value) &&
        fabsf(value) >= g_henka_action_minimum_scale_magnitude;
}

static bool henka_action_scale_vector_is_valid(henka_vec3 value)
{
    return henka_action_scale_component_is_valid(value.x) &&
        henka_action_scale_component_is_valid(value.y) &&
        henka_action_scale_component_is_valid(value.z);
}

static bool henka_action_name_is_valid(const char* value)
{
    return value != NULL && value[0] != '\0';
}

static bool henka_action_transform_is_valid(henka_transform transform)
{
    return henka_action_is_finite_float(transform.position.x) &&
        henka_action_is_finite_float(transform.position.y) &&
        henka_action_is_finite_float(transform.position.z) &&
        henka_action_is_finite_float(transform.rotation.x) &&
        henka_action_is_finite_float(transform.rotation.y) &&
        henka_action_is_finite_float(transform.rotation.z) &&
        henka_action_is_finite_float(transform.rotation.w) &&
        henka_action_scale_vector_is_valid(transform.scale);
}

static bool henka_action_vector_is_valid(henka_vec3 value)
{
    return henka_action_is_finite_float(value.x) &&
        henka_action_is_finite_float(value.y) &&
        henka_action_is_finite_float(value.z);
}

static bool henka_action_quat_is_valid(henka_quat value)
{
    return henka_action_is_finite_float(value.x) &&
        henka_action_is_finite_float(value.y) &&
        henka_action_is_finite_float(value.z) &&
        henka_action_is_finite_float(value.w);
}

static void henka_action_set_message(henka_action_result* result, const char* format, ...)
{
    va_list args;

    if (result == NULL || format == NULL)
    {
        return;
    }

    va_start(args, format);
    vsnprintf(result->message, sizeof(result->message), format, args);
    va_end(args);
}

static henka_action_default_transform_record* henka_action_find_default_transform_record(
    henka_action_context* context,
    henka_entity entity)
{
    size_t index;

    if (context == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return NULL;
    }

    for (index = 0U; index < context->default_transform_count; ++index)
    {
        if (context->default_transforms[index].entity == entity)
        {
            return &context->default_transforms[index];
        }
    }

    return NULL;
}

static const henka_action_default_transform_record* henka_action_find_default_transform_record_const(
    const henka_action_context* context,
    henka_entity entity)
{
    return henka_action_find_default_transform_record((henka_action_context*)context, entity);
}

static henka_result henka_action_ensure_default_transform_capacity(henka_action_context* context)
{
    size_t allocation_size;
    henka_action_default_transform_record* records;
    size_t new_capacity;
    size_t required;

    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (context->default_transform_count < context->default_transform_capacity)
    {
        return HENKA_SUCCESS;
    }

    if (!henka_checked_size_add(context->default_transform_count, 1U, &required) ||
        !henka_checked_capacity(
            context->default_transform_capacity,
            required,
            8U,
            HENKA_MAX_SCENE_ENTITIES,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*records), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    records = henka_realloc(context->default_transforms, allocation_size);
    if (records == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    context->default_transforms = records;
    context->default_transform_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static void henka_action_remove_default_transform(henka_action_context* context, henka_entity entity)
{
    size_t index;

    if (context == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return;
    }

    for (index = 0U; index < context->default_transform_count; ++index)
    {
        if (context->default_transforms[index].entity == entity)
        {
            if (index + 1U < context->default_transform_count)
            {
                memmove(
                    &context->default_transforms[index],
                    &context->default_transforms[index + 1U],
                    (context->default_transform_count - index - 1U) * sizeof(*context->default_transforms));
            }
            context->default_transform_count -= 1U;
            return;
        }
    }
}

static void henka_action_reset_state(henka_action_context* context)
{
    if (context == NULL)
    {
        return;
    }

    context->selected_entity = HENKA_INVALID_ENTITY;
    context->default_transform_count = 0U;
}

static bool henka_action_is_user_entity(const henka_action_context* context, henka_entity entity)
{
    if (context == NULL || context->scene == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return false;
    }

    return henka_scene_is_entity_valid(context->scene, entity) &&
        !henka_scene_is_entity_helper(context->scene, entity);
}

static bool henka_action_is_manipulable_entity(const henka_action_context* context, henka_entity entity)
{
    return henka_action_is_user_entity(context, entity) &&
        henka_scene_is_entity_visible(context->scene, entity);
}

static henka_action_status henka_action_validate_entity_target(
    const henka_action_context* context,
    henka_entity entity,
    bool require_visible)
{
    if (context == NULL || context->scene == NULL)
    {
        return HENKA_ACTION_STATUS_INVALID_CONTEXT;
    }

    if (entity == HENKA_INVALID_ENTITY || !henka_scene_is_entity_valid(context->scene, entity))
    {
        return HENKA_ACTION_STATUS_INVALID_ENTITY;
    }

    if (henka_scene_is_entity_helper(context->scene, entity))
    {
        return HENKA_ACTION_STATUS_HELPER_ENTITY;
    }

    if (require_visible && !henka_scene_is_entity_visible(context->scene, entity))
    {
        return HENKA_ACTION_STATUS_TARGET_HIDDEN;
    }

    return HENKA_ACTION_STATUS_OK;
}

static henka_result henka_action_fill_scene_summary(const henka_action_context* context, henka_action_scene_summary* out_summary)
{
    size_t active_count;
    size_t helper_count;
    size_t index;
    size_t visible_user_count;

    if (context == NULL || context->scene == NULL || out_summary == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    active_count = 0U;
    helper_count = 0U;
    visible_user_count = 0U;
    for (index = 0U; index < henka_scene_get_entity_count(context->scene); ++index)
    {
        henka_entity entity = henka_scene_get_entity_at_index(context->scene, index);
        if (entity == HENKA_INVALID_ENTITY)
        {
            continue;
        }

        active_count += 1U;
        if (henka_scene_is_entity_helper(context->scene, entity))
        {
            helper_count += 1U;
            continue;
        }

        if (henka_scene_is_entity_visible(context->scene, entity))
        {
            visible_user_count += 1U;
        }
    }

    out_summary->entity_count = active_count;
    out_summary->helper_entity_count = helper_count;
    out_summary->user_entity_count = active_count - helper_count;
    out_summary->visible_user_entity_count = visible_user_count;
    out_summary->selected_entity = henka_action_is_user_entity(context, context->selected_entity)
        ? context->selected_entity
        : HENKA_INVALID_ENTITY;
    return HENKA_SUCCESS;
}

static henka_result henka_action_fill_object_details(
    const henka_action_context* context,
    henka_entity entity,
    henka_action_object_details* out_details)
{
    const henka_action_default_transform_record* default_record;

    if (context == NULL || context->scene == NULL || out_details == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_info(context->scene, entity, &out_details->object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_details->selected = context->selected_entity == entity;
    out_details->helper = henka_scene_is_entity_helper(context->scene, entity);
    out_details->has_default_transform = false;
    out_details->default_transform = henka_transform_identity();
    default_record = henka_action_find_default_transform_record_const(context, entity);
    if (default_record != NULL)
    {
        out_details->has_default_transform = true;
        out_details->default_transform = default_record->transform;
    }

    return HENKA_SUCCESS;
}

static henka_bounds henka_action_get_primitive_bounds(henka_action_primitive primitive)
{
    switch (primitive)
    {
        case HENKA_ACTION_PRIMITIVE_PLANE:
            return (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.05f, 0.5f}};
        case HENKA_ACTION_PRIMITIVE_MARKER:
            return (henka_bounds){{0.0f, 0.5f, 0.0f}, {0.35f, 0.5f, 0.35f}};
        case HENKA_ACTION_PRIMITIVE_CUBE:
        default:
            return (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}};
    }
}

static const char* henka_action_get_primitive_tag(henka_action_primitive primitive)
{
    switch (primitive)
    {
        case HENKA_ACTION_PRIMITIVE_PLANE:
            return "primitive_plane";
        case HENKA_ACTION_PRIMITIVE_MARKER:
            return "primitive_marker";
        case HENKA_ACTION_PRIMITIVE_CUBE:
        default:
            return "primitive_cube";
    }
}

static henka_action_status henka_action_status_from_result(henka_result result)
{
    switch (result)
    {
        case HENKA_SUCCESS:
            return HENKA_ACTION_STATUS_OK;
        case HENKA_ERROR_OUT_OF_MEMORY:
            return HENKA_ACTION_STATUS_OUT_OF_MEMORY;
        case HENKA_ERROR_INVALID_ARGUMENT:
            return HENKA_ACTION_STATUS_INVALID_TRANSFORM;
        case HENKA_ERROR_UNKNOWN:
        case HENKA_ERROR_PLATFORM:
        case HENKA_ERROR_RENDERER:
        default:
            return HENKA_ACTION_STATUS_UNSUPPORTED;
    }
}

henka_result henka_action_context_create(henka_action_context** out_context)
{
    henka_action_context* context;

    if (out_context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_context = NULL;
    context = henka_calloc(1U, sizeof(*context));
    if (context == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    context->selected_entity = HENKA_INVALID_ENTITY;
    *out_context = context;
    return HENKA_SUCCESS;
}

void henka_action_context_destroy(henka_action_context* context)
{
    if (context == NULL)
    {
        return;
    }

    henka_free(context->default_transforms);
    henka_free(context);
}

henka_result henka_action_context_set_scene(henka_action_context* context, henka_scene* scene)
{
    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    context->scene = scene;
    henka_action_reset_state(context);
    return HENKA_SUCCESS;
}

henka_result henka_action_context_set_camera(henka_action_context* context, henka_camera* camera)
{
    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    context->camera = camera;
    return HENKA_SUCCESS;
}

henka_result henka_action_context_register_default_transform(
    henka_action_context* context,
    henka_entity entity,
    henka_transform default_transform)
{
    henka_action_default_transform_record* record;
    henka_result result;

    if (context == NULL || context->scene == NULL || !henka_action_is_user_entity(context, entity) || !henka_action_transform_is_valid(default_transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_action_find_default_transform_record(context, entity);
    if (record != NULL)
    {
        record->transform = default_transform;
        return HENKA_SUCCESS;
    }

    result = henka_action_ensure_default_transform_capacity(context);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    context->default_transforms[context->default_transform_count].entity = entity;
    context->default_transforms[context->default_transform_count].transform = default_transform;
    context->default_transform_count += 1U;
    return HENKA_SUCCESS;
}

henka_entity henka_action_context_get_selected_entity(const henka_action_context* context)
{
    if (!henka_action_is_user_entity(context, context != NULL ? context->selected_entity : HENKA_INVALID_ENTITY))
    {
        return HENKA_INVALID_ENTITY;
    }

    return context->selected_entity;
}

henka_result henka_action_get_scene_summary(const henka_action_context* context, henka_action_scene_summary* out_summary)
{
    return henka_action_fill_scene_summary(context, out_summary);
}

henka_result henka_action_list_objects(
    const henka_action_context* context,
    henka_action_object_details* out_objects,
    size_t capacity,
    size_t* out_count)
{
    size_t index;
    size_t object_count;

    if (context == NULL || context->scene == NULL || out_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    object_count = 0U;
    for (index = 0U; index < henka_scene_get_entity_count(context->scene); ++index)
    {
        henka_entity entity = henka_scene_get_entity_at_index(context->scene, index);
        if (!henka_action_is_user_entity(context, entity))
        {
            continue;
        }

        if (out_objects != NULL && object_count < capacity)
        {
            if (henka_action_fill_object_details(context, entity, &out_objects[object_count]) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        object_count += 1U;
    }

    *out_count = object_count;
    return HENKA_SUCCESS;
}

static void henka_action_prepare_result(
    henka_action_context* context,
    const henka_action_request* request,
    henka_action_result* result)
{
    memset(result, 0, sizeof(*result));
    result->command = request->command;
    result->dry_run = request->dry_run;
    result->status = HENKA_ACTION_STATUS_INVALID_COMMAND;
    result->engine_result = HENKA_ERROR_UNKNOWN;
    result->selected_entity = henka_action_context_get_selected_entity(context);
}

henka_result henka_action_execute(
    henka_action_context* context,
    const henka_action_request* request,
    henka_action_result* out_result)
{
    henka_action_result result;
    henka_action_status status;

    if (request == NULL || out_result == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_action_prepare_result(context, request, &result);
    if (context == NULL || context->scene == NULL)
    {
        result.status = HENKA_ACTION_STATUS_INVALID_CONTEXT;
        result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
        henka_action_set_message(&result, "Action context has no active scene.");
        *out_result = result;
        return HENKA_SUCCESS;
    }

    switch (request->command)
    {
        case HENKA_ACTION_COMMAND_CLEAR_SCENE:
        {
            henka_action_scene_summary summary;

            henka_action_fill_scene_summary(context, &summary);
            result.has_scene_summary = true;
            result.scene_summary = summary;
            result.status = HENKA_ACTION_STATUS_OK;
            result.engine_result = HENKA_SUCCESS;
            if (!request->dry_run)
            {
                while (henka_scene_get_entity_count(context->scene) > 0U)
                {
                    henka_entity entity = henka_scene_get_entity_at_index(context->scene, 0U);
                    if (entity == HENKA_INVALID_ENTITY)
                    {
                        break;
                    }
                    henka_scene_destroy_entity(context->scene, entity);
                }
                henka_action_reset_state(context);
                henka_action_fill_scene_summary(context, &result.scene_summary);
            }
            result.success = true;
            result.selected_entity = henka_action_context_get_selected_entity(context);
            henka_action_set_message(&result, request->dry_run ? "Validated clear scene." : "Cleared scene.");
            break;
        }

        case HENKA_ACTION_COMMAND_GET_SCENE_SUMMARY:
            result.engine_result = henka_action_fill_scene_summary(context, &result.scene_summary);
            result.has_scene_summary = result.engine_result == HENKA_SUCCESS;
            result.status = result.engine_result == HENKA_SUCCESS ? HENKA_ACTION_STATUS_OK : HENKA_ACTION_STATUS_INVALID_CONTEXT;
            result.success = result.engine_result == HENKA_SUCCESS;
            henka_action_set_message(&result, result.success ? "Scene summary ready." : "Scene summary could not be read.");
            break;

        case HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT:
        {
            henka_entity entity;
            henka_bounds bounds;

            if (!henka_action_name_is_valid(request->params.add_primitive.name))
            {
                result.status = HENKA_ACTION_STATUS_INVALID_NAME;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Primitive objects need a non-empty name.");
                break;
            }

            if (!henka_action_transform_is_valid(request->params.add_primitive.transform))
            {
                result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Primitive object transform is invalid.");
                break;
            }

            result.status = HENKA_ACTION_STATUS_OK;
            result.engine_result = HENKA_SUCCESS;
            result.success = true;
            henka_action_set_message(&result, request->dry_run ? "Validated primitive object creation." : "Primitive object created.");
            if (request->dry_run)
            {
                break;
            }

            entity = henka_scene_create_entity_named(context->scene, request->params.add_primitive.name);
            if (entity == HENKA_INVALID_ENTITY)
            {
                result.status = HENKA_ACTION_STATUS_OUT_OF_MEMORY;
                result.engine_result = HENKA_ERROR_OUT_OF_MEMORY;
                result.success = false;
                henka_action_set_message(&result, "Primitive object could not be created.");
                break;
            }

            bounds = henka_action_get_primitive_bounds(request->params.add_primitive.primitive);
            henka_scene_set_entity_transform(context->scene, entity, request->params.add_primitive.transform);
            henka_scene_set_entity_visible(context->scene, entity, request->params.add_primitive.visible);
            henka_scene_set_entity_local_bounds(context->scene, entity, bounds);
            henka_scene_set_entity_tag(context->scene, entity, henka_action_get_primitive_tag(request->params.add_primitive.primitive));
            henka_action_context_register_default_transform(context, entity, request->params.add_primitive.transform);
            result.affected_entity = entity;
            result.selected_entity = henka_action_context_get_selected_entity(context);
            henka_action_fill_object_details(context, entity, &result.object_details);
            result.has_object_details = true;
            break;
        }

        case HENKA_ACTION_COMMAND_DELETE_OBJECT:
            status = henka_action_validate_entity_target(context, request->params.entity.entity, false);
            if (status != HENKA_ACTION_STATUS_OK)
            {
                result.status = status;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Delete target is not a valid user object.");
                break;
            }
            result.affected_entity = request->params.entity.entity;
            result.status = HENKA_ACTION_STATUS_OK;
            result.engine_result = HENKA_SUCCESS;
            result.success = true;
            if (!request->dry_run)
            {
                henka_scene_destroy_entity(context->scene, request->params.entity.entity);
                henka_action_remove_default_transform(context, request->params.entity.entity);
                if (context->selected_entity == request->params.entity.entity)
                {
                    context->selected_entity = HENKA_INVALID_ENTITY;
                }
            }
            result.selected_entity = henka_action_context_get_selected_entity(context);
            henka_action_set_message(&result, request->dry_run ? "Validated object deletion." : "Deleted object.");
            break;

        case HENKA_ACTION_COMMAND_RENAME_OBJECT:
            status = henka_action_validate_entity_target(context, request->params.rename.entity, false);
            if (status != HENKA_ACTION_STATUS_OK)
            {
                result.status = status;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Rename target is not a valid user object.");
                break;
            }
            if (!henka_action_name_is_valid(request->params.rename.name))
            {
                result.status = HENKA_ACTION_STATUS_INVALID_NAME;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Object name must not be empty.");
                break;
            }
            result.affected_entity = request->params.rename.entity;
            result.status = HENKA_ACTION_STATUS_OK;
            result.engine_result = HENKA_SUCCESS;
            result.success = true;
            if (!request->dry_run)
            {
                result.engine_result = henka_scene_set_entity_name(context->scene, request->params.rename.entity, request->params.rename.name);
                result.status = henka_action_status_from_result(result.engine_result);
                result.success = result.engine_result == HENKA_SUCCESS;
            }
            henka_action_set_message(&result, result.success ? "Object name updated." : "Object name could not be updated.");
            break;

        case HENKA_ACTION_COMMAND_SELECT_OBJECT:
            status = henka_action_validate_entity_target(context, request->params.entity.entity, false);
            if (status != HENKA_ACTION_STATUS_OK)
            {
                result.status = status;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Selection target is not a valid user object.");
                break;
            }
            result.affected_entity = request->params.entity.entity;
            result.status = HENKA_ACTION_STATUS_OK;
            result.engine_result = HENKA_SUCCESS;
            result.success = true;
            if (!request->dry_run)
            {
                context->selected_entity = request->params.entity.entity;
            }
            result.selected_entity = request->params.entity.entity;
            henka_action_set_message(&result, request->dry_run ? "Validated object selection." : "Selected object.");
            break;

        case HENKA_ACTION_COMMAND_CLEAR_SELECTION:
            result.status = HENKA_ACTION_STATUS_OK;
            result.engine_result = HENKA_SUCCESS;
            result.success = true;
            if (!request->dry_run)
            {
                context->selected_entity = HENKA_INVALID_ENTITY;
            }
            result.selected_entity = HENKA_INVALID_ENTITY;
            henka_action_set_message(&result, request->dry_run ? "Validated clear selection." : "Selection cleared.");
            break;

        case HENKA_ACTION_COMMAND_GET_SELECTED_OBJECT:
            if (!henka_action_is_user_entity(context, context->selected_entity))
            {
                result.status = HENKA_ACTION_STATUS_TARGET_NOT_FOUND;
                result.engine_result = HENKA_ERROR_UNKNOWN;
                henka_action_set_message(&result, "No user object is selected.");
                break;
            }
            result.affected_entity = context->selected_entity;
            result.selected_entity = context->selected_entity;
            result.engine_result = henka_action_fill_object_details(context, context->selected_entity, &result.object_details);
            result.has_object_details = result.engine_result == HENKA_SUCCESS;
            result.status = result.engine_result == HENKA_SUCCESS ? HENKA_ACTION_STATUS_OK : HENKA_ACTION_STATUS_INVALID_ENTITY;
            result.success = result.engine_result == HENKA_SUCCESS;
            henka_action_set_message(&result, result.success ? "Selected object details ready." : "Selected object details could not be read.");
            break;

        case HENKA_ACTION_COMMAND_GET_OBJECT_DETAILS:
            status = henka_action_validate_entity_target(context, request->params.entity.entity, false);
            if (status != HENKA_ACTION_STATUS_OK)
            {
                result.status = status;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Object details target is not a valid user object.");
                break;
            }
            result.affected_entity = request->params.entity.entity;
            result.engine_result = henka_action_fill_object_details(context, request->params.entity.entity, &result.object_details);
            result.has_object_details = result.engine_result == HENKA_SUCCESS;
            result.status = result.engine_result == HENKA_SUCCESS ? HENKA_ACTION_STATUS_OK : HENKA_ACTION_STATUS_INVALID_ENTITY;
            result.success = result.engine_result == HENKA_SUCCESS;
            henka_action_set_message(&result, result.success ? "Object details ready." : "Object details could not be read.");
            break;

        case HENKA_ACTION_COMMAND_SET_POSITION:
        case HENKA_ACTION_COMMAND_SET_ROTATION:
        case HENKA_ACTION_COMMAND_SET_SCALE:
        case HENKA_ACTION_COMMAND_MOVE_BY_DELTA:
        case HENKA_ACTION_COMMAND_ROTATE_BY_DELTA:
        case HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER:
        case HENKA_ACTION_COMMAND_RESET_TRANSFORM:
        case HENKA_ACTION_COMMAND_HIDE_OBJECT:
        case HENKA_ACTION_COMMAND_SHOW_OBJECT:
        case HENKA_ACTION_COMMAND_FOCUS_CAMERA_ON_OBJECT:
        {
            henka_entity entity;
            henka_transform transform;
            henka_transform next_transform;
            const henka_action_default_transform_record* default_record;
            bool transform_mutation;

            entity = request->params.entity.entity;
            if (request->command == HENKA_ACTION_COMMAND_SET_POSITION)
            {
                entity = request->params.set_position.entity;
            }
            else if (request->command == HENKA_ACTION_COMMAND_SET_ROTATION)
            {
                entity = request->params.set_rotation.entity;
            }
            else if (request->command == HENKA_ACTION_COMMAND_SET_SCALE)
            {
                entity = request->params.set_scale.entity;
            }
            else if (request->command == HENKA_ACTION_COMMAND_MOVE_BY_DELTA)
            {
                entity = request->params.move_by_delta.entity;
            }
            else if (request->command == HENKA_ACTION_COMMAND_ROTATE_BY_DELTA)
            {
                entity = request->params.rotate_by_delta.entity;
            }
            else if (request->command == HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER)
            {
                entity = request->params.scale_by_multiplier.entity;
            }

            transform_mutation =
                request->command == HENKA_ACTION_COMMAND_SET_POSITION ||
                request->command == HENKA_ACTION_COMMAND_SET_ROTATION ||
                request->command == HENKA_ACTION_COMMAND_SET_SCALE ||
                request->command == HENKA_ACTION_COMMAND_MOVE_BY_DELTA ||
                request->command == HENKA_ACTION_COMMAND_ROTATE_BY_DELTA ||
                request->command == HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER ||
                request->command == HENKA_ACTION_COMMAND_RESET_TRANSFORM;

            status = henka_action_validate_entity_target(
                context,
                entity,
                transform_mutation || request->command == HENKA_ACTION_COMMAND_FOCUS_CAMERA_ON_OBJECT);
            if (status != HENKA_ACTION_STATUS_OK)
            {
                result.status = status;
                result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                henka_action_set_message(&result, "Action target is not valid for this command.");
                break;
            }

            result.affected_entity = entity;
            result.selected_entity = henka_action_context_get_selected_entity(context);

            if (transform_mutation)
            {
                if (henka_scene_get_entity_transform(context->scene, entity, &transform) != HENKA_SUCCESS)
                {
                    result.status = HENKA_ACTION_STATUS_INVALID_ENTITY;
                    result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                    henka_action_set_message(&result, "Object transform could not be read.");
                    break;
                }

                next_transform = transform;
                result.has_before_transform = true;
                result.before_transform = transform;

                switch (request->command)
                {
                    case HENKA_ACTION_COMMAND_SET_POSITION:
                        if (!henka_action_vector_is_valid(request->params.set_position.position))
                        {
                            result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                            result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                            henka_action_set_message(&result, "Position is not finite.");
                            goto mutation_done;
                        }
                        next_transform.position = request->params.set_position.position;
                        break;
                    case HENKA_ACTION_COMMAND_SET_ROTATION:
                        if (!henka_action_quat_is_valid(request->params.set_rotation.rotation))
                        {
                            result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                            result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                            henka_action_set_message(&result, "Rotation is not finite.");
                            goto mutation_done;
                        }
                        next_transform.rotation = request->params.set_rotation.rotation;
                        break;
                    case HENKA_ACTION_COMMAND_SET_SCALE:
                        if (!henka_action_scale_vector_is_valid(request->params.set_scale.scale))
                        {
                            result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                            result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                            henka_action_set_message(&result, "Scale components must be finite and not zero or near zero.");
                            goto mutation_done;
                        }
                        next_transform.scale = request->params.set_scale.scale;
                        break;
                    case HENKA_ACTION_COMMAND_MOVE_BY_DELTA:
                        if (!henka_action_vector_is_valid(request->params.move_by_delta.delta))
                        {
                            result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                            result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                            henka_action_set_message(&result, "Move delta is not finite.");
                            goto mutation_done;
                        }
                        next_transform.position = henka_vec3_add(transform.position, request->params.move_by_delta.delta);
                        break;
                    case HENKA_ACTION_COMMAND_ROTATE_BY_DELTA:
                        if (!henka_action_quat_is_valid(request->params.rotate_by_delta.delta_rotation))
                        {
                            result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                            result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                            henka_action_set_message(&result, "Rotation delta is not finite.");
                            goto mutation_done;
                        }
                        next_transform.rotation = henka_quat_multiply(request->params.rotate_by_delta.delta_rotation, transform.rotation);
                        break;
                    case HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER:
                        if (!henka_action_scale_vector_is_valid(request->params.scale_by_multiplier.scale_multiplier))
                        {
                            result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                            result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                            henka_action_set_message(&result, "Scale multiplier components must be finite and not zero or near zero.");
                            goto mutation_done;
                        }
                        next_transform.scale.x = transform.scale.x * request->params.scale_by_multiplier.scale_multiplier.x;
                        next_transform.scale.y = transform.scale.y * request->params.scale_by_multiplier.scale_multiplier.y;
                        next_transform.scale.z = transform.scale.z * request->params.scale_by_multiplier.scale_multiplier.z;
                        break;
                    case HENKA_ACTION_COMMAND_RESET_TRANSFORM:
                        default_record = henka_action_find_default_transform_record_const(context, entity);
                        if (default_record == NULL)
                        {
                            result.status = HENKA_ACTION_STATUS_NO_DEFAULT_TRANSFORM;
                            result.engine_result = HENKA_ERROR_UNKNOWN;
                            henka_action_set_message(&result, "No default transform is registered for this object.");
                            goto mutation_done;
                        }
                        next_transform = default_record->transform;
                        break;
                    default:
                        break;
                }

                if (!henka_action_transform_is_valid(next_transform))
                {
                    result.status = HENKA_ACTION_STATUS_INVALID_TRANSFORM;
                    result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                    henka_action_set_message(&result, "Resulting transform contains invalid or near-zero scale data.");
                    goto mutation_done;
                }

                result.has_after_transform = true;
                result.after_transform = next_transform;
                result.status = HENKA_ACTION_STATUS_OK;
                result.engine_result = HENKA_SUCCESS;
                result.success = true;
                if (!request->dry_run)
                {
                    result.engine_result = henka_scene_set_entity_transform(context->scene, entity, next_transform);
                    result.status = henka_action_status_from_result(result.engine_result);
                    result.success = result.engine_result == HENKA_SUCCESS;
                    if (result.success)
                    {
                        henka_scene_get_entity_transform(context->scene, entity, &result.after_transform);
                    }
                }
                if (result.success && request->command == HENKA_ACTION_COMMAND_RESET_TRANSFORM)
                {
                    henka_action_set_message(&result, request->dry_run ? "Validated transform reset." : "Object transform reset.");
                }
                else if (result.success)
                {
                    henka_action_set_message(&result, request->dry_run ? "Validated transform action." : "Object transform updated.");
                }
mutation_done:
                break;
            }

            if (request->command == HENKA_ACTION_COMMAND_HIDE_OBJECT || request->command == HENKA_ACTION_COMMAND_SHOW_OBJECT)
            {
                const bool visible = request->command == HENKA_ACTION_COMMAND_SHOW_OBJECT;
                result.status = HENKA_ACTION_STATUS_OK;
                result.engine_result = HENKA_SUCCESS;
                result.success = true;
                if (!request->dry_run)
                {
                    result.engine_result = henka_scene_set_entity_visible(context->scene, entity, visible);
                    result.status = henka_action_status_from_result(result.engine_result);
                    result.success = result.engine_result == HENKA_SUCCESS;
                    if (!visible && context->selected_entity == entity)
                    {
                        context->selected_entity = HENKA_INVALID_ENTITY;
                    }
                }
                result.selected_entity = henka_action_context_get_selected_entity(context);
                henka_action_set_message(&result, visible ? "Object shown." : "Object hidden.");
            }
            else if (request->command == HENKA_ACTION_COMMAND_FOCUS_CAMERA_ON_OBJECT)
            {
                henka_bounds bounds;

                if (context->camera == NULL)
                {
                    result.status = HENKA_ACTION_STATUS_NO_CAMERA;
                    result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                    henka_action_set_message(&result, "No camera is attached to the action context.");
                    break;
                }
                if (henka_scene_get_entity_world_bounds(context->scene, entity, &bounds) != HENKA_SUCCESS)
                {
                    result.status = HENKA_ACTION_STATUS_INVALID_ENTITY;
                    result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
                    henka_action_set_message(&result, "Object bounds are not available for camera focus.");
                    break;
                }
                result.status = HENKA_ACTION_STATUS_OK;
                result.engine_result = HENKA_SUCCESS;
                result.success = true;
                if (!request->dry_run && !henka_camera_focus_on_bounds(context->camera, bounds))
                {
                    result.status = HENKA_ACTION_STATUS_UNSUPPORTED;
                    result.engine_result = HENKA_ERROR_UNKNOWN;
                    result.success = false;
                    henka_action_set_message(&result, "Camera focus failed.");
                    break;
                }
                henka_action_set_message(&result, request->dry_run ? "Validated camera focus." : "Camera focused on object.");
            }
            break;
        }

        case HENKA_ACTION_COMMAND_NONE:
        default:
            result.status = HENKA_ACTION_STATUS_INVALID_COMMAND;
            result.engine_result = HENKA_ERROR_INVALID_ARGUMENT;
            henka_action_set_message(&result, "Action command is not supported.");
            break;
    }

    *out_result = result;
    return HENKA_SUCCESS;
}

henka_result henka_action_validate(
    henka_action_context* context,
    const henka_action_request* request,
    henka_action_result* out_result)
{
    henka_action_request dry_run_request;

    if (request == NULL || out_result == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    dry_run_request = *request;
    dry_run_request.dry_run = true;
    return henka_action_execute(context, &dry_run_request, out_result);
}

const char* henka_action_command_to_string(henka_action_command command)
{
    switch (command)
    {
        case HENKA_ACTION_COMMAND_CLEAR_SCENE:
            return "clear_scene";
        case HENKA_ACTION_COMMAND_GET_SCENE_SUMMARY:
            return "get_scene_summary";
        case HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT:
            return "add_primitive_object";
        case HENKA_ACTION_COMMAND_DELETE_OBJECT:
            return "delete_object";
        case HENKA_ACTION_COMMAND_RENAME_OBJECT:
            return "rename_object";
        case HENKA_ACTION_COMMAND_SELECT_OBJECT:
            return "select_object";
        case HENKA_ACTION_COMMAND_CLEAR_SELECTION:
            return "clear_selection";
        case HENKA_ACTION_COMMAND_GET_SELECTED_OBJECT:
            return "get_selected_object";
        case HENKA_ACTION_COMMAND_GET_OBJECT_DETAILS:
            return "get_object_details";
        case HENKA_ACTION_COMMAND_SET_POSITION:
            return "set_position";
        case HENKA_ACTION_COMMAND_SET_ROTATION:
            return "set_rotation";
        case HENKA_ACTION_COMMAND_SET_SCALE:
            return "set_scale";
        case HENKA_ACTION_COMMAND_MOVE_BY_DELTA:
            return "move_by_delta";
        case HENKA_ACTION_COMMAND_ROTATE_BY_DELTA:
            return "rotate_by_delta";
        case HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER:
            return "scale_by_multiplier";
        case HENKA_ACTION_COMMAND_RESET_TRANSFORM:
            return "reset_transform";
        case HENKA_ACTION_COMMAND_HIDE_OBJECT:
            return "hide_object";
        case HENKA_ACTION_COMMAND_SHOW_OBJECT:
            return "show_object";
        case HENKA_ACTION_COMMAND_FOCUS_CAMERA_ON_OBJECT:
            return "focus_camera_on_object";
        case HENKA_ACTION_COMMAND_NONE:
        default:
            return "none";
    }
}

const char* henka_action_status_to_string(henka_action_status status)
{
    switch (status)
    {
        case HENKA_ACTION_STATUS_OK:
            return "ok";
        case HENKA_ACTION_STATUS_INVALID_CONTEXT:
            return "invalid context";
        case HENKA_ACTION_STATUS_INVALID_COMMAND:
            return "invalid command";
        case HENKA_ACTION_STATUS_INVALID_ENTITY:
            return "invalid entity";
        case HENKA_ACTION_STATUS_INVALID_NAME:
            return "invalid name";
        case HENKA_ACTION_STATUS_INVALID_TRANSFORM:
            return "invalid transform";
        case HENKA_ACTION_STATUS_HELPER_ENTITY:
            return "helper entity";
        case HENKA_ACTION_STATUS_TARGET_HIDDEN:
            return "target hidden";
        case HENKA_ACTION_STATUS_TARGET_NOT_FOUND:
            return "target not found";
        case HENKA_ACTION_STATUS_NO_DEFAULT_TRANSFORM:
            return "no default transform";
        case HENKA_ACTION_STATUS_NO_CAMERA:
            return "no camera";
        case HENKA_ACTION_STATUS_UNSUPPORTED:
            return "unsupported";
        case HENKA_ACTION_STATUS_OUT_OF_MEMORY:
            return "out of memory";
        default:
            return "unknown";
    }
}
