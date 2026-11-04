# Report: Independent factory init review

## Scope and result

Independent read-only source review of `factory-init-static-census.md` and the alarm/media/telemetry/Bluetooth/Mesh claims in `factory-init-lifecycle-contracts.md`. Sources are the local factory extraction at `temp/rootfs-factory-full/`. No extracted script was sourced or executed. No device, network, configuration values, identities, keys, logs, recordings or user databases were accessed. No public sources were needed. Source line references below are relative to that extraction.

The census matches all **55/55** regular files immediately under `etc/init.d`: no missing or extra names, matching SHA-256 values, literal START/STOP/USE_PROCD assignments and lexical declarations. A fresh extracted-tree enumeration also finds zero symlinks. This verifies the defined census denominator, not original ROM link metadata, effective enablement or actual boot order. Function detection remains lexical rather than a shell parser.

The reviewed lifecycle statements are supported at their stated static scope. No whole-feature checkpoint should be promoted by this review. The following refinements make the failure boundaries precise.

## Alarm workday update: failure cleanup versus interruption

`etc/init.d/alarm:25–33` defines sync_failed: remove both fixed temporary workday files, clean JSON state, log and exit 1. The normal explicit rejection paths invoke it for literal Config-not-changed (39–41), nonzero/missing response code (47–50), unchanged version (53–56), and a missing regular temporary file (80–83). Thus it would be incorrect to say all ordinary validation failures leave stale temporary files.

However, `sync` does not first clear the fixed temporary files: it begins reading the version at 37 and querying metadata at 38. Selected items write the same two `/tmp` names at 65–75. There is no signal/exit trap in this script, so a previous invocation interrupted after creating one or both files can leave them. A later response with code 0 and a different version can pass the `-f` checks at 80 if both names exist, even when that later item list did not refresh them. This is a **static candidate scenario**, not evidence of an interruption, stale contents or a real incident.

The already reported weaker path is directly supported: shell output redirection at 68/74 creates/truncates a target before curl completion; curl status and contents are not tested. Lines 87–95 remove old destinations, move the temporary files and write/fsync version and need_update without checking each result. `alarm_log "success"` occurs at 86, before those operations. File existence and this success log therefore do not prove valid or durable replacement.

Helper cross-check: `usr/share/libubox/jshn.sh:226–233` sets the requested destination variable to the selected value/default and returns a presence test; the alarm script ignores the return from its version read. It only tests equality to the old version at 54, not nonempty/new-version schema. The helper does not add download-content or atomic-replacement validation. No remote metadata or workday file contents were fetched/read.

## Mediaplayer migration and country fallback

`etc/init.d/mediaplayer:35–43` confirms both optional legacy copies are followed by unconditional recursive removal of the legacy directory. There is no checked copy/compare/hash/backup step between them. This supports a possible loss of legacy data on copy failure; no actual failure or migrated content was inspected.

A further source-level gap is at 22–34: country fallback checks only `/usr/share/mico/mediaplayer.cfg<suffix>`, then creates both that link and `/usr/share/mico/player.cfg<suffix>`. The second source target is not independently tested. If the first country file exists but the second does not, the second requested link can be dangling. This is a conditional implication, not a claim that any checked-in or device country target is absent. The report should avoid suggesting both sources are verified by the fallback.

## Statpoints handoff

`etc/init.d/statpoints_daemon:11–15` returns when temporary source is absent; otherwise it appends to the persistent file and overwrites the temporary file with a newline regardless of append status. `start_service:18–21` appends an existing persistent file to the temporary file and removes the persistent source regardless of append status. The earlier description as truncating source is acceptable as destruction of prior contents, but the resulting file is not necessarily zero bytes because echo writes a newline. There is no lock/transaction or checked byte-count step in these functions. Actual append failure, concurrent writer effects and loss were not observed.

## Nice values: helper chain confirms the intended parameter values

`etc/init.d/miplay:21–22` calls `procd_set_param nice` followed by `procd_append_param nice -10`; `quickplayer:10–11` uses the same pattern with -20. It would be a false positive to infer that append ignores these values just because nice is a scalar.

The static helper chain is complete for these calls:

1. `lib/functions/procd.sh:201–218` routes set nice to `json_add_int nice "$1"`; the initial no-value call gives an empty typed scalar.
2. `usr/share/libubox/jshn.sh:153–157` stores the int type/value; its json_select at 246–272 accepts only object/array and returns 1 for an int scalar.
3. `_procd_append_param:325–333` handles that selection failure by invoking `_procd_set_param "$type" "$@"` and returning. Thus the second call overwrites nice with its supplied scalar argument, -10 or -20, through the same integer setter.
4. The subsequent `_procd_append_param` case list at 334–345 is not used for this scalar fallback. Respawn arrays use their own append path.

