# Cartridge and configuration persistence

New installations enable Flash loading/saving and configuration saving; state autoload stays off. Existing valid 792-byte configuration preferences, including Off, are retained. Expose Autosave Flash in Settings. BIOS/controller changes are detected by actual serialized bytes. Save Settings remains available when automatic saving is off; switching Autosave Settings off persists that selection.

A Flash file with bytes `53 00 00 00 08 00 00 00` is a valid old no-op, not corruption. It must have the exact header length and EOF. Prefer validated nonempty primary/backup/temporary progress over empty placeholders. An unreadable or damaged possible progress file cannot silently become an empty fallback. A valid empty primary cannot rotate over a progress-bearing backup; retain the backup and archive the placeholder as .bad (the suffix here does not mean the old empty record is malformed). Protect a sole valid or unreadable .tmp generation even if an empty .fla/.bak exists. Malformed leftover scratch remains reusable.

Validate all nonempty block addresses, permissions, exact block sizes, duplicates, declared file length and EOF before applying staged bytes. Late read/close or allocation failures do not partially overwrite cartridge RAM. A failed load blocks execution and writeback. Dirty flags represent changes from the original ROM and survive disk saves/same-cartridge resets. No dirty blocks or identical serialized content skips writes. No cartridge creates no Flash file. Failed autosave prevents switch/eject/supported exit.

Write .tmp, check write/flush/close, reopen and compare expected bytes, then retain a validated old generation and replace the primary. FAT rename is not power-loss atomic; do not power off during saving, and keep independent backups. No Flash/state/config format change. Flash load temporarily allocates the serialized save length; allocation failure is safe but may limit large saves in NTR mode.

Storage chooses existing settings/recovery in /data/ngpds, then /ngpds; otherwise an existing data folder or /ngpds. Freeze the ROM basename independently of browsing. When the data folder has no file/recovery, import legacy adjacent Flash/state files read-only. Same-basename ROMs still collide; use distinct names. Unknown damaged files are not automatically deleted. Old raw state files have no checksum; same-length arbitrary bit corruption remains outside structural checks.

Portable synthetic tests, from repository root:

```sh
clang -O1 -g -fsanitize=address,undefined tests/test_legacy_flash.c -o /tmp/ngpds-flash
/tmp/ngpds-flash
python3 tests/test_config.py
```

The legacy suite also executes the original persistence fault suite: 72 save schedules (26 reached), 60 load schedules; plus exact zero-block/13 malformed boundaries, 192 ranked-save fault schedules (45 reached), sole-temporary promotion/read failures and late config mutations. Actual loadSettings bodies are compiled with filesystem-only stubs, not mirrored logic. Integration runs reuse legally supplied private inputs and are summarized separately. No proprietary fixtures are distributed. Historical NDSi/R4 graphics performance evidence remains valid; no new hardware save result is claimed and no additional user hardware test is required for this review.
