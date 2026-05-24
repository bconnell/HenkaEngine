#ifndef HENKA_ASSETS_H
#define HENKA_ASSETS_H

#include <henka/result.h>

typedef struct henka_asset_manager henka_asset_manager;
typedef struct henka_engine henka_engine;
typedef struct henka_shader henka_shader;
typedef struct henka_texture henka_texture;

henka_asset_manager* henka_engine_get_asset_manager(henka_engine* engine);
const henka_asset_manager* henka_engine_get_asset_manager_const(const henka_engine* engine);
henka_result henka_assets_load_shader(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    henka_shader** out_shader);
henka_result henka_assets_load_texture(henka_asset_manager* manager, const char* path, henka_texture** out_texture);
henka_texture* henka_assets_get_white_texture(henka_asset_manager* manager);
henka_texture* henka_assets_get_error_texture(henka_asset_manager* manager);

#endif
