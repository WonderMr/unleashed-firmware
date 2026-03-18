#include "../subghz_i.h"
#include <lib/subghz/subghz_setting.h>

#define TAG "SubGhzSceneDetectedFrequencies"
#define LABEL_SIZE 16

static const char* const on_off_text[] = {"OFF", "ON"};

static void subghz_scene_detected_frequencies_toggle_cb(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_text[index]);

    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    size_t count = subghz_setting_get_hopper_frequency_count(setting);

    // Find which frequency index this item corresponds to
    for(size_t i = 0; i < count; i++) {
        if(variable_item_list_get(subghz->variable_item_list, i) == item) {
            uint32_t frequency = subghz_setting_get_hopper_frequency(setting, i);
            subghz_setting_set_hopper_frequency_enabled(setting, frequency, index == 1);
            subghz_setting_save_user_hopper(
                setting, EXT_PATH("subghz/assets/setting_user"));
            break;
        }
    }
}

void subghz_scene_detected_frequencies_on_enter(void* context) {
    SubGhz* subghz = context;
    VariableItemList* list = subghz->variable_item_list;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);

    size_t count = subghz_setting_get_hopper_frequency_count(setting);

    // Allocate heap labels that outlive the loop (freed in on_exit)
    char** labels = NULL;
    if(count > 0) {
        labels = malloc(count * sizeof(char*));
        for(size_t i = 0; i < count; i++) {
            labels[i] = malloc(LABEL_SIZE);
        }
    }
    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneDetectedFrequencies, (uint32_t)(uintptr_t)labels);

    for(size_t i = 0; i < count; i++) {
        uint32_t freq = subghz_setting_get_hopper_frequency(setting, i);
        snprintf(
            labels[i],
            LABEL_SIZE,
            "%lu.%02lu MHz",
            freq / 1000000,
            (freq % 1000000) / 10000);

        VariableItem* item = variable_item_list_add(
            list, labels[i], 2, subghz_scene_detected_frequencies_toggle_cb, subghz);

        bool enabled = subghz_setting_is_hopper_frequency_enabled(setting, freq);
        variable_item_set_current_value_index(item, enabled ? 1 : 0);
        variable_item_set_current_value_text(item, on_off_text[enabled ? 1 : 0]);
    }

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
}

bool subghz_scene_detected_frequencies_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void subghz_scene_detected_frequencies_on_exit(void* context) {
    SubGhz* subghz = context;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    size_t count = subghz_setting_get_hopper_frequency_count(setting);

    // Free heap-allocated labels
    char** labels = (char**)(uintptr_t)scene_manager_get_scene_state(
        subghz->scene_manager, SubGhzSceneDetectedFrequencies);
    if(labels) {
        for(size_t i = 0; i < count; i++) {
            free(labels[i]);
        }
        free(labels);
        scene_manager_set_scene_state(
            subghz->scene_manager, SubGhzSceneDetectedFrequencies, 0);
    }

    variable_item_list_set_selected_item(subghz->variable_item_list, 0);
    variable_item_list_reset(subghz->variable_item_list);
}
