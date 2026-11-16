#include "test_suite.h"

#include "../examples/sandbox3d/editor_automation_frame_capture.h"

#include <string.h>

void henka_test_sandbox3d_automation_frame_capture(void)
{
    char path[160];
    char short_path[8];
#if defined(_WIN32)
    const char* directory = "C:\\Henka\\capture";
    const char* expected = "C:\\Henka\\capture\\frame-0001-editor-ui.bmp";
    const char* relative_directory = "Henka\\capture";
#else
    const char* directory = "/tmp/henka/capture";
    const char* expected = "/tmp/henka/capture/frame-0001-editor-ui.bmp";
    const char* relative_directory = "Henka/capture";
#endif
    char oversized_label[50];

    HENKA_TEST_ASSERT(sandbox3d_automation_frame_capture_label_is_valid("editor-ui"));
    HENKA_TEST_ASSERT(sandbox3d_automation_frame_capture_label_is_valid("scale_200"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid(""));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid("two words"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid("../capture"));
    memset(oversized_label, 'a', sizeof(oversized_label));
    oversized_label[sizeof(oversized_label) - 1U] = '\0';
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid(oversized_label));

    HENKA_TEST_ASSERT(sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), directory, 1U, "editor-ui"));
    HENKA_TEST_ASSERT(strcmp(path, expected) == 0);
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), relative_directory, 1U, "editor-ui"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), directory, 0U, "editor-ui"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), directory, 129U, "editor-ui"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        short_path, sizeof(short_path), directory, 1U, "editor-ui"));
    HENKA_TEST_ASSERT(short_path[0] == '\0');
}
