# Report: Factory upgrade independent static review

## Scope and evidence boundaries

Independent source/control-flow review of extracted `bin/ota`, `bin/flash.sh`, `bin/boardupgrade.sh`, `bin/set_upgrade_status`, wireless init's OTA action, and ARM ELF `bin/EnterFactory`. Sources are the local factory extraction used by the coverage ledger; device firmware compatibility and current runtime deployment are not established by these files. Required references/prove-it rules were read. No factory script, download, verifier, image extraction, flash, mount, boot environment or partition access was executed. No actual identity, config, key, credential, logs or user data was read. This is not another hash census.

## Corrections and tighter limits for the coverage ledger

1. **flash.sh does not itself stop daemons through its claimed service-preparation hook.** flash.sh:173 calls board_prepare_upgrade; boardupgrade.sh:216–218 calls stop_service; stop_service:17–19 only writes3 to drop_caches. restore_service:21–23 only returns. The `# stop services` comment is not implemented service shutdown. Outer ota can stop named daemons on low-memory preparation, as distinguished below.
2. **Silent OTA cohort compares the date's final decimal digit, not the complete day number.** ota:385 strips SN newlines then takes its final byte. ota:386 takes two trailing bytes of ordinary `date +%d` output (last digit plus newline); command substitution strips the newline. Thus fixed text03\n and13\n both produce3,20\n produces0 and31\n produces1. This was cross-checked using host Python fixed byte strings only; no serial/date/device value was queried.
3. **EnterFactory is fixed boot0→boot1/system1 logic, not a generic alternate-slot algorithm.** Details below specify its hardcoded mtdblock5 and weak ls-output gate. The board-upgrade script separately computes A/B targets from partition names.

## Exact flash call order and return checks

Neither flash.sh nor boardupgrade.sh contains set-e. Explicit guards, rather than arbitrary command success, control progress. flash.sh's top-level failure blocks use return, with no actual shell execution performed to validate the deployed invocation semantics.

| Source | Static operation | Result handling and limit |
| --- | --- | --- |
| flash.sh:148–153 | PID-file/liveness reentry check | Reject path removes the supplied input file; lock acquisition is separate reads/writes, not atomic exclusion. Current PID/state not inspected. |
| flash.sh:155 | Model-specific wake lock | Return unchecked; early parameter/verify failures do not run the normal late unlock path. Model-dependent actual impact unknown. |
| flash.sh:157–169 | Input-file check, empty board parameter hook, miso -v or verifier fallback | Explicit nonzero guards. Failure removes input. Board hook always0; verifier semantics/trust still unresolved. |
| flash.sh:173 | board_prepare_upgrade | Only cache-drop hook, not service stop. Return unchecked. |
| flash.sh:186–187 | Prepare temporary directory, copy/move image, cd | No caller check for remount/rm/mkdir/copy/move/cd success before continuing. Prefix test is first4 characters `/tmp`, not canonical path containment. |
| flash.sh:191–196 | board_system_upgrade | Explicit nonzero check; failure removes temporary staging but shows no image/partition rollback. |
| flash.sh:200–217 | Remove staging; adapter/battery check | Staging removed before late power gate. Failure can return after writes, before slot switch. LX06 adapter branch returns0. |
| flash.sh:219 | upgrade_done_set_flags | Skips switch if do_not_switch_os marker exists; otherwise calls board slot helper. Return unchecked. Marker source/actual value unknown. |
| flash.sh:221–228 | Optional silent flag then wake unlock | Results unchecked. |
| flash.sh:230–244 | Third argument1 skips final reboot; otherwise records reboot status, rotates logs, syncs, signals DHCP client and reboots | This test occurs after board writes and slot-change request. It is not dry-run or image validation mode. Tail command failures do not establish completed reboot. |

Board_system_upgrade:224–263 obtains partition indices by grepping /proc/mtd names and reads boot_part. Only boot0/boot1 are accepted here: boot0 selects boot1/system1, boot1 selects boot0/system0, other value returns1. It optionally extracts version metadata then calls updtb, upboot, upker, upfs_squash in that order with `|| return1`. Those guards act on helper return values; they do not certify every command within each helper.

