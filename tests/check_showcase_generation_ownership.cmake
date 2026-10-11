if(NOT DEFINED SANDBOX_CMAKE OR NOT EXISTS "${SANDBOX_CMAKE}")
    message(FATAL_ERROR "SANDBOX_CMAKE must identify the Sandbox3D CMake source.")
endif()
if(NOT DEFINED SHOWCASE_TEST_SCRIPT OR NOT EXISTS "${SHOWCASE_TEST_SCRIPT}")
    message(FATAL_ERROR "SHOWCASE_TEST_SCRIPT must identify the Showcase asset test.")
endif()

file(READ "${SANDBOX_CMAKE}" sandbox_cmake_text)
file(READ "${SHOWCASE_TEST_SCRIPT}" showcase_test_text)

function(validate_showcase_generation_ownership sandbox_text test_text out_ok out_reason)
    set(reason "")
    string(CONCAT outputs_reference "$" "{HENKA_SHOWCASE_ASSET_OUTPUTS}")

    if(sandbox_text MATCHES "add_custom_command\\(TARGET henka_sandbox3d POST_BUILD[^)]*generate_showcase_assets\\.ps1[^)]*\\)")
        set(reason "Showcase assets are still generated as an unconditional Sandbox POST_BUILD action")
    elseif(NOT sandbox_text MATCHES "add_custom_command\\(OUTPUT \\$\\{HENKA_SHOWCASE_ASSET_OUTPUTS\\}")
        set(reason "Showcase generation is not modeled as an output-driven custom command")
    elseif(NOT sandbox_text MATCHES "DEPENDS[^)]*generate_showcase_assets\\.ps1[^)]*henka_script_common\\.ps1")
        set(reason "Showcase generation does not depend on both generator and canonical path helper inputs")
    elseif(NOT sandbox_text MATCHES "add_custom_target\\(henka_showcase_asset_generation DEPENDS \\$\\{HENKA_SHOWCASE_ASSET_OUTPUTS\\}")
        set(reason "Showcase generated outputs have no dedicated build target")
    elseif(NOT sandbox_text MATCHES "add_dependencies\\(henka_sandbox3d henka_showcase_asset_generation")
        set(reason "Sandbox3D does not depend on its generated Showcase assets")
    elseif(NOT sandbox_text MATCHES "HENKA_SHOWCASE_ASSET_MANIFEST" OR
        NOT sandbox_text MATCHES "-ReuseVerifiedOutput" OR
        NOT sandbox_text MATCHES "-ManifestPath")
        set(reason "Multi-configuration Showcase builds lack verified output-reuse ownership")
    else()
        string(REGEX MATCH
            "set\\(HENKA_SHOWCASE_ASSET_OUTPUT_DIRECTORY[^)]*\\)"
            output_directory_declaration
            "${sandbox_text}")
        if(output_directory_declaration STREQUAL "")
            set(reason "Showcase generation has no explicit output-directory authority")
        elseif(output_directory_declaration MATCHES "\\$<CONFIG>")
            set(reason "Configuration-independent Showcase assets are redundantly generated once per build configuration")
        elseif(NOT output_directory_declaration MATCHES "showcase_assets/assets/models")
            set(reason "Showcase assets are not generated beneath the shared build-owned showcase_assets directory")
        endif()

        string(FIND "${sandbox_text}" "add_custom_command(TARGET henka_sandbox3d POST_BUILD" first_postbuild_index)
        if(reason STREQUAL "" AND first_postbuild_index GREATER_EQUAL 0)
            string(SUBSTRING "${sandbox_text}" ${first_postbuild_index} -1 postbuild_commands)
            string(FIND "${postbuild_commands}" "\${HENKA_SHOWCASE_ASSET_OUTPUT_DIRECTORY}" shared_output_copy_index)
            string(FIND "${postbuild_commands}" "$<TARGET_FILE_DIR:henka_sandbox3d>/assets/models" target_models_directory_index)
            if(shared_output_copy_index LESS 0 OR target_models_directory_index LESS shared_output_copy_index)
                set(reason "Shared Showcase outputs are not copied into the active Sandbox configuration asset directory")
            endif()
        endif()

        foreach(expected_output IN ITEMS
                "cheeky_giraffe.gltf"
                "cheeky_giraffe.bin"
                "giraffe_base_color.png"
                "giraffe_detail_normal.png"
                "giraffe_metallic_roughness.png"
                "original_realistic_rocket.gltf"
                "original_realistic_rocket.bin"
                "rocket_base_color.png"
                "rocket_detail_normal.png"
                "rocket_metallic_roughness.png")
            if(NOT sandbox_text MATCHES "${expected_output}")
                set(reason "Showcase build outputs omit ${expected_output}")
                break()
            endif()
        endforeach()
    endif()

    if(reason STREQUAL "")
        string(REGEX MATCHALL "&[ \t]*\\$generator[^\r\n]*\\$candidateDirectory" candidate_invocations "${test_text}")
        list(LENGTH candidate_invocations candidate_invocation_count)
        if(NOT candidate_invocation_count EQUAL 2)
            set(reason "Showcase test must perform one fresh generation and one verified cache-hit probe")
        elseif(test_text MATCHES "determinism_second_run")
            set(reason "Showcase determinism test still creates a duplicate full-size output tree")
        elseif(NOT test_text MATCHES "BaselineDirectory" OR NOT test_text MATCHES "externalOutputProbe")
            set(reason "Showcase determinism test does not consume the build-owned baseline")
        elseif(test_text MATCHES "\\$imagePath = Join-Path \\$OutputDirectory" OR
            NOT test_text MATCHES "\\$imagePath = Join-Path \\$candidateDirectory")
            set(reason "Showcase candidate texture validation does not resolve dependencies from the freshly generated candidate directory")
        elseif(test_text MATCHES "Join-Path \\$OutputDirectory \"(cheeky_giraffe|original_realistic_rocket|giraffe_base_color|rocket_base_color)")
            set(reason "Showcase semantic checks read generated assets from the parent directory instead of the fresh candidate")
        elseif(NOT test_text MATCHES "LastWriteUtcTicks" OR
            NOT test_text MATCHES "\\[skip\\] Verified Showcase inputs and all generated output hashes are unchanged")
            set(reason "Showcase determinism test does not prove an unchanged cache hit avoids rewriting outputs")
        elseif(NOT test_text MATCHES "\\[generated\\] Wrote verified hashes for 10 Showcase assets")
            set(reason "Showcase determinism test does not require one fresh generation before the cache-hit probe")
        elseif(NOT test_text MATCHES "Get-ShowcaseFileHash")
            set(reason "Showcase determinism test no longer verifies generated output hashes")
        endif()
    endif()

    if(reason STREQUAL "")
        set(${out_ok} TRUE PARENT_SCOPE)
        set(${out_reason} "" PARENT_SCOPE)
    else()
        set(${out_ok} FALSE PARENT_SCOPE)
        set(${out_reason} "${reason}" PARENT_SCOPE)
    endif()
