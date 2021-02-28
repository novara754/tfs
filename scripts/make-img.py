#! /usr/bin/python3

FILE_LEN = 17_000_000

BLOCK_SIZE = 1024
FILE = 0
DIR = 1

def split_16bit(_16bit):
    """
    Split a 16-bit value into a list of 8-bit values
    in little endian format.
    """
    return [_16bit & 0xFF, (_16bit >> 7) & 0xFF]

def get_block_offset(block_idx):
    """
    Get byte offset from a block index.
    """
    return block_idx * BLOCK_SIZE

def get_data_block_offset(dblock_idx, reserved_blocks=0):
    """
    Get the byte offset from a data block index while respecting
    the number of reserved blocks at the beginning.
    """
    block_usage_bitmap_size = 2 * BLOCK_SIZE
    rootdir_size = 2 * BLOCK_SIZE
    return reserved_blocks * BLOCK_SIZE \
        + block_usage_bitmap_size \
        + rootdir_size \
        + dblock_idx * BLOCK_SIZE

def make_dir_entry(file_dir, name, start, length, size):
    """
    Generate a packed directory entry from the given parameters.
    """
    name_bytes = [ord(c) for c in name]
    if len(name_bytes) < 10:
        name_bytes += [0x00] * (10 - len(name_bytes))
    elif len(name_bytes) > 10 :
        name_bytes = name_bytes[:10]
    name_bytes[0] |= file_dir << 7
    return bytes(
        name_bytes +
        split_16bit(start) +
        split_16bit(length) +
        split_16bit(size)
    )

def main():
    with open('./test/test.img', 'wb') as f:
        ### Bootsector ###
        f.seek(510)
        # Set additional reserved blocks to 0 and write boot signature.
        f.write(bytes([0x00, 0x55, 0xAA]))
        ### ---------- ###

        ### Block usage bitmap ###
        # Set all blocks to free.
        f.seek(get_block_offset(1))
        f.write(bytes([0x00]))
        ### ------------------ ###

        ### Root directory ###
        f.seek(get_block_offset(3))
        f.write(make_dir_entry(DIR, '.', 0, 0, 0))
        f.write(make_dir_entry(FILE, 'hello.txt', 1, 1, 14))
        ### -------------- ###

        ### First data block ###
        f.seek(get_data_block_offset(0))
        f.write(bytes('Hello, World!\n', 'ascii'))
        ### ---------------- ###

        ### Fill rest of file ###
        f.seek(FILE_LEN)
        f.write(bytes([0x00]))
        ### ----------------- ###

if __name__ == '__main__':
    main()
