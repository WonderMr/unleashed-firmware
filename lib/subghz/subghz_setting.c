#include "subghz_setting.h"
#include "types.h" // IWYU pragma: keep

#include <furi.h>
#include <m-list.h>
#include <lib/subghz/devices/cc1101_configs.h>

#define TAG "SubGhzSetting"

#define SUBGHZ_SETTING_FILE_TYPE    "Flipper SubGhz Setting File"
#define SUBGHZ_SETTING_FILE_VERSION 1

#define FREQUENCY_FLAG_DEFAULT (1 << 31)
#define FREQUENCY_MASK         (0xFFFFFFFF ^ FREQUENCY_FLAG_DEFAULT)

/* Default */
static const uint32_t subghz_frequency_list[] = {
    /* 300 - 348 */
    300000000,
    302757000,
    303000000,
    303875000,
    303900000,
    304250000,
    307000000,
    307500000,
    307800000,
    309000000,
    310000000,
    312000000,
    312100000,
    312200000,
    313000000,
    313850000,
    314000000,
    314350000,
    314980000,
    315000000,
    318000000,
    320000000,
    320150000,
    330000000,
    345000000,
    348000000,
    350000000,

    /* 387 - 464 */
    387000000,
    390000000,
    418000000,
    430000000,
    430500000,
    431000000,
    431500000,
    433075000, /* LPD433 first */
    433220000,
    433420000,
    433657070,
    433889000,
    433920000 | FREQUENCY_FLAG_DEFAULT, /* LPD433 mid */
    434075000,
    434176948,
    434190000,
    434390000,
    434420000,
    434620000,
    434775000, /* LPD433 last channels */
    438900000,
    440175000,
    462750000,
    464000000,
    467750000,

    /* 779 - 928 */
    779000000,
    868350000,
    868400000,
    868460000,
    868800000,
    868950000,
    906400000,
    915000000,
    925000000,
    928000000,
    0,
};

static const uint32_t subghz_hopper_frequency_list[] = {
    315000000,
    390000000,
    430500000,
    433920000,
    434420000,
    868350000,
    0,
};

typedef struct {
    FuriString* custom_preset_name;
    uint8_t* custom_preset_data;
    size_t custom_preset_data_size;
} SubGhzSettingCustomPresetItem;

ARRAY_DEF(SubGhzSettingCustomPresetItemArray, SubGhzSettingCustomPresetItem, M_POD_OPLIST) //-V658

#define M_OPL_SubGhzSettingCustomPresetItemArray_t() \
    ARRAY_OPLIST(SubGhzSettingCustomPresetItemArray, M_POD_OPLIST)

LIST_DEF(FrequencyList, uint32_t)

#define M_OPL_FrequencyList_t() LIST_OPLIST(FrequencyList)

typedef struct {
    SubGhzSettingCustomPresetItemArray_t data;
} SubGhzSettingCustomPresetStruct;

struct SubGhzSetting {
    FrequencyList_t frequencies;
    FrequencyList_t hopper_frequencies;
    SubGhzSettingCustomPresetStruct* preset;
};

SubGhzSetting* subghz_setting_alloc(void) {
    SubGhzSetting* instance = malloc(sizeof(SubGhzSetting));
    FrequencyList_init(instance->frequencies);
    FrequencyList_init(instance->hopper_frequencies);
    instance->preset = malloc(sizeof(SubGhzSettingCustomPresetStruct));
    SubGhzSettingCustomPresetItemArray_init(instance->preset->data);
    return instance;
}

static void subghz_setting_preset_reset(SubGhzSetting* instance) {
    for
        M_EACH(item, instance->preset->data, SubGhzSettingCustomPresetItemArray_t) {
            furi_string_free(item->custom_preset_name);
            free(item->custom_preset_data);
        }
    SubGhzSettingCustomPresetItemArray_reset(instance->preset->data);
}

