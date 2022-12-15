#pragma once

#ifndef _FILE_SYSTEM_H_
#define _FILE_SYSTEM_H_


#define ERR_FILE_ALREADY_EXISTS -1
#define ERR_FILE_DOES_NOT_EXIST -2
#define ERR_FILE_ALREADY_OPENED -3
#define ERR_FILE_NOT_OPENED -4
#define ERR_TOO_MANY_FILES -5
#define ERR_NO_FREE_DIR_ENTRY -6
#define ERR_SEEK_OUT_OF_RANGE -7  // current position is past the end of file
#define ERR_PATH_TOO_LONG -8
#define ERR_DISK_IS_FULL -9
#define ERR_TOO_MANY_FILES_OPENED -10

// init()
int FS_init();
int FS_close();

int create(const char *path);
int destroy(const char *path);
int open(const char *path);                             // open()
int close(int fh);                                      // close()
int read(int fh, void *buff, unsigned int len);         // read()
int write(int fh, const void *buff, unsigned int len);  // write()
int seek(int fh, int pos);                              // seek()
int tell(int fh);                                       // ftell()
int eof(int fh);                                        // feof()
int directory();

#endif  //_FILE_SYSTEM_H_
