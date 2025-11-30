# SortedStringTable Binary Format Specification v1

## Overview
This document describes the tape format of the on-disk segments.

## Layout

```
HEADER (24 bytes):
+--------+--------+--------+--------+--------+--------+
| magic  | ver    | body   | index  | size   | crc32  |
| (u32)  | (u32)  | offset | offset | (u32)  | (u32)  |
| bytes  | bytes  | (u32)  | (u32)  | bytes  | bytes  |
| 0-3    | 4-7    | 8-11   | 12-15  | 16-19  | 20-23  |
+--------+--------+--------+--------+--------+--------+

BODY (variable):
+----------+-----+----------+-----+-----------+
| key_len  | key | val_len  | val | timestamp |
| (varint) | var | (varint) | var | (u32)     |
+----------+-----+----------+-----+-----------+
Repeated for each entry

INDEX (variable):
+----------+-----+----------+
| key_len  | key | body_off |
| (varint) | var | (u32)    |
+----------+-----+----------+
Every entry (dense) or every k-th entry (sparse)
```

## Sections

### Header

```
Header {
    u32 magic = 0xDEADBEEF;
    u32 version = 1;
    u32 body_offset;      // where body starts
    u32 index_offset;     // where index starts
    u32 payload_size;     // total size (body + index)
    u32 checksum;         // CRC32 of entire file except this field
};
```

### Body

```
Body {
  for each entry in table:
      varint key_len
      u8[key_len] key
      varint value_len
      u8[value_len] value
      u32 timestamp
};
```

### Index

```
Index {
  for each entry in table:  // dense initially
      varint key_len
      u8[key_len] key
      u32 offset_in_body
};
```

## CRC32 Computation
CRC32 calculated based on the whole file excluding the checksum field itself.

## Edge Cases

1. **Truncated File**: If file ends before reaching `index_offset`,
   deserialization fails with error "File truncated".

2. **Checksum Mismatch**: If computed CRC32 ≠ stored checksum,
   throw error "Data corruption detected". File must not be read.

3. **Bad Magic Number**: If magic != 0xDEADBEEF, throw error
   "Invalid SST file format".

4. **Empty Segment**: 0 entries allowed; `body_offset == index_offset`.
   Valid serialized state with header only.

5. **Invalid Offsets**: If `body_offset < 24` or `index_offset < body_offset`,
   file is malformed, throw error "Invalid file structure".

6. **Payload Size Mismatch**: If `payload_size != (file_size - 24)`,
   file may be truncated or corrupted, throw error "Payload size mismatch".

## Example

  **Input Table:**
  - Entry 1: key="foo", value="bar", timestamp=100
  - Entry 2: key="x", value="yz", timestamp=200

  **Binary Output (hex dump):**

```
  HEADER (bytes 0-23):
    EF BE AD DE 01 00 00 00 18 00 00 00 2E 00 00 00 24 00 00 00 08 E2 98 1F
    ^magic      ^version   ^body_off   ^index_off  ^payload_sz ^checksum

  BODY (bytes 24-45, 22 bytes total):
    03 66 6F 6F 03 62 61 72 64 00 00 00 01 78 02 79 7A C8 00 00 00
    ^key_len(3) ^"foo"     ^val_len(3) ^"bar"     ^ts(100)  ^key_len(1) ^"x" ^val_len(2) ^"yz" ^ts(200)

  INDEX (bytes 46-59, 14 bytes total):
    03 66 6F 6F 00 00 00 00 01 78 0D 00 00 00
    ^key_len(3) ^"foo"     ^offset(0) ^key_len(1) ^"x" ^offset(13)

  **Offset Summary:**
  | Section | Offset | Size | End  |
  |---------|--------|------|------|
  | Header  | 0      | 24   | 23   |
  | Body    | 24     | 22   | 45   |
  | Index   | 46     | 14   | 59   |
  | **Total** | **0** | **60** | **59** |

  **CRC32 Calculation:**
  Computed over bytes 0-19 (header without checksum) + 24-59 (body + index) = 0x1F98E208
```