Therefore **the shell helper construction requests nice -10 for mpas and -20 for quickplayer**. This does not prove the service-manager server accepts the payload, applies that scheduling value or that the device currently runs at it. No script or JSON helper was executed.

## Bluetooth and Mesh launch gates

`etc/init.d/bluetooth:57–77` and `bluetooth_ble:20–31` both leave the wait loop when count exceeds 10 and proceed to their launch calls; neither fails startup on a missing bluetoothd after this bounded wait. Bluetooth starts BLE only when `/data/status/config_done` is absent. This confirms the report's wait/gate statements; actual process readiness and BLE availability remain unknown.

`bluetoothd:75–107` selects hardware setup by board IDs, creates/copies configuration and uses an alias in preference to the base name. Debug mode at 15–27 launches bluetoothd outside procd and starts capture helpers; the normal branch submits the daemon through procd. `bluetoothd:110–121` confirms attach helpers are killed and debug capture files removed at stop. This is static lifecycle behavior; source paths and variable names are not actual identity/configuration values.

`mibt_mesh:10–25` confirms rtl-name selection for the three literal Wi-Fi chip strings, the data-package preference, factory-name fallback and ultimate generic factory fallback. The tests are `-f`, not executable, integrity or signature tests. This source alone neither validates the chosen artifact nor proves which branch ran. It does support keeping factory and updated runtime ELF interpretations separate.

## Service wrapper result limitation

`etc/rc.common:104–110` calls start_service and then close_service without preserving/checking the former result. `_procd_ubus_call` at `lib/functions/procd.sh:56–62` invokes ubus and then json_cleanup without preserving ubus status; the public procd wrappers also finish with namespace restoration at 41–47. Consequently a wrapper return or printed command success cannot stand in for actual daemon startup or service-manager acceptance. This reinforces the existing evidence limit rather than changing a launch gate into an authorization claim. These helpers were read only.

## Source hashes

| Source | SHA-256 |
| --- | --- |
| `etc/init.d/alarm` | `f5a82d8d5284014eaa056a8db76e5f55ef511d5278b96375a56306685edb44a8` |
| `etc/init.d/mediaplayer` | `b190336455397795279313f703d7c68bec27da165e081d301f7328a7b0e751c0` |
| `etc/init.d/statpoints_daemon` | `8eb3db2686790e40116f47503aa1150c18da661458a80bce7ca77669f49d9db0` |
| `etc/init.d/bluetooth` | `2d4d07b39c0720b51469a3b80ada30d6c35694f75dbe64c6c4050017d0992673` |
| `etc/init.d/bluetooth_ble` | `ac4c7d79f0009888ee5f0a52dc2e6b2f3b09e859f86c97ea33620a37a53163fb` |
| `etc/init.d/bluetoothd` | `048a74b947386dc352602cb5d89fb1cae6f51a899fe404e11a04b209a9edb847` |
| `etc/init.d/mibt_mesh` | `6a6388c3f5376e0544d2cd3706be4a044c41bc034bda016e0c5b2ee74bc34943` |
| `etc/init.d/miplay` | `788136874ab88811c13e77ccdf42bf6dd48c7ad77d0d78b662e234fe3ca3b525` |
| `etc/init.d/quickplayer` | `92502cdfa42ceac99805a3497a51fd0089cc0aaaa6ed74466da50f7b5e240618` |
| `etc/rc.common` | `bffb7c33efb85ace49c086b74101658b8a1b5c0b0965a3a576c4287156c8fa41` |
| `lib/functions/procd.sh` | `b517fd152f13a6f11861f49a2ec3e38c6be06e599603bed7827b160887e3f450` |
| `usr/share/libubox/jshn.sh` | `9adc78ff3e07c555b430e8fe13c86928de753f21ac1352cacbb1709763bc74dd` |

## Checkpoint recommendation

Attach this report as independent cross-check evidence to the relevant init lifecycle, alarms, player, telemetry and Bluetooth/Mesh facets while keeping them open. The 55-file lexical census is fully checked within its narrow denominator; complete product-feature entrypoints, contracts, side effects, safety/recovery and effective runtime enablement are not proved by it.

## Actual workday need_update consumer (ARM static follow-up)

This section is distinct from the hourly-chime path `0x166a8`/`0x1e91c`. The exact `/data/workday/need_update` literal is at alarmd VMA `0x2204c`, with literal-pool pointers at `0x171d0` (startup fallback) and `0x197dc` (periodic consumer). Raw VMA/file translation uses the first PT_LOAD for code/rodata; BSS globals are not file bytes. Symbol/PLT names below were independently resolved from the three ARM PLT words and dynamic GOT relocations, rather than guessed from addresses.

### Timer registration and polling cadence

