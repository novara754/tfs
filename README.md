# TFS - Tiny File System

TFS is a small and simple filesystem designed by me for self-educational
purposes. This project implements the filesystem in userspace
using [libfuse](https://github.com/libfuse/libfuse).

## Filesystem Design

### Overview

```
+-------------------------+ 
|                         |
| Bootsector              |
|                         |
+-------------------------+
|                         |
| Reserved blocks (0..n)  |
|                         |
+-------------------------+
|                         |
| Block usage bitmap      |
|                         |
+-------------------------+
|                         |
| Root directory          |
|                         |
+-------------------------+
|                         |
| Data blocks (0..16384)  |
|                         |
+-------------------------+
```

The filesystem is split into 1024-byte blocks.

### Reserved blocks

At the very beginning there are blocks reserved for the bootloader.
There is always at least one of these blocks, however more can be added as
needed. The number of additional reserved blocks can be set by changing the 510th byte in the first block, directly in front of the
[BIOS Boot Signature](https://en.wikipedia.org/wiki/Master_boot_record#SIG).
A value of `0x00` sets just one reserved block, `0x01` for two, and so on.

### Block usage bitmap

After the [reserved blocks](#reserved-blocks) follows a "Block Usage Bitmap"
which spans 2 blocks. Each bit in these two blocks corresponds to one of the
[data blocks](#data-blocks) and signifies whether the data block is in use (1)
or not (0).  
Because the bitmap is 16384 bits long (`2 * 1024 * 8`) and as such 16384 blocks
can be mapped, roughly 16 MiB of storage are available.

### Directories

Directories are single blocks made up of sequence of
[directory-entries](#directory-entries).
As each directory entry is 16 bytes long and a directory is 1024 bytes long,
there is a maximum 64 entries per directory.

Each entry in a directory can either be a file or another directory.

### Files

Files are consecutive blocks containing nothing but their raw data.
Files are identified by directory entries pointing to their respective
blocks.

### Root directory

The root directory is a [directory](#directories) right after the
[block usage bitmap](#block-usage-bitmap) and represents the root of the
filesystem tree.  
It is not part of the block usage bitmap because it always exists and always
takes up a single block.

### Directory entries

Directory entries are 16-byte structures laid out as follows:

| Field       | Offset (bytes) | Length (bytes) |
|-------------|---------------:|---------------:|
| Name        |              0 |             10 |
| Start block |             10 |              2 |
| Num blocks  |             12 |              2 |
| Size        |             14 |              2 |

**Name:**
The name of the file or directory stored in 7-bit ASCII encoding.
Allowed is everything from 0x20 to 0x7D except for 0x2F (`"/"`).  
The MSB of the first byte signifies whether the entry is for a file (0)
or another directory (1).

**Start block:**
Index of the first data block of the file or directory. Keep in mind that
data blocks start *after* the root directory. An index of 0 corresponds to the
first block after the root directory.

**Num blocks:**
The number of blocks the file occupies. Ignored when file is a directory.

**Size:**
The number of bytes occupying the last block of the file (all other blocks
are always full).

An unused entry has all its bytes set to `0x7F`, while an entry consisting of
only `0x00` marks the end of a directory, no more used entries may follow it. 

## License

Licensed under the [MIT License](./LICENSE).
