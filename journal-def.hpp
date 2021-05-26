#pragma once
#include <cstdint>

typedef enum {
    OBJECT_UNUSED,
    OBJECT_DATA,
    OBJECT_FIELD,
    OBJECT_ENTRY,
    OBJECT_DATA_HASH_TABLE,
    OBJECT_FIELD_HASH_TABLE,
    OBJECT_ENTRY_ARRAY,
    OBJECT_TAG,
    OBJECT_TYPE_MAX
} object_type_t;


/* The journald definitions make extensive use of nested flexible array members 
 * which are "problematic" in C++. I was left with the choice of using arrays
 * with a size of 1-- which produces a lot of hackery for this file format,
 * removing pedantic warnings entirely or writing everything in C ...
 *
 * OR I can selectively disable the warnings for the data structure definitions.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint8_t res[6];
    uint64_t size;
    uint8_t payload[0];
} __attribute__((packed)) object_header_t;

typedef struct {
    object_header_t object;
    uint64_t hash;
    uint64_t next_hash_offset;
    uint64_t next_field_offset;
    uint64_t entry_offset;
    uint64_t entry_array_offset;
    uint64_t n_entries;
    uint8_t payload[0];
} __attribute__((packed)) data_object_t;

typedef struct {
    object_header_t object;
    uint64_t hash;
    uint64_t next_hash_offset;
    uint64_t head_data_offset;
    uint8_t payload[0];
} __attribute__((packed)) field_object_t;

typedef struct {
    uint64_t object_offset;
    uint64_t hash;
} __attribute__((packed)) entry_item_t;

typedef struct {
    object_header_t object;
    uint64_t seqnum;
    uint64_t realtime;
    uint64_t monotonic;
    uint64_t boot_id[2];
    uint64_t xor_hash;
    entry_item_t items[0];
} __attribute__((packed)) entry_object_t;

typedef struct {
    uint64_t head_hash_offset;
    uint64_t tail_hash_offset;
} __attribute__((packed)) hash_item_t;

typedef struct {
    object_header_t object;
    hash_item_t items[0];
} __attribute__((packed)) hash_table_object_t;

typedef struct {
    object_header_t object;
    uint64_t next_entry_array_offset;
    uint64_t items[0];
} __attribute__((packed)) entry_array_object_t;

#define TAG_LENGTH (256/8)

typedef struct {
    object_header_t object;
    uint64_t seqnum;
    uint64_t epoch;
    uint8_t tag[TAG_LENGTH];
} __attribute__((packed)) tag_object_t;

typedef union {
    object_header_t object;
    data_object_t data;
    field_object_t field;
    entry_object_t entry;
    hash_table_object_t hash_table;
    entry_array_object_t entry_array;
    tag_object_t tag;
} object_t;

#pragma GCC diagnostic pop

enum {
    STATE_OFFLINE = 0,
    STATE_ONLINE = 1,
    STATE_ARCHIVED = 2,
    STATE_MAX
};

enum {
    HEADER_INCOMPATIBLE_COMPRESSED_XZ = 1 << 0,
    HEADER_INCOMPATIBLE_COMPRESSED_LZ4 = 1 << 1,
    HEADER_INCOMPATIBLE_KEYED_HASH = 1 << 2,
    HEADER_INCOMPATIBLE_COMPRESSED_ZSTD = 1 << 3
};

typedef enum {
    OBJECT_COMPRESSED_XZ    = 1 << 0,
    OBJECT_COMPRESSED_LZ4   = 1 << 1,
    OBJECT_COMPRESSED_ZSTD  = 1 << 2,
    OBJECT_COMPRESSION_MASK = (OBJECT_COMPRESSED_XZ|OBJECT_COMPRESSED_LZ4|OBJECT_COMPRESSED_ZSTD),
    OBJECT_COMPRESSED_MAX   = OBJECT_COMPRESSION_MASK
} object_compression_type_t;

#define IS_HEADER_INCOMPATIBLE_COMPRESSED_XZ(x) ((x & HEADER_INCOMPATIBLE_COMPRESSED_XZ) == HEADER_INCOMPATIBLE_COMPRESSED_XZ)
#define IS_HEADER_INCOMPATIBLE_COMPRESSED_LZ4(x) ((x & HEADER_INCOMPATIBLE_COMPRESSED_LZ4) == HEADER_INCOMPATIBLE_COMPRESSED_LZ4)
#define IS_HEADER_INCOMPATIBLE_KEYED_HASH(x) ((x & HEADER_INCOMPATIBLE_KEYED_HASH) == HEADER_INCOMPATIBLE_KEYED_HASH)
#define IS_HEADER_INCOMPATIBLE_COMPRESSED_ZSTD(x) ((x & HEADER_INCOMPATIBLE_COMPRESSED_ZSTD) == HEADER_INCOMPATIBLE_COMPRESSED_ZSTD)

#define HEADER_INCOMPATIBLE_ANY (   HEADER_INCOMPATIBLE_COMPRESSED_XZ|  \
                                    HEADER_INCOMPATIBLE_COMPRESSED_LZ4| \
                                    HEADER_INCOMPATIBLE_KEYED_HASH|     \
                                    HEADER_INCOMPATIBLE_COMPRESSED_ZSTD)

#define IS_HEADER_INCOMPATIBLE_ANY(x) ( 0 != IS_HEADER_INCOMPATIBLE_COMPRESSED_XZ(x) || \
                                        0 != IS_HEADER_INCOMPATIBLE_COMPRESSED_LZ4(x) || \
                                        0 != IS_HEADER_INCOMPATIBLE_KEYED_HASH(x) || \
                                        0 != IS_HEADER_INCOMPATIBLE_COMPRESSED_ZSTD(x))

enum {
    HEADER_COMPATIBLE_SEALED = 1 << 0
};

#define IS_HEADER_COMPATIBLE_SEALED(x) ((x & HEADER_COMPATIBLE_SEALED) == HEADER_COMPATIBLE_SEALED)

#define HEADER_COMPATIBLE_ANY HEADER_COMPATIBLE_SEALED

#define HEADER_SIGNATURE "LPKSHHRH"
//((const char[]) { 'L', 'P', 'K', 'S', 'H', 'H', 'R', 'H' })

typedef struct {
    uint8_t signature[8];
    uint32_t compatible_flags;
    uint32_t incompatible_flags;
    uint8_t state;
    uint8_t reserved[7];
    uint64_t file_id[2];
    uint64_t machine_id[2];
    uint64_t boot_id[2];
    uint64_t seqnum_id[2];
    uint64_t header_size;
    uint64_t arena_size;
    uint64_t data_hash_table_offset;
    uint64_t data_hash_table_size;
    uint64_t field_hash_table_offset;
    uint64_t field_hash_table_size;
    uint64_t tail_object_offset;
    uint64_t n_objects;
    uint64_t n_entries;
    uint64_t tail_entry_seqnum;
    uint64_t head_entry_seqnum;
    uint64_t entry_array_offset;
    uint64_t head_entry_realtime;
    uint64_t tail_entry_realtime;
    uint64_t tail_entry_monotonic;
    uint64_t n_data;
    uint64_t n_fields;
    uint64_t n_tags;
    uint64_t n_entry_arrays;
    uint64_t data_hash_chain_depth;
    uint64_t field_hash_chain_depth;
} __attribute__((packed)) header_contents_t;


#define FSS_HEADER_SIGNATURE "KSHHRHLP"
//((const char[]) { 'K', 'S', 'H', 'H', 'R', 'H', 'L', 'P' })

typedef struct {
    uint8_t signature[8];
    uint32_t compatible_flags;
    uint32_t incompatible_flags;
    uint64_t machine_id[2];
    uint64_t boot_id[2];
    uint64_t header_size;
    uint64_t start_usec;
    uint64_t interval_usec;
    uint16_t fsprg_secpar;
    uint16_t reserved[3];
    uint64_t fsprg_state_size;
} __attribute__((packed)) fss_header_t;

#define ALIGN64(x) (((x) + 7ULL) & ~7ULL)
#define VALID64(x) (((x) & 7ULL) == 0ULL)

#define FLAGS_SET(v, flags) ((~(v) & (flags)) == 0)

#define JOURNAL_HEADER_CONTAINS(h, field) \
    (get_uint64((h)->header_size) >= offsetof(header_contents_t, field) + sizeof((h)->field))

#define JOURNAL_HEADER_SEALED(h) \
        FLAGS_SET(get_uint32(h), HEADER_COMPATIBLE_SEALED)

#define JOURNAL_HEADER_COMPRESSED_XZ(h) \
        FLAGS_SET(get_uint32(h), HEADER_INCOMPATIBLE_COMPRESSED_XZ)

#define JOURNAL_HEADER_COMPRESSED_LZ4(h) \
        FLAGS_SET(get_uint32((h), HEADER_INCOMPATIBLE_COMPRESSED_LZ4)

#define JOURNAL_HEADER_COMPRESSED_ZSTD(h) \
        FLAGS_SET(get_uint32((h), HEADER_INCOMPATIBLE_COMPRESSED_ZSTD)

#define JOURNAL_HEADER_KEYED_HASH(h) \
        FLAGS_SET(get_uint32(h), HEADER_INCOMPATIBLE_KEYED_HASH)


#define DEFAULT_FIELD_HASH_TABLE_SIZE (333ULL*sizeof(hash_item_t))
#define DEFAULT_DATA_HASH_TABLE_SIZE (2047ULL*sizeof(hash_item_t))
