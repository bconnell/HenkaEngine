#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script_state.h>

static const henka_script_state_identity identity_a = {11U, 101U};
static const henka_script_state_identity identity_b = {12U, 102U};

static void test_typed_values_and_capacity(void)
{
    henka_script_state_store* store = NULL;
    henka_script_state_value value;
    bool present = false;
    size_t index;

    assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               store,
               identity_a,
               1U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32,
                   {.i32 = -7}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               store,
               identity_a,
               2U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_BOOL,
                   {.boolean = true}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               store,
               identity_b,
               3U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_FLOAT32,
                   {.f32 = 2.5F}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               store,
               identity_b,
               4U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_VEC3,
                   {.vec3 = {1.0F, 2.0F, 3.0F}}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_get_count(store) == 4U);
    assert(henka_script_state_store_get(store, identity_a, 1U, &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == -7);
    assert(henka_script_state_store_get(store, identity_a, 99U, &value, &present) == HENKA_SUCCESS);
    assert(!present);
    assert(henka_script_state_store_set(
               store,
               identity_a,
               1U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32,
                   {.i32 = 8}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_get_count(store) == 4U);
    assert(henka_script_state_store_remove(store, identity_b, 3U) == HENKA_SUCCESS);
    assert(henka_script_state_store_get_count(store) == 3U);

    henka_script_state_store_clear(store);
    for (index = 0U; index < HENKA_SCRIPT_STATE_MAX_VALUES; ++index)
    {
        assert(henka_script_state_store_set(
                   store,
                   (henka_script_state_identity){index + 1U, 1U},
                   (uint32_t)(index + 1U),
                   (henka_script_state_value){
                       HENKA_SCRIPT_STATE_VALUE_I32,
                       {.i32 = (int32_t)index}}) == HENKA_SUCCESS);
    }
    assert(henka_script_state_store_set(
               store,
               (henka_script_state_identity){UINT64_C(9000), 1U},
               9000U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32,
                   {.i32 = 1}}) == HENKA_ERROR_LIMIT);
    henka_script_state_store_destroy(store);
}

static void test_transactional_file_round_trip(void)
{
    const char* root = "build/test_tmp/script_state";
    const char* relative_path = "roundtrip.hstate";
    henka_script_state_store* source = NULL;
    henka_script_state_store* loaded = NULL;
    henka_script_state_value value;
    bool present = false;

    assert(henka_script_state_store_create(&source) == HENKA_SUCCESS);
    assert(henka_script_state_store_create(&loaded) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               source,
               identity_a,
               7U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_VEC3,
                   {.vec3 = {4.0F, 5.0F, 6.0F}}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               source,
               identity_b,
               8U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_BOOL,
                   {.boolean = true}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_save_file(source, root, relative_path) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               loaded,
               (henka_script_state_identity){99U, 99U},
               99U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32,
                   {.i32 = 123}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_load_file(loaded, root, relative_path) == HENKA_SUCCESS);
    assert(henka_script_state_store_get_count(loaded) == 2U);
    assert(henka_script_state_store_get(loaded, identity_a, 7U, &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_VEC3);
    assert(value.as.vec3.x == 4.0F && value.as.vec3.y == 5.0F && value.as.vec3.z == 6.0F);
    assert(henka_script_state_store_get(loaded, identity_b, 8U, &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_BOOL && value.as.boolean);
    henka_script_state_store_destroy(loaded);
    henka_script_state_store_destroy(source);
    (void)remove("build/test_tmp/script_state/roundtrip.hstate");
}

static void test_invalid_values_and_load_retention(void)
{
    henka_script_state_store* store = NULL;
    henka_script_state_value value;
    bool present = false;
    FILE* file;

    assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               store,
               identity_a,
               1U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32,
                   {.i32 = 55}}) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               store,
               identity_a,
               2U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_FLOAT32,
                   {.f32 = NAN}}) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_script_state_store_set(
               store,
               (henka_script_state_identity){0U, 1U},
               3U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32,
                   {.i32 = 1}}) == HENKA_ERROR_INVALID_ARGUMENT);

#if defined(_MSC_VER)
    assert(fopen_s(
               &file,
               "build/test_tmp/script_state/malformed.hstate",
               "wb") == 0);
#else
    file = fopen("build/test_tmp/script_state/malformed.hstate", "wb");
#endif
    assert(file != NULL);
    (void)fwrite("HKS", 1U, 3U, file);
    assert(fclose(file) == 0);
    assert(henka_script_state_store_load_file(
               store,
               "build/test_tmp/script_state",
               "malformed.hstate") != HENKA_SUCCESS);
    assert(henka_script_state_store_get(
               store, identity_a, 1U, &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 55);
    (void)remove("build/test_tmp/script_state/malformed.hstate");
    henka_script_state_store_destroy(store);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_typed_values_and_capacity();
    test_transactional_file_round_trip();
    test_invalid_values_and_load_retention();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_script_state_tests: PASS");
    return 0;
}
