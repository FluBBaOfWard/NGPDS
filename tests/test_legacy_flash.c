/* Synthetic data only. Run after the existing persistence suite in the same TU. */
#define main previousPersistenceSuite
#include "test_persistence.c"
#undef main
static const uint8_t emptyRecord[8] = {0x53, 0, 0, 0, 8, 0, 0, 0};
static void put(const char *name, const void *bytes, size_t length) {
	FILE *f = fopen(name, "wb");
	assert(f && fwrite(bytes, 1, length, f) == length && !fclose(f));
}
static void emptyWithBackup(void) {
	fixture();
	put("game.fla.bak", saved, sizeof(saved));
	put("game.fla", emptyRecord, sizeof(emptyRecord));
}
int main(void) {
	assert(!previousPersistenceSuite());
	char dir[] = "/tmp/ngpds-legacy-XXXXXX";
	assert(mkdtemp(dir) && !chdir(dir));
	for (unsigned i = 6; i < 35; i++) perms[i] = 2;
	clean();
	put("game.fla", emptyRecord, 8);
	assert(safeFileValid("game.fla", flashStoreValidate, &ctx));
	memset(data, 0xa7, sizeof(data));
	memset(dirty, 0, sizeof(dirty));
	uint8_t before[sizeof(data)];
	memcpy(before, data, sizeof(data));
	assert(flashStoreLoad("game.fla", &ctx) == 1);
	assert(!memcmp(before, data, sizeof(data)) && !current(&ctx).count);
	unsigned writes = flashStoreWrites;
	assert(!flashStoreSave("game.fla", &ctx) && writes == flashStoreWrites);
	assert(equalFile("game.fla", emptyRecord, 8));
	/* A new native save may replace the valid legacy no-op. */
	dirty[6] = true;
	data[offFn(6)] ^= 1;
	assert(flashStoreSave("game.fla", &ctx) == 1);
	assert(equalFile("game.fla.bak", emptyRecord, 8));
	memset(data, 0, sizeof(data));
	assert(flashStoreLoad("game.fla", &ctx) == 1 && data[offFn(6)] == (0xa7 ^ 1));
	/* Empty primary must not win over progress or rotate over that backup. */
	emptyWithBackup();
	memset(data, 0, sizeof(data)); memset(dirty, 0, sizeof(dirty));
	assert(flashStoreLoad("game.fla", &ctx) == 1);
	assert(!memcmp(data + offFn(6), saved + 16, 8192));
	assert(flashStoreSave("game.fla", &ctx) == 1);
	assert(equalFile("game.fla", saved, sizeof(saved)));
	assert(equalFile("game.fla.bak", saved, sizeof(saved)));
	assert(equalFile("game.fla.bad", emptyRecord, 8));
	writes = flashStoreWrites;
	assert(!flashStoreSave("game.fla", &ctx) && writes == flashStoreWrites);
	/* Preserve existing progress through every reached fault point. */
	unsigned schedules = 0, triggered = 0;
	for (int kind = 1; kind <= 6; kind++) for (int at = 1; at <= 32; at++) {
		emptyWithBackup();
		memset(data, 0, sizeof(data)); memset(dirty, 0, sizeof(dirty));
		assert(flashStoreLoad("game.fla", &ctx) == 1);
		data[offFn(6) + 10] ^= 1;
		faultKind = kind; faultAt = at; calls = hits = 0;
		int status = flashStoreSave("game.fla", &ctx);
		triggered += hits != 0; faultKind = 0;
		assert(equalFile("game.fla.bak", saved, sizeof(saved)) ||
		       equalFile("game.fla", saved, sizeof(saved)));
		if (status > 0) assert(safeFileValid("game.fla", matches, &ctx));
		schedules++;
	}
	/* A sole valid nonempty temporary file also outranks an empty primary. */
	fixture(); put("game.fla.tmp", saved, sizeof(saved));
	put("game.fla", emptyRecord, 8); remove("game.fla.bak");
	assert(flashStoreLoad("game.fla", &ctx) == 1);
	assert(equalFile("game.fla", saved, sizeof(saved)) || equalFile("game.fla.tmp", saved, sizeof(saved)));
	/* Failed promotion must not let a later write truncate sole progress in .tmp,
	 * even when an empty .bak makes the generic existence check look safe. */
	for (int renameAt = 1; renameAt <= 2; renameAt++) {
		emptyWithBackup(); put("game.fla.tmp", saved, sizeof(saved));
		put("game.fla.bak", emptyRecord, 8);
		faultKind = 4; faultAt = renameAt; calls = hits = 0;
		assert(flashStoreLoad("game.fla", &ctx) == 1);
		faultKind = 0; data[offFn(6) + 11] ^= 1;
		assert(flashStoreSave("game.fla", &ctx) == -1);
		assert(equalFile("game.fla.tmp", saved, sizeof(saved)));
	}
	/* A temporary-file read failure is not proof it is disposable scratch. */
	for (int at = 1; at <= 30; at++) {
		emptyWithBackup(); put("game.fla.tmp", saved, sizeof(saved));
		put("game.fla.bak", emptyRecord, 8); dirty[6] = true;
		faultKind = 6; faultAt = at; calls = hits = 0;
		assert(flashStoreSave("game.fla", &ctx) == -1);
		faultKind = 0;
		assert(equalFile("game.fla.tmp", saved, sizeof(saved)));
	}
	/* Transient failure to read the only progress backup cannot become a
	 * successful empty fallback. No cartridge bytes may change on failure. */
	emptyWithBackup(); memset(data, 0xa7, sizeof(data)); memcpy(before, data, sizeof(data));
	faultKind = 6; faultAt = 3; calls = hits = 0;
	assert(flashStoreLoad("game.fla", &ctx) == -1);
	faultKind = 0;
	assert(!memcmp(before, data, sizeof(data)) && equalFile("game.fla.bak", saved, sizeof(saved)));
	/* Known malformed leftover scratch remains reusable after a failed first save. */
	clean(); put("game.fla", emptyRecord, 8); put("game.fla.tmp", emptyRecord, 7);
	dirty[6] = true; assert(flashStoreSave("game.fla", &ctx) == 1);
	/* Unknown backup content is retained, not deleted because primary is empty. */
	clean(); put("game.fla", emptyRecord, 8);
	const uint8_t unknown[] = {0x53, 0, 1, 0, 0xff, 0xff, 0xff, 0xff};
	put("game.fla.bak", unknown, sizeof(unknown)); dirty[6] = true;
	assert(flashStoreLoad("game.fla", &ctx) == -1);
	assert(flashStoreSave("game.fla", &ctx) == -1);
	assert(equalFile("game.fla.bak", unknown, sizeof(unknown)));
	assert(equalFile("game.fla", emptyRecord, 8));
	/* Exact boundary validation: all truncated headers and invalid empty shapes. */
	unsigned malformed = 0;
	for (unsigned n = 0; n < 8; n++) {
		clean(); put("game.fla", emptyRecord, n);
		assert(flashStoreLoad("game.fla", &ctx) == -1); malformed++;
	}
	for (unsigned mutation = 0; mutation < 5; mutation++) {
		clean(); uint8_t bytes[9] = {0x53, 0, 0, 0, 8, 0, 0, 0, 0};
		if (mutation == 0) bytes[0] = 0x52;
		if (mutation == 1) bytes[4] = 7;
		if (mutation == 2) bytes[4] = 9;
		if (mutation == 3) bytes[2] = 1;
		put("game.fla", bytes, mutation == 4 ? 9 : 8);
		memcpy(before, data, sizeof(data));
		assert(flashStoreLoad("game.fla", &ctx) == -1);
		assert(!memcmp(before, data, sizeof(data))); malformed++;
	}
	/* A valid nonempty primary still wins over an older backup. */
	fixture(); uint8_t expected[sizeof(saved)];
	FILE *f = fopen("game.fla", "rb"); assert(f && fread(expected, 1, sizeof(expected), f) == sizeof(expected)); fclose(f);
	memset(data, 0, sizeof(data)); assert(flashStoreLoad("game.fla", &ctx) == 1);
	assert(!memcmp(data + offFn(6), expected + 16, 8192));
	clean(); chdir("/"); assert(!rmdir(dir));
	printf("PASS legacy empty: exact no-op, new-save upgrade, nonempty backup/temp priority, backup preservation, unchanged skip, %u malformed boundaries; %u ranked-save fault schedules (%u reached)\n", malformed, schedules, triggered);
}