## Segment writers and BL2/TPL copies

updtb:58–78 tries dtb.img then meson.dtb. If both segment checks fail it returns0, meaning skip. Once a segment is selected, extraction, dd to fixed `/dev/dtb` with bs128K/count1, and rm are unguarded, followed by unconditional return0. Therefore dd/extraction failure is not propagated to board_system_upgrade's `|| return1` guard. A successful helper step cannot be treated as a verified DTB write.

upboot:81–156 returns0 if either required BL2/TPL segment check fails or nandwrite is unavailable. Extract commands are unguarded. For first BL2/TPL copies it checks nandwrite status but not the preceding flash_erase status. Replicated BL2 copies2–8 and TPL copies2–4 are unguarded. After rm it returns0 regardless of those later statuses. The previously reported sixth BL2 mismatch is confirmed literally: erase offset0x140000 at129, nandwrite --start=0x14000 at130. It is a suspected script offset error, not a measured NAND result or recovery failure. Remaining BL2 offsets are0x40000/0x80000/0xc0000/0x100000/0x180000/0x1c0000; later TPL writes are0x200000/0x400000/0x600000. Shared DTB/bootloader/TPL writes are outside the alternate kernel/rootfs pair.

upker/upfs_squash require their respective boot.img/root.squashfs segment and call pipe_upgrade. pipe_upgrade:33–55 first calls `miso -c` and returns the default failure value if that check is nonzero. On a passing check it sends `miso -r -x ... -n` stdout to `mtd write - $dest`; an `|| echo $? >&8` branch sends a nonzero extractor status through fd8, while `8>&1 >&9` routes fd8 into the command-substitution capture and pipeline stdout to the saved original stdout. The `-z "$pipestatus0"` test therefore checks for captured extractor failure text. The second test operand is `$?` immediately after `local pipestatus0=...`; the script intends it to represent the pipeline/mtd status, but this extraction did not establish whether this target's BusyBox `ash` `local` builtin preserves the command-substitution status or returns its own status. Treat detection of `mtd write` failure as **unresolved**, not as proven by this source alone. Even if both statuses propagate as intended, this is not read-back verification. No shell, pipeline, or native utility was run. Restore hook after kernel failure is empty. No rollback, content readback or boot-health check is shown in these scripts.

board_upgrade_done_set_flag:203–214 re-reads boot_part and sets boot1 only for boot0; its else sets boot0. It does not reapply the earlier boot0/boot1 validity guard and unconditionally returns0 after fw_env. Thus slot-change success is not established, nor is stable environment state between target selection and switching.

## OTA caller and scheduled/control entries

The factory root cron line5 runs ota slient each minute during hour03. ota:383–399 uses the final-digit cohort, waits a pseudorandom0..3599 seconds, sets SILENT=1 and invokes query. Multiple cron starts and lock timing were not executed; no claim of serialized jobs follows from the cron line.

ota:164–187 computes required free-memory estimate from declared file size. Under pressure it calls try_release_mem:22–46, which stops only PNS for L09B and PNS/player/MIIO/AIVS/miplay/IDM for other models. It rechecks free memory, logs if still insufficient and returns0 anyway. This is a best-effort memory preparation rather than a hard space acceptance gate. These daemon stops occur before download, distinct from flash.sh's cache-only hook.

ota:135–160 first uses wget, then curl with one of three DNS values on failure. HTTP transport exit is the acceptance criterion there; curl lacks an explicit HTTP-error/fail option. The unused MD5 argument and no-op verify_upgrade are confirmed, while flash.sh later still runs its verifier. No downgrade/crypto bypass is inferred from the missing MD5 comparison alone. `version_check` extracts metadata before the later full flash verifier; only query/silent paths call it, comparing formatted new version >= current. Equal versions are not rejected by version_compare. The version-exemption helper uses an outer model variable while version_check itself declares model local; actual surrounding variable initialization matters and remains unexecuted.

ota:338 passes DNS_SELECT as flash.sh's third argument; flash interprets literal1 as skip-reboot, whereas this script's retry assignments are DNS address strings. This mismatch is confirmed. It does not establish that a supported path actually passes1. set_upgrade_status writes the local status then calls matool_set_device_status, so stage labels can have external effects beyond a local marker; call results are not proof of a device update.

