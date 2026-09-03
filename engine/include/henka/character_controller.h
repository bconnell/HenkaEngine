#ifndef HENKA_CHARACTER_CONTROLLER_H
#define HENKA_CHARACTER_CONTROLLER_H

#include <stdbool.h>

#include <henka/physics.h>

typedef struct henka_character_controller henka_character_controller;

/*
 * The v1 controller is a production dynamic-body component. It owns the
 * body it creates, but not the physics world or linked scene. The body uses
 * an upright capsule collider; a zero half_height is sphere-equivalent.
 * Planar input is applied during prepare_step and the caller advances the
 * shared physics world exactly once before sync_after_step. A grounded
 * controller inherits linear motion from a live kinematic support body;
 * destroyed or stale support identities are ignored until the next sync. When
 * both linked_scene and linked_entity identify a live scene entity, the owned
 * body uses the normal physics transform path to synchronize that entity after
 * creation, teleport, and successful fixed steps. Both layer and mask must
 * contain at least one collision bit so the controller cannot be created as
 * an inert collision participant. The link is borrowed; the
 * controller does not own the scene or entity. The linked scene may be
 * destroyed before the controller; its contents are retired immediately and
 * its storage remains valid until the controller releases the body link.
 */
typedef struct henka_character_controller_desc
{
    henka_transform transform;
    float radius;
    float max_speed;
    float jump_speed;
    uint32_t layer;
    uint32_t mask;
    henka_scene* linked_scene;
    henka_entity linked_entity;
    float half_height;
} henka_character_controller_desc;

typedef struct henka_character_controller_state
{
    henka_physics_body_id body;
    henka_transform transform;
    henka_vec3 velocity;
    bool grounded;
    bool jump_queued;
    henka_vec3 ground_normal;
} henka_character_controller_state;

henka_result henka_character_controller_create(
    henka_physics_world* world,
    const henka_character_controller_desc* desc,
    henka_character_controller** out_controller);

/*
 * The world must outlive the controller. Release is idempotent when the
 * owned body was already removed, but other destruction failures retain the
 * controller for retry.
 */
henka_result henka_character_controller_destroy(
    henka_character_controller* controller);

henka_result henka_character_controller_set_planar_velocity(
    henka_character_controller* controller,
    henka_vec3 desired_velocity);
/*
 * Sets planar movement response in world units per second squared. A zero
 * acceleration or deceleration preserves the legacy immediate response for
 * that direction. The update is atomic when validation fails.
 */
henka_result henka_character_controller_set_movement_tuning(
    henka_character_controller* controller,
    float acceleration,
    float deceleration);
/* Sets the fraction of requested planar velocity applied while airborne.
 * Zero preserves current horizontal velocity; one preserves full air control. */
henka_result henka_character_controller_set_air_control(
    henka_character_controller* controller,
    float air_control);
/* Sets the maximum walkable surface angle in degrees. The default is 45
 * degrees. Grounding is updated by sync_after_step from physics contacts. */
henka_result henka_character_controller_set_slope_limit(
    henka_character_controller* controller,
    float slope_limit_degrees);
/* Repositions the owned body through the physics transform contract. A failed
 * transform leaves the controller and its body unchanged. */
henka_result henka_character_controller_teleport(
    henka_character_controller* controller,
    henka_transform transform,
    bool clear_velocity);
henka_result henka_character_controller_queue_jump(
    henka_character_controller* controller);

/*
 * Applies the pending command to the owned body. The caller must then step
 * the shared physics world and call sync_after_step. A failed call preserves
 * the pending command and the controller's last synchronized state.
 */
henka_result henka_character_controller_prepare_step(
    henka_character_controller* controller);
henka_result henka_character_controller_sync_after_step(
    henka_character_controller* controller);
henka_result henka_character_controller_get_state(
    const henka_character_controller* controller,
    henka_character_controller_state* out_state);

#endif
