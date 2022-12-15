#include "FS.h"
#include <cassert>
#include <cstring>
#include <iostream>

#include "disk.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#define MAX_FILE_NAME_LEN 4

#define BLK_STS_FREE 0
#define BLK_STS_OCCUPIED 1

#define DESCRIPTOR_BLOCKS 6
#define DESCRIPTOR_MAX_BLOCKS 3

#define BUFFER_BLOCKS (1 + DESCRIPTOR_BLOCKS)

#define OFTS 4  // number of OFTs

struct DESCRIPTOR_T {
    int file_size;
    int block[DESCRIPTOR_MAX_BLOCKS];
};

struct DIRECTORY_ENTRY_T {
    char file_name[MAX_FILE_NAME_LEN];
    int descriptor;
};

struct OFT_T {
    byte buffer[BLOCK_SIZE];
    int pos, size, descriptor;
};

//////////////////////////////////////////////////////////////////////////////

// GLOBAL VARS

static byte D_COPY[BUFFER_BLOCKS][BLOCK_SIZE];
static OFT_T OFT[OFTS];
static int ROOT;

// HELPER FUNCTIONS

static int FS_init_disk();
#if (DEBUG)
static void print_blocks_status();
#endif
// static int block_status(int b);
// Set the block status in bitmap.
static void set_block_status(int b, int status);
// Get one free descriptor.
static int get_free_descriptor();
// Get one free block.
static int get_free_block();
// Get one free OFT.
static int get_free_oft();
// Since all descriptors are buffered, or we should use
// DESCRIPTOR_T get_descriptor() and void set_descriptor() to
// access the descriptors at disk.
static DESCRIPTOR_T *get_descriptor(int d);

// OTHER

// Number of descriptors in each block.
static const int desc_each_block = BLOCK_SIZE / sizeof(DESCRIPTOR_T);
// Total number of descriptors.
static const int descriptors = desc_each_block * DESCRIPTOR_BLOCKS;
static_assert(BLK_STS_FREE == 0 && BLK_STS_OCCUPIED == 1);
// Two blocks should be reserved, one for bitmap, at least one for file
// content.
static_assert(DESCRIPTOR_BLOCKS > 0 && DESCRIPTOR_BLOCKS <= BLOCKS - 2);
static_assert(descriptors > 0);

//////////////////////////////////////////////////////////////////////////////

int FS_init() {
    // Since the disk is in RAM now, so we should call FS_init_disk() every time
    // on fs_init, and if the disk is in actual disk, this should be done only
    // once
    FS_init_disk();

    // Buffer blocks
    for (int i = 0; i < BUFFER_BLOCKS; ++i) {
        read_block(i, D_COPY[i]);
    }

    // Init OFTs
    for (int i = 0; i < OFTS; ++i) {
        OFT[i].pos = -1;
        OFT[i].size = -1;
        OFT[i].descriptor = -1;
    }

    // Open the root directory
    ROOT = open(NULL);
    assert(ROOT == 0 && OFT[ROOT].descriptor == 0);

    return 0;
}

