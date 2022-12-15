#pragma once

#ifndef _DISK_H_
#define _DISK_H_

#define BLOCKS 64       // number of blocks
#define BLOCK_SIZE 512  // block size

typedef unsigned char byte;

int init_block(unsigned int b, int val);
int read_block(unsigned int b, byte* I);
int write_block(unsigned int b, const byte* O);

#endif  //_DISK_H_
