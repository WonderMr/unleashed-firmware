#include "subghz_saved_dump_index.h"
#include <storage/storage.h>
#include <lib/toolbox/dir_walk.h>
#include <lib/toolbox/path.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/types.h>
#include <m-array.h>
#include <string.h>

#define TAG "SubGhzSavedDumpIndex"

#define SUBGHZ_SAVED_DUMP_INDEX_MAX_ENTRIES 500

ARRAY_DEF(SubGhzSavedDumpEntryArray, SubGhzSavedDumpEntry, M_POD_OPLIST) //-V658

#define M_OPL_SubGhzSavedDumpEntryArray_t() ARRAY_OPLIST(SubGhzSavedDumpEntryArray, M_POD_OPLIST)

struct SubGhzSavedDumpIndex {
    SubGhzSavedDumpEntryArray_t entries;
    bool is_built;
};

// FNV-1a 32-bit hash
static uint32_t fnv1a_hash_init(void) {
    return 2166136261u;
}

static uint32_t fnv1a_hash_update(uint32_t hash, const uint8_t* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t subghz_saved_dump_index_compute_hash(
    const char* protocol,
    uint16_t bit,
    const uint8_t* key_data,
    size_t key_data_size) {
    uint32_t hash = fnv1a_hash_init();
    hash = fnv1a_hash_update(hash, (const uint8_t*)protocol, strlen(protocol));
    hash = fnv1a_hash_update(hash, (const uint8_t*)&bit, sizeof(bit));
    hash = fnv1a_hash_update(hash, key_data, key_data_size);
    return hash;
}

static int subghz_saved_dump_entry_compare(const void* a, const void* b) {
    const SubGhzSavedDumpEntry* ea = a;
    const SubGhzSavedDumpEntry* eb = b;
    if(ea->hash < eb->hash) return -1;
    if(ea->hash > eb->hash) return 1;
    // Stable tie-breaker by filepath for deterministic ordering
    return strcmp(
        furi_string_get_cstr(ea->filepath), furi_string_get_cstr(eb->filepath));
}

SubGhzSavedDumpIndex* subghz_saved_dump_index_alloc(void) {
    SubGhzSavedDumpIndex* index = malloc(sizeof(SubGhzSavedDumpIndex));
    SubGhzSavedDumpEntryArray_init(index->entries);
    index->is_built = false;
    return index;
}

static void subghz_saved_dump_index_clear_entries(SubGhzSavedDumpIndex* index) {
    for
        M_EACH(entry, index->entries, SubGhzSavedDumpEntryArray_t) {
            furi_string_free(entry->filename);
            furi_string_free(entry->filepath);
        }
    SubGhzSavedDumpEntryArray_reset(index->entries);
}

void subghz_saved_dump_index_free(SubGhzSavedDumpIndex* index) {
    furi_assert(index);
    subghz_saved_dump_index_clear_entries(index);
    SubGhzSavedDumpEntryArray_clear(index->entries);
    free(index);
}

void subghz_saved_dump_index_set_dirty(SubGhzSavedDumpIndex* index) {
    furi_assert(index);
    index->is_built = false;
}

static bool subghz_saved_dump_index_dir_filter(const char* name, FileInfo* fileinfo, void* ctx) {
    UNUSED(ctx);
    // DirWalk recurses into directories unconditionally (regardless of filter).
    // Skipping unwanted paths (e.g. /assets/) is done in the build loop.
    if(fileinfo->flags & FSF_DIRECTORY) {
        return false; // don't yield directories as results
    }
    // Only accept .sub files
    size_t len = strlen(name);
    return (len > 4 && strcmp(name + len - 4, ".sub") == 0);
}

bool subghz_saved_dump_index_build(SubGhzSavedDumpIndex* index) {
    furi_assert(index);

    if(index->is_built) return false;

    // Build into a temporary array so the existing index stays intact on failure
    SubGhzSavedDumpEntryArray_t temp_entries;
    SubGhzSavedDumpEntryArray_init(temp_entries);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    DirWalk* dir_walk = dir_walk_alloc(storage);
    dir_walk_set_recursive(dir_walk, true);
    dir_walk_set_filter_cb(dir_walk, subghz_saved_dump_index_dir_filter, NULL);

    FuriString* path = furi_string_alloc();
    FuriString* protocol = furi_string_alloc();
    FuriString* filetype = furi_string_alloc();

    bool scan_success = false;

    if(dir_walk_open(dir_walk, SUBGHZ_APP_FOLDER)) {
        FileInfo fileinfo;
        bool walk_error = false;
        while(SubGhzSavedDumpEntryArray_size(temp_entries) <
              SUBGHZ_SAVED_DUMP_INDEX_MAX_ENTRIES) {
            DirWalkResult result = dir_walk_read(dir_walk, path, &fileinfo);
            if(result == DirWalkLast) {
                scan_success = true;
                break;
            }
            if(result == DirWalkError) {
                walk_error = true;
                FURI_LOG_E(TAG, "DirWalk error during index build");
                break;
            }

            // Skip files inside the assets directory
            if(furi_string_search_str(path, "/assets/") != FURI_STRING_FAILURE) continue;

            // Try to read Protocol, Bit, Key from the file
            FlipperFormat* ff = flipper_format_file_alloc(storage);
            do {
                if(!flipper_format_file_open_existing(ff, furi_string_get_cstr(path))) break;

                // Read file type, skip RAW files
                if(!flipper_format_read_string(ff, "Filetype", filetype)) break;
                if(furi_string_cmp_str(filetype, SUBGHZ_KEY_FILE_TYPE) != 0) break;

                // Read protocol
                if(!flipper_format_read_string(ff, "Protocol", protocol)) break;

                // Read bit count
                uint32_t bit32 = 0;
                if(!flipper_format_read_uint32(ff, "Bit", &bit32, 1)) break;
                uint16_t bit = (uint16_t)bit32;

                // Read key
                uint8_t key_data[sizeof(uint64_t)] = {0};
                if(!flipper_format_read_hex(ff, "Key", key_data, sizeof(uint64_t))) break;

                // Compute hash and add entry
                uint32_t hash = subghz_saved_dump_index_compute_hash(
                    furi_string_get_cstr(protocol), bit, key_data, sizeof(uint64_t));

                SubGhzSavedDumpEntry* entry =
                    SubGhzSavedDumpEntryArray_push_raw(temp_entries);
                entry->hash = hash;
                entry->filepath = furi_string_alloc_set(path);
                entry->filename = furi_string_alloc();
                path_extract_filename(path, entry->filename, true);

            } while(false);

            flipper_format_free(ff);
        }
        // If we hit the max entries limit, consider it a success (partial but usable)
        if(!walk_error && !scan_success) {
            scan_success = true;
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open directory: %s", SUBGHZ_APP_FOLDER);
    }

    furi_string_free(filetype);
    furi_string_free(protocol);
    furi_string_free(path);
    dir_walk_close(dir_walk);
    dir_walk_free(dir_walk);
    furi_record_close(RECORD_STORAGE);

    if(scan_success) {
        // Sort temp entries by hash for binary search
        size_t count = SubGhzSavedDumpEntryArray_size(temp_entries);
        if(count > 1) {
            qsort(
                SubGhzSavedDumpEntryArray_get(temp_entries, 0),
                count,
                sizeof(SubGhzSavedDumpEntry),
                subghz_saved_dump_entry_compare);
        }

        // Swap: discard old entries, take ownership of temp
        subghz_saved_dump_index_clear_entries(index);
        SubGhzSavedDumpEntryArray_move(index->entries, temp_entries);

        index->is_built = true;
        FURI_LOG_I(TAG, "Index built: %zu entries", count);
        return true;
    } else {
        // Scan failed — free temp entries, keep existing index intact
        for
            M_EACH(entry, temp_entries, SubGhzSavedDumpEntryArray_t) {
                furi_string_free(entry->filename);
                furi_string_free(entry->filepath);
            }
        SubGhzSavedDumpEntryArray_clear(temp_entries);
        FURI_LOG_W(TAG, "Index build failed, will retry on next enter");
        return false;
    }
}

uint16_t subghz_saved_dump_index_lookup(
    SubGhzSavedDumpIndex* index,
    uint32_t hash,
    FuriString* first_name,
    FuriString* first_path) {
    furi_assert(index);

    uint16_t match_count = 0;
    size_t count = SubGhzSavedDumpEntryArray_size(index->entries);

    // Binary search for the hash
    size_t lo = 0;
    size_t hi = count;
    size_t found = count; // sentinel

    while(lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        SubGhzSavedDumpEntry* entry = SubGhzSavedDumpEntryArray_get(index->entries, mid);
        if(entry->hash < hash) {
            lo = mid + 1;
        } else if(entry->hash > hash) {
            hi = mid;
        } else {
            found = mid;
            hi = mid; // find first occurrence
        }
    }

    if(found == count) {
        // Check lo position (binary search result)
        if(lo < count) {
            SubGhzSavedDumpEntry* entry = SubGhzSavedDumpEntryArray_get(index->entries, lo);
            if(entry->hash == hash) {
                found = lo;
            }
        }
    }

    if(found >= count) return 0;

    // Scan from found position forward to count all matches
    for(size_t i = found; i < count; i++) {
        SubGhzSavedDumpEntry* entry = SubGhzSavedDumpEntryArray_get(index->entries, i);
        if(entry->hash != hash) break;
        if(match_count == 0 && first_name && first_path) {
            furi_string_set(first_name, entry->filename);
            furi_string_set(first_path, entry->filepath);
        }
        match_count++;
    }

    return match_count;
}

uint16_t subghz_saved_dump_index_get_matches(
    SubGhzSavedDumpIndex* index,
    uint32_t hash,
    SubGhzSavedDumpEntry** entries,
    uint16_t max_entries) {
    furi_assert(index);
    furi_assert(entries);

    size_t count = SubGhzSavedDumpEntryArray_size(index->entries);
    uint16_t match_count = 0;

    // Binary search for first occurrence
    size_t lo = 0;
    size_t hi = count;

    while(lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        SubGhzSavedDumpEntry* entry = SubGhzSavedDumpEntryArray_get(index->entries, mid);
        if(entry->hash < hash) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    // Collect all matches
    for(size_t i = lo; i < count && match_count < max_entries; i++) {
        SubGhzSavedDumpEntry* entry = SubGhzSavedDumpEntryArray_get(index->entries, i);
        if(entry->hash != hash) break;
        entries[match_count] = entry;
        match_count++;
    }

    return match_count;
}
