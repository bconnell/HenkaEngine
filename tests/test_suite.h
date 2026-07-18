#ifndef HENKA_TEST_SUITE_H
#define HENKA_TEST_SUITE_H

#include <stdio.h>

#include <math.h>

extern int g_henka_test_failures;

#define HENKA_TEST_ASSERT(condition)                                                                    \
    do                                                                                                  \
    {                                                                                                   \
        if (!(condition))                                                                               \
        {                                                                                               \
            fprintf(stderr, "assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);       \
            ++g_henka_test_failures;                                                                    \
            return;                                                                                     \
        }                                                                                               \
    } while (0)

#define HENKA_TEST_ASSERT_FLOAT_CLOSE(actual, expected, epsilon)                                        \
    do                                                                                                  \
    {                                                                                                   \
        const double _henka_actual_value = (double)(actual);                                            \
        const double _henka_expected_value = (double)(expected);                                        \
        const double _henka_epsilon_value = (double)(epsilon);                                          \
        if (!isfinite(_henka_actual_value) || !isfinite(_henka_expected_value) ||                       \
            !isfinite(_henka_epsilon_value) || _henka_epsilon_value < 0.0 ||                            \
            fabs(_henka_actual_value - _henka_expected_value) > _henka_epsilon_value)                   \
        {                                                                                               \
            fprintf(stderr, "float assertion failed at %s:%d: %s ~= %s\n", __FILE__, __LINE__, #actual, #expected); \
            ++g_henka_test_failures;                                                                    \
            return;                                                                                     \
        }                                                                                               \
    } while (0)

void henka_test_result(void);
void henka_test_memory(void);
void henka_test_assets(void);
void henka_test_action(void);
void henka_test_gizmo(void);
void henka_test_input(void);
void henka_test_math(void);
void henka_test_camera(void);
void henka_test_editor_controls(void);
void henka_test_material(void);
void henka_test_model(void);
void henka_test_persistence(void);
void henka_test_physics(void);
void henka_test_sandbox3d_interaction(void);
void henka_test_sandbox3d_workspace(void);
void henka_test_scene(void);
void henka_test_ui(void);
void henka_test_workspace(void);

#endif
