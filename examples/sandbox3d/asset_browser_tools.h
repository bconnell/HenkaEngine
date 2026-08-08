#ifndef SANDBOX3D_ASSET_BROWSER_TOOLS_H
#define SANDBOX3D_ASSET_BROWSER_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/assets.h>

typedef struct sandbox3d_asset_browser_item
{
    size_t metadata_index;
    henka_asset_metadata metadata;
} sandbox3d_asset_browser_item;

size_t sandbox3d_asset_browser_collect(
    const henka_asset_manager* manager,
    henka_asset_type type,
    sandbox3d_asset_browser_item* out_items,
    size_t capacity);

size_t sandbox3d_asset_browser_page_count(
    const henka_asset_manager* manager,
    henka_asset_type type,
    size_t page_size);

size_t sandbox3d_asset_browser_collect_page(
    const henka_asset_manager* manager,
    henka_asset_type type,
    size_t page_index,
    size_t page_size,
    sandbox3d_asset_browser_item* out_items,
    size_t capacity);

typedef struct sandbox3d_texture_slot_display
{
    bool assigned;
    char usage[24];
    char asset_identity[96];
    char dimensions[32];
    char format[32];
    char mip_state[32];
    char residency[48];
    char state[32];
} sandbox3d_texture_slot_display;

henka_result sandbox3d_format_material_texture_slot(
    const henka_asset_manager* manager,
    const henka_material* material,
    henka_material_texture_slot slot,
    sandbox3d_texture_slot_display* out_display);

henka_result sandbox3d_assign_material_instance_texture(
    const henka_asset_manager* manager,
    henka_material_instance* instance,
    henka_material_texture_slot slot,
    henka_texture* texture);

henka_result sandbox3d_restore_material_instance_texture(
    const henka_asset_manager* manager,
    henka_material_instance* instance,
    henka_material_texture_slot slot);

#endif
