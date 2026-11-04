# Report: Notification and child-mode method contracts

## Scope and evidence

Static follow-up of the ten already-registered methods in factory `usr/bin/mico-helper` (SHA-256 `c7b18b8e15914d74cd52daeeaa0dd6590d856176310c0a015a68ced3833e1683`). Entry/owner checkpoints were already satisfied and are not re-proved here. ARM disassembly, raw policy/table bytes and PLT/GOT relocations establish the following request/reply paths. No ROM command, notification text, ubus method, actual notification/configuration file or device operation was executed/read.

## Common reply framing and error dictionary

notify handlers share blob buffer `0x252d0`. Helper `0x1324c` writes a **16-bit** field `code` (BLOBMSG type6) from the low 16 bits of its input. Helper `0x12d94` adds a **string** field; `code_message` and `info` use it. `0x11690` maps a code through the twelve-entry table at `0x25150` (12 passed to map_get_data); the literal messages are:

| Code | Literal message |
| --- | --- |
| 0 | success. |
| 1 | parameter missing. |
| 2 | parameter empty. |
| 3 | parameter format error. |
| 4 | parameter unexpect value. |
| 5 | unkonwn error. |
| 6 | no memory. |
| 7 | file open faild. |
| 8 | message check failed. |
| 9 | message not exist. |
| 10 | message id exists. |
| 11 | no such api. |

Spelling above preserves the binary literals. Handler return zero is distinct from application code, helper success, durable write or audible playback. Setter/helper write returns and several send results are ignored.

## Ten-method ABI matrix

| Method / handler | Request | Application reply and branch contract |
| --- | --- | --- |
| notify.create / `0x138d4` | id STRING; level INT32 required. expire INT32, volume INT32, text STRING optional at handler layer. | Missing id/level code1. Absent optional fields are passed as zero/NULL. Calls constructor `0x120d4(id,level,expire,volume,text)` and replies with its code plus code_message. Thus text absent reaches constructor code4; optional in policy is not unconditional acceptance. |
| notify.query / `0x1362c` | Optional id STRING. | Calls `0x12544(id,&out)` then explicitly replies code0/code_message, regardless of helper result. Nonempty id looks up one node; missing/empty id traverses all four levels. Not-found produces an empty message array, not code9. If output exists, info is a JSON **string** with a `message` array. |
| notify.delete / `0x13424` | Optional id STRING. | Missing/empty id clears all queues via `0x1244c`, code0. Nonempty id calls `0x122f8`: found deletes and returns0; not-found returns9. Replies code/code_message. |
| notify.play / `0x132c8` | No fields. | Calls queue-player `0x11fc0`, updates local indication via `0x12644`, replies helper code/code_message. Nodes are removed immediately after requesting playback; no playback-success retention/retry. |
| notify.stop / `0x13284` | No fields. | Calls `0x12038`, which consists solely of `bx lr`; no stop request or state mutation occurs in that helper. It does not explicitly set r0; response code inherits the value surviving prior blob_buf_init and empty helper. No code_message is added in this handler. |
| notify.clean / `0x13330` | No fields. | Calls all-queue deletion `0x1244c`, updates local indication, replies code/code_message. Deletion helper returns0 after traversal, regardless of unlink success in node cleanup. |
| notify.status / `0x1352c` | Optional level INT32. | For unsigned level0..3 calls `0x12b30` for one level; missing/out-of-range level calls `0x12c34` for all four. Replies helper code/code_message and JSON-string info if present. Single-level info contains config array with level/enable/autoplay; all-level form additionally includes volume. |
| notify.set / `0x13764` | Required level INT32; optional enable/autoplay/volume INT32. | Missing level code1; otherwise replies0/code_message, ignoring per-field helper rejection/write results. Autoplay implementation anomaly detailed below. |
| path_child_mode.set / `0x12dd0` | Required message STRING containing nested JSON with isChildMode. | Nested decoding, write and replies detailed below; does not use notify common INT16 reply helper. |
| path_child_mode.status / `0x13a3c` | No fields. | Returns INT32 code0; info string obtained from a JSON object containing boolean isChildMode. File-open/read failures can be represented as false rather than error. Handler returns ubus_send_reply result, unlike notify handlers' explicit zero. |

## Query and status output shape

Query row serializer `0x1232c` adds id STRING (node+0x14), level INT (byte+0x20), timestamp INT (+8), timestamp_usec INT (+0xc), expire INT (+0x10), and text STRING (+0x18), then appends the row to an array. It does not include the node's stored volume field. The wrapper `0x12544` adds the array under `message`, serializes and strdup's it. Example **shape only**, never requested: `{"code":0,"code_message":"success.","info":"{\"message\":[]}"}`.

Status single-level helper `0x12b30` serializes `{"config":[{"level":n,"enable":n,"autoplay":n}]}`. All-level helper `0x12c34` iterates exactly four 16-byte records from `0x251f0`, additionally adding integer volume. Both return zero after serialization, without complete allocation-result guards. Code0 therefore does not prove the info field is present or that current files were readable; these are in-memory policy responses.

## notify.set: field routing and persistence

Raw policy `0x14684` is four consecutive 8-byte records: level/type5, enable/type5, autoplay/type5, volume/type5. The decoder stores enable in r4 (`0x137d4`); when autoplay exists it parses autoplay into r5 but places **r4** in r1 before calling `0x129d8` (`0x137f8–0x13804`). It repeats the same tb[2] check/parse and helper call again at `0x13808–0x13824`. Thus the parsed autoplay value is not the value passed to its setter. If enable is absent r4 was set to zero, so an autoplay field routes value0; if enable is present its value is passed twice. This is direct register evidence, not a claimed tested device bug.

