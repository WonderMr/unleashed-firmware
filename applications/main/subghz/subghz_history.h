
#pragma once

#include <math.h>
#include <furi.h>
#include <furi_hal.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/types.h>

typedef struct SubGhzHistory SubGhzHistory;

/** Allocate SubGhzHistory
 * 
 * @return SubGhzHistory* 
 */
SubGhzHistory* subghz_history_alloc(void);

/** Free SubGhzHistory
 * 
 * @param instance - SubGhzHistory instance
 */
void subghz_history_free(SubGhzHistory* instance);

/** Clear history
 * 
 * @param instance - SubGhzHistory instance
 */
void subghz_history_reset(SubGhzHistory* instance);

void subghz_history_delete_item(SubGhzHistory* instance, uint16_t idx);

/** Get frequency to history[idx]
 * 
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index  
 * @return frequency - frequency Hz
 */
uint32_t subghz_history_get_frequency(SubGhzHistory* instance, uint16_t idx);

SubGhzRadioPreset* subghz_history_get_radio_preset(SubGhzHistory* instance, uint16_t idx);

/** Get preset to history[idx]
 * 
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index  
 * @return preset   - preset name
 */
const char* subghz_history_get_preset(SubGhzHistory* instance, uint16_t idx);

/** Get history index write 
 * 
 * @param instance  - SubGhzHistory instance
 * @return idx      - current record index  
 */
uint16_t subghz_history_get_item(SubGhzHistory* instance);

/** Get type protocol to history[idx]
 * 
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index  
 * @return type      - type protocol  
 */
uint8_t subghz_history_get_type_protocol(SubGhzHistory* instance, uint16_t idx);

/** Get name protocol to history[idx]
 * 
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index  
 * @return name      - const char* name protocol  
 */
const char* subghz_history_get_protocol_name(SubGhzHistory* instance, uint16_t idx);

/** Get datetime from history[idx]
 * 
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index  
 * @return datetime - DateTime received timestamp  
 */
DateTime subghz_history_get_datetime(SubGhzHistory* instance, uint16_t idx);

/** Get string item menu to history[idx]
 * 
 * @param instance  - SubGhzHistory instance
 * @param output    - FuriString* output
 * @param idx       - record index
 */
void subghz_history_get_text_item_menu(SubGhzHistory* instance, FuriString* output, uint16_t idx);

/** Get time item menu to history[idx]
 * 
 * @param instance  - SubGhzHistory instance
 * @param output    - FuriString* output
 * @param idx       - record index
 */
void subghz_history_get_time_item_menu(SubGhzHistory* instance, FuriString* output, uint16_t idx);

/** Get string the remaining number of records to history
 * 
 * @param instance  - SubGhzHistory instance
 * @param output    - FuriString* output
 * @return bool - is FULL
 */
bool subghz_history_get_text_space_left(SubGhzHistory* instance, FuriString* output);

/** Return last index
 *
 * @param instance - SubGhzHistory instance
 * @return
 */
uint16_t subghz_history_get_last_index(SubGhzHistory* instance);

/** Add protocol to history
 * 
 * @param instance  - SubGhzHistory instance
 * @param context    - SubGhzProtocolCommon context
 * @param preset    - SubGhzRadioPreset preset
 * @return bool;
 */
bool subghz_history_add_to_history(
    SubGhzHistory* instance,
    void* context,
    SubGhzRadioPreset* preset);

/** Get SubGhzProtocolCommonLoad to load into the protocol decoder bin data
 * 
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index
 * @return SubGhzProtocolCommonLoad*
 */
FlipperFormat* subghz_history_get_raw_data(SubGhzHistory* instance, uint16_t idx);

/** Set saved dump info for history item
 *
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index
 * @param name      - saved filename (display name)
 * @param path      - full path to saved .sub file
 * @param count     - number of matching saved files
 */
void subghz_history_set_saved_info(
    SubGhzHistory* instance,
    uint16_t idx,
    const char* name,
    const char* path,
    uint16_t count);

/** Get saved dump file path for history[idx]
 *
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index
 * @return path     - const char* path or NULL if not matched
 */
const char* subghz_history_get_saved_path(SubGhzHistory* instance, uint16_t idx);

/** Get saved dump match count for history[idx]
 *
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index
 * @return count    - number of saved matches (0 = not matched)
 */
uint16_t subghz_history_get_saved_count(SubGhzHistory* instance, uint16_t idx);

/** Get saved dump hash for history[idx] (for use with index get_matches).
 *  Note: hash 0 is a valid value. Use has_saved_hash() to check if set.
 *
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index
 * @return hash     - saved dump hash
 */
uint32_t subghz_history_get_saved_hash(SubGhzHistory* instance, uint16_t idx);

/** Set saved dump hash for history[idx]
 *
 * @param instance  - SubGhzHistory instance
 * @param idx       - record index
 * @param hash      - hash value
 */
void subghz_history_set_saved_hash(SubGhzHistory* instance, uint16_t idx, uint32_t hash);

/** Check if history[idx] has a computed saved hash (valid even if hash == 0) */
bool subghz_history_has_saved_hash(SubGhzHistory* instance, uint16_t idx);

/** Clear saved match metadata from all history items.
 *  Called when the saved dump index is rebuilt so stale data doesn't persist.
 */
void subghz_history_clear_all_saved_info(SubGhzHistory* instance);
