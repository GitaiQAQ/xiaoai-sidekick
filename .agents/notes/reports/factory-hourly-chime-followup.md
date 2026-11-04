# Report: Hourly-chime request and scheduler contract follow-up

## Evidence and limits

Read-only static review of local factory `usr/bin/alarmd`, SHA-256 `3e3e174ed262690c469c4523b6d428887676a328bb249ef471b5e516e8b0f6a7`. Evidence combines ARM disassembly, dynamic PLT/GOT relocations, raw PT_LOAD mapping, policy/table records and literal decoding. No ROM code, ubus method, device operation, personal configuration, alarm database or audio content was executed/read.

## Correction: chime scheduler, not workday refresh

The previous coverage interpretation of `0x166a8` / caller `0x1e91c` as a workday-data refresh is withdrawn. The containing routine starts at `0x1e8fc`; literal `0x250df` names it `chime_hourly_file_update`, and it writes `/data/chimehourly/hourly`.

The `.data` type descriptors directly identify the records tested by `0x166a8`:

| Descriptor VMA | Name pointer / text | Integer type | Begin callback | End callback |
| --- | --- | --- | --- | --- |
| `0x37420` | `0x25459` / `chimehourly` | 7 | 0 | `0x1cd64` |
| `0x37430` | `0x21526` / `preload_chimehourly` | 8 | 0 | `0x1cc3c` |

These type integers are distinct from the recurrence enum; they must not be conflated with WORKDAY/HOLIDAY. The refresh helper reads record type at +`0x10c`, accepts 7/8, obtains the hour index from +`0x25c`, reads the corresponding byte at passed-state+5+index, stores that value in record+`0x14`, and calls recurrence selector `0x12644` when the byte is zero (`0x166f0–0x1671c`). It then invokes `0x134e4`. Selector reuse does not make the caller a workday update.

## ubus owner and policy

The existing alarm object has a 25-entry method table at VMA `0x232e0`. Raw 24-byte record `0x23448` is `{name=0x240d0/chime_hourly_set, handler=0x1b674, 0, 0, policy=0x232b8, count=2}`. Neighbor `0x23460` maps `chime_hourly_get` to `0x1b24c`, with count zero. The setter's two 8-byte policies are `status`, BLOBMSG_TYPE_STRING (3), and `chime_hourly`, BLOBMSG_TYPE_ARRAY (1). These are direct object-table/handler links; upstream authorization and external reachability are not newly established here.

## Actual setter contract

`0x1b674` parses two fields and calls `str_equal` at PLT `0x11eac` against exact literals `ON` (`0x2627e`) and `OFF` (`0x2535b`). Return zero means equality on these branches.

| Input path | Static result |
| --- | --- |
| Missing/filtered-out status | Explicit reply code 1 at `0x1b6e4`. |
| Status `ON`, missing array | Branch `0x1b730` reaches reply helper with retained `r0=0`: code 0, no schedule mutation. |
| Status `ON`, array blob type not ARRAY | Branch `0x1b744` likewise retains code 0 and performs no schedule mutation. Normal policy filtering may already turn invalid types into missing values. |
| Status `ON`, valid array | `0x1e868` initializes all 24 hour bytes to 1. Each array element with type INT32 (5) and unsigned value <=23 calls `0x1e888(hour,0)`; other element types or out-of-range values are skipped. Negative signed numbers fail the unsigned bound. Duplicates simply write zero again. Empty array leaves all hours at 1. Calls `0x1e8fc` after iteration. |
| Status `OFF` | Calls `0x1ea7c`; no array is required or inspected. |
| Any other status spelling | Explicit reply code 5 at `0x1b834`. |

Thus `chime_hourly` is a list of enabled hour numbers, not a 24-item boolean vector. Example shape inferred directly from policy and decoder: `{"status":"ON","chime_hourly":[8,12,18]}`. This example was not sent to a device.

The reply helper `0x19f54` adds a 16-bit blob field named `code` (literal `0x236c2`); handler `0x1b700` calls ubus_send_reply and then returns ubus handler status zero regardless of the application code. A successful ubus transport return therefore does not establish that the application operation or persistence succeeded.

## State and persistence

Global chime state starts at `0x374f8`: word +0 is manipulated as OFF=1 / ON=0, byte +4 tracks initialized scheduling, and bytes +5..+28 are the 24 per-hour disable flags (enabled=0). OFF helper `0x1ea7c` sets word +0 to 1, calls `0x1673c` to set record+`0x14` to 1 for both type7/type8, then calls status writer `0x1e6f4(1)`. It returns the scheduler helper's result, not the writer result.

