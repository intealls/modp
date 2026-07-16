// Copyright intealls
// License: GPL v3

#ifndef GLUI_UIOPTIONS_H
#define GLUI_UIOPTIONS_H

#include <stddef.h>
#include <stdbool.h>

/* Forward declaration to avoid circular dependency */
typedef struct UiCfg UiCfg;

/**
 * UIOption - Single source of truth for UI visual options
 * 
 * This struct serves both:
 * - CLI/config file parsing (via config_name)
 * - Runtime Options Editor UI (via display_name, description)
 * 
 * Defined once in GLWindow.c, referenced from Main.c for Option array.
 */
typedef struct UIOption {
    const char* config_name;    /* e.g., "bg_red" - for config file / CLI */
    const char* display_name;   /* e.g., "BG Color R" - for editor UI */
    const char* description;    /* e.g., "Background red (0.0-1.0)" */
    float min_val;
    float max_val;
    float* value_ptr;           /* Runtime: points to actual value in UiCfg */
    bool editable;              /* Can be changed in runtime editor? */
} UIOption;

/* Global UI options array - defined in GLWindow.c */
extern UIOption ui_options[];
extern const size_t NUM_UI_OPTIONS;

/* Initialize value pointers - call from GLWindow_Init after opts is available */
void GLUI_InitUIOptions(UiCfg* ui);

/* Get option index by config name (returns -1 if not found) */
int GLUI_GetUIOptionIndex(const char* config_name);

#endif /* GLUI_UIOPTIONS_H */
