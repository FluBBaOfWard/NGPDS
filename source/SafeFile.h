#ifndef SAFE_FILE_H
#define SAFE_FILE_H
#include <stdio.h>
#include <stdbool.h>

typedef bool (*SafeValidate)(FILE *, void *);
typedef bool (*SafeWrite)(FILE *, void *);
/* Negative = invalid/unreadable; zero = valid placeholder; positive = progress. */
typedef int (*SafeRank)(FILE *, void *);

bool safeFileValid(const char *, SafeValidate, void *);
bool safeFileSelect(const char *, char *, size_t, SafeValidate, void *);
bool safeFileReplace(const char *, SafeWrite, SafeValidate, SafeValidate, void *);
bool safeFileReplaceRanked(const char *, SafeWrite, SafeValidate, SafeValidate, SafeRank, void *);
#endif
