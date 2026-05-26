#ifndef HENKA_ACTION_H
#define HENKA_ACTION_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/camera.h>
#include <henka/result.h>
#include <henka/scene.h>

typedef struct henka_action_context henka_action_context;

typedef enum henka_action_command
{
    HENKA_ACTION_COMMAND_NONE = 0,
    HENKA_ACTION_COMMAND_CLEAR_SCENE,
    HENKA_ACTION_COMMAND_GET_SCENE_SUMMARY,
    HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT,
    HENKA_ACTION_COMMAND_DELETE_OBJECT,
    HENKA_ACTION_COMMAND_RENAME_OBJECT,
    HENKA_ACTION_COMMAND_SELECT_OBJECT,
    HENKA_ACTION_COMMAND_GET_SELECTED_OBJECT,
    HENKA_ACTION_COMMAND_GET_OBJECT_DETAILS,
    HENKA_ACTION_COMMAND_SET_POSITION,
    HENKA_ACTION_COMMAND_SET_ROTATION,
    HENKA_ACTION_COMMAND_SET_SCALE,
    HENKA_ACTION_COMMAND_MOVE_BY_DELTA,
    HENKA_ACTION_COMMAND_ROTATE_BY_DELTA,
    HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER,
    HENKA_ACTION_COMMAND_RESET_TRANSFORM,
    HENKA_ACTION_COMMAND_HIDE_OBJECT,
    HENKA_ACTION_COMMAND_SHOW_OBJECT,
    HENKA_ACTION_COMMAND_FOCUS_CAMERA_ON_OBJECT
} henka_action_command;

typedef enum henka_action_status
{
    HENKA_ACTION_STATUS_OK = 0,
    HENKA_ACTION_STATUS_INVALID_CONTEXT,
    HENKA_ACTION_STATUS_INVALID_COMMAND,
    HENKA_ACTION_STATUS_INVALID_ENTITY,
    HENKA_ACTION_STATUS_INVALID_NAME,
    HENKA_ACTION_STATUS_INVALID_TRANSFORM,
    HENKA_ACTION_STATUS_HELPER_ENTITY,
    HENKA_ACTION_STATUS_TARGET_HIDDEN,
    HENKA_ACTION_STATUS_TARGET_NOT_FOUND,
    HENKA_ACTION_STATUS_NO_DEFAULT_TRANSFORM,
    HENKA_ACTION_STATUS_NO_CAMERA,
    HENKA_ACTION_STATUS_UNSUPPORTED,
    HENKA_ACTION_STATUS_OUT_OF_MEMORY
} henka_action_status;

typedef enum henka_action_primitive
{
    HENKA_ACTION_PRIMITIVE_CUBE = 0,
    HENKA_ACTION_PRIMITIVE_PLANE,
    HENKA_ACTION_PRIMITIVE_MARKER
} henka_action_primitive;

typedef struct henka_action_scene_summary
{
    size_t entity_count;
    size_t helper_entity_count;
    size_t user_entity_count;
    size_t visible_user_entity_count;
    henka_entity selected_entity;
} henka_action_scene_summary;

typedef struct henka_action_object_details
{
    henka_scene_object_info object;
    bool selected;
    bool helper;
    bool has_default_transform;
    henka_transform default_transform;
} henka_action_object_details;

typedef struct henka_action_request
{
    henka_action_command command;
    bool dry_run;
    union
    {
        struct
        {
            henka_action_primitive primitive;
            const char* name;
            henka_transform transform;
            bool visible;
        } add_primitive;
        struct
        {
            henka_entity entity;
        } entity;
        struct
        {
            henka_entity entity;
            const char* name;
        } rename;
        struct
        {
            henka_entity entity;
            henka_vec3 position;
        } set_position;
        struct
        {
            henka_entity entity;
            henka_quat rotation;
        } set_rotation;
        struct
        {
            henka_entity entity;
            henka_vec3 scale;
        } set_scale;
        struct
        {
            henka_entity entity;
            henka_vec3 delta;
        } move_by_delta;
        struct
        {
            henka_entity entity;
            henka_quat delta_rotation;
        } rotate_by_delta;
        struct
        {
            henka_entity entity;
            henka_vec3 scale_multiplier;
        } scale_by_multiplier;
    } params;
} henka_action_request;

typedef struct henka_action_result
{
    bool success;
    bool dry_run;
    henka_action_command command;
    henka_action_status status;
    henka_result engine_result;
    henka_entity affected_entity;
    henka_entity selected_entity;
    bool has_before_transform;
    bool has_after_transform;
    henka_transform before_transform;
    henka_transform after_transform;
    bool has_object_details;
    henka_action_object_details object_details;
    bool has_scene_summary;
    henka_action_scene_summary scene_summary;
    char message[160];
} henka_action_result;

henka_result henka_action_context_create(henka_action_context** out_context);
void henka_action_context_destroy(henka_action_context* context);
henka_result henka_action_context_set_scene(henka_action_context* context, henka_scene* scene);
henka_result henka_action_context_set_camera(henka_action_context* context, henka_camera* camera);
henka_result henka_action_context_register_default_transform(
    henka_action_context* context,
    henka_entity entity,
    henka_transform default_transform);
henka_entity henka_action_context_get_selected_entity(const henka_action_context* context);
henka_result henka_action_get_scene_summary(const henka_action_context* context, henka_action_scene_summary* out_summary);
henka_result henka_action_list_objects(
    const henka_action_context* context,
    henka_action_object_details* out_objects,
    size_t capacity,
    size_t* out_count);
henka_result henka_action_execute(
    henka_action_context* context,
    const henka_action_request* request,
    henka_action_result* out_result);
henka_result henka_action_validate(
    henka_action_context* context,
    const henka_action_request* request,
    henka_action_result* out_result);
const char* henka_action_command_to_string(henka_action_command command);
const char* henka_action_status_to_string(henka_action_status status);

#endif
