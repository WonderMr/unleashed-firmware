#pragma once

#include <furi.h>
#include <stdint.h>

#define SUBGHZ_SAVED_DUMP_MAX_SELECTABLE 20

typedef struct SubGhzSavedDumpIndex SubGhzSavedDumpIndex;

typedef struct {
    uint32_t hash;
    FuriString* filename; // display name without extension
    FuriString* filepath; // full path to .sub file
} SubGhzSavedDumpEntry;

SubGhzSavedDumpIndex* subghz_saved_dump_index_alloc(void);
void subghz_saved_dump_index_free(SubGhzSavedDumpIndex* index);

/** Build index by scanning SD card for .sub files.
 * Returns true if a successful rebuild occurred.
 * Returns false if the index was already up-to-date OR if the scan failed
 * (in which case the existing index is preserved).
 */
bool subghz_saved_dump_index_build(SubGhzSavedDumpIndex* index);

/** Mark index as needing rebuild on next build() call */
void subghz_saved_dump_index_set_dirty(SubGhzSavedDumpIndex* index);

/** Compute hash for a signal identified by protocol, bit count, and key data */
uint32_t subghz_saved_dump_index_compute_hash(
    const char* protocol,
    uint16_t bit,
    const uint8_t* key_data,
    size_t key_data_size);

/** Lookup signal in index.
 * Returns number of matches found.
 * If match found, first_name and first_path are filled with first match info.
 */
uint16_t subghz_saved_dump_index_lookup(
    SubGhzSavedDumpIndex* index,
    uint32_t hash,
    FuriString* first_name,
    FuriString* first_path);

/** Get all matching entries for a given hash.
 * Returns number of matches written to entries array (up to max_entries).
 */
uint16_t subghz_saved_dump_index_get_matches(
    SubGhzSavedDumpIndex* index,
    uint32_t hash,
    SubGhzSavedDumpEntry** entries,
    uint16_t max_entries);