int FS_close() {
    // Write back buffered blocks
    // Although the disk is now in RAM and this is useless
    for (int i = 0; i < BUFFER_BLOCKS; ++i) {
        write_block(i, D_COPY[i]);
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

int create(const char *path) {
    if (strlen(path) >= MAX_FILE_NAME_LEN) {
        return ERR_PATH_TOO_LONG;
    }

    int free_entry = -1;

    seek(ROOT, 0);
    DIRECTORY_ENTRY_T de;
    while (!eof(ROOT)) {
        if (read(ROOT, &de, sizeof(de)) > 0) {
            if (de.file_name[0] == '\0') {
                if (free_entry == -1) {
                    free_entry = tell(ROOT - sizeof(DIRECTORY_ENTRY_T));
                }
            } else if (strncmp(de.file_name, path, MAX_FILE_NAME_LEN) == 0) {
                return ERR_FILE_ALREADY_EXISTS;
            }
        }
    }

    int descriptor = get_free_descriptor();
    if (descriptor < 0) return ERR_TOO_MANY_FILES;

    DESCRIPTOR_T *d = get_descriptor(descriptor);
    d->file_size = 0;

    if (free_entry == -1) {
        free_entry = tell(ROOT);
    }

    if (free_entry != -1) {
        seek(ROOT, free_entry);
        de.descriptor = descriptor;
        strncpy(de.file_name, path, MAX_FILE_NAME_LEN);
        write(ROOT, &de, sizeof(de));
        return 0;
    }
    return ERR_NO_FREE_DIR_ENTRY;
}

int destroy(const char *path) {
    seek(ROOT, 0);

    DIRECTORY_ENTRY_T de;
    while (!eof(ROOT)) {
        if (read(ROOT, &de, sizeof(de)) > 0) {
            if (strncmp(de.file_name, path, MAX_FILE_NAME_LEN) == 0) {
                DESCRIPTOR_T *d = get_descriptor(de.descriptor);
                d->file_size = -1;
                for (int i = 0; i < DESCRIPTOR_MAX_BLOCKS; ++i) {
                    if (d->block[i] > 0) {
                        set_block_status(d->block[i], BLK_STS_FREE);
                        d->block[i] = -1;
                    } else {
                        break;
                    }
                }
                de.file_name[0] = '\0';
                seek(ROOT, tell(ROOT) - sizeof(DIRECTORY_ENTRY_T));
                write(ROOT, &de, sizeof(de));
                return 0;
            }
        }
    }
    return ERR_FILE_DOES_NOT_EXIST;
}

int open(const char *path) {
    if (path && strlen(path) >= MAX_FILE_NAME_LEN) {
        return ERR_PATH_TOO_LONG;
    }

    int descriptor = -1;
    if (path != NULL) {
        // Find file
        seek(ROOT, 0);
        DIRECTORY_ENTRY_T de;
        while (!eof(ROOT)) {
            if (read(ROOT, &de, sizeof(de)) > 0) {
                if (strncmp(de.file_name, path, MAX_FILE_NAME_LEN) == 0) {
                    descriptor = de.descriptor;
                    break;
                }
            }
        }
        if (descriptor == -1) {
            return ERR_FILE_DOES_NOT_EXIST;
        }
    } else {
        descriptor = 0;  // the root dir descriptor
    }

    int fh = get_free_oft();
    if (fh < 0) {
        return ERR_TOO_MANY_FILES_OPENED;
    }
    DESCRIPTOR_T *d = get_descriptor(descriptor);
    OFT[fh].pos = 0;
    OFT[fh].size = d->file_size;
    OFT[fh].descriptor = descriptor;
    if (d->block[0] != -1) {
        // Buffer the first block
        read_block(d->block[0], OFT[fh].buffer);
    }
    return fh;
}

int close(int fh) {
    DESCRIPTOR_T *d = get_descriptor(OFT[fh].descriptor);

    // Write buffer to disk
    if (!eof(fh)) {
        int buffered_block = OFT[fh].pos / BLOCK_SIZE;
        write_block(d->block[buffered_block], OFT[fh].buffer);
    }

    // Update file size in descriptor
    d->file_size = OFT[fh].size;

    // Mark OFT entry as free 
    OFT[fh].pos = -1;
    OFT[fh].size = -1;
    OFT[fh].descriptor = -1;

    return 0;
}

int read(int fh, void *buff, unsigned int len) {
    unsigned int remain = OFT[fh].size - OFT[fh].pos;
    if (remain < len) {
        len = remain;
    }

    DESCRIPTOR_T *d = get_descriptor(OFT[fh].descriptor);

    unsigned int n_read = 0, n;
    while (len > 0) {
        unsigned int begin = OFT[fh].pos % BLOCK_SIZE;
        n = BLOCK_SIZE - begin;
        if (len < n) n = len;

#if (DEBUG)
        printf("fh:%d begin:%u n:%u\n", fh, begin, n);
#endif
        memcpy(buff, OFT[fh].buffer + begin, n);

        OFT[fh].pos += n;
        if (OFT[fh].pos % BLOCK_SIZE == 0) {
            unsigned int new_buffer = OFT[fh].pos / BLOCK_SIZE;
            assert(new_buffer > 0);
            // copy buffer into appropriate block on disk
            write_block(d->block[new_buffer - 1], OFT[fh].buffer);
            // copy block from disk to buffer
            if (!eof(fh)) read_block(d->block[new_buffer], OFT[fh].buffer);
        }

        n_read += n;
        buff = (char *)buff + n;
        len -= n;
    }

    return n_read;
}

int write(int fh, const void *buff, unsigned int len) {
    DESCRIPTOR_T *d = get_descriptor(OFT[fh].descriptor);

    if (d->block[0] < 0) {
        d->block[0] = get_free_block();
        if (d->block[0] < 0) {
            return ERR_DISK_IS_FULL;  // DISK IS FULL
        }
    }

    unsigned int n_write = 0, n;
    while (len > 0) {
        unsigned int begin = OFT[fh].pos % BLOCK_SIZE;
        n = BLOCK_SIZE - begin;
        if (len < n) n = len;

#if (DEBUG)
        printf("fh:%d begin:%u n:%u\n", fh, begin, n);
#endif
        memcpy(OFT[fh].buffer + begin, buff, n);

        OFT[fh].pos += n;
        if (OFT[fh].size < OFT[fh].pos) {
            OFT[fh].size = OFT[fh].pos;
            d->file_size = OFT[fh].pos;
        }
        if (OFT[fh].pos % BLOCK_SIZE == 0) {
            unsigned int new_buffer = OFT[fh].pos / BLOCK_SIZE;
            assert(new_buffer > 0);
            // copy buffer into appropriate block on disk
            write_block(d->block[new_buffer - 1], OFT[fh].buffer);
            if (new_buffer < DESCRIPTOR_MAX_BLOCKS) {
                if (d->block[new_buffer] < 0) {
                    d->block[new_buffer] = get_free_block();
                    if (d->block[new_buffer] < 0) {
                        break;  // DISK IS FULL
                    }
                } else {
                    // copy block from disk to buffer
                    read_block(d->block[new_buffer], OFT[fh].buffer);
                }
            } else {
                break;  // MAX_FILE_SIZE reached
            }
        }

        n_write += n;
        buff = (char *)buff + n;
        len -= n;
    }

    return n_write;
}

int seek(int fh, int pos) {
    if (pos < 0 || OFT[fh].size < pos) {
        return ERR_SEEK_OUT_OF_RANGE;
    }
    int old_buffer = OFT[fh].pos / BLOCK_SIZE;
    int new_buffer = pos / BLOCK_SIZE;
    if (old_buffer != new_buffer) {
        DESCRIPTOR_T *d = get_descriptor(OFT[fh].descriptor);
        // copy buffer into appropriate block on disk
        if (!eof(fh)) write_block(d->block[old_buffer], OFT[fh].buffer);
        // copy block from disk to buffer
        read_block(d->block[new_buffer], OFT[fh].buffer);
    }
    // set current position to pos
    OFT[fh].pos = pos;
    return 0;
}

int tell(int fh) { return OFT[fh].pos; }

int eof(int fh) { return OFT[fh].pos == OFT[fh].size; }

int directory() {
    int count = 0;

    seek(ROOT, 0);
    DIRECTORY_ENTRY_T de;
    while (!eof(ROOT)) {
        if (read(ROOT, &de, sizeof(de)) > 0) {
            if (de.file_name[0] != '\0') {
                DESCRIPTOR_T *d = get_descriptor(de.descriptor);
                if (count) {
                    printf(" %s %u", de.file_name, d->file_size);
                } else {
                    printf("%s %u", de.file_name, d->file_size);
                }
                ++count;
            }
        }
    }
    printf("\n");

    return count;
}

//////////////////////////////////////////////////////////////////////////////

static int FS_init_disk() {
    // Init the bitmap
    memset(D_COPY[0], 0, sizeof(byte) * BLOCK_SIZE);
    //  [0]        [1-6]            [7]
    // bitmap   descriptors    ROOT's first block
    for (int i = 0; i < (1 + DESCRIPTOR_BLOCKS + 1); ++i) {
        set_block_status(i, BLK_STS_OCCUPIED);
    }
    write_block(0, D_COPY[0]);

    // Create the ROOT directory on disk
    memset(D_COPY[0], -1, sizeof(byte) * BLOCK_SIZE);
    DESCRIPTOR_T d;
    d.file_size = 0;
    d.block[0] = (1 + DESCRIPTOR_BLOCKS);
    d.block[1] = -1;
    memcpy(D_COPY[0], &d, sizeof(DESCRIPTOR_T));
    write_block(1, D_COPY[0]);

    // Init the rest of descriptor blocks
    for (unsigned int i = 2; i <= DESCRIPTOR_BLOCKS; ++i) {
        init_block(i, -1);
    }

    //? TODO Init the rest of blocks

    return 0;
}

#if (DEBUG)
static void print_blocks_status() {
    printf("block status:");
    for (unsigned int i = 0; i < BLOCKS / (sizeof(byte) * 8); ++i) {
        printf(" %03o", (int)D_COPY[0][i]);
    }
    printf("\n");
}
#endif

// static int block_status(int b) {
//     return D_COPY[0][b / sizeof(D_COPY[0][0])] &
//            (1 << (b % sizeof(D_COPY[0][0])));
// }

static void set_block_status(int b, int status) {
    if (status) {
        // set bit
        D_COPY[0][b / (sizeof(byte) * 8)] |=
            (byte)(1 << (b % (sizeof(byte) * 8)));
    } else {
        // clear bit
        D_COPY[0][b / (sizeof(byte) * 8)] &=
            (~(byte)(1 << (b % (sizeof(byte) * 8))));
    }

#if (DEBUG)
    print_blocks_status();
#endif
}

static int get_free_descriptor() {
    for (int i = 0; i < descriptors; ++i) {
        if (get_descriptor(i)->file_size == -1) {
            return i;
        }
    }
    return -1;
}

static int get_free_block() {
    unsigned char *bitmap = (unsigned char *)D_COPY[0];
    const int bits = sizeof(unsigned char) * 8;

    // Check the first possible available (the first 1 + DESCRIPTOR_BLOCKS are
    // occupied) unsigned char bitmap
    const int first = (1 + DESCRIPTOR_BLOCKS) / bits;
    if (bitmap[first] != ((unsigned char)~0)) {
        for (unsigned int j = (1 + DESCRIPTOR_BLOCKS) % bits;
             j < sizeof(unsigned char) * 8; ++j) {
            if (bitmap[first] ^ (1 << j)) {
                bitmap[first] |= (1 << j);
                return first * bits + j;
            }
        }
    }

    static_assert(BLOCKS % bits == 0);

    // Check the rest
    for (int i = first + 1; i < BLOCKS / bits; ++i) {
        if (bitmap[i] != ((unsigned char)~0)) {
            for (unsigned int j = 0; j < sizeof(unsigned char) * 8; ++j) {
                if (bitmap[i] ^ (1 << j)) {
                    bitmap[i] |= (1 << j);
                    return i * bits + j;
                }
            }
        }
    }

    return -1;
}

static int get_free_oft() {
    for (int i = 0; i < OFTS; ++i) {
        if (OFT[i].pos == -1) {
            return i;
        }
    }
    return -1;
}

static DESCRIPTOR_T *get_descriptor(int d) {
    int block = d / desc_each_block;
    return ((DESCRIPTOR_T *)D_COPY[block + 1]) + d % desc_each_block;
}
