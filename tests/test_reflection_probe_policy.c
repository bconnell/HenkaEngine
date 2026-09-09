#include <stdio.h>
#include <string.h>

#include <henka/scene.h>

#include "../engine/src/renderer/reflection_probe_policy.h"

static void poison_descriptor_padding(
    henka_scene_reflection_probe_desc* descriptor,
    unsigned char value)
{
    memset(descriptor, value, sizeof(*descriptor));
    descriptor->position = (henka_vec3){1.0f, 2.0f, 3.0f};
    descriptor->extents = (henka_vec3){4.0f, 5.0f, 6.0f};
    descriptor->influence = 0.75f;
    descriptor->enabled = true;
    descriptor->box_projection = true;
}

int main(void)
{
    henka_scene_reflection_probe_desc left;
    henka_scene_reflection_probe_desc right;

    poison_descriptor_padding(&left, 0x11U);
    poison_descriptor_padding(&right, 0xEEU);
    if (!henka_reflection_probe_desc_equal(&left, &right) ||
        henka_reflection_probe_desc_equal(NULL, &right) ||
        henka_reflection_probe_desc_equal(&left, NULL))
    {
        fprintf(stderr, "semantic reflection-probe descriptor equality failed\n");
        return 1;
    }

    right = left;
    right.position.x += 1.0f;
    if (henka_reflection_probe_desc_equal(&left, &right))
    {
        fprintf(stderr, "changed reflection-probe position compared equal\n");
        return 1;
    }

    right = left;
    right.extents.y += 1.0f;
    if (henka_reflection_probe_desc_equal(&left, &right))
    {
        fprintf(stderr, "changed reflection-probe extents compared equal\n");
        return 1;
    }

    right = left;
    right.influence += 0.1f;
    if (henka_reflection_probe_desc_equal(&left, &right))
    {
        fprintf(stderr, "changed reflection-probe influence compared equal\n");
        return 1;
    }

    right = left;
    right.enabled = !right.enabled;
    if (henka_reflection_probe_desc_equal(&left, &right))
    {
        fprintf(stderr, "changed reflection-probe enabled flag compared equal\n");
        return 1;
    }

    right = left;
    right.box_projection = !right.box_projection;
    if (henka_reflection_probe_desc_equal(&left, &right))
    {
        fprintf(stderr, "changed reflection-probe projection flag compared equal\n");
        return 1;
    }

    puts("semantic reflection-probe descriptor equality passed");
    return 0;
}
