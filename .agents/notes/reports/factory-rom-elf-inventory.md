# Factory ROM ELF dependency inventory

Generated from the local extracted factory ROM by `tools/rom-elf-inventory.py`.
This is a static artifact census, not a runtime loader or product-feature completion report.

Regenerate: `python3 tools/rom-elf-inventory.py temp/rootfs-factory-full --format markdown --output .agents/notes/reports/factory-rom-elf-inventory.md`

Scope: top-level bin/sbin/usr/bin/usr/sbin plus recursive lib/usr/lib.

Limits: Static SONAME/basename candidates only; no loader search simulation, symlink recovery, dlopen or execution proof.

ELF files: 427. Parse errors: 0. DT_NEEDED entries: 1842.

## Dependency names without an extracted provider

- `libfdk-aac.so.1`: `usr/bin/mpap`

A missing provider can reflect absent alias metadata or a separately supplied object; it does not prove runtime failure. A candidate provider is matched by basename or SONAME without resolving actual loader paths, architecture compatibility or symbol versions.

## Per-file metadata

| Path | ELF type / architecture | Entry | SONAME | Interpreter | DT_NEEDED |
| --- | --- | --- | --- | --- | --- |
| `bin/EnterFactory` | ET_EXEC / ARM 32 | 0x1044c | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `bin/busybox` | ET_EXEC / ARM 32 | 0x17bf0 | — | /lib/ld-linux-armhf.so.3 | `libcrypt.so.1`, `libm.so.6`, `libpam.so.0`, `libpam_misc.so.0`, `libresolv.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/ledserver` | ET_EXEC / ARM 32 | 0x11640 | — | /lib/ld-linux-armhf.so.3 | `libjson-c.so.2`, `libubus.so`, `libubox.so`, `libpthread.so.0`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/miso` | ET_EXEC / ARM 32 | 0x112d8 | — | /lib/ld-linux-armhf.so.3 | `libcrypto.so.1.0.0`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/procps-ng-kill` | ET_EXEC / ARM 32 | 0x11e40 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/procps-ng-w` | ET_EXEC / ARM 32 | 0x11264 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/procps-ng-watch` | ET_EXEC / ARM 32 | 0x12118 | — | /lib/ld-linux-armhf.so.3 | `libncursesw.so.6`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/touchpad` | ET_EXEC / ARM 32 | 0x11ad0 | — | /lib/ld-linux-armhf.so.3 | `libjson-c.so.2`, `libubus.so`, `libubox.so`, `libpthread.so.0`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/ubus` | ET_EXEC / ARM 32 | 0x10f3c | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/uclient-fetch` | ET_EXEC / ARM 32 | 0x117ac | — | /lib/ld-linux-armhf.so.3 | `libuclient.so`, `libubox.so`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `bin/wl` | ET_EXEC / ARM 32 | 0x8100 | — | — | — |
| `lib/ld-2.25.so` | ET_DYN / ARM 32 | 0xa80 | ld-linux-armhf.so.3 | — | — |
| `lib/libanl-2.25.so` | ET_DYN / ARM 32 | 0x948 | libanl.so.1 | — | `libpthread.so.0`, `libc.so.6` |
| `lib/libblobmsg_json.so` | ET_DYN / ARM 32 | 0xa48 | libblobmsg_json.so | — | `libubox.so`, `libjson-c.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/libc-2.25.so` | ET_DYN / ARM 32 | 0x169ec | libc.so.6 | /lib/ld-linux-armhf.so.3 | `ld-linux-armhf.so.3` |
| `lib/libcidn-2.25.so` | ET_DYN / ARM 32 | 0xa28 | libcidn.so.1 | — | `libc.so.6` |
| `lib/libcrypt-2.25.so` | ET_DYN / ARM 32 | 0x7a0 | libcrypt.so.1 | — | `libc.so.6` |
| `lib/libdl-2.25.so` | ET_DYN / ARM 32 | 0x900 | libdl.so.2 | — | `libc.so.6`, `ld-linux-armhf.so.3` |
| `lib/libfstools.so` | ET_DYN / ARM 32 | 0x17dc | libfstools.so | — | `librt.so.1`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/libgcc_s.so.1` | ET_DYN / ARM 32 | 0x3b60 | libgcc_s.so.1 | — | `libc.so.6` |
| `lib/libjson_script.so` | ET_DYN / ARM 32 | 0xab4 | libjson_script.so | — | `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/libm-2.25.so` | ET_DYN / ARM 32 | 0x4528 | libm.so.6 | — | `libc.so.6` |
| `lib/libnsl-2.25.so` | ET_DYN / ARM 32 | 0x2e40 | libnsl.so.1 | — | `libc.so.6` |
| `lib/libnss_dns-2.25.so` | ET_DYN / ARM 32 | 0x99c | libnss_dns.so.2 | — | `libresolv.so.2`, `libc.so.6` |
| `lib/libnss_files-2.25.so` | ET_DYN / ARM 32 | 0x18d4 | libnss_files.so.2 | — | `libc.so.6` |
| `lib/libpam.so.0.84.1` | ET_DYN / ARM 32 | 0x1bf0 | libpam.so.0 | — | `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/libpam_misc.so.0.82.1` | ET_DYN / ARM 32 | 0xa6c | libpam_misc.so.0 | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/libpamc.so.0.82.1` | ET_DYN / ARM 32 | 0x91c | libpamc.so.0 | — | `libc.so.6` |
| `lib/libpthread-2.25.so` | ET_DYN / ARM 32 | 0x4790 | libpthread.so.0 | /lib/ld-linux-armhf.so.3 | `libc.so.6`, `ld-linux-armhf.so.3` |
| `lib/libresolv-2.25.so` | ET_DYN / ARM 32 | 0x2214 | libresolv.so.2 | — | `libc.so.6` |
| `lib/librt-2.25.so` | ET_DYN / ARM 32 | 0x16a0 | librt.so.1 | — | `libpthread.so.0`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `lib/libsetlbf.so` | ET_DYN / ARM 32 | 0x3ac | libsetlbf.so | — | `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/libssp.so.0.0.0` | ET_DYN / ARM 32 | 0x820 | libssp.so.0 | — | `libgcc_s.so.1`, `libc.so.6` |
| `lib/libubox.so` | ET_DYN / ARM 32 | 0x21e8 | libubox.so | — | `libgcc_s.so.1`, `libc.so.6` |
| `lib/libubus.so` | ET_DYN / ARM 32 | 0x129c | libubus.so | — | `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/libuci.so` | ET_DYN / ARM 32 | 0x153c | libuci.so | — | `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/libutil-2.25.so` | ET_DYN / ARM 32 | 0x958 | libutil.so.1 | — | `libc.so.6` |
| `lib/libvalidate.so` | ET_DYN / ARM 32 | 0xc00 | libvalidate.so | — | `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/modules/4.9.61/8821cs.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/auxin.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/aw20054.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/bt8xxx.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/btintel.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/btusb.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/cfg80211.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/compat.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/et6037.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/gpio-ir-rx.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/gpio-ir-tx.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/hci_uart.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/hci_uart_8987.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/ip6_tables.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/ip6t_REJECT.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/ip6table_filter.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/ip6table_mangle.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/ip_tables.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/ipt_REJECT.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/iptable_filter.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/iptable_mangle.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/miso.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/mlan.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/mlan_8987.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_conntrack.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_conntrack_ipv4.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_conntrack_ipv6.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_conntrack_rtcache.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_defrag_ipv4.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_defrag_ipv6.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_log_common.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_log_ipv4.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_log_ipv6.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_reject_ipv4.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/nf_reject_ipv6.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/rtk_btuart.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/sd8987.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/sd8xxx.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/snd-compress.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/snd-mixer-oss.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/snd-pcm-oss.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/uinput.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/x_tables.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_LOG.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_TCPMSS.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_comment.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_conntrack.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_limit.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_mac.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_mark.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_multiport.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_state.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_tcpudp.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/modules/4.9.61/xt_time.ko` | ET_REL / AArch64 64 | 0x0 | — | — | — |
| `lib/security/libmico-pam.so` | ET_DYN / ARM 32 | 0x13e0 | — | — | `libcrypto.so.1.0.0`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/security/pam_access.so` | ET_DYN / ARM 32 | 0xad8 | pam_access.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_debug.so` | ET_DYN / ARM 32 | 0x604 | pam_debug.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_deny.so` | ET_DYN / ARM 32 | 0x304 | pam_deny.so | — | `libpam.so.0`, `libdl.so.2` |
| `lib/security/pam_echo.so` | ET_DYN / ARM 32 | 0x69c | pam_echo.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_env.so` | ET_DYN / ARM 32 | 0x9d0 | pam_env.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_exec.so` | ET_DYN / ARM 32 | 0xc50 | pam_exec.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_faildelay.so` | ET_DYN / ARM 32 | 0x6d4 | pam_faildelay.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_filter/upperLOWER` | ET_DYN / ARM 32 | 0x884 | — | /lib/ld-linux-armhf.so.3 | `libpam.so.0`, `libc.so.6` |
| `lib/security/pam_filter.so` | ET_DYN / ARM 32 | 0xa0c | pam_filter.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_ftp.so` | ET_DYN / ARM 32 | 0x4f8 | pam_ftp.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_group.so` | ET_DYN / ARM 32 | 0x974 | pam_group.so | — | `libpam.so.0`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/security/pam_issue.so` | ET_DYN / ARM 32 | 0x980 | pam_issue.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_keyinit.so` | ET_DYN / ARM 32 | 0x60c | pam_keyinit.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_limits.so` | ET_DYN / ARM 32 | 0xca0 | pam_limits.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_listfile.so` | ET_DYN / ARM 32 | 0x860 | pam_listfile.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_localuser.so` | ET_DYN / ARM 32 | 0x5d8 | pam_localuser.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_loginuid.so` | ET_DYN / ARM 32 | 0x638 | pam_loginuid.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_mail.so` | ET_DYN / ARM 32 | 0x7d8 | pam_mail.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_mkhomedir.so` | ET_DYN / ARM 32 | 0x6bc | pam_mkhomedir.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_motd.so` | ET_DYN / ARM 32 | 0x500 | pam_motd.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_namespace.so` | ET_DYN / ARM 32 | 0x1230 | pam_namespace.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_nologin.so` | ET_DYN / ARM 32 | 0x5f8 | pam_nologin.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_permit.so` | ET_DYN / ARM 32 | 0x3c4 | pam_permit.so | — | `libpam.so.0`, `libdl.so.2` |
| `lib/security/pam_pwhistory.so` | ET_DYN / ARM 32 | 0xc2c | pam_pwhistory.so | — | `libpam.so.0`, `libdl.so.2`, `libcrypt.so.1`, `libc.so.6` |
| `lib/security/pam_rootok.so` | ET_DYN / ARM 32 | 0x3c8 | pam_rootok.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_securetty.so` | ET_DYN / ARM 32 | 0x690 | pam_securetty.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_shells.so` | ET_DYN / ARM 32 | 0x558 | pam_shells.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_stress.so` | ET_DYN / ARM 32 | 0x65c | pam_stress.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_succeed_if.so` | ET_DYN / ARM 32 | 0x880 | pam_succeed_if.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_tally.so` | ET_DYN / ARM 32 | 0x924 | pam_tally.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_tally2.so` | ET_DYN / ARM 32 | 0x9a4 | pam_tally2.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_time.so` | ET_DYN / ARM 32 | 0x7b8 | pam_time.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_timestamp.so` | ET_DYN / ARM 32 | 0xc5c | pam_timestamp.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_umask.so` | ET_DYN / ARM 32 | 0x8a4 | pam_umask.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_unix.so` | ET_DYN / ARM 32 | 0x17c0 | pam_unix.so | — | `libpam.so.0`, `libdl.so.2`, `libcrypt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `lib/security/pam_warn.so` | ET_DYN / ARM 32 | 0x3f8 | pam_warn.so | — | `libpam.so.0`, `libdl.so.2` |
| `lib/security/pam_wheel.so` | ET_DYN / ARM 32 | 0x610 | pam_wheel.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `lib/security/pam_xauth.so` | ET_DYN / ARM 32 | 0x10a4 | pam_xauth.so | — | `libpam.so.0`, `libdl.so.2`, `libc.so.6` |
| `sbin/askfirst` | ET_EXEC / ARM 32 | 0x10434 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/init` | ET_EXEC / ARM 32 | 0x11574 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libubox.so`, `libubus.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libjson_script.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/jffs2reset` | ET_EXEC / ARM 32 | 0x10974 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libfstools.so`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/kmodloader` | ET_EXEC / ARM 32 | 0x116e0 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/mount_root` | ET_EXEC / ARM 32 | 0x108e8 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libfstools.so`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/mtd` | ET_EXEC / ARM 32 | 0x11d5c | — | /lib/ld-linux-armhf.so.3 | `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/procd` | ET_EXEC / ARM 32 | 0x12c44 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libubox.so`, `libubus.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libjson_script.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/ubusd` | ET_EXEC / ARM 32 | 0x112e4 | — | /lib/ld-linux-armhf.so.3 | `libubox.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/uci` | ET_EXEC / ARM 32 | 0x1116c | — | /lib/ld-linux-armhf.so.3 | `libuci.so`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/udevtrigger` | ET_EXEC / ARM 32 | 0x1086c | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/upgraded` | ET_EXEC / ARM 32 | 0x10bc4 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `sbin/validate_data` | ET_EXEC / ARM 32 | 0x10a2c | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libubox.so`, `libuci.so`, `libvalidate.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/adbd` | ET_EXEC / ARM 32 | 0x11f14 | — | /lib/ld-linux-armhf.so.3 | `libc.so.6`, `libpthread.so.0`, `libz.so.1`, `libcrypto.so.1.0.0`, `libcrypt.so.1`, `libgio-2.0.so.0`, `libgobject-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libgcc_s.so.1` |
| `usr/bin/agent_client` | ET_EXEC / ARM 32 | 0x10670 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/agent_sendline` | ET_EXEC / ARM 32 | 0x105bc | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/alarmd` | ET_EXEC / ARM 32 | 0x12510 | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libmico-common.so`, `libcurl.so.4`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/alsamixer` | ET_EXEC / ARM 32 | 0x135c8 | — | /lib/ld-linux-armhf.so.3 | `libformw.so.6`, `libmenuw.so.6`, `libpanelw.so.6`, `libncursesw.so.6`, `libasound.so.2`, `libm.so.6`, `libdl.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/amixer` | ET_EXEC / ARM 32 | 0x13720 | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `libasound.so.2`, `libdl.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/aplay` | ET_EXEC / ARM 32 | 0x18e58 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libasound.so.2`, `libm.so.6`, `libdl.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/arecord` | ET_EXEC / ARM 32 | 0x18e58 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libasound.so.2`, `libm.so.6`, `libdl.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/bccmd` | ET_EXEC / ARM 32 | 0x11624 | — | /lib/ld-linux-armhf.so.3 | `libncursesw.so.6`, `libreadline.so.7`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/bluealsa` | ET_EXEC / ARM 32 | 0x13588 | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libasound_module_rate_speexrate.so`, `libbluetooth.so.3`, `libgio-2.0.so.0`, `libgobject-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libfdk-aac.so.2`, `libsbc.so.1`, `libm.so.6`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/bluealsa-aplay` | ET_EXEC / ARM 32 | 0x12aac | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libasound_module_rate_speexrate.so`, `libasound.so.2`, `libbluetooth.so.3`, `libgio-2.0.so.0`, `libgobject-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libm.so.6`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/bluemoon` | ET_EXEC / ARM 32 | 0x11244 | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/bluetoothctl` | ET_EXEC / ARM 32 | 0x13a44 | — | /lib/ld-linux-armhf.so.3 | `libgthread-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libdbus-1.so.3`, `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/bluetoothd` | ET_EXEC / ARM 32 | 0x19420 | — | /lib/ld-linux-armhf.so.3 | `libgthread-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libdbus-1.so.3`, `libdl.so.2`, `librt.so.1`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/bluez_mibt_ble` | ET_EXEC / ARM 32 | 0x13d68 | — | /lib/ld-linux-armhf.so.3 | `libpthread.so.0`, `libbluetooth.so.3`, `libjson-c.so.2`, `libubus.so`, `libubox.so`, `libcrypto.so.1.0.0`, `libmico-common.so`, `libbt_common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/bluez_mibt_classical` | ET_EXEC / ARM 32 | 0x15df0 | — | /lib/ld-linux-armhf.so.3 | `libglib-2.0.so.0`, `libintl.so.8`, `libdbus-1.so.3`, `libpthread.so.0`, `libbluetooth.so.3`, `libjson-c.so.2`, `libubus.so`, `libubox.so`, `libmico-common.so`, `librt.so.1`, `libcurl.so.4`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/btmgmt` | ET_EXEC / ARM 32 | 0x11da4 | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/btmon` | ET_EXEC / ARM 32 | 0x11c30 | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/central_lite` | ET_EXEC / ARM 32 | 0x16ba8 | — | /lib/ld-linux-armhf.so.3 | `libdl.so.2`, `librt.so.1`, `libpthread.so.0`, `libdns_sd.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/ciptool` | ET_EXEC / ARM 32 | 0x11250 | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/coap-client` | ET_EXEC / ARM 32 | 0x12994 | — | /lib/ld-linux-armhf.so.3 | `libcoap-1.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/curl` | ET_EXEC / ARM 32 | 0x11f10 | — | /lib/ld-linux-armhf.so.3 | `libcurl.so.4`, `libcares.so.2`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/dbus-launch.real` | ET_EXEC / ARM 32 | 0x11adc | — | /lib/ld-linux-armhf.so.3 | `libdbus-1.so.3`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/dbus-uuidgen` | ET_EXEC / ARM 32 | 0x10864 | — | /lib/ld-linux-armhf.so.3 | `libdbus-1.so.3`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/dlna` | ET_EXEC / ARM 32 | 0x11914 | — | /lib/ld-linux-armhf.so.3 | `libglib-2.0.so.0`, `libgobject-2.0.so.0`, `libgupnp-1.0.so.4`, `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libpthread.so.0`, `libmbedtls.so.9`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/drill` | ET_EXEC / ARM 32 | 0x14b14 | — | /lib/ld-linux-armhf.so.3 | `libldns.so.1`, `libcrypto.so.1.0.0`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/fw_env` | ET_EXEC / ARM 32 | 0x10aa4 | — | /lib/ld-linux-armhf.so.3 | `libz.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/fwtool` | ET_EXEC / ARM 32 | 0x10b24 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/gatttool` | ET_EXEC / ARM 32 | 0x12a50 | — | /lib/ld-linux-armhf.so.3 | `libgthread-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/getevent` | ET_EXEC / ARM 32 | 0x11294 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/getrandom` | ET_EXEC / ARM 32 | 0x105f4 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/hciattach` | ET_EXEC / ARM 32 | 0x11ba0 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/hciconfig` | ET_EXEC / ARM 32 | 0x11148 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/hcidump` | ET_EXEC / ARM 32 | 0x118b4 | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/hcitool` | ET_EXEC / ARM 32 | 0x10f28 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/heartbeatagent` | ET_EXEC / ARM 32 | 0x11d84 | — | /lib/ld-linux-armhf.so.3 | `libxiaomi_heartbeat.so`, `libxiaomi_mico.so`, `libxiaomi_crypto.so`, `libssl.so.1.0.0`, `libxiaomi_http.so`, `libxiaomi_json.so`, `libxiaomi_utils.so`, `libcrypto.so.1.0.0`, `libuuid.so.1`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libmico-common.so`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/hex2hcd` | ET_EXEC / ARM 32 | 0x11054 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/htop` | ET_EXEC / ARM 32 | 0x15804 | — | /lib/ld-linux-armhf.so.3 | `libncursesw.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/idmruntime` | ET_DYN / ARM 32 | 0x431f4 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libpthread.so.0`, `libdl.so.2`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/bin/iperf` | ET_EXEC / ARM 32 | 0x11bfc | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libpthread.so.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/jshn` | ET_EXEC / ARM 32 | 0x1130c | — | /lib/ld-linux-armhf.so.3 | `libblobmsg_json.so`, `libjson-c.so.2`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/jsonfilter` | ET_EXEC / ARM 32 | 0x115dc | — | /lib/ld-linux-armhf.so.3 | `libubox.so`, `libjson-c.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/l2ping` | ET_EXEC / ARM 32 | 0x11468 | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/l2test` | ET_EXEC / ARM 32 | 0x11e78 | — | /lib/ld-linux-armhf.so.3 | `libncursesw.so.6`, `libreadline.so.7`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/linein` | ET_EXEC / ARM 32 | 0x11b80 | — | /lib/ld-linux-armhf.so.3 | `libjson-c.so.2`, `libubus.so`, `libubox.so`, `libpthread.so.0`, `libmico-common.so`, `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/matool` | ET_EXEC / ARM 32 | 0x1a6f0 | — | /lib/ld-linux-armhf.so.3 | `libxiaomi_mico.so`, `libxiaomi_miot.so`, `libmico-common.so`, `libxiaomi_http.so`, `libxiaomi_json.so`, `libxiaomi_crypto.so`, `libssl.so.1.0.0`, `libxiaomi_utils.so`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libcrypto.so.1.0.0`, `libuuid.so.1`, `libglib-2.0.so.0`, `libgobject-2.0.so.0`, `libiconv.so.2`, `libintl.so.8`, `libthrift_c_glib.so.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mdnsd` | ET_EXEC / ARM 32 | 0x13f74 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mediaplayer` | ET_EXEC / ARM 32 | 0x15a7c | — | /lib/ld-linux-armhf.so.3 | `libdl.so.2`, `libubox.so`, `libubus.so`, `libblobmsg_json.so`, `libffmpeg-miplayer.so`, `libxiaomimediaplayerlite.so`, `libasound.so.2`, `libcurl.so.4`, `libjson-c.so.2`, `libpthread.so.0`, `libmbedtls.so.9`, `libcrypto.so.1.0.0`, `libconfig++.so.11`, `libuuid.so.1`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/messagingagent` | ET_EXEC / ARM 32 | 0x24b58 | — | /lib/ld-linux-armhf.so.3 | `libcrypto.so.1.0.0`, `libssl.so.1.0.0`, `libxiaomi_mosquitto.so`, `libxiaomi_mico.so`, `libmico-common.so`, `libxiaomi_dns.so`, `libxiaomi_http.so`, `libxiaomi_crypto.so`, `libxiaomi_json.so`, `libxiaomi_utils.so`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libmbedtls.so.9`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libevent-2.0.so.5`, `libevent_pthreads-2.0.so.5`, `libldns.so.1`, `libjson-c.so.2`, `libpthread.so.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mfgutil` | ET_EXEC / ARM 32 | 0x110d0 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mibrain_service` | ET_EXEC / ARM 32 | 0x12f60 | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libmibrain-common-sdk.so`, `libmibrain-common-util.so`, `libmico-common.so`, `libpthread.so.0`, `libaivs-message-util.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mibt_mesh` | ET_EXEC / ARM 32 | 0x19e2c | — | /lib/ld-linux-armhf.so.3 | `libpthread.so.0`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mibt_mesh_rtl` | ET_EXEC / ARM 32 | 0x1aa2c | — | /lib/ld-linux-armhf.so.3 | `libpthread.so.0`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mico-helper` | ET_EXEC / ARM 32 | 0x11610 | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libpthread.so.0`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mico_ai_crontab` | ET_EXEC / ARM 32 | 0x11540 | — | /lib/ld-linux-armhf.so.3 | `libpthread.so.0`, `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mico_aivs_lab` | ET_EXEC / ARM 32 | 0x20544 | — | /lib/ld-linux-armhf.so.3 | `libaivs_sdk.so`, `libaivs-message-util.so`, `libpthread.so.0`, `libz.so.1`, `libjson-c.so.2`, `libubus.so`, `libubox.so`, `libblobmsg_json.so`, `libmico-common.so`, `libir-agent-codec.so`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mico_ir_agent` | ET_EXEC / ARM 32 | 0x11494 | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libpthread.so.0`, `libir-agent-codec.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/micocfg` | ET_EXEC / ARM 32 | 0x13a9c | — | /lib/ld-linux-armhf.so.3 | `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/miio_agent` | ET_EXEC / ARM 32 | 0x11848 | — | /lib/ld-linux-armhf.so.3 | `libjson-c.so.2`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/miio_client` | ET_EXEC / ARM 32 | 0x167a0 | — | /lib/ld-linux-armhf.so.3 | `libpthread.so.0`, `librt.so.1`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/miio_helper` | ET_EXEC / ARM 32 | 0x15298 | — | /lib/ld-linux-armhf.so.3 | `libz.so.1`, `libubus.so`, `libubox.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libpthread.so.0`, `libmico-common.so`, `libir-agent-codec.so`, `libmicowpa.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/miio_mpkg` | ET_EXEC / ARM 32 | 0x12690 | — | /lib/ld-linux-armhf.so.3 | `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/miplayer` | ET_EXEC / ARM 32 | 0x114b4 | — | /lib/ld-linux-armhf.so.3 | `libdl.so.2`, `libffmpeg-miplayer.so`, `libxiaomimediaplayerlite.so`, `libasound.so.2`, `libjson-c.so.2`, `libpthread.so.0`, `libcrypto.so.1.0.0`, `libmbedtls.so.9`, `libcurl.so.4`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mipns-xiaomi` | ET_EXEC / ARM 32 | 0x13e38 | — | /lib/ld-linux-armhf.so.3 | `libxaudio_engine.so`, `libanl.so.1`, `libmdspeech.so`, `libiotdcm.so`, `libaivs-message-util.so`, `libubox.so`, `libubus.so`, `libjson-c.so.2`, `libpthread.so.0`, `libasound.so.2`, `libmico-common.so`, `libm.so.6`, `libcurl.so.4`, `libconfig.so.11`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mitv_pstream` | ET_EXEC / ARM 32 | 0x11930 | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libcurl.so.4`, `libmico-common.so`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mpap` | ET_EXEC / ARM 32 | 0x18f04 | — | /lib/ld-linux-armhf.so.3 | `libdl.so.2`, `libpthread.so.0`, `libfdk-aac.so.1`, `libasound.so.2`, `libFLAC.so.8`, `libprotobuf-c.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/mpas` | ET_EXEC / ARM 32 | 0x231b4 | — | /lib/ld-linux-armhf.so.3 | `libdl.so.2`, `libglog.so.0.3.5`, `libcurl.so.4`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libjson-c.so.2`, `libidmsdk.so`, `libubox.so`, `libubus.so`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/bin/mtd_crash_log` | ET_EXEC / ARM 32 | 0x11a04 | — | /lib/ld-linux-armhf.so.3 | `libmbedtls.so.9`, `libcurl.so.4`, `libmico-common.so`, `libjson-c.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/nano_httpd` | ET_EXEC / ARM 32 | 0x10e30 | — | /lib/ld-linux-armhf.so.3 | `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/pns_ubus_helper` | ET_EXEC / ARM 32 | 0x118ec | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libmibrainsdk.so`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-free` | ET_EXEC / ARM 32 | 0x113e0 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-pgrep` | ET_EXEC / ARM 32 | 0x119f0 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-pkill` | ET_EXEC / ARM 32 | 0x119f0 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-pmap` | ET_EXEC / ARM 32 | 0x12e10 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-pwdx` | ET_EXEC / ARM 32 | 0x10c48 | — | /lib/ld-linux-armhf.so.3 | `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-skill` | ET_EXEC / ARM 32 | 0x11e40 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-slabtop` | ET_EXEC / ARM 32 | 0x1164c | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libncursesw.so.6`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-snice` | ET_EXEC / ARM 32 | 0x11e40 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-tload` | ET_EXEC / ARM 32 | 0x11004 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-top` | ET_EXEC / ARM 32 | 0x12ca8 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libncursesw.so.6`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-uptime` | ET_EXEC / ARM 32 | 0x10a34 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/procps-ng-vmstat` | ET_EXEC / ARM 32 | 0x12280 | — | /lib/ld-linux-armhf.so.3 | `libprocps.so.5`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/qplayer` | ET_EXEC / ARM 32 | 0x1156c | — | /lib/ld-linux-armhf.so.3 | `libubox.so`, `libubus.so`, `libasound.so.2`, `libsndfile.so.1`, `libopusfile.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/quickplayer` | ET_EXEC / ARM 32 | 0x11980 | — | /lib/ld-linux-armhf.so.3 | `libubox.so`, `libubus.so`, `libasound.so.2`, `libsndfile.so.1`, `libopusfile.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/rctest` | ET_EXEC / ARM 32 | 0x118a0 | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/rfcomm` | ET_EXEC / ARM 32 | 0x1142c | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/sbcdec` | ET_EXEC / ARM 32 | 0x1096c | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/sbcenc` | ET_EXEC / ARM 32 | 0x10f64 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/sbcinfo` | ET_EXEC / ARM 32 | 0x106bc | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/sdptool` | ET_EXEC / ARM 32 | 0x11310 | — | /lib/ld-linux-armhf.so.3 | `libgthread-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libreadline.so.7`, `libncursesw.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/bin/sound_effect` | ET_EXEC / ARM 32 | 0x10b10 | — | /lib/ld-linux-armhf.so.3 | `libjson-c.so.2`, `libdts.so`, `libubus.so`, `libubox.so`, `libpthread.so.0`, `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/sqlite3` | ET_EXEC / ARM 32 | 0x1575c | — | /lib/ld-linux-armhf.so.3 | `libreadline.so.7`, `libncursesw.so.6`, `libdl.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/statpoints` | ET_EXEC / ARM 32 | 0x1193c | — | /lib/ld-linux-armhf.so.3 | `libcurl.so.4`, `libdl.so.2`, `libjson-c.so.2`, `libm.so.6`, `libmbedtls.so.9`, `libz.so.1`, `liblibma.so`, `libmico-common.so`, `libblobmsg_json.so`, `libglog.so.0.3.5`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/statpoints_daemon` | ET_EXEC / ARM 32 | 0x11314 | — | /lib/ld-linux-armhf.so.3 | `libxiaomi_utils.so`, `libcrypto.so.1.0.0`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libmico-common.so`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/strace` | ET_EXEC / ARM 32 | 0x12978 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/traceroute` | ET_EXEC / ARM 32 | 0x11ea0 | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/usb_monitor` | ET_EXEC / ARM 32 | 0x105e0 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/usign` | ET_EXEC / ARM 32 | 0x112dc | — | /lib/ld-linux-armhf.so.3 | `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/voip_service` | ET_EXEC / ARM 32 | 0x124c4 | — | /lib/ld-linux-armhf.so.3 | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libpthread.so.0`, `libblobmsg_json.so`, `libcurl.so.4`, `libcrypto.so.1.0.0`, `libmico-common.so`, `libdl.so.2`, `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/bin/wget-ssl` | ET_EXEC / ARM 32 | 0x16214 | — | /lib/ld-linux-armhf.so.3 | `libpcre.so.1`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libdl.so.2`, `libz.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_ctl_arcam_av.so` | ET_DYN / ARM 32 | 0x1414 | libasound_module_ctl_arcam_av.so | — | `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_ctl_bluealsa.so` | ET_DYN / ARM 32 | 0x13ac | libasound_module_ctl_bluealsa.so | — | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libasound_module_rate_speexrate.so`, `libasound.so.2`, `libm.so.6`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_ctl_oss.so` | ET_DYN / ARM 32 | 0xb34 | libasound_module_ctl_oss.so | — | `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_pcm_bluealsa.so` | ET_DYN / ARM 32 | 0x19c8 | libasound_module_pcm_bluealsa.so | — | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libasound_module_rate_speexrate.so`, `libasound.so.2`, `libbluetooth.so.3`, `libm.so.6`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_pcm_oss.so` | ET_DYN / ARM 32 | 0xbe8 | libasound_module_pcm_oss.so | — | `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_pcm_speex.so` | ET_DYN / ARM 32 | 0xae0 | libasound_module_pcm_speex.so | — | `libspeexdsp.so.1`, `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_pcm_upmix.so` | ET_DYN / ARM 32 | 0xa18 | libasound_module_pcm_upmix.so | — | `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_pcm_usb_stream.so` | ET_DYN / ARM 32 | 0xd80 | libasound_module_pcm_usb_stream.so | — | `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_pcm_vdownmix.so` | ET_DYN / ARM 32 | 0x81c | libasound_module_pcm_vdownmix.so | — | `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_rate_speexrate.so` | ET_DYN / ARM 32 | 0x748 | libasound_module_rate_speexrate.so | — | `libspeexdsp.so.1`, `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_rate_speexrate_best.so` | ET_DYN / ARM 32 | 0x748 | libasound_module_rate_speexrate.so | — | `libspeexdsp.so.1`, `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/alsa-lib/libasound_module_rate_speexrate_medium.so` | ET_DYN / ARM 32 | 0x748 | libasound_module_rate_speexrate.so | — | `libspeexdsp.so.1`, `libasound.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/dbus-1/dbus-daemon-launch-helper` | ET_EXEC / ARM 32 | 0x12d1c | — | /lib/ld-linux-armhf.so.3 | `libdbus-1.so.3`, `libexpat.so.1`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libFLAC.so.8.3.0` | ET_DYN / ARM 32 | 0x66d8 | libFLAC.so.8 | — | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libagora-rtc-sdk.so` | ET_DYN / ARM 32 | 0xc1b8 | libagora-rtc-sdk.so | — | `libpthread.so.0`, `librt.so.1`, `libm.so.6`, `libc.so.6` |
| `usr/lib/libaivs-message-util.so` | ET_DYN / ARM 32 | 0x3298 | libaivs-message-util.so | — | `libprotobuf-c.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libaivs_bt.so` | ET_DYN / ARM 32 | 0x1224 | libaivs_bt.so | — | `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/lib/libaivs_sdk.so` | ET_DYN / ARM 32 | 0x6be60 | libaivs_sdk.so | — | `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/lib/libasound.so.2.0.0` | ET_DYN / ARM 32 | 0x257a0 | libasound.so.2 | — | `libm.so.6`, `libdl.so.2`, `libpthread.so.0`, `librt.so.1`, `libFLAC.so.8`, `libprotobuf-c.so.1`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/lib/libattr.so.1.1.2448` | ET_DYN / ARM 32 | 0xd70 | libattr.so.1 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libbluetooth.so.3.18.10` | ET_DYN / ARM 32 | 0x45c4 | libbluetooth.so.3 | — | `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/lib/libbt_common.so` | ET_DYN / ARM 32 | 0xf80 | — | — | `libpthread.so.0`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libcares.so.2.2.0` | ET_DYN / ARM 32 | 0x1a90 | libcares.so.2 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libcoap-1.so.0.0.0` | ET_DYN / ARM 32 | 0x25f8 | libcoap-1.so.0 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libconfig++.so.11.0.2` | ET_DYN / ARM 32 | 0x6bd0 | libconfig++.so.11 | — | `libstdc++.so.6`, `libm.so.6`, `libc.so.6`, `libgcc_s.so.1` |
| `usr/lib/libconfig.so.11.0.2` | ET_DYN / ARM 32 | 0x2720 | libconfig.so.11 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libcrypto.so.1.0.0` | ET_DYN / ARM 32 | 0x376c0 | libcrypto.so.1.0.0 | — | `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libcurl.so.4.4.0` | ET_DYN / ARM 32 | 0x5228 | libcurl.so.4 | — | `libcares.so.2`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libdbi.so.1.1.0` | ET_DYN / ARM 32 | 0x3508 | libdbi.so.1 | — | `libdl.so.2`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libdbus-1.so.3.14.5` | ET_DYN / ARM 32 | 0xa05c | libdbus-1.so.3 | — | `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libdns_sd.so` | ET_DYN / ARM 32 | 0x1020 | — | — | — |
| `usr/lib/libdts.so` | ET_DYN / ARM 32 | 0x18860 | — | — | `libdl.so.2`, `libm.so.6`, `libc.so.6` |
| `usr/lib/libevent-2.0.so.5.1.10` | ET_DYN / ARM 32 | 0x9090 | libevent-2.0.so.5 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libevent_core-2.0.so.5.1.10` | ET_DYN / ARM 32 | 0x5cd0 | libevent_core-2.0.so.5 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libevent_extra-2.0.so.5.1.10` | ET_DYN / ARM 32 | 0x4c90 | libevent_extra-2.0.so.5 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libevent_openssl-2.0.so.5.1.10` | ET_DYN / ARM 32 | 0x15bc | libevent_openssl-2.0.so.5 | — | `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libevent_pthreads-2.0.so.5.1.10` | ET_DYN / ARM 32 | 0x8d8 | libevent_pthreads-2.0.so.5 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libevtlog.so` | ET_DYN / ARM 32 | 0x1038 | libevtlog.so.0 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libevtlog.so.0` | ET_DYN / ARM 32 | 0x1038 | libevtlog.so.0 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libevtlog.so.0.0.0` | ET_DYN / ARM 32 | 0x1038 | libevtlog.so.0 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libexpat.so.1.6.7` | ET_DYN / ARM 32 | 0x21e0 | libexpat.so.1 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libfdk-aac.so.2.0.2` | ET_DYN / ARM 32 | 0x1660 | libfdk-aac.so.2 | — | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libffi.so.6.0.4` | ET_DYN / ARM 32 | 0x130c | libffi.so.6 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libffmpeg-miplayer.so` | ET_DYN / ARM 32 | 0x1dc80 | — | — | `libpthread.so.0`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libm.so.6`, `libz.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libformw.so.6.0` | ET_DYN / ARM 32 | 0x2588 | libformw.so.6 | — | `libc.so.6`, `libncursesw.so.6`, `libgcc_s.so.1` |
| `usr/lib/libgflags.so.2.2.2` | ET_DYN / ARM 32 | 0x506c | libgflags.so.2.2 | — | `libpthread.so.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgflags_nothreads.so.2.2.2` | ET_DYN / ARM 32 | 0x4ea0 | libgflags_nothreads.so.2.2 | — | `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgio-2.0.so.0.5501.0` | ET_DYN / ARM 32 | 0x2aa50 | libgio-2.0.so.0 | — | `libgobject-2.0.so.0`, `libffi.so.6`, `libgmodule-2.0.so.0`, `libdl.so.2`, `libglib-2.0.so.0`, `libiconv.so.2`, `libpthread.so.0`, `libz.so.1`, `libresolv.so.2`, `libintl.so.8`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libglib-2.0.so.0.5501.0` | ET_DYN / ARM 32 | 0x159f8 | libglib-2.0.so.0 | — | `libiconv.so.2`, `libpthread.so.0`, `libintl.so.8`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libglog.so.0` | ET_DYN / ARM 32 | 0x6c68 | libglog.so.0.3.5 | — | `libpthread.so.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgmodule-2.0.so.0.5501.0` | ET_DYN / ARM 32 | 0xc54 | libgmodule-2.0.so.0 | — | `libdl.so.2`, `libglib-2.0.so.0`, `libiconv.so.2`, `libpthread.so.0`, `libintl.so.8`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgmp.so.10.3.2` | ET_DYN / ARM 32 | 0x8a20 | libgmp.so.10 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgnutls.so.30.14.8` | ET_DYN / ARM 32 | 0x19880 | libgnutls.so.30 | — | `libnettle.so.6`, `libhogweed.so.4`, `libgmp.so.10`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgobject-2.0.so.0.5501.0` | ET_DYN / ARM 32 | 0x88bc | libgobject-2.0.so.0 | — | `libglib-2.0.so.0`, `libiconv.so.2`, `libpthread.so.0`, `libffi.so.6`, `libintl.so.8`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgssdp-1.0.so.3.0.1` | ET_DYN / ARM 32 | 0x3944 | libgssdp-1.0.so.3 | — | `libiconv.so.2`, `libsoup-2.4.so.1`, `libgio-2.0.so.0`, `libgobject-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/lib/libgthread-2.0.so.0.5501.0` | ET_DYN / ARM 32 | 0x410 | libgthread-2.0.so.0 | — | `libglib-2.0.so.0`, `libiconv.so.2`, `libpthread.so.0`, `libintl.so.8`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libgupnp-1.0.so.4.0.1` | ET_DYN / ARM 32 | 0x8c00 | libgupnp-1.0.so.4 | — | `libiconv.so.2`, `libgmodule-2.0.so.0`, `libgssdp-1.0.so.3`, `libsoup-2.4.so.1`, `libgio-2.0.so.0`, `libgobject-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libxml2.so.2`, `libuuid.so.1`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/lib/libhistory.so.7.0` | ET_DYN / ARM 32 | 0x1b30 | libhistory.so.7 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libhogweed.so.4.3` | ET_DYN / ARM 32 | 0x53d0 | libhogweed.so.4 | — | `libnettle.so.6`, `libgmp.so.10`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libical.so.0.48.0` | ET_DYN / ARM 32 | 0x124d8 | libical.so.0 | — | `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libicalss.so.0.48.0` | ET_DYN / ARM 32 | 0x5b30 | libicalss.so.0 | — | `libical.so.0`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libicalvcal.so.0.48.0` | ET_DYN / ARM 32 | 0x3b20 | libicalvcal.so.0 | — | `libical.so.0`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libiconv.so.2.4.0` | ET_DYN / ARM 32 | 0xb7c | libiconv.so.2 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libidmsdk.so` | ET_DYN / ARM 32 | 0x1a980 | libidmsdk.so | — | `libpthread.so.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/lib/libintl.so.8.1.5` | ET_DYN / ARM 32 | 0x169c | libintl.so.8 | — | `libc.so.6`, `libgcc_s.so.1` |
| `usr/lib/libiotdcm.so` | ET_DYN / ARM 32 | 0x17540 | libiotdcm.so | — | `libstdc++.so.6`, `libm.so.6`, `libc.so.6`, `libgcc_s.so.1`, `libpthread.so.0` |
| `usr/lib/libip4tc.so.0.1.0` | ET_DYN / ARM 32 | 0xf58 | libip4tc.so.0 | — | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libip6tc.so.0.1.0` | ET_DYN / ARM 32 | 0x100c | libip6tc.so.0 | — | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libiptext.so` | ET_DYN / ARM 32 | 0x1dcc | — | — | `libm.so.6`, `libxtables.so.12`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libiptext4.so` | ET_DYN / ARM 32 | 0xb88 | — | — | `libm.so.6`, `libxtables.so.12`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libiptext6.so` | ET_DYN / ARM 32 | 0xae4 | — | — | `libm.so.6`, `libxtables.so.12`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libir-agent-codec.so` | ET_DYN / ARM 32 | 0xa7c | — | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libjson-c.so.2.0.2` | ET_DYN / ARM 32 | 0x1e20 | libjson-c.so.2 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libldns.so.1.6.17` | ET_DYN / ARM 32 | 0x10198 | libldns.so.1 | — | `libcrypto.so.1.0.0`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/liblibma.so` | ET_DYN / ARM 32 | 0x2b48 | liblibma.so | — | `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/liblua.so.5.1.5` | ET_DYN / ARM 32 | 0x3c20 | liblua.so.5.1.5 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmbedcrypto.so.2.6.0` | ET_DYN / ARM 32 | 0x5880 | libmbedcrypto.so.0 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmbedtls.so.1.3.14` | ET_DYN / ARM 32 | 0x6e38 | libmbedtls.so.9 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmbedtls.so.2.6.0` | ET_DYN / ARM 32 | 0x3e60 | libmbedtls.so.10 | — | `libmbedx509.so.0`, `libmbedcrypto.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmbedx509.so.2.6.0` | ET_DYN / ARM 32 | 0x2728 | libmbedx509.so.0 | — | `libmbedcrypto.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmdspeech.so` | ET_DYN / ARM 32 | 0x3570 | libmdspeech.so | — | `libpthread.so.0`, `libc.so.6`, `libm.so.6`, `libiotdcm.so`, `libaivs_bt.so`, `libgcc_s.so.1`, `ld-linux-armhf.so.3` |
| `usr/lib/libmenuw.so.6.0` | ET_DYN / ARM 32 | 0x17a4 | libmenuw.so.6 | — | `libc.so.6`, `libncursesw.so.6`, `libgcc_s.so.1` |
| `usr/lib/libmesh.so` | ET_DYN / ARM 32 | 0x99cc | — | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmibrain-common-sdk.so` | ET_DYN / ARM 32 | 0x2724 | — | — | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libmibrainsdk.so`, `libdl.so.2`, `libmico-common.so`, `libir-agent-codec.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmibrain-common-util.so` | ET_DYN / ARM 32 | 0xe3c | — | — | `libubus.so`, `libubox.so`, `libjson-c.so.2`, `libblobmsg_json.so`, `libmibrainsdk.so`, `libdl.so.2`, `libmico-common.so`, `libir-agent-codec.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmibrainsdk.so` | ET_DYN / ARM 32 | 0xbc20 | — | — | `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6` |
| `usr/lib/libmico-common-api.so` | ET_DYN / ARM 32 | 0x5d8 | — | — | `libz.so.1`, `libubus.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libpthread.so.0`, `libconfig.so.11`, `libuuid.so.1`, `libcurl.so.4`, `libwpa_client.so`, `libmbedtls.so.10`, `libiconv.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmico-common.so` | ET_DYN / ARM 32 | 0xe3ec | — | — | `libz.so.1`, `libubus.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libpthread.so.0`, `libconfig.so.11`, `libuuid.so.1`, `libcurl.so.4`, `libwpa_client.so`, `libmbedtls.so.10`, `libiconv.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmicowpa.so` | ET_DYN / ARM 32 | 0x4b10 | — | — | `libz.so.1`, `libubus.so`, `libubox.so`, `libblobmsg_json.so`, `libjson-c.so.2`, `libwpa_client.so`, `libpthread.so.0`, `libmico-common.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libmilink.so` | ET_DYN / ARM 32 | 0x39040 | libmilink.so | — | `libcrypto.so.1.0.0`, `libprotobuf-lite.so.13`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/lib/libncursesw.so.6.0` | ET_DYN / ARM 32 | 0xc0c4 | libncursesw.so.6 | — | `libc.so.6`, `libgcc_s.so.1` |
| `usr/lib/libnettle.so.6.3` | ET_DYN / ARM 32 | 0x6b80 | libnettle.so.6 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libnl-tiny.so` | ET_DYN / ARM 32 | 0x1c30 | — | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libogg.so.0.8.2` | ET_DYN / ARM 32 | 0x1358 | libogg.so.0 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libopus.so.0.6.1` | ET_DYN / ARM 32 | 0x1900 | libopus.so.0 | — | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libopusfile.so.0.4.4` | ET_DYN / ARM 32 | 0x1850 | libopusfile.so.0 | — | `libogg.so.0`, `libopus.so.0`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libopusurl.so.0.4.4` | ET_DYN / ARM 32 | 0x1468 | libopusurl.so.0 | — | `libopusfile.so.0`, `libogg.so.0`, `libopus.so.0`, `libm.so.6`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libpanelw.so.6.0` | ET_DYN / ARM 32 | 0x858 | libpanelw.so.6 | — | `libc.so.6`, `libncursesw.so.6`, `libgcc_s.so.1` |
| `usr/lib/libpcap.so.1` | ET_DYN / ARM 32 | 0x4df8 | libpcap.so.1 | — | `libc.so.6`, `libgcc_s.so.1` |
| `usr/lib/libpcre.so.1.2.9` | ET_DYN / ARM 32 | 0xd84 | libpcre.so.1 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libpcreposix.so.0.0.5` | ET_DYN / ARM 32 | 0x650 | libpcreposix.so.0 | — | `libpcre.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libprocps.so` | ET_DYN / ARM 32 | 0x26f0 | libprocps.so.5 | — | `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libprocps.so.5` | ET_DYN / ARM 32 | 0x26f0 | libprocps.so.5 | — | `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libprocps.so.5.0.0` | ET_DYN / ARM 32 | 0x26f0 | libprocps.so.5 | — | `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libprotobuf-c.so.1.0.0` | ET_DYN / ARM 32 | 0x8ec | libprotobuf-c.so.1 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libprotobuf-lite.so.13.0.0` | ET_DYN / ARM 32 | 0x170a8 | libprotobuf-lite.so.13 | — | `libpthread.so.0`, `libz.so.1`, `libstdc++.so.6`, `libm.so.6`, `libc.so.6`, `ld-linux-armhf.so.3`, `libgcc_s.so.1` |
| `usr/lib/libreadline.so.7.0` | ET_DYN / ARM 32 | 0xd230 | libreadline.so.7 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libsbc.so.1.2.1` | ET_DYN / ARM 32 | 0x710 | libsbc.so.1 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libsha1.so` | ET_DYN / ARM 32 | 0x7b0 | — | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libsndfile.so.1.0.28` | ET_DYN / ARM 32 | 0x2608 | libsndfile.so.1 | — | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libsoup-2.4.so.1.8.0` | ET_DYN / ARM 32 | 0x24550 | libsoup-2.4.so.1 | — | `libiconv.so.2`, `libgio-2.0.so.0`, `libgobject-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libxml2.so.2`, `libsqlite3.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libspeexdsp.so.1.5.0` | ET_DYN / ARM 32 | 0x1570 | libspeexdsp.so.1 | — | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libsqlite3.so.0.8.6` | ET_DYN / ARM 32 | 0x6968 | libsqlite3.so.0 | — | `libdl.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libssl.so.1.0.0` | ET_DYN / ARM 32 | 0xe160 | libssl.so.1.0.0 | — | `libcrypto.so.1.0.0`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libstack.so` | ET_DYN / ARM 32 | 0x3df0 | — | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libstdc++.so.6.0.22` | ET_DYN / ARM 32 | 0x70078 | libstdc++.so.6 | — | `libm.so.6`, `libc.so.6`, `ld-linux-armhf.so.3`, `libgcc_s.so.1` |
| `usr/lib/libthrift_c_glib.so.0.0.0` | ET_DYN / ARM 32 | 0x8238 | libthrift_c_glib.so.0 | — | `libgobject-2.0.so.0`, `libglib-2.0.so.0`, `libintl.so.8`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `librt.so.1`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libuclient.so` | ET_DYN / ARM 32 | 0x12d4 | libuclient.so | — | `libubox.so`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libutils.so` | ET_DYN / ARM 32 | 0x1b38 | — | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libuuid.so.1.3.0` | ET_DYN / ARM 32 | 0xe80 | libuuid.so.1 | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libvad.so` | ET_DYN / ARM 32 | 0x3118 | — | — | `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libvoip.so` | ET_DYN / ARM 32 | 0x87af8 | libvoip.so | — | `libagora-rtc-sdk.so`, `libmilink.so`, `libasound.so.2`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/lib/libwpa_client.so` | ET_DYN / ARM 32 | 0x235c | — | — | `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libwrap.so.0.7.6` | ET_DYN / ARM 32 | 0x1cb4 | libwrap.so.0 | — | `libc.so.6`, `libnsl.so.1`, `libgcc_s.so.1` |
| `usr/lib/libxaudio_engine.so` | ET_DYN / ARM 32 | 0x140a0 | libxaudio_engine.so | — | `libgcc_s.so.1`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `usr/lib/libxiaomi_crypto.so` | ET_DYN / ARM 32 | 0x43dc | libxiaomi_crypto.so | — | `libcrypto.so.1.0.0`, `libssl.so.1.0.0`, `libxiaomi_utils.so`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libmico-common.so`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_dns.so` | ET_DYN / ARM 32 | 0x2dd4 | libxiaomi_dns.so | — | `libevent-2.0.so.5`, `libevent_pthreads-2.0.so.5`, `libldns.so.1`, `libmico-common.so`, `libjson-c.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_heartbeat.so` | ET_DYN / ARM 32 | 0x2588 | libxiaomi_heartbeat.so | — | `libxiaomi_mico.so`, `libmico-common.so`, `libxiaomi_http.so`, `libxiaomi_json.so`, `libxiaomi_crypto.so`, `libxiaomi_utils.so`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libcrypto.so.1.0.0`, `libssl.so.1.0.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_http.so` | ET_DYN / ARM 32 | 0x2110 | libxiaomi_http.so | — | `libxiaomi_utils.so`, `libmico-common.so`, `libcrypto.so.1.0.0`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_json.so` | ET_DYN / ARM 32 | 0x166c | libxiaomi_json.so | — | `libxiaomi_utils.so`, `libcrypto.so.1.0.0`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libmico-common.so`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_mico.so` | ET_DYN / ARM 32 | 0xa578 | libxiaomi_mico.so | — | `libxiaomi_crypto.so`, `libxiaomi_http.so`, `libxiaomi_json.so`, `libmico-common.so`, `libssl.so.1.0.0`, `libxiaomi_utils.so`, `libcrypto.so.1.0.0`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_miot.so` | ET_DYN / ARM 32 | 0x1c7c | libxiaomi_miot.so | — | `libglib-2.0.so.0`, `libgobject-2.0.so.0`, `libiconv.so.2`, `libintl.so.8`, `libthrift_c_glib.so.0`, `libxiaomi_crypto.so`, `libmico-common.so`, `libssl.so.1.0.0`, `libxiaomi_utils.so`, `libcrypto.so.1.0.0`, `liblibma.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libuuid.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_mosquitto.so` | ET_DYN / ARM 32 | 0x30ac | libxiaomi_mosquitto.so | — | `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomi_utils.so` | ET_DYN / ARM 32 | 0xfedc | libxiaomi_utils.so | — | `libcrypto.so.1.0.0`, `liblibma.so`, `libuuid.so.1`, `libmico-common.so`, `libblobmsg_json.so`, `libcurl.so.4`, `libglog.so.0.3.5`, `libjson-c.so.2`, `libmbedtls.so.9`, `libpthread.so.0`, `libubox.so`, `libubus.so`, `libz.so.1`, `librt.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxiaomimediaplayerlite.so` | ET_DYN / ARM 32 | 0x13568 | — | — | `libpthread.so.0`, `libz.so.1`, `libm.so.6`, `libffmpeg-miplayer.so`, `libasound.so.2`, `libopus.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxml2.so.2.9.7` | ET_DYN / ARM 32 | 0x1a380 | libxml2.so.2 | — | `libdl.so.2`, `libz.so.1`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libxtables.so.12.0.0` | ET_DYN / ARM 32 | 0x23d0 | libxtables.so.12 | — | `libm.so.6`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/lib/libz.so.1.2.11` | ET_DYN / ARM 32 | 0x16e4 | libz.so.1 | — | `libc.so.6`, `libgcc_s.so.1` |
| `usr/sbin/alsactl` | ET_EXEC / ARM 32 | 0x14d98 | — | /lib/ld-linux-armhf.so.3 | `libasound.so.2`, `libm.so.6`, `libdl.so.2`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/dbus-daemon` | ET_EXEC / ARM 32 | 0x1654c | — | /lib/ld-linux-armhf.so.3 | `libdbus-1.so.3`, `libexpat.so.1`, `libpthread.so.0`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/dnsmasq` | ET_EXEC / ARM 32 | 0x15348 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/dropbear` | ET_EXEC / ARM 32 | 0x128c8 | — | /lib/ld-linux-armhf.so.3 | `libutil.so.1`, `libpam.so.0`, `libcrypt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/flash_erase` | ET_EXEC / ARM 32 | 0x1146c | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/hostapd` | ET_EXEC / ARM 32 | 0x13fd0 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libiconv.so.2`, `libubox.so`, `libubus.so`, `libm.so.6`, `libnl-tiny.so`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/htpdate` | ET_EXEC / ARM 32 | 0x11860 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/hwstamp_ctl` | ET_EXEC / ARM 32 | 0x10878 | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/iw` | ET_EXEC / ARM 32 | 0x11740 | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `libnl-tiny.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/memtester` | ET_EXEC / ARM 32 | 0x10eac | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/mkhomedir_helper` | ET_EXEC / ARM 32 | 0x10b78 | — | /lib/ld-linux-armhf.so.3 | `libpam.so.0`, `libc.so.6` |
| `usr/sbin/mlanutl` | ET_EXEC / ARM 32 | 0x10fcc | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/nandwrite` | ET_EXEC / ARM 32 | 0x11c08 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/odhcp6c` | ET_EXEC / ARM 32 | 0x128a4 | — | /lib/ld-linux-armhf.so.3 | `libresolv.so.2`, `libubox.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/odhcpd` | ET_EXEC / ARM 32 | 0x126ac | — | /lib/ld-linux-armhf.so.3 | `libresolv.so.2`, `libubox.so`, `libuci.so`, `libnl-tiny.so`, `libubus.so`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/pam_tally` | ET_EXEC / ARM 32 | 0x11054 | — | /lib/ld-linux-armhf.so.3 | `libc.so.6` |
| `usr/sbin/pam_tally2` | ET_EXEC / ARM 32 | 0x111b0 | — | /lib/ld-linux-armhf.so.3 | `libpam.so.0`, `libc.so.6` |
| `usr/sbin/pam_timestamp_check` | ET_DYN / ARM 32 | 0x11ac | — | /lib/ld-linux-armhf.so.3 | `libpam.so.0`, `libc.so.6` |
| `usr/sbin/phc2sys` | ET_EXEC / ARM 32 | 0x124d4 | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/phc_ctl` | ET_EXEC / ARM 32 | 0x11530 | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/pmc` | ET_EXEC / ARM 32 | 0x11fbc | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ptp4l` | ET_EXEC / ARM 32 | 0x11b4c | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/rtk_hciattach` | ET_EXEC / ARM 32 | 0x11898 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/rtlbtmp` | ET_EXEC / ARM 32 | 0x11da8 | — | /lib/ld-linux-armhf.so.3 | `libpthread.so.0`, `librt.so.1`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/rtwpriv` | ET_EXEC / AArch64 64 | 0x400264 | — | — | — |
| `usr/sbin/syslog-ng` | ET_EXEC / ARM 32 | 0x15a00 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libnsl.so.1`, `libglib-2.0.so.0`, `libintl.so.8`, `libevtlog.so.0`, `libssl.so.1.0.0`, `libcrypto.so.1.0.0`, `libz.so.1`, `libdbi.so.1`, `libpcre.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/tcpdump` | ET_EXEC / ARM 32 | 0x14518 | — | /lib/ld-linux-armhf.so.3 | `libpcap.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/timemaster` | ET_EXEC / ARM 32 | 0x12058 | — | /lib/ld-linux-armhf.so.3 | `libm.so.6`, `librt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubiattach` | ET_EXEC / ARM 32 | 0x10f64 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubiblock` | ET_EXEC / ARM 32 | 0x10c30 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubicrc32` | ET_EXEC / ARM 32 | 0x10848 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubidetach` | ET_EXEC / ARM 32 | 0x10d48 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubiformat` | ET_EXEC / ARM 32 | 0x122d8 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubimkvol` | ET_EXEC / ARM 32 | 0x11250 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubinfo` | ET_EXEC / ARM 32 | 0x10f88 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubinize` | ET_EXEC / ARM 32 | 0x11f84 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubirename` | ET_EXEC / ARM 32 | 0x10b30 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubirmvol` | ET_EXEC / ARM 32 | 0x10dfc | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubirsvol` | ET_EXEC / ARM 32 | 0x10fb4 | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/ubiupdatevol` | ET_EXEC / ARM 32 | 0x1132c | — | /lib/ld-linux-armhf.so.3 | `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/unix_chkpwd` | ET_DYN / ARM 32 | 0x14e4 | — | /lib/ld-linux-armhf.so.3 | `libcrypt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/unix_update` | ET_DYN / ARM 32 | 0x12f0 | — | /lib/ld-linux-armhf.so.3 | `libcrypt.so.1`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/wpa_cli` | ET_EXEC / ARM 32 | 0x11aa4 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/wpa_passphrase` | ET_EXEC / ARM 32 | 0x10778 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libiconv.so.2`, `libm.so.6`, `libnl-tiny.so`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/wpa_supplicant` | ET_EXEC / ARM 32 | 0x19478 | — | /lib/ld-linux-armhf.so.3 | `librt.so.1`, `libiconv.so.2`, `libm.so.6`, `libnl-tiny.so`, `libdl.so.2`, `libgcc_s.so.1`, `libc.so.6` |
| `usr/sbin/xtables-multi` | ET_EXEC / ARM 32 | 0x125c0 | — | /lib/ld-linux-armhf.so.3 | `libiptext.so`, `libiptext4.so`, `libiptext6.so`, `libip4tc.so.0`, `libip6tc.so.0`, `libxtables.so.12`, `libdl.so.2`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |

## Candidate provider map

| Dependency name | Extracted candidates |
| --- | --- |
| `ld-linux-armhf.so.3` | `lib/ld-2.25.so` |
| `libFLAC.so.8` | `usr/lib/libFLAC.so.8.3.0` |
| `libagora-rtc-sdk.so` | `usr/lib/libagora-rtc-sdk.so` |
| `libaivs-message-util.so` | `usr/lib/libaivs-message-util.so` |
| `libaivs_bt.so` | `usr/lib/libaivs_bt.so` |
| `libaivs_sdk.so` | `usr/lib/libaivs_sdk.so` |
| `libanl.so.1` | `lib/libanl-2.25.so` |
| `libasound.so.2` | `usr/lib/libasound.so.2.0.0` |
| `libasound_module_rate_speexrate.so` | `usr/lib/alsa-lib/libasound_module_rate_speexrate.so`, `usr/lib/alsa-lib/libasound_module_rate_speexrate_best.so`, `usr/lib/alsa-lib/libasound_module_rate_speexrate_medium.so` |
| `libblobmsg_json.so` | `lib/libblobmsg_json.so` |
| `libbluetooth.so.3` | `usr/lib/libbluetooth.so.3.18.10` |
| `libbt_common.so` | `usr/lib/libbt_common.so` |
| `libc.so.6` | `lib/libc-2.25.so` |
| `libcares.so.2` | `usr/lib/libcares.so.2.2.0` |
| `libcoap-1.so.0` | `usr/lib/libcoap-1.so.0.0.0` |
| `libconfig++.so.11` | `usr/lib/libconfig++.so.11.0.2` |
| `libconfig.so.11` | `usr/lib/libconfig.so.11.0.2` |
| `libcrypt.so.1` | `lib/libcrypt-2.25.so` |
| `libcrypto.so.1.0.0` | `usr/lib/libcrypto.so.1.0.0` |
| `libcurl.so.4` | `usr/lib/libcurl.so.4.4.0` |
| `libdbi.so.1` | `usr/lib/libdbi.so.1.1.0` |
| `libdbus-1.so.3` | `usr/lib/libdbus-1.so.3.14.5` |
| `libdl.so.2` | `lib/libdl-2.25.so` |
| `libdns_sd.so` | `usr/lib/libdns_sd.so` |
| `libdts.so` | `usr/lib/libdts.so` |
| `libevent-2.0.so.5` | `usr/lib/libevent-2.0.so.5.1.10` |
| `libevent_pthreads-2.0.so.5` | `usr/lib/libevent_pthreads-2.0.so.5.1.10` |
| `libevtlog.so.0` | `usr/lib/libevtlog.so`, `usr/lib/libevtlog.so.0`, `usr/lib/libevtlog.so.0.0.0` |
| `libexpat.so.1` | `usr/lib/libexpat.so.1.6.7` |
| `libfdk-aac.so.1` | unresolved |
| `libfdk-aac.so.2` | `usr/lib/libfdk-aac.so.2.0.2` |
| `libffi.so.6` | `usr/lib/libffi.so.6.0.4` |
| `libffmpeg-miplayer.so` | `usr/lib/libffmpeg-miplayer.so` |
| `libformw.so.6` | `usr/lib/libformw.so.6.0` |
| `libfstools.so` | `lib/libfstools.so` |
| `libgcc_s.so.1` | `lib/libgcc_s.so.1` |
| `libgio-2.0.so.0` | `usr/lib/libgio-2.0.so.0.5501.0` |
| `libglib-2.0.so.0` | `usr/lib/libglib-2.0.so.0.5501.0` |
| `libglog.so.0.3.5` | `usr/lib/libglog.so.0` |
| `libgmodule-2.0.so.0` | `usr/lib/libgmodule-2.0.so.0.5501.0` |
| `libgmp.so.10` | `usr/lib/libgmp.so.10.3.2` |
| `libgobject-2.0.so.0` | `usr/lib/libgobject-2.0.so.0.5501.0` |
| `libgssdp-1.0.so.3` | `usr/lib/libgssdp-1.0.so.3.0.1` |
| `libgthread-2.0.so.0` | `usr/lib/libgthread-2.0.so.0.5501.0` |
| `libgupnp-1.0.so.4` | `usr/lib/libgupnp-1.0.so.4.0.1` |
| `libhogweed.so.4` | `usr/lib/libhogweed.so.4.3` |
| `libical.so.0` | `usr/lib/libical.so.0.48.0` |
| `libiconv.so.2` | `usr/lib/libiconv.so.2.4.0` |
| `libidmsdk.so` | `usr/lib/libidmsdk.so` |
| `libintl.so.8` | `usr/lib/libintl.so.8.1.5` |
| `libiotdcm.so` | `usr/lib/libiotdcm.so` |
| `libip4tc.so.0` | `usr/lib/libip4tc.so.0.1.0` |
| `libip6tc.so.0` | `usr/lib/libip6tc.so.0.1.0` |
| `libiptext.so` | `usr/lib/libiptext.so` |
| `libiptext4.so` | `usr/lib/libiptext4.so` |
| `libiptext6.so` | `usr/lib/libiptext6.so` |
| `libir-agent-codec.so` | `usr/lib/libir-agent-codec.so` |
| `libjson-c.so.2` | `usr/lib/libjson-c.so.2.0.2` |
| `libjson_script.so` | `lib/libjson_script.so` |
| `libldns.so.1` | `usr/lib/libldns.so.1.6.17` |
| `liblibma.so` | `usr/lib/liblibma.so` |
| `libm.so.6` | `lib/libm-2.25.so` |
| `libmbedcrypto.so.0` | `usr/lib/libmbedcrypto.so.2.6.0` |
| `libmbedtls.so.10` | `usr/lib/libmbedtls.so.2.6.0` |
| `libmbedtls.so.9` | `usr/lib/libmbedtls.so.1.3.14` |
| `libmbedx509.so.0` | `usr/lib/libmbedx509.so.2.6.0` |
| `libmdspeech.so` | `usr/lib/libmdspeech.so` |
| `libmenuw.so.6` | `usr/lib/libmenuw.so.6.0` |
| `libmibrain-common-sdk.so` | `usr/lib/libmibrain-common-sdk.so` |
| `libmibrain-common-util.so` | `usr/lib/libmibrain-common-util.so` |
| `libmibrainsdk.so` | `usr/lib/libmibrainsdk.so` |
| `libmico-common.so` | `usr/lib/libmico-common.so` |
| `libmicowpa.so` | `usr/lib/libmicowpa.so` |
| `libmilink.so` | `usr/lib/libmilink.so` |
| `libncursesw.so.6` | `usr/lib/libncursesw.so.6.0` |
| `libnettle.so.6` | `usr/lib/libnettle.so.6.3` |
| `libnl-tiny.so` | `usr/lib/libnl-tiny.so` |
| `libnsl.so.1` | `lib/libnsl-2.25.so` |
| `libogg.so.0` | `usr/lib/libogg.so.0.8.2` |
| `libopus.so.0` | `usr/lib/libopus.so.0.6.1` |
| `libopusfile.so.0` | `usr/lib/libopusfile.so.0.4.4` |
| `libpam.so.0` | `lib/libpam.so.0.84.1` |
| `libpam_misc.so.0` | `lib/libpam_misc.so.0.82.1` |
| `libpanelw.so.6` | `usr/lib/libpanelw.so.6.0` |
| `libpcap.so.1` | `usr/lib/libpcap.so.1` |
| `libpcre.so.1` | `usr/lib/libpcre.so.1.2.9` |
| `libprocps.so.5` | `usr/lib/libprocps.so`, `usr/lib/libprocps.so.5`, `usr/lib/libprocps.so.5.0.0` |
| `libprotobuf-c.so.1` | `usr/lib/libprotobuf-c.so.1.0.0` |
| `libprotobuf-lite.so.13` | `usr/lib/libprotobuf-lite.so.13.0.0` |
| `libpthread.so.0` | `lib/libpthread-2.25.so` |
| `libreadline.so.7` | `usr/lib/libreadline.so.7.0` |
| `libresolv.so.2` | `lib/libresolv-2.25.so` |
| `librt.so.1` | `lib/librt-2.25.so` |
| `libsbc.so.1` | `usr/lib/libsbc.so.1.2.1` |
| `libsndfile.so.1` | `usr/lib/libsndfile.so.1.0.28` |
| `libsoup-2.4.so.1` | `usr/lib/libsoup-2.4.so.1.8.0` |
| `libspeexdsp.so.1` | `usr/lib/libspeexdsp.so.1.5.0` |
| `libsqlite3.so.0` | `usr/lib/libsqlite3.so.0.8.6` |
| `libssl.so.1.0.0` | `usr/lib/libssl.so.1.0.0` |
| `libstdc++.so.6` | `usr/lib/libstdc++.so.6.0.22` |
| `libthrift_c_glib.so.0` | `usr/lib/libthrift_c_glib.so.0.0.0` |
| `libubox.so` | `lib/libubox.so` |
| `libubus.so` | `lib/libubus.so` |
| `libuci.so` | `lib/libuci.so` |
| `libuclient.so` | `usr/lib/libuclient.so` |
| `libutil.so.1` | `lib/libutil-2.25.so` |
| `libuuid.so.1` | `usr/lib/libuuid.so.1.3.0` |
| `libvalidate.so` | `lib/libvalidate.so` |
| `libwpa_client.so` | `usr/lib/libwpa_client.so` |
| `libxaudio_engine.so` | `usr/lib/libxaudio_engine.so` |
| `libxiaomi_crypto.so` | `usr/lib/libxiaomi_crypto.so` |
| `libxiaomi_dns.so` | `usr/lib/libxiaomi_dns.so` |
| `libxiaomi_heartbeat.so` | `usr/lib/libxiaomi_heartbeat.so` |
| `libxiaomi_http.so` | `usr/lib/libxiaomi_http.so` |
| `libxiaomi_json.so` | `usr/lib/libxiaomi_json.so` |
| `libxiaomi_mico.so` | `usr/lib/libxiaomi_mico.so` |
| `libxiaomi_miot.so` | `usr/lib/libxiaomi_miot.so` |
| `libxiaomi_mosquitto.so` | `usr/lib/libxiaomi_mosquitto.so` |
| `libxiaomi_utils.so` | `usr/lib/libxiaomi_utils.so` |
| `libxiaomimediaplayerlite.so` | `usr/lib/libxiaomimediaplayerlite.so` |
| `libxml2.so.2` | `usr/lib/libxml2.so.2.9.7` |
| `libxtables.so.12` | `usr/lib/libxtables.so.12.0.0` |
| `libz.so.1` | `usr/lib/libz.so.1.2.11` |