Helper `0x129d8` writes byte+9 of policy record `0x251f0+16*level`, formats `/data/etc/notify_%d.cfg`, then calls cfg_set_int with literal key `autoplay` (`0x12a68`→`0x1462e`). This closes the field meaning independently of the helper name. Helper `0x12928` similarly writes byte+8/key enable; volume helper `0x12a88` writes word+0xc/key volume. The boolean helpers accept unsigned level<=4 and value<=1, reject others with -1. The status path's public range is only0..3; the level4 write target lies beyond the four records iterated by status. Its surrounding allocation/object boundary is not yet classified, so this is a range mismatch without an asserted memory-corruption exploit.

Volume helper accepts level<=4, replaces unsigned volume>100 with -1 in memory, but passes its original volume argument to cfg_set_int. Therefore in-memory normalization and written integer can differ. Field helpers ignore cfg_set_int return and return zero after the write call. The public handler ignores helper rejection too and forces code0 when level exists; no transactional rollback or explicit commit call is shown in this path.

## Existing execution and cleanup boundaries retained

Create constructor has separate queued and immediate/autoplay paths; volume bounds and absolute Unix-second expire semantics are already traced in the main coverage report. This follow-up does not erase those distinctions. Manual play passes text into `0x13ed4`, whose command path embeds it in a shell command through pipe_exe/popen; the full request handler does not add a visible quote-escaping transform before that path. No payload was constructed or executed. Queue removal/unlink failures do not establish successful playback or recovery.

## Remaining work in this follow-up

Complete child-mode setter branches/write/reply; create constructor's lower-level validation return cases; all-reply allocation/error paths; level4 surrounding object boundary and stop inherited r0 require additional targeted evidence. Runtime authorization, actual delivery/persistence and safe failure recovery remain unverified. No feature-wide satisfaction is claimed by this partial report.

## 2026-11-04 bounded continuation and handoff

The local source image was re-hashed during this pass: `rom/LX06_1.94.13.squashfs`, SHA-256 `317a54909f12b63f22128965bf9005c06435d15f61c4202bb88c4524bf1e2ef4`, matching `rom/SHA256SUMS` and the rootfs extraction report. This confirms which image is present, not that the existing extracted-ELF notes were freshly regenerated from it. The environment has no `unsquashfs` or 7-Zip executable; `bsdtar` did not recognize the SquashFS image. No ROM or factory script was executed, no package/driver installed, and no alternate extraction helper was run. Consequently this continuation does not claim independent instruction-level confirmation beyond the address evidence already recorded above and in `factory-rom-feature-coverage.md`.

The existing evidence supports this bounded child-mode contract, pending a fresh ELF pass:

| Method | Request and observed branches | Reply/persistence boundary established so far |
| --- | --- | --- |
| `path_child_mode.set` (`0x12dd0`) | Required `message` string contains nested JSON; `isChildMode` is read with `json_object_get_boolean`. Missing nested field takes an error branch. True/false selects literal `on`/`off` (`0x14876`/`0x1487a`). | Opens `/data/misc_config/kid_mode` with `O_RDWR|O_CREAT|O_TRUNC` (`0x242`) and mode `0600` (`0x13098–0x130a4`); calls `write` with selected literal (`0x13128+`). Open/write failure branches exist. Exact reply code/shape, JSON parse failure handling, short-write handling, close/fsync/commit behavior and whether downstream services enforce this flag are not established. |
| `path_child_mode.status` (`0x13a3c`) | No request fields. | Emits INT32 `code=0`; `info` is a string serialized from a JSON object containing boolean `isChildMode`. File open/read failure can appear as false rather than a surfaced error. Exact path-read branch details, serialization-allocation failure behavior and ubus reply failure contract remain unverified. |

For notify, the current report already contains the ten-method table (8 `notify` + 2 `path_child_mode`), request policy and reply shape where visible, common error-message map, queue expiry/play boundary and the `notify.set` register anomaly. The key setter finding remains specifically: `0x137f8–0x13804` parses autoplay into `r5` but calls `0x129d8` with `r1=r4` (enable); `0x13808–0x13824` repeats that call with the same wrong argument. `0x129d8` writes the autoplay byte/config key. This is code-path evidence, not a runtime reproduction. The notify configuration helpers target `/data/etc/notify_%d.cfg`; message queue cleanup targets `/data/notify/%d/%ld_%ld`. Existing evidence shows writes/unlinks/sync calls on selected paths, but not atomicity, durable recovery after interruption, or that every success reply corresponds to successful disk state. See the table and sections above plus coverage headings `Player index and child-mode setters`, `Notification expiry...`, and `Notification queue playback and autoplay` for the previously recorded source locations.

### Explicit continuation card

Next static pass needs a SquashFS-capable extractor available without modifying the image, then should re-hash the extracted `usr/bin/mico-helper` and compare it with the previously cited ELF SHA-256 `c7b18b8e15914d74cd52daeeaa0dd6590d856176310c0a015a68ced3833e1683` before relying on prior VMA evidence. Then trace: (1) both child-mode methods through every error and `ubus_send_reply` argument/return; (2) notify.create constructor branches and exact status/config/message file writers/loaders; (3) `notify.stop`'s inherited `r0` at reply; (4) allocation/serialization failures and level 4 record bounds. Keep actual authorization, durability and delivery as open runtime questions unless separate safe evidence exists. No checkpoint is advanced by this report.
