#include <stdint.h>
#include <string.h>

#include "test_suite.h"

#include "../engine/src/henka_internal.h"
#include "../examples/sandbox3d/asset_browser_tools.h"

static void henka_test_sandbox3d_asset_browser_collection(void)
{
    henka_asset_manager manager;
    henka_asset_texture_entry textures[2];
    henka_asset_mesh_entry meshes[1];
    sandbox3d_asset_browser_item items[2];

    memset(&manager, 0, sizeof(manager));
    memset(textures, 0, sizeof(textures));
    memset(meshes, 0, sizeof(meshes));
    textures[0].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    textures[0].metadata.source_path = "assets/textures/a.png";
    textures[0].metadata.display_name = "a.png";
    textures[1].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    textures[1].metadata.source_path = "assets/textures/b.ktx2";
    textures[1].metadata.display_name = "b.ktx2";
    meshes[0].metadata.type = HENKA_ASSET_TYPE_MESH;
    meshes[0].metadata.source_path = "assets/models/a.obj";
    manager.texture_entries = textures;
    manager.texture_count = 2U;
    manager.mesh_entries = meshes;
    manager.mesh_count = 1U;

    HENKA_TEST_ASSERT(sandbox3d_asset_browser_collect(&manager, HENKA_ASSET_TYPE_TEXTURE, items, 1U) == 2U);
    HENKA_TEST_ASSERT(items[0].metadata_index == 0U);
    HENKA_TEST_ASSERT(strcmp(items[0].metadata.display_name, "a.png") == 0);
    HENKA_TEST_ASSERT(sandbox3d_asset_browser_collect(&manager, HENKA_ASSET_TYPE_MESH, items, 2U) == 1U);
    HENKA_TEST_ASSERT(items[0].metadata_index == 2U);
    HENKA_TEST_ASSERT(sandbox3d_asset_browser_collect(&manager, HENKA_ASSET_TYPE_MATERIAL, items, 2U) == 0U);
}

static void henka_test_sandbox3d_asset_browser_paging(void)
{
    henka_asset_manager manager;
    henka_asset_texture_entry textures[7];
    sandbox3d_asset_browser_item items[3];
    size_t index;

    memset(&manager, 0, sizeof(manager));
    memset(textures, 0, sizeof(textures));
    for (index = 0U; index < 7U; ++index)
    {
        textures[index].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
        textures[index].metadata.source_path = "assets/textures/paged.png";
    }
    manager.texture_entries = textures;
    manager.texture_count = 7U;

    HENKA_TEST_ASSERT(sandbox3d_asset_browser_page_count(&manager, HENKA_ASSET_TYPE_TEXTURE, 3U) == 3U);
    HENKA_TEST_ASSERT(sandbox3d_asset_browser_page_count(&manager, HENKA_ASSET_TYPE_TEXTURE, 0U) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_asset_browser_collect_page(
        &manager, HENKA_ASSET_TYPE_TEXTURE, 1U, 3U, items, 3U) == 3U);
    HENKA_TEST_ASSERT(items[0].metadata_index == 3U);
    HENKA_TEST_ASSERT(items[2].metadata_index == 5U);
    HENKA_TEST_ASSERT(sandbox3d_asset_browser_collect_page(
        &manager, HENKA_ASSET_TYPE_TEXTURE, 2U, 3U, items, 1U) == 1U);
    HENKA_TEST_ASSERT(items[0].metadata_index == 6U);
    HENKA_TEST_ASSERT(sandbox3d_asset_browser_collect_page(
        &manager, HENKA_ASSET_TYPE_TEXTURE, 3U, 3U, items, 3U) == 0U);
}

static void henka_test_sandbox3d_asset_browser_texture_and_assignment(void)
{
    henka_asset_manager manager;
    henka_asset_texture_entry texture_entry;
    henka_texture texture;
    henka_material material;
    henka_material_asset definition;
    henka_material_instance instance;
    henka_material before;
    sandbox3d_texture_slot_display display;

    memset(&manager, 0, sizeof(manager));
    memset(&texture_entry, 0, sizeof(texture_entry));
    memset(&texture, 0, sizeof(texture));
    texture.width = 256;
    texture.height = 128;
    texture.backend_data = (void*)(uintptr_t)1U;
    texture.descriptor = henka_texture_descriptor_default_color();
    texture.resident_gpu_bytes = 65536U;
    texture.resident_mip_count = 4U;
    texture.mip_count = 6U;
    texture_entry.texture = &texture;
    texture_entry.metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    texture_entry.metadata.source_path = "assets/textures/authoring.png";
    texture_entry.metadata.display_name = "authoring.png";
    texture_entry.metadata.loaded = true;
    manager.texture_entries = &texture_entry;
    manager.texture_count = 1U;

    material = henka_material_default();
    material.shader = (henka_shader*)(uintptr_t)1U;
    material.base_color_texture = &texture;
    material.use_texture = true;
    memset(&definition, 0, sizeof(definition));
    definition.material = material;
    definition.revision = 1U;
    HENKA_TEST_ASSERT(sandbox3d_format_material_texture_slot(
        &manager,
        &material,
        HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR,
        &display) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(display.assigned);
    HENKA_TEST_ASSERT(strcmp(display.asset_identity, "assets/textures/authoring.png") == 0);
    HENKA_TEST_ASSERT(strcmp(display.dimensions, "256 x 128") == 0);
    HENKA_TEST_ASSERT(strcmp(display.mip_state, "4/6 resident") == 0);

    memset(&instance, 0, sizeof(instance));
    instance.definition = &definition;
    instance.definition_revision = 1U;
    instance.material = material;
    before = instance.material;
    HENKA_TEST_ASSERT(sandbox3d_assign_material_instance_texture(
        &manager,
        &instance,
        HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR,
        NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(instance.material.base_color_texture == NULL);
    HENKA_TEST_ASSERT(!instance.material.use_texture);
    HENKA_TEST_ASSERT(sandbox3d_assign_material_instance_texture(
        &manager,
        &instance,
        HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR,
        (henka_texture*)(uintptr_t)2U) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(instance.material.base_color_texture == NULL);
    HENKA_TEST_ASSERT(before.base_color_texture == &texture);
}

void henka_test_sandbox3d_asset_browser(void)
{
    henka_test_sandbox3d_asset_browser_collection();
    henka_test_sandbox3d_asset_browser_paging();
    henka_test_sandbox3d_asset_browser_texture_and_assignment();
}
