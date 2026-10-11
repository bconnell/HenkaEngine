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
    else()
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
        if(NOT candidate_invocation_count EQUAL 1)
            set(reason "Showcase determinism test must perform one fresh generation against the build-owned baseline")
        elseif(test_text MATCHES "determinism_second_run")
            set(reason "Showcase determinism test still creates a duplicate full-size output tree")
        elseif(NOT test_text MATCHES "BaselineDirectory" OR NOT test_text MATCHES "externalOutputProbe")
            set(reason "Showcase determinism test does not consume the build-owned baseline")
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

message(STATUS "Showcase build ownership passes and rejects unconditional regeneration")