Service setup at `0x1b984–0x1b9ac` writes callback `0x19618` to timer object `0x375cc + 0xc` and calls uloop_timeout_set(timer,1000). The callback uses state base `0x37490`. Its word counter at +8 (`0x37498`) is in `.bss` (`0x37440` onward), so the ELF supplies an initial zero under normal ELF zero-initialization; this is not a current device-memory read and does not prove no other writer exists.

At `0x196f4–0x19704`, the counter is incremented; unsigned values through 119 are stored and bypass the marker check. At `0x19708–0x19714`, the next pass stores zero and calls access(marker,0). Only return -1 bypasses reload (`0x19718–0x1971c`). After either path, `0x19750–0x19758` schedules the timer for another 1000 ms. Thus the marker is checked every 120 callback executions on this path, nominally about 120 seconds, **not an exact wall-clock two-minute guarantee**: callback work and event-loop delays can shift elapsed time, and errors/reentrancy have not been exercised.

### Reload, global replacement and marker removal

When access does not return -1, `0x19744` calls helper `0x170f4`. On return, `0x19748` immediately loads the marker path into r0 and `0x1974c` calls PLT `0x120a4` (`remove`). There is **no comparison/branch on the reload return between these instructions**. The remove return is likewise not tested before timer re-arm. This is a removal request regardless of loader success, not proof that the marker actually disappeared.

Reload helper `0x170f4` reads the global object pointer from `0x37560`. It frees first date array if nonnull and writes zero to that object's first pointer (`0x1710c–0x17114`); it frees the second array if nonnull (`0x17118–0x17124`), then frees the outer object (`0x17128–0x1712c`). It tail-branches to loader `0x16d54` at `0x17134`. The global itself is not nulled between free and that call.

Loader `0x16d54` allocates 20 bytes and immediately stores the allocation result into global `0x37560` at `0x16d68`. A null allocation therefore overwrites the global with null and returns -1, rather than retaining the old object as a permanent dangling global. For nonnull allocation it zeroes the object's array/count fields, loads workday first (`0x16dac–0x16dc0`) and holiday second (`0x16de8–0x16e00`) through `0x16b78`. Any nonzero parser return branches to -1 without restoring the old tables. On second-parser failure the new object may retain the first parsed table and zero second fields. Old tables have already been freed. This proves lack of all-or-nothing replacement and old-table rollback in this traced helper; it is **not a verified use-after-free, crash or loss incident**.

The reload result is ignored by the timer before marker removal, so a failed replacement does not preserve the marker as a retry signal in this code path. Actual remove success, later external marker recreation and downstream behavior remain unknown.

### Date-file parser limits

Parser `0x16b78` calls file_linecount at `0x16ba0` and returns -1 for a nonpositive count. It allocates count×16 bytes at `0x16c14–0x16c20`, opens the file with mode r, then loops using feof and __isoc99_fscanf with format `%d-%d-%d` (`0x16c48–0x16c78`; literal `0x21e4f`). The branch rejects only fscanf return -1; it does not require three successfully converted fields. The checks compare the low 16 bits of (scanned year−2000) against 1000 and use only the low byte of scanned month/day before checking 1–12 and 1–31. They are not full-width input range validation; month-specific day validation is absent in this block. Invalid values are logged and the loop continues; accepted records use 16-byte stride and increment the output count. No successful-record capacity comparison against the linecount allocation appears before the store at `0x16d10–0x16d20`. These are parser limitations warranting further review, not proof that actual files trigger them. No workday configuration contents were read.

### Startup path and fallback still to close

The main path calls loader at `0x1230c`; on nonzero it calls fallback `0x1713c` at `0x1233c` and exits(1) if fallback also returns nonzero. The fallback calls pipe_exe with literal `cat /data/workday/version`, opens the same version path with w+, writes an integer derived from the first captured byte, launches literal `/etc/init.d/alarm sync &`, sleeps 3, invokes reload and requests unlink of the marker. This is not an observed network update. Exact version arithmetic and all fallback failure results should remain separately qualified until their call conventions are fully cross-checked. No fallback or ROM command was run.

Relevant PLT/GOT resolutions: `0x11f0c` access; `0x120a4` remove; `0x11e4c` uloop_timeout_set; `0x12284` file_linecount; `0x120ec` malloc; `0x11e7c` fopen; `0x11fc0` feof; `0x11da4` __isoc99_fscanf; `0x11e1c` fclose; `0x11f90` unlink; `0x122b4` pipe_exe; `0x120d4` fprintf; `0x1214c` sleep; `0x11f00` free. Source SHA-256 for alarmd: `3e3e174ed262690c469c4523b6d428887676a328bb249ef471b5e516e8b0f6a7`.

### Startup fallback arithmetic and completion boundary

