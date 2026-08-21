#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <henka/scene_document.h>

static bool test_scene_document_files_equal(const char* left_path, const char* right_path)
{
    FILE* left = fopen(left_path, "rb");
    FILE* right = fopen(right_path, "rb");
    unsigned char left_buffer[256];
    unsigned char right_buffer[256];
    size_t left_size;
    size_t right_size;
    bool equal = true;

    if (left == NULL || right == NULL)
    {
        if (left != NULL) fclose(left);
        if (right != NULL) fclose(right);
        return false;
    }
    do
    {
        left_size = fread(left_buffer, 1U, sizeof(left_buffer), left);
        right_size = fread(right_buffer, 1U, sizeof(right_buffer), right);
        if (left_size != right_size || memcmp(left_buffer, right_buffer, left_size) != 0)
        {
            equal = false;
            break;
        }
    } while (left_size != 0U);
    if (fgetc(left) != EOF || fgetc(right) != EOF)
    {
        equal = false;
    }
    fclose(left);
    fclose(right);
    return equal;
}

static bool test_scene_document_write_bytes(const char* path, const void* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    bool result;
    if (file == NULL)
    {
        return false;
    }
    result = fwrite(data, 1U, size, file) == size;
    if (fclose(file) != 0)
    {
        result = false;
    }
    return result;
}

static bool test_scene_document_patch_u32(const char* path, long offset, uint32_t value)
{
    unsigned char bytes[4];
    FILE* file = fopen(path, "r+b");
    bool result;
    if (file == NULL)
    {
        return false;
    }
    bytes[0] = (unsigned char)(value & UINT32_C(0xFF));
    bytes[1] = (unsigned char)((value >> 8U) & UINT32_C(0xFF));
    bytes[2] = (unsigned char)((value >> 16U) & UINT32_C(0xFF));
    bytes[3] = (unsigned char)((value >> 24U) & UINT32_C(0xFF));
    result = fseek(file, offset, SEEK_SET) == 0 &&
        fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
    if (fclose(file) != 0)
    {
        result = false;
    }
    return result;
}

int main(void)
{
    const char* first_path = "build/test_tmp/scene_document_slice_b.hscene";
    const char* second_path = "build/test_tmp/scene_document_slice_b_copy.hscene";
    const char* malformed_path = "build/test_tmp/scene_document_malformed.hscene";
    const unsigned char malformed_data[] = {'H', 'S', 'C', 'N', 1U};
    henka_scene_document* document = NULL;
    henka_scene_document* loaded = NULL;
    henka_scene_document* exhausted = NULL;
    henka_scene_document_object object;
    henka_scene_document_object loaded_object;
    henka_scene_document_object maximum_id_object;
    henka_scene_document_object recycled_id_object;
    henka_scene_document_id first_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id added_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id duplicate_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    char inspection[HENKA_SCENE_DOCUMENT_MAX_INSPECTION_BYTES];
    size_t index;
    size_t inspection_size = 0U;
    int result = 1;

    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_document_create(&loaded) != HENKA_SUCCESS ||
        henka_scene_document_create(&exhausted) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 96U; ++index)
    {
        int written;
        object = henka_scene_document_object_default();
        written = snprintf(object.name, sizeof(object.name), "object_%zu", index);
        if (written <= 0 || (size_t)written >= sizeof(object.name))
        {
            goto cleanup;
        }
        object.transform.position.x = (float)index;
        object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
        object.source.primitive = index % 2U == 0U
            ? HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX
            : HENKA_SCENE_DOCUMENT_PRIMITIVE_SPHERE;
        object.source.primitive_dimensions = (henka_vec3){1.0f, 2.0f, 3.0f};
        object.interaction.enabled = index % 3U == 0U;
        object.interaction.max_distance = 12.0f;
        (void)snprintf(object.interaction.prompt, sizeof(object.interaction.prompt), "Use object %zu", index);
        if (henka_scene_document_add_object(document, &object, &added_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (index == 0U)
        {
            const henka_scene_document_id original_id = added_id;
            first_id = added_id;
            if (henka_scene_document_duplicate_object(document, original_id, &duplicate_id) != HENKA_SUCCESS ||
                duplicate_id == original_id)
            {
                goto cleanup;
            }
        }
    }
    if (henka_scene_document_get_object_count(document) != 97U ||
        henka_scene_document_validate(document) != HENKA_SUCCESS ||
        henka_scene_document_save_file(document, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_save_file(document, ".", second_path) != HENKA_SUCCESS ||
        !test_scene_document_files_equal(first_path, second_path) ||
        henka_scene_document_format_inspection(
            document, inspection, sizeof(inspection), &inspection_size) != HENKA_SUCCESS ||
        inspection_size == 0U || strstr(inspection, "HSCN version=1 objects=97") == NULL)
    {
        fprintf(stderr, "scene document test failed during deterministic save/inspection\n");
        goto cleanup;
    }
    if (henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 97U ||
        henka_scene_document_get_object(loaded, first_id, &loaded_object) != HENKA_SUCCESS ||
        strcmp(loaded_object.name, "object_0") != 0 ||
        loaded_object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE ||
        henka_scene_document_get_object(loaded, duplicate_id, &loaded_object) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during round-trip load\n");
        goto cleanup;
    }
    if (henka_scene_document_save_file(document, ".", "../escape.hscene") != HENKA_ERROR_INVALID_ARGUMENT ||
        !test_scene_document_write_bytes(malformed_path, malformed_data, sizeof(malformed_data)) ||
        henka_scene_document_load_file(loaded, ".", malformed_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 97U)
    {
        fprintf(stderr, "scene document test failed during confinement/malformed retention\n");
        goto cleanup;
    }
    if (!test_scene_document_patch_u32(first_path, 36L, UINT32_C(1)) ||
        henka_scene_document_load_file(loaded, ".", first_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 97U ||
        !test_scene_document_patch_u32(second_path, 48L, UINT32_C(0x80000000)) ||
        henka_scene_document_load_file(loaded, ".", second_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 97U)
    {
        fprintf(stderr, "scene document test failed during corrupted-header retention\n");
        goto cleanup;
    }
    maximum_id_object = henka_scene_document_object_default();
    maximum_id_object.id = UINT64_MAX;
    recycled_id_object = henka_scene_document_object_default();
    recycled_id_object.id = UINT64_C(10000);
    if (henka_scene_document_add_object(exhausted, &maximum_id_object, &first_id) != HENKA_SUCCESS ||
        henka_scene_document_add_object(exhausted, &object, &duplicate_id) != HENKA_ERROR_LIMIT ||
        henka_scene_document_add_object(exhausted, &recycled_id_object, &duplicate_id) != HENKA_ERROR_LIMIT ||
        henka_scene_document_validate(exhausted) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during ID exhaustion checks\n");
        goto cleanup;
    }
    result = 0;

cleanup:
    henka_scene_document_destroy(exhausted);
    henka_scene_document_destroy(loaded);
    henka_scene_document_destroy(document);
    return result;
}