ON file update `0x1e8fc` sets word +0 to zero; if byte +4 is already nonzero it passes state to `0x166a8`, otherwise it calls `0x1e7a0` and sets byte +4 to 1. It opens `/data/chimehourly/hourly` in `w+` mode at `0x1e958`; on success it writes 24 lines with format `%s_%d:%d\n`, fixed prefix `ALERT`, hour index 0..23 and corresponding disable byte, then fclose/sync (`0x1e9d8–0x1ea18`). fopen failure logs an error but still reaches status writer `0x1e6f4(0)`. Neither fprintf, fclose nor sync results are checked. The function returns the earlier scheduling result held in r6, not the status-file writer result.

Status writer `0x1e6f4` opens `/data/chimehourly/status` with `w+`, writes `%d`, closes and syncs. It returns -1 for fopen failure and zero after the successful-open path, but does not check fprintf/fclose results. The ON/OFF callers ignore this writer result. No temporary replacement/rename or rollback is shown in these routines, so failed/truncated writes and in-memory/durable disagreement remain possible static failure paths; actual filesystem behavior has not been tested.

## Remaining work

Setter, getter, startup/default parsing, type ownership and persistence-return contracts are now traced below. Exact downstream playback/volume and external-event contracts, authorization, concurrency and recovery remain open. These dimensions prevent feature-wide checkpoint satisfaction.

## Startup and default schedule

Initializer `0x1ec04` ensures the chime directory exists (mkdir argument 504, octal 0770), opens the status file in `r` mode, reads at most eight bytes into a zero-initialized buffer and applies atoi. It normalizes any nonzero parsed integer to word +0=1, zero to 0; inability to open the status file selects 1. It does not validate that status contains only 0/1. Only eight bytes are shown pre-cleared; an exact eight-byte fread can overwrite all of them, so a further terminating byte is not established by this review.

It sets byte +4 to 1 only when access(F_OK) succeeds for both status and hourly files (`0x1eca4–0x1ecc8`), then fills the 24 hour bytes with 1. Thus the byte initially reflects file existence, not proven scheduler validity. When both files exist, it opens hourly for reading; fopen failure returns -1. It reads chunks with fgets size 16, strips a final newline when present, locates a colon, then a first underscore in the nonempty prefix; it parses the suffix following underscore as hour and after colon as disable value with atoi. Negative hours are skipped; `0x1e888` additionally rejects unsigned hours >23 and normalizes nonzero disable values to 1. The parser does not check the prefix is ALERT. The underscore search result is dereferenced without a visible NULL test (`0x1ee94–0x1ee98`), so malformed nonempty colon lines without underscore are a static unsafe-input path; no malformed input was executed.

After hourly read/close/sync it invokes `0x1e7a0` and then explicitly returns zero, ignoring that construction result (`0x1ee80–0x1ee88`). If either file is absent, it defaults to enabled hours 7 through 21 inclusive by calling `0x1e888(hour,0)` in a loop that stops before 22 (`0x1eec0–0x1eed4`), then returns zero. This branch does not call `0x1e7a0`; later ON update can create records when byte +4 is zero. These are static startup branches, not observed current scheduling.

`0x1e7a0` iterates all 24 hours, calling `0x164f0(hour,disable)` with disable=1 when global OFF is nonzero, otherwise the stored hour byte. It stops on the first nonzero construction result and returns that result; it is not limited to constructing only enabled hours. `0x164f0` constructs both the `chimehourly` and `preload_chimehourly` records through common record builder `0x14504`. ON update sets initialized byte +4 even if that first construction returns nonzero; there is no rollback of already constructed earlier hours in the visible caller.

## Getter response contract

`chime_hourly_get` handler `0x1b24c` calls `0x1eaa8` with an output-string pointer, appends the returned application code through `0x19f54`, and, if code is zero and an output string exists, appends it under **string field `info`** via `0x19af8` (BLOBMSG_TYPE_STRING=3). `info` is serialized JSON, not a nested blob object. The handler sends its shared reply buffer and returns handler status zero. Example shape inferred statically: `{"code":0,"info":"{\"status\":\"ON\",\"chime_hourly\":[8,12]}"}`; no request was sent.

