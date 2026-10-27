#ifndef FLASH_STORE_H
#define FLASH_STORE_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
typedef struct {
    uint8_t *rom;
    uint32_t bytes,chipSize;
    unsigned chips;
    bool (*dirty)(int,int);
    bool (*mark)(int,int);
    int (*offset)(int);
    int (*size)(int);
    int (*block)(int);
    const uint8_t *permissions[2];
} FlashStore;
/* 1 loaded/saved (including a valid legacy empty load),
 * 0 absent/no changes, -1 malformed/I/O/allocation failure. */
int flashStoreLoad(const char *,FlashStore *);
int flashStoreSave(const char *,FlashStore *);
bool flashStoreValidate(FILE *,void *);
extern uint32_t flashStoreWrites,flashStoreUnchanged,flashStoreFailures;
#endif
