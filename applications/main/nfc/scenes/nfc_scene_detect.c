#include "../nfc_app_i.h"
#include <dolphin/dolphin.h>

#define TAG "NfcDetect"

void nfc_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);

    NfcApp* instance = context;

    if(event.type == NfcScannerEventTypeDetected) {
        nfc_detected_protocols_set(
            instance->detected_protocols, event.data.protocols, event.data.protocol_num);
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventWorkerExit);
    }
}

void nfc_scene_detect_on_enter(void* context) {
    NfcApp* instance = context;

    nfc_show_loading_popup(instance, true);

    uint32_t t0 = furi_get_tick();
    nfc_supported_cards_load_cache(instance->nfc_supported_cards);
    uint32_t t1 = furi_get_tick();
    FURI_LOG_D(TAG, "load_cache: %lu ms", t1 - t0);

    // Pre-copy MF Classic dictionaries for potential dict attack (heavy SD I/O)
    instance->nfc_dict_context.nested_dicts_copied = false;

    uint32_t t2 = furi_get_tick();
    if(keys_dict_check_presence(NFC_APP_MF_CLASSIC_DICT_SYSTEM_NESTED_PATH)) {
        storage_common_remove(instance->storage, NFC_APP_MF_CLASSIC_DICT_SYSTEM_NESTED_PATH);
    }
    if(keys_dict_check_presence(NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH)) {
        storage_common_copy(
            instance->storage,
            NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH,
            NFC_APP_MF_CLASSIC_DICT_SYSTEM_NESTED_PATH);
    }
    uint32_t t3 = furi_get_tick();
    FURI_LOG_D(TAG, "copy system dict: %lu ms", t3 - t2);

    if(keys_dict_check_presence(NFC_APP_MF_CLASSIC_DICT_USER_NESTED_PATH)) {
        storage_common_remove(instance->storage, NFC_APP_MF_CLASSIC_DICT_USER_NESTED_PATH);
    }
    if(keys_dict_check_presence(NFC_APP_MF_CLASSIC_DICT_USER_PATH)) {
        storage_common_copy(
            instance->storage,
            NFC_APP_MF_CLASSIC_DICT_USER_PATH,
            NFC_APP_MF_CLASSIC_DICT_USER_NESTED_PATH);
    }
    uint32_t t4 = furi_get_tick();
    FURI_LOG_D(TAG, "copy user dict: %lu ms", t4 - t3);

    instance->nfc_dict_context.nested_dicts_copied = true;

    instance->nfc_dict_context.pre_user_dict = NULL;
    instance->nfc_dict_context.pre_system_dict = NULL;

    FURI_LOG_D(TAG, "detect on_enter total loading: %lu ms", t4 - t0);
    nfc_show_loading_popup(instance, false);

    // Setup view
    popup_reset(instance->popup);
    popup_set_header(instance->popup, "Reading", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        instance->popup, "Hold card next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcViewPopup);

    nfc_detected_protocols_reset(instance->detected_protocols);

    instance->scanner = nfc_scanner_alloc(instance->nfc);
    nfc_scanner_start(instance->scanner, nfc_scene_detect_scan_callback, instance);

    nfc_blink_detect_start(instance);
}

bool nfc_scene_detect_on_event(void* context, SceneManagerEvent event) {
    NfcApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventWorkerExit) {
            if(nfc_detected_protocols_get_num(instance->detected_protocols) > 1) {
                notification_message(instance->notifications, &sequence_single_vibro);
                scene_manager_next_scene(instance->scene_manager, NfcSceneSelectProtocol);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcSceneRead);
            }
            consumed = true;
        }
    }

    return consumed;
}

void nfc_scene_detect_on_exit(void* context) {
    NfcApp* instance = context;

    nfc_scanner_stop(instance->scanner);
    nfc_scanner_free(instance->scanner);
    popup_reset(instance->popup);

    nfc_blink_stop(instance);

    // Note: pre-opened dicts are cleaned up in dict_attack on_exit if used,
    // or freed here on next Detect entry (re-initialized to NULL then re-opened)
}