`0x1eaa8` delegates to `0x167ac` when initialized byte is nonzero and global status word is zero. That helper serializes status ON/OFF according to the global word. For ON it scans scheduler records and includes hour +0x25c only for type7 records whose +0x14 is zero; it does not include type8 preload records. For OFF it uses the in-memory 24-hour byte array instead, including entries whose disable byte is zero. The OFF branch of this internal helper is not the normal getter route, since 0x1eaa8 delegates to it only for initialized ON. The normal ON getter therefore reflects existing scheduler records rather than merely the requested hour list. No sorting or deduplication is added beyond underlying scan order.

For the other branch (uninitialized or OFF), `0x1eaa8` explicitly serializes status OFF and the zero-valued hour-byte indices in ascending 0..23 order. Both helpers build JSON via json-c and duplicate the serialized string. Allocation/serialization failure paths are not comprehensively guarded: the handler may emit code 0 with no info if no output string was produced. No actual configured schedule was read.

## Type7 playback and type8 preload callbacks

The type7 callback `0x1cd64` looks up an existing cached resource using `0x1dbd0` and applies `0x1f2b8` to the callback argument's hour at +8. The gate returns 1 if global chime OFF word is nonzero, 2 if hour is unsigned >23, otherwise the **separate byte array at global+0x1d+hour**. This is not the setter's disable array at +5. A nonzero gate discards an available cache record via `0x1db4c` and returns zero; a zero return does not prove playback occurred.

When allowed, it builds a notification-tone command for `/usr/share/common_sound/notify.opus` and then either takes a command from cached resource +0x4c or calls resource helper `0x1ef4c`; if the latter produces no URL, it substitutes `/usr/share/sound/hourly_chime.opus` (`0x1cf7c–0x1cf9c`). It serializes the command list through generate_command_list/comandlist_to_str and calls local helper `0x1bad8` with source `chimehourly` and selected volume. `0x1bad8` resolves `mediaplayer`, adds fields `message` (string), `src` (string) and `volume` (integer), and invokes `player_play_resource` with timeout argument 100. No reply callback is supplied in the examined call. Lookup/invoke errors are returned by that helper but ignored by `0x1cd64`, which frees temporary command/resource allocations and returns zero. This proves a request path and cleanup, not audible playback or restoration.

Volume selection at `0x1cf14–0x1cf28` uses `0x161ac()` if positive, otherwise callback argument+4. The helper runs fixed command `ubus -S call mediaplayer get_alarm_volume` through pipe_exe into a 32-byte buffer, parses the result into temporary config at global `0x3745c`, reads integer key `volume` with cfg_get_string_int, and applies an internal lower bound stored at global `0x37448+0x34`; parse/command/empty-result paths select zero. It does not itself read ordinary media volume, write a new volume, or restore a previous volume. The volume field passed to factory player is therefore not evidence that prior volume was restored; that contract remains with the downstream factory-player handler and its completion paths.

Type8 uses generic callback `0x1cc3c`. For callback type8 (also type4/6) it calls `0x1de38`; that helper's type8 branch invokes `0x1ef4c` for a resource URL, creates a TTS/speak command, serializes it and stores a command-list pointer in a cached resource record before linking it into the global cache list (`0x1dfbc–0x1dffc`). Missing URL frees the new record and exits. The generic callback explicitly returns zero regardless of the preload helper's result. Allocation failure guards and complete cache expiry/concurrency are not established here.

`0x1ef4c` executes this fixed command through pipe_exe_multiline: `ubus -t 10 -S call mibrain aivs_event_post '{"namespace":"Alerts","name":"ChimeHourly"}'`. It parses outer JSON, retrieves string `info`, parses that string as JSON, retrieves string `payload`, parses that as JSON, then extracts `url` and duplicates it to the output pointer. Missing parses/fields or shell-helper failure take error paths; fallback in the main callback can still request the local chime sound. This is a static cloud-facing assistant event path; no request, response body or URL was accessed.

The local static setter/getter/startup/persistence and callback-to-player/preload request chains are now traced. The following sections close the playback-gate producer and a bounded factory-player volume change/restore request path. Still open: complete resource cache lifetime, mixer readback, interruption/nesting behavior, external event semantics, authentication, concurrency and actual successful playback/recovery. The broader alarms/chimes checkpoint remains open.

## Playback suppression-mask producer

The second array is initialized to zero by `0x1f1d0` (called during alarm initialization at `0x1638c`). Setter `0x1f1f0(timestamp,op)` calls `0x1c91c`; the latter requires timestamp divisible by 3600, calls localtime_r and returns the local hour, otherwise -1. For hour 0..23, the setter writes bool(op) into global+0x1d+hour. It logs `chime_hourly_allow_update` but does not maintain a per-hour reference count.