The exact fallback arithmetic is now closed: `0x17140–0x17154` provides a zero-initialized 4-byte output buffer to pipe_exe with command `cat /data/workday/version`. `0x17160` reads **only its first byte** with ldrb; `0x17174` subtracts 49, and fprintf at `0x1717c` writes that integer using `%d` after opening the version path with w+. This is not a complete decimal-version parse or a checked version decrement for arbitrary multi-digit input. The pipe_exe result is not checked. Actual version data was not read.

The call at `0x1718c` uses pipe_exe with null output/size zero and command `/etc/init.d/alarm sync &`; sleep(3) at `0x17194` precedes reload `0x17198`. There is no completion handshake or retry-loop check for that background update. The fallback records the reload result in r4 at `0x1719c`, unlinks the marker regardless at `0x171a4`, closes its original version FILE and returns r4. A failed fopen branches to the common fclose call with null FILE; this is an unchecked error-path input to the C library, not an observed crash.

Independent helper evidence: library pipe_exe `0x10e34` formats the command with vsnprintf, uses popen at `0x10ee4`, zeroes a nonnull caller output buffer and reads one fgets at `0x10f20`, strips newline/CR and calls pclose at `0x10f5c`. For the second command the shell ampersand backgrounds the init action; waiting for the popen shell is not proof the background update completed. None of these commands were executed in this review.

### Allocation denominator is fgets chunks, not parsed records

The imported file_linecount resolves to `libmico-common.so:0x13108`, symbol size `0x78`. The library opens the target in r mode, repeatedly calls fgets into a 128-byte stack buffer (`0x13150–0x1315c`), increments a count for each nonnull result (`0x1314c`) and closes the file. Long lines can count as multiple chunks; multiple date tokens in one short line still count as one chunk. Thus the name is not a guarantee of one allocation slot per successfully parsed date.

The date parser allocates chunk-count×16 (`0x16c14–0x16c20`), then scanf-parses successive `%d-%d-%d` records and appends them at index×16 without a capacity guard (`0x16d10–0x16d20`). **Static risk inference:** if one fgets chunk contains more than one valid date record separated by scanf-skippable whitespace, the parser can append more records than the allocated slot count. This is a concrete mismatch between two recovered loops, not a runtime overrun test or evidence that real vendor files contain such input. It remains a candidate until ABI/library behavior and an isolated non-device reproduction are separately reviewed. No payload was delivered or parser executed.

Dynamic relocation cross-checks for library helpers: `0xcbf8` fopen, `0xcd3c` fgets, `0xdc84` fclose, `0xe068` vsnprintf, `0xde1c` popen and `0xe3b0` pclose. These agree with the argument setup observed in the disassembly. The independently reviewed library hash is recorded above; no source-version substitution was used.

### Predicate behavior with absent tables or allocation failure

Exact byte inspection confirms the date scan format at `0x21e4f` is `b'%d-%d-%d'` followed immediately by NUL: it contains no literal space or newline delimiter. Allocation reasoning above uses fgets **chunks**, not lines, and no candidate input was generated or run.

Workday predicate `0x16f80` loads global `0x37560` into r8 at `0x16f98`, then calls localtime and selects a weekend/weekday branch. There is no comparison of the global object pointer to null before its field dereference. This is a missing outer-pointer guard in the traced routine; it must not be described as an observed crash or use-after-free.

With a valid allocated outer object and null first table, the weekend branch loads the first pointer at `0x16fd0`, leaves r4 zero when null and returns through the logging path as non-workday. With null second table, the weekday branch at `0x17080–0x1708c` takes `0x17078`, sets r4=1 and returns workday. Consequently, if the first date-file load fails after zeroing the new object, both absent exception tables reduce this predicate to ordinary weekday/weekend classification; if only the holiday-file load fails after successful workday-file load, weekend workday exceptions can remain while weekday holiday exceptions are absent. These are static consequences for those explicit pointer states, not current calendar/alarms observed on the device.

If outer allocation itself returns null, loader `0x16d68` stores null globally and returns -1. The timer still requests marker removal. A later call to the predicate would reach a dereference through that null global absent another successful replacement; the initial startup fallback's `0x170f4` also assumes a nonnull object at its first dereference. The null-state risk is thus separate from the small free-to-malloc interval; no thread, callback interleaving, process termination or recovery has been tested, so no UAF claim is made.

## Remaining work after this review

- Validate parser semantics and the allocation-denominator mismatch through an independently reviewed, isolated harness only if separately authorized; this pass intentionally did not execute factory code or generate malformed input.
- Close all callers of file loaders and any other writers of counter/global state before treating the 120-callback cadence or table states as exhaustive.
- Establish actual native daemon/version parity, service-manager acceptance, marker filesystem behavior and deployment-safe recovery independently; script/ELF findings do not establish those runtime facts.
- Date recurrence scheduling/persistence and hourly chime are separate feature contracts. No old `0x166a8` workday attribution is relied on here.
