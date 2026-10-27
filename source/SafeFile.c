#include "SafeFile.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

/* FAT rename is not power-fail atomic. Keep a validated previous generation
 * and verify the closed temporary file before changing the primary entry. */
static bool suffix(char *out, size_t n, const char *p, const char *s) {
	int k = snprintf(out, n, "%s%s", p, s);
	return k >= 0 && (size_t)k < n;
}

static bool exists(const char *p) {
	struct stat st;
	return stat(p, &st) == 0 || errno != ENOENT;
}

static bool erase(const char *p) {
	return remove(p) == 0 || errno == ENOENT;
}

bool safeFileValid(const char *p, SafeValidate validate, void *ctx) {
	FILE *f = fopen(p, "rb");
	if (!f) return false;
	bool ok = validate(f, ctx);
	if (fclose(f)) ok = false;
	return ok;
}

bool safeFileSelect(const char *p, char *out, size_t n, SafeValidate validate, void *ctx) {
	char bak[512], tmp[512];
	if (!suffix(bak, sizeof(bak), p, ".bak") ||
		!suffix(tmp, sizeof(tmp), p, ".tmp")) return false;
	const char *selected = NULL;
	if (safeFileValid(p, validate, ctx)) selected = p;
	else if (safeFileValid(bak, validate, ctx)) selected = bak;
	else if (safeFileValid(tmp, validate, ctx)) {
		/* Never truncate the sole valid temporary copy. Promote it when
		 * possible; otherwise replacement below stays write-protected. */
		bool clear = !exists(p);
		if (!clear) {
			char bad[512];
			clear = suffix(bad, sizeof(bad), p, ".bad") &&
				!exists(bad) && !rename(p, bad);
		}
		if (clear && !rename(tmp, p)) selected = p;
		else selected = tmp;
	}
	if (!selected || strlen(selected) >= n) return false;
	strcpy(out, selected);
	return true;
}

static int fileRank(const char *p, SafeRank rank, SafeValidate validate, void *ctx) {
	if (!rank) return safeFileValid(p, validate, ctx) ? 1 : -1;
	FILE *f = fopen(p, "rb");
	if (!f) return -1;
	errno = 0;
	int value = rank(f, ctx);
	bool readFailed = ferror(f) || (value < 0 && errno != 0);
	if (fclose(f)) readFailed = true;
	/* Distinguish structurally invalid scratch from unreadable scratch. */
	return readFailed ? -2 : value;
}

bool safeFileReplace(const char *p, SafeWrite write, SafeValidate validate,
		SafeValidate verify, void *ctx) {
	return safeFileReplaceRanked(p, write, validate, verify, NULL, ctx);
}

bool safeFileReplaceRanked(const char *p, SafeWrite write, SafeValidate validate,
		SafeValidate verify, SafeRank rank, void *ctx) {
	char bak[512], tmp[512];
	if (!suffix(bak, sizeof(bak), p, ".bak") ||
		!suffix(tmp, sizeof(tmp), p, ".tmp")) return false;
	int primaryRank = fileRank(p, rank, validate, ctx);
	int backupRank = fileRank(bak, rank, validate, ctx);
	bool primary = primaryRank >= 0, backup = backupRank >= 0;
	/* An empty placeholder cannot justify deleting an unreadable backup.
	 * Archive it instead of rotating it over a backup containing progress.
	 * Classification and validation share one open file. */
	if (primaryRank == 0 && exists(bak) && !backup) return false;
	bool retainBackup = primaryRank == 0 && backupRank > 0;
	if (rank && primaryRank <= 0 && backupRank <= 0 && exists(tmp)) {
		int temporaryRank = fileRank(tmp, rank, validate, ctx);
		/* Empty primary/backup files do not protect the only progress copy.
		 * Retry may reuse malformed scratch, but never valid or unreadable data. */
		if (temporaryRank != -1) return false;
	}
	if (exists(p) && !primary && !backup) return false;
	if (!primary && !backup && safeFileValid(tmp, validate, ctx)) return false;
	FILE *f = fopen(tmp, "wb");
	if (!f) return false;
	bool ok = write(f, ctx);
	if (fflush(f)) ok = false;
	if (fclose(f)) ok = false;
	if (!ok || !safeFileValid(tmp, validate, ctx) ||
		(verify && !safeFileValid(tmp, verify, ctx))) return false;
	if (primary && !retainBackup) {
		if (!erase(bak) || rename(p, bak)) return false;
	}
	else if (exists(p)) {
		char bad[512];
		if (!suffix(bad, sizeof(bad), p, ".bad") ||
			exists(bad) || rename(p, bad)) return false;
	}
	if (rename(tmp, p)) {
		/* This also restores a primary moved to .bak in this transaction. */
		if (!exists(p) && (backup || primary)) rename(bak, p);
		return false;
	}
	return safeFileValid(p, validate, ctx);
}