The direct forwarding caller is `0x13b58`: it ignores name prefixes `preload_alarm`, `baby_course`, `preload_baby_course`, otherwise passes timestamp/op through to the second-array setter. Call sites include creation `0x14be8` with op1 and close `0x1589c` with op0; update/remove/bootstrap sites also call it. This supports suppression by other exactly-hour-aligned alarm lifecycle events rather than an independently established once-per-hour playback deduplication policy. Multiple alarms at the same hour, DST timestamp changes and order-dependent clearing need separate review; a boolean slot alone does not prove correct overlap handling.

## Factory-player volume change and restoration path

A narrowly scoped downstream review uses extracted `usr/bin/mediaplayer`, SHA-256 `3339a811d8d09c8b5d9ba9f7b6a48a1ae949c76e03b136665e5798c4a12a4a3b`. Its `player_play_resource` handler `0x16ee4` decodes message/src/volume and calls exported `mi_player_resource` `0x27d60`. That routine saves volume at global `0x53c50+0x24`, calls parse_source_player and returns zero without propagating the parser result. The ubus response therefore cannot stand in for successful parse/playback.

The subsequent play_url path uses strncmp to compare the first 11 bytes of source against `chimehourly` (a prefix comparison, not exact equality) at `0x1e1b4–0x1e1c8`. On match it sets player-state byte +0x29c to 1, calls stash_play_volume (`0x1da74`), and, if the stored requested volume is nonzero, calls audio_volume_operation(`default`,`mysoftvol`,1,requested_volume) at `0x1e234`. stash_play_volume allocates/obtains the stash pointer if absent and captures get_audio_volume only when player-state +0x139 is zero. Thus prior nested playback state can alter whether a fresh volume is stashed.

A static completion path exists: handle_play_complete_event calls handle_beep_complete_event at `0x1d3a0`; the latter checks the chime flag at +0x29c, clears it and calls restore_play_volume (`0x1bb70–0x1bbb0`). The error-event path checks/clears the same flag and calls restore_current_player (`0x1d520–0x1d560`), whose non-null-stash path first invokes restore_play_volume at `0x1aef8`. Exact log literals say `restore volume after chimehourly`, independently corroborating the flag's role.

restore_play_volume `0x1aa74` does nothing beyond logging if the stash pointer is absent. Otherwise it loads the stored volume and calls audio_volume_operation(`default`,`mysoftvol`,1,stored_volume) at `0x1aac4`. The helper's return is not checked before later logging; restoration is therefore an attempted static path, not confirmed mixer readback. Wakeup also clears +0x29c at `0x1eebc`, so interruption/nesting paths must not be assumed equivalent to normal completion. No playback, volume mutation, event injection or current player-state read was performed.

### Request-volume to track-field bridge

The request volume saved at global `0x53c50+0x24` is copied into the generated track's +`0x318`: `mi_play_resource` loads its global base through literal `0x27378=0x53c50`, then instructions `0x270f8` / `0x270fc` load global+0x24 and store track+0x318; track pointer is global+0x20. The URL-generation branch independently performs the same copy at `0x274a8` / `0x274b4`. Thus the play_url chime volume branch reads the corresponding request-derived track field rather than an unlinked similarly named value. The chime source condition uses imported strncmp (PLT `0x15434`) length 11, so source strings sharing that prefix also take the branch.

## Bounded resource-cache lifetime

Cache list sentinel `0x374f0` is initialized by `0x1db38`. Preload helper `0x1de38` allocates an 80-byte record, forms its key at record+8 from the callback identifier (subtracting a 12-byte prefix from the source length), stores generated command-list state at +0x4c and links the record into the list on success. Main chime callback queries by callback argument+12 through `0x1dbd0`. Lookup uses strncmp for **strlen(query)** bytes, so it is prefix matching rather than separately checking equal lengths; no exact-key or uniqueness guarantee is established.

Main chime callback removes the selected cache record on both playback-gate rejection and the ordinary cleanup path through `0x1db4c`. That helper frees optional pointer +0x48, unlinks the list node and frees the record. Explicit cleanup `0x1dcd8` also looks up by key, passes command pointer +0x4c to a dedicated imported destructor, and then removes the record; direct callers include modification/close/delete paths at `0x14f60`, `0x1588c`, `0x15d94`. Other behavior callbacks also remove selected entries. The reviewed direct-call inventory has no demonstrated time-to-live timer for this cache; this is not proof that no indirect cleanup exists. Ownership of command-list elements after transfer, duplicate-prefix keys, concurrent mutation, allocator-failure handling and complete event paths remain open.