endfunction()

validate_showcase_generation_ownership(
    "${sandbox_cmake_text}"
    "${showcase_test_text}"
    production_ok
    production_reason)
if(NOT production_ok)
    message(FATAL_ERROR "${production_reason}")
endif()

string(CONCAT config_specific_path "$" "<CONFIG>/assets/models")
string(REPLACE
    "showcase_assets/assets/models"
    "${config_specific_path}"
    config_specific_sandbox_cmake_text
    "${sandbox_cmake_text}")
validate_showcase_generation_ownership(
    "${config_specific_sandbox_cmake_text}"
    "${showcase_test_text}"
    config_negative_control_ok
    config_negative_control_reason)
if(config_negative_control_ok OR
    NOT config_negative_control_reason MATCHES "once per build configuration")
    message(FATAL_ERROR "Negative control failed to reject per-configuration Showcase generation.")
endif()

string(REPLACE
    "-ReuseVerifiedOutput"
    ""
    unverified_reuse_sandbox_cmake_text
    "${sandbox_cmake_text}")
validate_showcase_generation_ownership(
    "${unverified_reuse_sandbox_cmake_text}"
    "${showcase_test_text}"
    unverified_reuse_negative_control_ok
    unverified_reuse_negative_control_reason)
if(unverified_reuse_negative_control_ok OR
    NOT unverified_reuse_negative_control_reason MATCHES "verified output-reuse ownership")
    message(FATAL_ERROR "Negative control failed to reject cross-configuration Showcase generation without verified output reuse.")
endif()

string(REPLACE
    "$imagePath = Join-Path $candidateDirectory"
    "$imagePath = Join-Path $OutputDirectory"
    wrong_candidate_asset_root_test_text
    "${showcase_test_text}")
validate_showcase_generation_ownership(
    "${sandbox_cmake_text}"
    "${wrong_candidate_asset_root_test_text}"
    candidate_root_negative_control_ok
    candidate_root_negative_control_reason)
if(candidate_root_negative_control_ok OR
    NOT candidate_root_negative_control_reason MATCHES "freshly generated candidate directory")
    message(FATAL_ERROR "Negative control failed to reject parent-directory candidate texture validation.")
endif()

string(REPLACE
        "add_custom_command(OUTPUT ${outputs_reference}"
    "add_custom_command(TARGET henka_sandbox3d POST_BUILD"
    broken_sandbox_cmake_text
    "${sandbox_cmake_text}")
validate_showcase_generation_ownership(
    "${broken_sandbox_cmake_text}"
    "${showcase_test_text}"
    negative_control_ok
    negative_control_reason)
if(negative_control_ok OR negative_control_reason STREQUAL "")
    message(FATAL_ERROR "Negative control failed to reject unconditional Showcase regeneration.")
endif()

message(STATUS "Showcase generation ownership passes shared-output/cache checks and rejects duplicated or unverified generation")
