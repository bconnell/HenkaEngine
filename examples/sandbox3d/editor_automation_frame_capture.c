#include "editor_automation_frame_capture.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SANDBOX3D_AUTOMATION_CAPTURE_PATH_BYTES 1024U
#define SANDBOX3D_AUTOMATION_CAPTURE_LABEL_BYTES 48U
#define SANDBOX3D_AUTOMATION_CAPTURE_REQUEST_BYTES 64U
#define SANDBOX3D_AUTOMATION_CAPTURE_MAX_COUNT 128U
#define SANDBOX3D_AUTOMATION_CAPTURE_MAX_REQUEST_RETRY_FRAMES 60U

struct sandbox3d_automation_frame_capture
{
    bool enabled;
    bool request_stream_failed;
    uint32_t sequence;
    uint32_t request_retry_frames;
    char output_directory[SANDBOX3D_AUTOMATION_CAPTURE_PATH_BYTES];
    char request_file[SANDBOX3D_AUTOMATION_CAPTURE_PATH_BYTES];
};

static bool sandbox3d_automation_capture_path_is_absolute(const char* path)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }
#if defined(_WIN32)
    return (((path[0] >= 'A' && path[0] <= 'Z') ||
             (path[0] >= 'a' && path[0] <= 'z')) &&
            path[1] == ':' && (path[2] == '\\' || path[2] == '/')) ||
        (path[0] == '\\' && path[1] == '\\') ||
        (path[0] == '/' && path[1] == '/');
#else
    return path[0] == '/';
#endif
}

bool sandbox3d_automation_frame_capture_label_is_valid(const char* label)
{
    size_t length;
    size_t index;

    if (label == NULL)
    {
        return false;
    }
    length = strlen(label);
    if (length == 0U || length > SANDBOX3D_AUTOMATION_CAPTURE_LABEL_BYTES)
    {
        return false;
    }
    for (index = 0U; index < length; ++index)
    {
        const char character = label[index];
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '_' || character == '-'))
        {
            return false;
        }
    }
    return true;
}