### Cache ownership: preload → playback → cleanup

The shared-library ownership helpers were additionally decoded in the same factory libmico-common.so used by the native-reset review (SHA-256 `a8fe45c6b7f5a20e348223e3f359265eba86d3520b0cfc5dc0ad1e2d488cf5ae`):

1. Preload creates a command node and passes a temporary stack-list head to generate_command_list (`alarmd 0x1dfdc`). Library `0x217e4` allocates a **96-byte container**, gives it a list sentinel at +0x58, calls command_list_copy (`0x2163c`) on a nonempty source, then command_list_reset (`0x214a0`) on that source. Copy allocates 80 bytes for each command node and duplicates optional +0x48/+0x4c strings; reset unlinks/frees source node strings and nodes, but does not free the caller's list-head container. The resulting 96-byte container is stored in cacheRecord+0x4c.
2. Main chime playback calls helper `0x1e12c(cacheRecord+0x4c)`. This returns the first command-node pointer read at container+0x58, or null if it equals the sentinel. It does **not** free the container or unlink the node. Main callback relinks the returned node into its temporary stack list at `0x1ceb8–0x1cecc`.
3. Main callback invokes generate_command_list at `0x1ced4`, deep-copying that temporary list into a new container and resetting/freeing its source nodes. The cached container itself is not passed as the source head: the source head is the callback's stack address. The old cached container therefore is not freed by command_list_reset in this path.
4. Main callback calls comandlist_to_str at `0x1ced8`. Library serializer explicitly calls command_list_free on **its input new container** at `0x21e0c–0x21e10` (PLT `0xe200`, independently matched through GOT relocation), then releases JSON intermediates and returns the duplicated serialized string. The temporary new container/nodes are therefore destroyed after serialization. Main callback frees the serialized string at `0x1cf50`.
5. Main callback ends with `0x1ce08 → 0x1db4c(cacheRecord)`. That helper frees only cacheRecord+0x48 if present, unlinks and frees the cache record; it neither reads nor destroys +0x4c. The original cached 96-byte command-list container has no matched release in this consumed-preload path. On a rejected-playback gate, it takes the same cache-record cleanup without first consuming its command nodes, so the cached container and its owned nodes likewise lack a matched release in that path.
6. The distinct explicit cleanup `0x1dcd8` calls imported command_list_free on cacheRecord+0x4c at `0x1dce8–0x1dcec`, then removes the cache record. Library destructor `0x2186c` walks/frees node optional strings, nodes, and finally container itself. This path has a matching release.

This is a static unmatched-allocation **candidate**, not a measured heap leak, crash or device-memory result. Alternate interposed helper implementations, untraced aliases and runtime execution have not been tested. The ownership conclusion applies to the named local factory helper implementations and traced normal/rejected main-callback branches; it is not a universal statement about every cache consumer.

Lookup's strlen(query) is computed once at `0x1dc1c` and held in r9. Subsequent iterations jump `0x1dc78 → 0x1dc28` and reuse the same length, confirming the prefix-match description rather than an accidental second-iteration length from another register.

### Suppression-mask temporal scope

The mask stores a boolean indexed only by local hour 0..23, with no date carried in the slot. Direct reset `0x1f1d0` is called from initialization `0x1638c`; the reviewed direct call inventory does not establish a daily reset. Creation/update writes op1 (`0x14bdc`, `0x159fc`), close/delete writes op0 (`0x15890`, `0x15d78`), and expiry/bootstrap paths also update it. Since there is no per-hour count or date key in the demonstrated setter, removing one same-hour alarm can clear a slot independently of other same-hour records, and alarms on different dates share the same index. These are code-structure limitations, not a reproduced cross-day or overlapping-alarm failure. A complete invariant maintained by other scheduler paths has not been established.

## Final bounded closure and remaining evidence

Static setter/getter/startup/persistence, type7/type8 ownership, primary playback/preload requests, request-volume→track bridge, normal/error volume-restoration attempt, hour suppression-mask writes and cache ownership branches are now documented with addresses. No live acceptance or broad alarms feature completion is claimed.

Remaining evidence gaps are: actual playback and mixer readback; interrupted/nested sessions and wakeup clearing the chime flag; complete authorization and external assistant-event behavior; runtime cache allocation growth or alternate ownership paths; overlapping/cross-date suppression invariants and DST behavior; factory-version equivalence with active device binaries; and recovery after partial file writes or allocation failures. These require additional safe evidence and keep feature-wide prove-it facets open. They must not cause previously traced setter/getter/startup work to be repeated.