## Version comparison and distinct OTA callers

`ota:197–205` formats exactly the first three dot-separated fields with `awk` as `%d%03d%03d` and compares the resulting decimal-like strings numerically with `-ge`. The fields are minimum-width, not range-checked or strictly validated; extra fields are ignored. Equal formatted values pass. This comparator is only reached through `version_check()` at `ota:322–332`, which is selected for mode `query` or `silent`; it is not a universal gate on all OTA invocations.

`version_check()` at `ota:208–239` selects package metadata by model: `mico_version` for LX06/L06A/L09A/L09B, `version` for LX05A, and `system.cfg` otherwise. The `miso -r -x` result and subsequent extraction/parsing are not explicitly checked before comparing `ver_new` to `micocfg_version`; malformed or absent metadata is not explicitly rejected by a dedicated validation branch. The model exemption check at `ota:190–195` reads `$model`, but `version_check()` declares and sets a local `model` at209–212. The exemption therefore depends on an outer/global value not established at this callsite; no execution was used to resolve shell scoping or caller initialization.

Caller differences visible in `ota:351–379` and `401–423`: `ble` invokes the query mode, `upgrade` invokes app mode, and `test` invokes test mode; only query/silent modes run the version comparison. With an explicit URL, the second formal argument (MD5) is passed through but never consumed, and a missing size is replaced with 30 MiB (or 1 MiB for listed `skr_model_list` models). With no URL, the script calls `matool_check_upgrade`, extracts its first two whitespace-delimited fields as URL and size, then invokes the requested mode; a nonzero matool result takes `no_need` status. All these OTA modes still reach `flash.sh`, whose image-verifier gate is separate. The scheduled `slient` branch at383–398 reaches query-style mode after its cohort delay.

The wireless init action at1403–1423 is a separate downloader: it uses `curl` (with `-k` for HTTPS) to save `/tmp/mico_all.bin`, checks curl's exit status, and directly calls `flash.sh` with no optional arguments. This path does not pass through `ota`'s matool lookup, memory-preparation helper, query/silent version check, adapter/battery gate, OTA status helper, or OTA failure-restart logic. It still passes through `flash.sh`'s own image verifier, board-specific writes, post-write adapter check, slot-switch helper and normal reboot path. The static caller uses `ota_url`; its value and any upstream authorization were not read. This distinction is path ownership, not proof that the wireless action is reachable or usable on a particular device.

## EnterFactory fixed-layout owner chain

ELF startup points to main0x103f4. It calls helper0x104cc, which popens fixed `fw_env -g boot_part`, fgets up to64 bytes and strncmp-compares first5 bytes with boot0. Nonmatch prints a message and returns; no boot1→system0 counterpart is present. Null pipe/read handling is not proved safe by this code.

On a matching boot0 prefix, main calls system at0x10408 with fixed `mkdir -p /tmp/system1;sleep 0.5;mount -t squashfs /dev/mtdblock5 /tmp/system1`. The return is ignored. Helper0x10520 then popens `ls /tmp/system1/etc/factory`, fgets up to64 bytes and compares its first24 bytes to that path literal. This is an output-prefix test, not image-signature validation or robust file-content verification. On match, main calls system at0x1041c with `fw_env -s boot_part boot1;reboot -f`; the semicolon does not guard reboot on environment-write success. Else branches only print no-factory messages. No unmount/readback/rollback is established. The fixed mtdblock5 layout must not be generalized to other images or devices.

MSTC_EnterFactory, reviewed separately, simply requests boot_part boot1 then force reboot; it lacks this boot0/marker/mount check. Neither path is the selective user-data reset or generic overlay reset. Full normal-product callers and authorization remain unresolved.

## Acceptance boundary

These checks correct script ownership, silent cohort and fixed factory-layout claims, and strengthen known source contracts. They do not close image trust-root/format semantics, actual partition mapping, low-level write correctness, all entry authorization, power-loss recovery or bootloader rollback. Keep full firmware_upgrade_factory_slot checkpoints open pending those independent conditions. No device write or flash action is authorized or implied by this static report.
