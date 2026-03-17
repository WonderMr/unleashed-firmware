#include "../subghz_i.h"
#include "../helpers/subghz_saved_dump_index.h"

#define TAG "SubGhzSceneSavedDumpSelect"

// Max matches defined in subghz_saved_dump_index.h as SUBGHZ_SAVED_DUMP_MAX_SELECTABLE

static void subghz_scene_saved_dump_select_submenu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

void subghz_scene_saved_dump_select_on_enter(void* context) {
    SubGhz* subghz = context;

    uint32_t hash = subghz_history_get_saved_hash(subghz->history, subghz->idx_menu_chosen);

    SubGhzSavedDumpEntry* matches[SUBGHZ_SAVED_DUMP_MAX_SELECTABLE];
    uint16_t match_count = subghz_saved_dump_index_get_matches(
        subghz->saved_dump_index, hash, matches, SUBGHZ_SAVED_DUMP_MAX_SELECTABLE);

    if(match_count == 0) {
        // Index was rebuilt/cleared, no matches — fall back to receiver info
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverInfo);
        return;
    }

    for(uint16_t i = 0; i < match_count; i++) {
        submenu_add_item(
            subghz->submenu,
            furi_string_get_cstr(matches[i]->filename),
            i,
            subghz_scene_saved_dump_select_submenu_callback,
            subghz);
    }

    // Store hash in scene state for re-lookup in on_event
    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneSavedDumpSelect, hash);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
}

bool subghz_scene_saved_dump_select_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t hash = scene_manager_get_scene_state(
            subghz->scene_manager, SubGhzSceneSavedDumpSelect);

        SubGhzSavedDumpEntry* matches[SUBGHZ_SAVED_DUMP_MAX_SELECTABLE];
        uint16_t match_count = subghz_saved_dump_index_get_matches(
            subghz->saved_dump_index, hash, matches, SUBGHZ_SAVED_DUMP_MAX_SELECTABLE);

        uint32_t selected = event.event;
        if(selected < match_count) {
            const char* path = furi_string_get_cstr(matches[selected]->filepath);
            furi_string_set(subghz->file_path, path);
            if(subghz_key_load(subghz, path, true)) {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateRAWLoad);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSavedMenu);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    subghz->scene_manager, SubGhzSceneReceiver);
            }
            return true;
        }
    }
    return false;
}

void subghz_scene_saved_dump_select_on_exit(void* context) {
    SubGhz* subghz = context;
    submenu_reset(subghz->submenu);
}
