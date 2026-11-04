# Report: Other reset and factory entrypoints

## Scope

Static local review of extracted firstboot, jffs2reset, libfstools.so and MSTC_EnterFactory. Host objdump/raw ELF words were inspected; none of these programs/scripts was executed. No actual partition, mount, board-ID, environment, user database or credentials were read. Factory mode, overlay reset, selective data cleanup and unbind wipe are separate operations.

## Factory slot selection script

`bin/MSTC_EnterFactory` is a shell script, not a ubus handler or ELF. Its complete active sequence is fw_env setting boot_part to literal boot1, sleep1, reboot-f, followed by an echo marker if execution returns. It contains no explicit data deletion, partition formatting, image verification or backup path. Script name and echo marker do not establish successful factory-mode entry. No normal product caller or hardware trigger is established; it is absent from miio_helper's58 native method names. Treat it as a static boot-slot-switch maintenance candidate, not demonstrated user-data reset.

## Generic firstboot and jffs2reset entry

`sbin/firstboot` invokes `/sbin/jffs2reset $@`. Argument forwarding is unquoted in the source. The ELF's libc startup literal0x109a8 points to main0x107d0. getopt at0x107f4 uses literal yr: y sets the confirmation-bypass variable, r sets the reboot variable. Without y, main logs a warning and requires getchar to return lowercase y. No confirmation was sent.

The next gate calls find_filesystem with literal overlay; nonzero is the logged failure branch. No effective supported-filesystem state was read. The program calls volume_find with literal rootfs_data at0x1086c. A null volume fails; otherwise an optional driver callback is invoked through volume->driver+0x10. Thus it does not directly choose fixed mtd6 or the vendor /data partition.

At0x10898 it compares argv0 against literal jffs2mark. Exact equality takes the raw-marker helper0x109f4; extraction symlink/argv0 alias metadata is missing, so an actual alias cannot be assumed. Other names look up a mount point for volume's device path at+8 via find_mount_point(device,1).

## Mounted and unmounted branches

When a mount point is found, main first calls fs_state_set(`/overlay`,1) at0x108d8 (PLT0x10710). It then passes mountpoint and flag0 to overlay_delete at0x108e4 (PLT0x1077c), followed by mount with the found mountpoint as source, `/` target, null filesystem, raw flags32 and null data at0x108fc. These results are not checked before the next branch. If r was supplied, it calls sync, sleep2 and reboot with raw command0x01234567, then loops. Without r it returns0 on this path. This is not a verified successful cleanup/remount.

When a mount point is absent, main tail-calls helper0x109f4 with the device-path field. That helper calls open(device,O_WRONLY) and compares its return to zero at0x10a24. The branch is BNE to the write path: descriptor0 is incorrectly treated as error here, while a conventional failure value-1 is nonzero and also enters write. It then writes four bytes containing raw word0xdec0adde, closes the descriptor and returns0 only when write returned4. It does not itself erase the full volume. Its error path logs and returns-1. The r flag is not passed to this helper or tested after its tail-call; this branch does not itself issue the mounted-branch reboot sequence. Later boot interpretation of the marker and actual recoverability remain unverified.

## Volume provider and deletion ownership

The linked libfstools.so exports volume_register_driver0x33c8 and volume_find0x33e8. The latter traverses a registered linked list and calls each driver's find pointer at+0x18 until one returns a volume. Init-array entries0x17dc/0x17ec/0x17fc register driver records0x16154/0x16184/0x161b4. Their names are mtd/ubi/rootdisk and find pointers0x392c/0x416c/0x46b4. These are provider choices, not proof of which backend claims rootfs_data on LX06. Runtime partition names/topology were not inspected.

The exported overlay_delete0x2bf8 records its second argument in a byte and delegates to foreachdir0x2ae4 with callback0x28fc. foreachdir uses bounded path construction/glob traversal, a mode query and recursion excluding symlink mode. Callback0x28fc opens each directory, uses dirfd/readdir and an __fxstatat wrapper with rawflags256, skips directory-mode entries, and unlinks other entries with unlinkat. It preserves exact filename sysupgrade.tgz only when the recorded second-argument byte is nonzero. **This jffs2reset caller passes0, so it does not activate that preservation exception.** After scanning, the callback calls rmdir but then returns the prior readdir null value, not rmdir status; unlinkat results are not checked. These unchecked results preclude inferring completed deletion from helper return. Thus overlay_delete is a filesystem traversal path, while the raw marker branch is a separate mechanism. Neither is the selective native user_data_clean, the broad unbind script nor the MSTC slot switch.

## Remaining boundaries

The firstboot wrapper and generic reset-button script supply command entry paths, but LX06 hardware event mapping remains open. Missing symlink metadata obscures jffs2mark alias use. Exact volume provider selected, marker-consumption lifecycle, deletion preservation rules, effective mounts, power-loss behavior, backup and recovery remain unverified. No complete reset feature facet is promoted by these static subchains.

SHA-256 `sbin/firstboot`: `6cf5041c8b9e525a4e4344a55fe87db2765505e36b7ba2c63fdf15b184974e5c`.

SHA-256 `sbin/jffs2reset`: `b99421355c5a8f74085b2927ed5968a11763388553624d5e9872b1c45fa50989`.

SHA-256 `lib/libfstools.so`: `0c33e881fbbdad16fdc9f700e685d9700590ef1ecf622f517c6dbfec27700f26`.

SHA-256 `bin/MSTC_EnterFactory`: `74ae6c5666b181e1755474f7357a8d03c6db475b8cef999da10026a18e0a8794`.

## Independent review correction

The main-agent review caught and this report corrects two draft decoding errors: PLT0x10710 is fs_state_set and0x1077c is overlay_delete, so the latter receives flag0 rather than1; the raw-marker helper's open test is zero/nonzero rather than a negative-error test. A complete program PLT/GOT map was independently regenerated from ARM words and relocation records after the correction. Prior interpretations of preservation or ordinary open-error gating must not be reused.