void subghz_setting_free(SubGhzSetting* instance) {
    furi_check(instance);
    FrequencyList_clear(instance->frequencies);
    FrequencyList_clear(instance->hopper_frequencies);
    for
        M_EACH(item, instance->preset->data, SubGhzSettingCustomPresetItemArray_t) {
            furi_string_free(item->custom_preset_name);
            free(item->custom_preset_data);
        }
    SubGhzSettingCustomPresetItemArray_clear(instance->preset->data);
    free(instance->preset);
    free(instance);
}

static void subghz_setting_load_default_preset(
    SubGhzSetting* instance,
    const char* preset_name,
    const uint8_t* preset_data) {
    furi_assert(instance);
    furi_assert(preset_data);
    uint32_t preset_data_count = 0;
    SubGhzSettingCustomPresetItem* item =
        SubGhzSettingCustomPresetItemArray_push_raw(instance->preset->data);

    item->custom_preset_name = furi_string_alloc();
    furi_string_set(item->custom_preset_name, preset_name);

    while(preset_data[preset_data_count]) {
        preset_data_count += 2;
    }
    preset_data_count += 2;
    item->custom_preset_data_size = sizeof(uint8_t) * preset_data_count + sizeof(uint8_t) * 8;
    item->custom_preset_data = malloc(item->custom_preset_data_size);
    //load preset register + pa table
    memcpy(&item->custom_preset_data[0], &preset_data[0], item->custom_preset_data_size);
}

static void subghz_setting_load_default_region(
    SubGhzSetting* instance,
    const uint32_t frequencies[],
    const uint32_t hopper_frequencies[]) {
    furi_assert(instance);

    FrequencyList_reset(instance->frequencies);
    FrequencyList_reset(instance->hopper_frequencies);
    subghz_setting_preset_reset(instance);

    while(*frequencies) {
        FrequencyList_push_back(instance->frequencies, *frequencies);
        frequencies++;
    }

    while(*hopper_frequencies) {
        FrequencyList_push_back(instance->hopper_frequencies, *hopper_frequencies);
        hopper_frequencies++;
    }

    subghz_setting_load_default_preset(
        instance, "AM270", subghz_device_cc1101_preset_ook_270khz_async_regs);
    subghz_setting_load_default_preset(
        instance, "AM650", subghz_device_cc1101_preset_ook_650khz_async_regs);
    subghz_setting_load_default_preset(
        instance, "FM238", subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);
    subghz_setting_load_default_preset(
        instance, "FM476", subghz_device_cc1101_preset_2fsk_dev47_6khz_async_regs);
    subghz_setting_load_default_preset(
        instance, "FM12K", subghz_device_cc1101_preset_2fsk_dev12khz_async_regs);
}

// Region check removed
void subghz_setting_load_default(SubGhzSetting* instance) {
    subghz_setting_load_default_region(
        instance, subghz_frequency_list, subghz_hopper_frequency_list);
}

