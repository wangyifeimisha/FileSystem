#include "disk.h"
#include <cstring>

static byte D[BLOCKS][BLOCK_SIZE];

static_assert(sizeof(byte) == 1);
static_assert(BLOCKS <= BLOCK_SIZE * 8);

int init_block(unsigned int b, int val) {
    if (b >= BLOCKS) return -1;
    memset(D[b], val ? -1 : 0, sizeof(byte) * BLOCK_SIZE);
    return 0;
}

int read_block(unsigned int b, byte* I) {
    if (b >= BLOCKS) return -1;
    memcpy(I, D[b], sizeof(byte) * BLOCK_SIZE);
    return 0;
}

int write_block(unsigned int b, const byte* O) {
    if (b >= BLOCKS) return -1;
    memcpy(D[b], O, sizeof(byte) * BLOCK_SIZE);
    return 0;
}