bool sandbox3d_automation_frame_capture_format_path(
    char* out_path,
    size_t out_path_capacity,
    const char* output_directory,
    uint32_t sequence,
    const char* label)
{
    size_t directory_length;
    const char* separator;
    int written;

    if (out_path != NULL && out_path_capacity > 0U)
    {
        out_path[0] = '\0';
    }
    if (out_path == NULL || out_path_capacity == 0U ||
        !sandbox3d_automation_capture_path_is_absolute(output_directory) ||
        sequence == 0U || sequence > SANDBOX3D_AUTOMATION_CAPTURE_MAX_COUNT ||
        !sandbox3d_automation_frame_capture_label_is_valid(label))
    {
        return false;
    }

    directory_length = strlen(output_directory);
    if (directory_length >= SANDBOX3D_AUTOMATION_CAPTURE_PATH_BYTES)
    {
        return false;
    }
    separator = (output_directory[directory_length - 1U] == '/' ||
                 output_directory[directory_length - 1U] == '\\')
        ? ""
#if defined(_WIN32)
        : "\\";
#else
        : "/";
#endif
    written = snprintf(
        out_path,
        out_path_capacity,
        "%s%sframe-%04u-%s.bmp",
        output_directory,
        separator,
        sequence,
        label);
    if (written < 0 || (size_t)written >= out_path_capacity)
    {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

static bool sandbox3d_automation_capture_copy_environment_path(
    const char* variable,
    char* destination,
    size_t destination_capacity,
    bool* out_present)
{
    const char* value;
    char* owned_value = NULL;
    size_t length;

    if (out_present != NULL)
    {
        *out_present = false;
    }
    if (variable == NULL || destination == NULL ||
        destination_capacity == 0U || out_present == NULL)
    {
        return false;
    }
    destination[0] = '\0';
#if defined(_WIN32)
    {
        size_t value_size = 0U;
        if (_dupenv_s(&owned_value, &value_size, variable) != 0)
        {
            return false;
        }
        value = owned_value;
    }
#else
    value = getenv(variable);
#endif
    if (value == NULL || value[0] == '\0')
    {
        free(owned_value);
        return true;
    }
    *out_present = true;
    length = strlen(value);
    if (length >= destination_capacity ||
        !sandbox3d_automation_capture_path_is_absolute(value))
    {
        free(owned_value);
        return false;
    }
    memcpy(destination, value, length + 1U);
    free(owned_value);
    return true;
}

henka_result sandbox3d_automation_frame_capture_create(
    sandbox3d_automation_frame_capture** out_capture)
{
    sandbox3d_automation_frame_capture* capture;
    bool output_directory_present = false;
    bool request_file_present = false;

    if (out_capture != NULL)
    {
        *out_capture = NULL;
    }
    if (out_capture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    capture = (sandbox3d_automation_frame_capture*)calloc(1U, sizeof(*capture));
    if (capture == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (!sandbox3d_automation_capture_copy_environment_path(
            "HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY",
            capture->output_directory,
            sizeof(capture->output_directory),
            &output_directory_present) ||
        !sandbox3d_automation_capture_copy_environment_path(
            "HENKA_AUTOMATION_CAPTURE_REQUEST_FILE",
            capture->request_file,
            sizeof(capture->request_file),
            &request_file_present))
    {
        fprintf(
            stderr,
            "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=configuration_failed reason=invalid_absolute_or_oversized_path\n");
        free(capture);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (output_directory_present != request_file_present)
    {
        fprintf(
            stderr,
            "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=configuration_failed reason=output_and_request_paths_must_be_configured_together\n");
        free(capture);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    capture->enabled = output_directory_present;
    *out_capture = capture;
    return HENKA_SUCCESS;
}

void sandbox3d_automation_frame_capture_destroy(
    sandbox3d_automation_frame_capture* capture)
{
    free(capture);
}

static bool sandbox3d_automation_capture_read_request(
    const char* request_file,
    char* out_label,
    size_t out_label_capacity,
    bool* out_exists,
    bool* out_retryable,
    const char** out_failure_reason,
    int* out_error_code)
{
    FILE* file;
    size_t bytes_read;
    size_t length;
    bool read_failed;
    int open_error = 0;
    int error_code = 0;
    char request[SANDBOX3D_AUTOMATION_CAPTURE_REQUEST_BYTES];

    if (out_label != NULL && out_label_capacity > 0U)
    {
        out_label[0] = '\0';
    }
    if (request_file == NULL || out_label == NULL || out_label_capacity == 0U ||
        out_exists == NULL || out_retryable == NULL ||
        out_failure_reason == NULL || out_error_code == NULL)
    {
        return false;
    }
    *out_exists = false;
    *out_retryable = false;
    *out_failure_reason = "invalid_request";
    *out_error_code = 0;
    errno = 0;
#if defined(_WIN32)
    open_error = (int)fopen_s(&file, request_file, "rb");
    if (open_error != 0)
    {
        file = NULL;
    }
#else
    file = fopen(request_file, "rb");
#endif
    if (file == NULL)
    {
        error_code = open_error != 0 ? open_error : errno;
        if (error_code == ENOENT)
        {
            return true;
        }
        *out_exists = true;
        *out_retryable = true;
        *out_failure_reason = "open_failed";
        *out_error_code = error_code;
        return false;
    }
    *out_exists = true;
    bytes_read = fread(request, 1U, sizeof(request), file);
    read_failed = ferror(file) != 0;
    if (read_failed)
    {
        error_code = errno;
    }
    if (fclose(file) != 0)
    {
        read_failed = true;
        error_code = errno;
    }
    if (read_failed)
    {
        *out_retryable = true;
        *out_failure_reason = "read_failed";
        *out_error_code = error_code;
        return false;
    }
    if (bytes_read >= sizeof(request))
    {
        *out_failure_reason = "request_too_large";
        (void)remove(request_file);
        return false;
    }
    if (memchr(request, '\0', bytes_read) != NULL)
    {
        *out_failure_reason = "request_contains_nul";
        (void)remove(request_file);
        return false;
    }
    length = bytes_read;
    while (length > 0U &&
           (request[length - 1U] == '\n' || request[length - 1U] == '\r'))
    {
        --length;
    }
    if (length == 0U || length >= out_label_capacity)
    {
        *out_failure_reason = "invalid_label_length";
        (void)remove(request_file);
        return false;
    }
    request[length] = '\0';
    if (!sandbox3d_automation_frame_capture_label_is_valid(request))
    {
        *out_failure_reason = "invalid_label";
        (void)remove(request_file);
        return false;
    }
    if (remove(request_file) != 0)
    {
        *out_retryable = true;
        *out_failure_reason = "request_remove_failed";
        *out_error_code = errno;
        return false;
    }
    memcpy(out_label, request, length + 1U);
    return true;
}

void sandbox3d_automation_frame_capture_update(
    sandbox3d_automation_frame_capture* capture,
    henka_engine* engine)
{
    char label[SANDBOX3D_AUTOMATION_CAPTURE_LABEL_BYTES + 1U];
    char output_path[SANDBOX3D_AUTOMATION_CAPTURE_PATH_BYTES];
    const char* request_failure_reason = "invalid_request";
    bool request_exists = false;
    bool request_retryable = false;
    uint32_t next_sequence;
    int request_error_code = 0;
    henka_result result;

    if (capture == NULL || !capture->enabled || capture->request_stream_failed)
    {
        return;
    }
    if (!sandbox3d_automation_capture_read_request(
            capture->request_file,
            label,
            sizeof(label),
            &request_exists,
            &request_retryable,
            &request_failure_reason,
            &request_error_code))
    {
        if (request_exists)
        {
            if (request_retryable &&
                capture->request_retry_frames <
                    SANDBOX3D_AUTOMATION_CAPTURE_MAX_REQUEST_RETRY_FRAMES)
            {
                ++capture->request_retry_frames;
                if (capture->request_retry_frames == 1U)
                {
                    fprintf(
                        stderr,
                        "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=request_retry reason=%s crt_error=%d attempt=%u\n",
                        request_failure_reason,
                        request_error_code,
                        capture->request_retry_frames);
                }
                return;
            }
            capture->request_stream_failed = true;
            fprintf(
                stderr,
                "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=request_failed reason=%s crt_error=%d retries=%u\n",
                request_failure_reason,
                request_error_code,
                capture->request_retry_frames);
        }
        return;
    }
    if (!request_exists)
    {
        return;
    }
    capture->request_retry_frames = 0U;
    if (engine == NULL ||
        capture->sequence >= SANDBOX3D_AUTOMATION_CAPTURE_MAX_COUNT)
    {
        fprintf(
            stderr,
            "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=request_failed label=%s reason=%s\n",
            label,
            engine == NULL ? "engine_unavailable" : "capture_limit_reached");
        return;
    }

    next_sequence = capture->sequence + 1U;
    if (!sandbox3d_automation_frame_capture_format_path(
            output_path,
            sizeof(output_path),
            capture->output_directory,
            next_sequence,
            label))
    {
        fprintf(
            stderr,
            "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=request_failed label=%s reason=output_path_limit\n",
            label);
        return;
    }
    result = henka_engine_request_frame_capture(engine, output_path);
    if (result != HENKA_SUCCESS)
    {
        fprintf(
            stderr,
            "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=request_failed label=%s result=%s\n",
            label,
            henka_result_to_string(result));
        return;
    }
    capture->sequence = next_sequence;
    fprintf(
        stdout,
        "HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=requested sequence=%u label=%s\n",
        capture->sequence,
        label);
    fflush(stdout);
}