void subghz_setting_load(SubGhzSetting* instance, const char* file_path) {
    furi_check(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    uint32_t temp_data32;
    bool temp_bool;

    subghz_setting_load_default(instance);

    if(file_path) {
        do {
            if(!flipper_format_file_open_existing(fff_data_file, file_path)) {
                FURI_LOG_I(TAG, "File is not used %s", file_path);
                break;
            }

            if(!flipper_format_read_header(fff_data_file, temp_str, &temp_data32)) {
                FURI_LOG_E(TAG, "Missing or incorrect header");
                break;
            }

            if((!strcmp(furi_string_get_cstr(temp_str), SUBGHZ_SETTING_FILE_TYPE)) &&
               temp_data32 == SUBGHZ_SETTING_FILE_VERSION) {
            } else {
                FURI_LOG_E(TAG, "Type or version mismatch");
                break;
            }

            // Standard frequencies (optional)
            temp_bool = true;
            flipper_format_read_bool(fff_data_file, "Add_standard_frequencies", &temp_bool, 1);
            if(!temp_bool) {
                FURI_LOG_I(TAG, "Removing standard frequencies");
                FrequencyList_reset(instance->frequencies);
                FrequencyList_reset(instance->hopper_frequencies);
            } else {
                FURI_LOG_I(TAG, "Keeping standard frequencies");
            }

            // Load frequencies
            if(!flipper_format_rewind(fff_data_file)) {
                FURI_LOG_E(TAG, "Rewind error");
                break;
            }
            while(flipper_format_read_uint32(
                fff_data_file, "Frequency", (uint32_t*)&temp_data32, 1)) {
                //Todo FL-3535: add a frequency support check depending on the selected radio device
                if(furi_hal_subghz_is_frequency_valid(temp_data32)) {
                    FURI_LOG_I(TAG, "Frequency loaded %lu", temp_data32);
                    FrequencyList_push_back(instance->frequencies, temp_data32);
                } else {
                    FURI_LOG_E(TAG, "Frequency not supported %lu", temp_data32);
                }
            }

            // Load hopper frequencies
            if(!flipper_format_rewind(fff_data_file)) {
                FURI_LOG_E(TAG, "Rewind error");
                break;
            }
            while(flipper_format_read_uint32(
                fff_data_file, "Hopper_frequency", (uint32_t*)&temp_data32, 1)) {
                if(furi_hal_subghz_is_frequency_valid(temp_data32)) {
                    // Deduplicate: skip if already in list (e.g. default hopper frequencies)
                    bool duplicate = false;
                    for(size_t i = 0; i < FrequencyList_size(instance->hopper_frequencies);
                        i++) {
                        if(*FrequencyList_get(instance->hopper_frequencies, i) == temp_data32) {
                            duplicate = true;
                            break;
                        }
                    }
                    if(!duplicate) {
                        FURI_LOG_I(TAG, "Hopper frequency loaded %lu", temp_data32);
                        FrequencyList_push_back(instance->hopper_frequencies, temp_data32);
                    }
                } else {
                    FURI_LOG_E(TAG, "Hopper frequency not supported %lu", temp_data32);
                }
            }

            // Default frequency (optional)
            if(!flipper_format_rewind(fff_data_file)) {
                FURI_LOG_E(TAG, "Rewind error");
                break;
            }
            if(flipper_format_read_uint32(fff_data_file, "Default_frequency", &temp_data32, 1)) {
                subghz_setting_set_default_frequency(instance, temp_data32);
            }

            // custom preset (optional)
            if(!flipper_format_rewind(fff_data_file)) {
                FURI_LOG_E(TAG, "Rewind error");
                break;
            }
            furi_string_reset(temp_str);
            while(flipper_format_read_string(fff_data_file, "Custom_preset_name", temp_str)) {
                FURI_LOG_I(TAG, "Custom preset loaded %s", furi_string_get_cstr(temp_str));
                subghz_setting_load_custom_preset(
                    instance, furi_string_get_cstr(temp_str), fff_data_file);
            }

        } while(false);
    }

    furi_string_free(temp_str);
    flipper_format_free(fff_data_file);
    furi_record_close(RECORD_STORAGE);

    if(!FrequencyList_size(instance->frequencies) ||
       !FrequencyList_size(instance->hopper_frequencies)) {
        FURI_LOG_E(TAG, "Error loading user settings, loading default settings");
        subghz_setting_load_default(instance);
    }
}

void subghz_setting_set_default_frequency(SubGhzSetting* instance, uint32_t frequency_to_setup) {
    for
        M_EACH(frequency, instance->frequencies, FrequencyList_t) {
            *frequency &= FREQUENCY_MASK;
            if(*frequency == frequency_to_setup) {
                *frequency |= FREQUENCY_FLAG_DEFAULT;
            }
        }
}

size_t subghz_setting_get_frequency_count(SubGhzSetting* instance) {
    furi_check(instance);
    return FrequencyList_size(instance->frequencies);
}

size_t subghz_setting_get_hopper_frequency_count(SubGhzSetting* instance) {
    furi_check(instance);
    return FrequencyList_size(instance->hopper_frequencies);
}

size_t subghz_setting_get_preset_count(SubGhzSetting* instance) {
    furi_check(instance);
    return SubGhzSettingCustomPresetItemArray_size(instance->preset->data);
}

const char* subghz_setting_get_preset_name(SubGhzSetting* instance, size_t idx) {
    furi_check(instance);
    if(idx >= SubGhzSettingCustomPresetItemArray_size(instance->preset->data)) {
        idx = 0;
    }
    SubGhzSettingCustomPresetItem* item =
        SubGhzSettingCustomPresetItemArray_get(instance->preset->data, idx);
    return furi_string_get_cstr(item->custom_preset_name);
}

int subghz_setting_get_inx_preset_by_name(SubGhzSetting* instance, const char* preset_name) {
    furi_check(instance);
    size_t idx = 0;
    for
        M_EACH(item, instance->preset->data, SubGhzSettingCustomPresetItemArray_t) {
            if(strcmp(furi_string_get_cstr(item->custom_preset_name), preset_name) == 0) {
                return idx;
            }
            idx++;
        }
    furi_crash("SubGhz: No name preset.");
    return -1;
}

bool subghz_setting_load_custom_preset(
    SubGhzSetting* instance,
    const char* preset_name,
    FlipperFormat* fff_data_file) {
    furi_check(instance);
    furi_check(preset_name);
    uint32_t temp_data32;
    SubGhzSettingCustomPresetItem* item =
        SubGhzSettingCustomPresetItemArray_push_raw(instance->preset->data);
    item->custom_preset_name = furi_string_alloc();
    furi_string_set(item->custom_preset_name, preset_name);
    do {
        if(!flipper_format_get_value_count(fff_data_file, "Custom_preset_data", &temp_data32))
            break;
        if(!temp_data32 || (temp_data32 % 2)) {
            FURI_LOG_E(TAG, "Integrity error Custom_preset_data");
            break;
        }
        item->custom_preset_data_size = sizeof(uint8_t) * temp_data32;
        item->custom_preset_data = malloc(item->custom_preset_data_size);
        if(!flipper_format_read_hex(
               fff_data_file,
               "Custom_preset_data",
               item->custom_preset_data,
               item->custom_preset_data_size)) {
            FURI_LOG_E(TAG, "Missing Custom_preset_data");
            break;
        }
        return true;
    } while(true);
    return false;
}

bool subghz_setting_delete_custom_preset(SubGhzSetting* instance, const char* preset_name) {
    furi_check(instance);
    furi_check(preset_name);
    SubGhzSettingCustomPresetItemArray_it_t it;
    SubGhzSettingCustomPresetItemArray_it_last(it, instance->preset->data);
    while(!SubGhzSettingCustomPresetItemArray_end_p(it)) {
        SubGhzSettingCustomPresetItem* item = SubGhzSettingCustomPresetItemArray_ref(it);
        if(strcmp(furi_string_get_cstr(item->custom_preset_name), preset_name) == 0) {
            furi_string_free(item->custom_preset_name);
            free(item->custom_preset_data);
            SubGhzSettingCustomPresetItemArray_remove(instance->preset->data, it);
            return true;
        }
        SubGhzSettingCustomPresetItemArray_previous(it);
    }
    return false;
}

uint8_t* subghz_setting_get_preset_data(SubGhzSetting* instance, size_t idx) {
    furi_check(instance);
    SubGhzSettingCustomPresetItem* item =
        SubGhzSettingCustomPresetItemArray_get(instance->preset->data, idx);
    return item->custom_preset_data;
}

size_t subghz_setting_get_preset_data_size(SubGhzSetting* instance, size_t idx) {
    furi_check(instance);
    SubGhzSettingCustomPresetItem* item =
        SubGhzSettingCustomPresetItemArray_get(instance->preset->data, idx);
    return item->custom_preset_data_size;
}

uint8_t* subghz_setting_get_preset_data_by_name(SubGhzSetting* instance, const char* preset_name) {
    furi_check(instance);
    SubGhzSettingCustomPresetItem* item = SubGhzSettingCustomPresetItemArray_get(
        instance->preset->data, subghz_setting_get_inx_preset_by_name(instance, preset_name));
    return item->custom_preset_data;
}

uint32_t subghz_setting_get_frequency(SubGhzSetting* instance, size_t idx) {
    furi_check(instance);
    if(idx < FrequencyList_size(instance->frequencies)) {
        return (*FrequencyList_get(instance->frequencies, idx)) & FREQUENCY_MASK;
    } else {
        return 0;
    }
}

uint32_t subghz_setting_get_hopper_frequency(SubGhzSetting* instance, size_t idx) {
    furi_check(instance);
    if(idx < FrequencyList_size(instance->hopper_frequencies)) {
        return *FrequencyList_get(instance->hopper_frequencies, idx);
    } else {
        return 0;
    }
}

uint32_t subghz_setting_get_frequency_default_index(SubGhzSetting* instance) {
    furi_check(instance);
    for(size_t i = 0; i < FrequencyList_size(instance->frequencies); i++) {
        uint32_t frequency = *FrequencyList_get(instance->frequencies, i);
        if(frequency & FREQUENCY_FLAG_DEFAULT) {
            return i;
        }
    }
    return 0;
}

uint32_t subghz_setting_get_default_frequency(SubGhzSetting* instance) {
    furi_check(instance);
    return subghz_setting_get_frequency(
        instance, subghz_setting_get_frequency_default_index(instance));
}

bool subghz_setting_add_hopper_frequency(SubGhzSetting* instance, uint32_t frequency) {
    furi_check(instance);

    if(!furi_hal_subghz_is_frequency_valid(frequency)) {
        return false;
    }

    // Check if already in hopper list
    for(size_t i = 0; i < FrequencyList_size(instance->hopper_frequencies); i++) {
        if(*FrequencyList_get(instance->hopper_frequencies, i) == frequency) {
            return false;
        }
    }

    // Add to hopper list
    FrequencyList_push_back(instance->hopper_frequencies, frequency);

    // Also add to main frequency list if absent (for nearest-frequency matching)
    bool found_in_main = false;
    for(size_t i = 0; i < FrequencyList_size(instance->frequencies); i++) {
        if((*FrequencyList_get(instance->frequencies, i) & FREQUENCY_MASK) == frequency) {
            found_in_main = true;
            break;
        }
    }
    if(!found_in_main) {
        FrequencyList_push_back(instance->frequencies, frequency);
    }

    FURI_LOG_I(TAG, "Added hopper frequency %lu", frequency);
    return true;
}

void subghz_setting_save_user_hopper(SubGhzSetting* instance, const char* file_path) {
    furi_check(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // First, read existing file to preserve non-hopper settings
    FlipperFormat* fff_read = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();

    bool add_standard = true;
    uint32_t default_frequency = 0;
    bool has_default_frequency = false;

    // Collect existing custom Frequency entries
    FrequencyList_t saved_frequencies;
    FrequencyList_init(saved_frequencies);

    if(flipper_format_file_open_existing(fff_read, file_path)) {
        uint32_t temp_data32;
        if(flipper_format_read_header(fff_read, temp_str, &temp_data32)) {
            if(!strcmp(furi_string_get_cstr(temp_str), SUBGHZ_SETTING_FILE_TYPE) &&
               temp_data32 == SUBGHZ_SETTING_FILE_VERSION) {
                // Read Add_standard_frequencies
                flipper_format_read_bool(fff_read, "Add_standard_frequencies", &add_standard, 1);

                // Read custom Frequency entries
                if(flipper_format_rewind(fff_read)) {
                    while(flipper_format_read_uint32(fff_read, "Frequency", &temp_data32, 1)) {
                        FrequencyList_push_back(saved_frequencies, temp_data32);
                    }
                }

                // Read Default_frequency
                if(flipper_format_rewind(fff_read)) {
                    if(flipper_format_read_uint32(
                           fff_read, "Default_frequency", &temp_data32, 1)) {
                        default_frequency = temp_data32;
                        has_default_frequency = true;
                    }
                }
            }
        }
    }
    flipper_format_free(fff_read);

    // Now write the new file preserving existing settings
    FlipperFormat* fff_write = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_always(fff_write, file_path)) {
            FURI_LOG_E(TAG, "Unable to open file for writing: %s", file_path);
            break;
        }

        if(!flipper_format_write_header_cstr(
               fff_write, SUBGHZ_SETTING_FILE_TYPE, SUBGHZ_SETTING_FILE_VERSION)) {
            FURI_LOG_E(TAG, "Unable to write header");
            break;
        }

        if(!flipper_format_write_bool(fff_write, "Add_standard_frequencies", &add_standard, 1)) {
            break;
        }

        // Preserve custom Frequency entries from existing file
        bool write_ok = true;
        for(size_t i = 0; write_ok && i < FrequencyList_size(saved_frequencies); i++) {
            uint32_t freq = *FrequencyList_get(saved_frequencies, i);
            write_ok = flipper_format_write_uint32(fff_write, "Frequency", &freq, 1);
        }
        if(!write_ok) break;

        // Write all hopper frequencies
        for(size_t i = 0; write_ok && i < FrequencyList_size(instance->hopper_frequencies); i++) {
            uint32_t freq = *FrequencyList_get(instance->hopper_frequencies, i);
            write_ok = flipper_format_write_uint32(fff_write, "Hopper_frequency", &freq, 1);
        }
        if(!write_ok) {
            FURI_LOG_E(TAG, "Unable to write hopper frequencies");
            break;
        }

        // Preserve Default_frequency
        if(has_default_frequency) {
            if(!flipper_format_write_uint32(
                   fff_write, "Default_frequency", &default_frequency, 1)) {
                break;
            }
        }

        FURI_LOG_I(
            TAG,
            "Saved %u hopper frequencies to %s",
            FrequencyList_size(instance->hopper_frequencies),
            file_path);
    } while(false);

    FrequencyList_clear(saved_frequencies);
    furi_string_free(temp_str);
    flipper_format_free(fff_write);
    furi_record_close(RECORD_STORAGE);
}

uint8_t subghz_setting_customs_presets_to_log(SubGhzSetting* instance) {
    furi_assert(instance);
#ifndef FURI_DEBUG
    FURI_LOG_I(TAG, "Logging loaded presets allow only Debug build");
#else
    uint8_t count = 0;
    FuriString* temp = furi_string_alloc();

    FURI_LOG_I(TAG, "Loaded presets");
    for
        M_EACH(item, instance->preset->data, SubGhzSettingCustomPresetItemArray_t) {
            furi_string_reset(temp);

            for(uint8_t i = 0; i < item->custom_preset_data_size; i++) {
                furi_string_cat_printf(temp, "%02u ", item->custom_preset_data[i]);
            }

            FURI_LOG_I(
                TAG, "%u  -  %s", count + 1, furi_string_get_cstr(item->custom_preset_name));
            FURI_LOG_I(TAG, "  Size: %u", item->custom_preset_data_size);
            FURI_LOG_I(TAG, "  Data: %s", furi_string_get_cstr(temp));

            count++;
        }

    furi_string_free(temp);

    return count;
#endif
    return 0;
}
