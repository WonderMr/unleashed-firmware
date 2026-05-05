
#pragma once

#include <math.h>
#include <furi.h>
#include <furi_hal.h>
#include <lib/flipper_format/flipper_format.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUBGHZ_SETTING_DEFAULT_PRESET_COUNT 4

typedef struct SubGhzSetting SubGhzSetting;

SubGhzSetting* subghz_setting_alloc(void);

void subghz_setting_free(SubGhzSetting* instance);

void subghz_setting_load(SubGhzSetting* instance, const char* file_path);

size_t subghz_setting_get_frequency_count(SubGhzSetting* instance);

size_t subghz_setting_get_hopper_frequency_count(SubGhzSetting* instance);

size_t subghz_setting_get_preset_count(SubGhzSetting* instance);

const char* subghz_setting_get_preset_name(SubGhzSetting* instance, size_t idx);

int subghz_setting_get_inx_preset_by_name(SubGhzSetting* instance, const char* preset_name);

uint8_t* subghz_setting_get_preset_data(SubGhzSetting* instance, size_t idx);

size_t subghz_setting_get_preset_data_size(SubGhzSetting* instance, size_t idx);

uint8_t* subghz_setting_get_preset_data_by_name(SubGhzSetting* instance, const char* preset_name);

bool subghz_setting_load_custom_preset(
    SubGhzSetting* instance,
    const char* preset_name,
    FlipperFormat* fff_data_file);

bool subghz_setting_delete_custom_preset(SubGhzSetting* instance, const char* preset_name);

uint32_t subghz_setting_get_frequency(SubGhzSetting* instance, size_t idx);

uint32_t subghz_setting_get_hopper_frequency(SubGhzSetting* instance, size_t idx);

uint32_t subghz_setting_get_frequency_default_index(SubGhzSetting* instance);

uint32_t subghz_setting_get_default_frequency(SubGhzSetting* instance);

void subghz_setting_set_default_frequency(SubGhzSetting* instance, uint32_t frequency_to_setup);

uint8_t subghz_setting_customs_presets_to_log(SubGhzSetting* instance);

/** Add a frequency to the hopper list (if not already present).
 *  Also adds to the main frequency list if absent.
 *
 * @param instance SubGhzSetting instance
 * @param frequency frequency in Hz
 * @return true if added, false if already present or invalid
 */
bool subghz_setting_add_hopper_frequency(SubGhzSetting* instance, uint32_t frequency);

/** Save hopper frequencies to a user settings file (full rewrite).
 *
 * @param instance SubGhzSetting instance
 * @param file_path path to the settings file
 */
void subghz_setting_save_user_hopper(SubGhzSetting* instance, const char* file_path);

/** Append a single hopper frequency to the settings file.
 *  Creates the file with defaults if it doesn't exist.
 *  Much faster than save_user_hopper for single-frequency additions.
 *
 * @param instance SubGhzSetting instance
 * @param file_path path to the settings file
 * @param frequency frequency in Hz to append
 */
void subghz_setting_append_hopper_frequency(
    SubGhzSetting* instance,
    const char* file_path,
    uint32_t frequency);

/** Check if a hopper frequency is enabled (not in the disabled list).
 *
 * @param instance SubGhzSetting instance
 * @param frequency frequency in Hz
 * @return true if enabled, false if disabled
 */
bool subghz_setting_is_hopper_frequency_enabled(SubGhzSetting* instance, uint32_t frequency);

/** Enable or disable a hopper frequency.
 *
 * @param instance SubGhzSetting instance
 * @param frequency frequency in Hz
 * @param enabled true to enable, false to disable
 */
void subghz_setting_set_hopper_frequency_enabled(
    SubGhzSetting* instance,
    uint32_t frequency,
    bool enabled);

#ifdef __cplusplus
}
#endif
