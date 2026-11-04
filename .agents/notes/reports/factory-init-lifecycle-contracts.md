# Report: Factory init lifecycle contracts

Read-only local script review, 2026-11-03. Sources are `temp/rootfs-factory-full/etc/init.d/` scripts named below, plus `usr/bin/clear_user_data`, `sbin/firstboot`, `etc/rc.button/reset`, and `usr/sbin/boot_function.sh`. Init source hashes are recorded in [the census](factory-init-static-census.md). These are static command/gate contracts; daemon internals, configured values, effective enablement and runtime success are not established. No factory script was sourced or executed and no user configuration, logs, keys or databases were read.

## Reset scope and completion boundaries

`clear_user_data` removes files matched by `/data/alarm/*` and `/data/timer/*`, one assistant recognition-record file and recursively `/data/.mediaplayerconfig`. It then calls `mphelper pause`, sleeps one second and reboots. Commands are not joined by success guards, so deletion/pause failures do not prevent the reboot request. Shell `*` ordinarily excludes dotfiles; this is not a complete data wipe. The separate mediaplayer init migrates old configuration into `/data/player`, which this reset script does not delete. The reset path therefore does not establish removal of the migrated player files. A caller string in miio_helper is already recorded in the feature ledger, but full caller authorization and reachability remain open.

`firstboot` forwards unquoted positional arguments to jffs2reset; binary reset semantics are not recovered here. Generic reset-button release with SEEN below 1 requests sync/reboot; above 5 and with an overlay mount it starts `jffs2reset -y && reboot` asynchronously. Exactly 1 through 5 satisfies neither release branch. Hardware mapping and actual event timing remain unknown. Never execute these paths as a validation shortcut.

## Media and audio startup

| Script | Static launch contract and side effects | Unresolved boundary |
| --- | --- | --- |
| miplay | No active START assignment; config-done must equal yes; starts mpas, nice -10. Stop notifies mediaplayer status 0 and removes multiroom FIFO. | Effective caller/enablement, concurrent FIFO users and playback effects. |
| quickplayer | START 41; starts quickplayer, nice -20, stdout/stderr disabled. | Playback API and exact real-time behavior. |
| dlnainit | START 90; starts only if parsed dlna_status equals 1. Creates descriptor if absent, replaces friendlyName/UDN and syncs; starts dlna. | Effective config, XML escaping and network publication. |
| linein | START 61; starts linein with respawn tuple 3600/5/5. | Physical signal state and restart behavior of this procd version. |
| bluetooth_aplay | No active START/STOP; invokes bluealsa-aplay with wildcard-looking all-zero address, hci0, default device, A2DP and stereo options. | Address option semantics, enablement, actual negotiation. |
| mediaplayer | START 70; chooses country suffix with CN/default fallback, creates /data/player links; conditionally copies old volume/latest config and unconditionally removes old directory, then syncs; starts daemon, nice -15. | Copy failures are not checked before removal; migrated content and actual links not read. |
| sound_effect | START 40; starts sound_effect with respawn 3600/5/5. | Effect meanings and hardware routing. |
| alsa | START 15/STOP 10; boot calls auen and restores persisted mixer state if file exists, otherwise factory state; stop stores state. | Command results, route and volume on the device. |

## Alarm, notifications and infrared

Alarm startup skips Bluetooth work mode, invokes alarm_translate.sh, prepares workday files, then launches alarmd. Workday preparation returns immediately if destination version file exists; otherwise it uses country files with default fallback and creates links/copies version. Extra action `sync` reads a remote configuration through matool_get_work_day_config, requires response code 0 and a different version, selects exact country filenames, and redirects curl output into two /tmp files. It only checks file existence before removing old destination files, moving replacements, writing/fsyncing version and update marker. Curl exit, nonempty bodies, content schema, move and fsync results are not checked. Redirection can create an empty file before a failed curl, so existence is insufficient to establish valid replacement. No request/download/update was performed.

mico_ai_crontab startup also skips Bluetooth mode; infrared startup has no such work-mode gate in its init script. Both launch their named daemons with stdout/stderr disabled. This difference does not prove that infrared operations work during Bluetooth mode.

mico_helper creates /data/notify and initializes only missing notify_0.cfg through notify_3.cfg with enable=1/autoplay=1, fsyncing each, then starts mico-helper. Existing level configs are retained. Return values of initialization calls are not checked. This is default creation behavior, not current notification policy.

## MIoT, discovery, messaging and telemetry

miio init explicitly starts miio_client and miio_agent before submitting miio_helper to procd; stop explicitly stops both children. All three declare START 62. miio_agent starts only if its binary is a regular file. miio_client uses -l 0 -n 126 -d /tmp/ -o DISABLE_PSM and additionally -o MSC only if micocfg_miot_mjac_se_support equals true. Exact option meanings and secure-element enforcement are unproven.

central_lite has no active START; it submits two procd instances, mdnsd and central_lite with a device-ID argument and /data directory argument. The script queries the ID at source time; no ID was queried in this review. A comment says start when mesh enabled, but comments are not an implemented gate. idmruntime declares START 79 and skips Bluetooth work mode. messagingagent declares START 70, has the same skip gate, starts with 8 handler threads, disables stdout and enables stderr; its stop hook removes a lock file. Stale-lock effects and actual IPC remain unknown.

statpoints_daemon has no active START. Start appends a persistent points file into the temporary points file and removes the persistent source without checking append status. Its do_rotate action appends the temporary file into the persistent file then truncates the temporary source, again without checking append status. These source-loss paths are static observations, not evidence of actual loss or telemetry upload. miio_mpkg simply launches its daemon at START 81; update authorization and package lifecycle require binary review.

## System support and manufacturing access

adbd declares START 50 but returns immediately for release channel. Other channels background USB gadget setup only if the gadget directory is absent. Setup builds RNDIS and ADB functionfs, starts adbd and usb_monitor, binds the controller and configures usb0 as <private-ip>/24. Source contains a serial-derived value with a fixed fallback; no actual serial or account data was read. Directory existence is the idempotence gate; partial prior setup can suppress repair. Authentication and USB exposure require daemon/board evidence.

dbus creates runtime directories, optionally ensures a UUID, and starts the system bus; stop removes the PID file only after service_stop succeeds. sysctl iterates the main config and conf.d files; its memory-derived min-free helper is defined but commented out at the callsite. urandom_seed starts its helper without a respawn parameter in the script. logrotate is a STOP-only hook invoking optional easy_logcut then sync.

boot_function time_init_buildtime first initializes timezone and then sets the clock to a nonempty configured build-time string. This is boot-time clock initialization, not an NTP contract. Configured timezone/build time and synchronization network paths remain unexamined.

## Evidence limits

Lexical/script review can independently corroborate daemon entry names and some cleanup boundaries already recovered from ELF, but cannot satisfy feature-wide entrypoints, complete side effects, safety or independent cross-check facets. Missing rcS/link metadata is deferred. Unresolved binary contracts remain open while other static paths are reviewed.

## Bluetooth, Mesh and speech lifecycle follow-up

bluetoothd selects Realtek for board IDs 3/6/9, NXP UART for 5/7/10/11, otherwise the bt8xxx branch. The NXP path toggles rfkill, attaches at 115200, sends a vendor command, kills hciattach and reattaches at 2000000. Debug mode starts bluetoothd outside procd and starts HCI/kernel-log capture; the normal branch uses procd. No debug file or captured content was read. Configuration directories can be created with mode 0777; the main config is copied afresh and its Name uses alias when nonempty, otherwise BT name. Stop kills attach helpers; debug stop also deletes capture files and forcibly kills bluetoothd. These are source paths, not claims about current permissions or authentication.

bluetooth startup invokes marvell_setup unconditionally, polls for bluetoothd for a bounded loop but proceeds after timeout, starts bluealsa, waits two seconds and starts bluez_mibt_classical. It requests bluetooth_ble start when config_done is absent. BLE init likewise proceeds after its wait timeout and starts bluez_mibt_ble without a security-level override, consistent with the binary-default finding in the coverage ledger. Bluetooth stop resets hci0. No controller command was executed here.

mibt_mesh chooses the rtl binary name for wifi chips 8733bs/8723ds/8821cs, prefers the matching `/data/miio_mpkg/bin` file, then `/usr/bin`, then falls back to `/usr/bin/mibt_mesh`. This corroborates why an updated runtime Mesh ELF cannot be substituted with the extracted factory ELF. Existence alone is the script gate; integrity, authenticity, compatibility and actual current selection are not checked by this source path.

mico_aivs_lab declares START 85, starts the daemon under procd and logs ancestry command lines on lifecycle actions. Its stop hook calls procd_kill and separately uses ps/grep/awk with kill -9. mibrain_service and nano_httpd declare START 96 and skip Bluetooth work mode. pns_ubus_helper starts at 54 with suppressed stdout/stderr. These launch order declarations do not prove enabled boot order.

PNS chooses nuance/xiaomi/gmems/soundiot/aispeech/horizon/soundai from micocfg_speech_vendor, returning without daemon launch for unrecognized vendor. It passes VoIP-specific variants when query status is not idle or the voip flag is present. Successful dispatch is followed by an asynchronous pnshelper start event with no reply check. Stop removes upload/vendor temporary directories, emits the stop event asynchronously and kills a vendor-derived process name; several vendor labels differ from the binary names used in start/running detection, so exact stop coverage needs separate tracing. No actual vendor selection, speech data or process state was read.

PNS mic_get reports on/off solely from existence of `/tmp/mipns/mute`; it is not hardware microphone readback. refresh restarts only when service_running passes. mitv_on/off writes persistent and temporary flags then signals mipns-sai, while mitv_get defaults off if the persistent file is absent. These paths do not establish TV control or acknowledgement by the selected speech backend.

## Network and production-test hooks

DHCP server init selects <private-ip>/24 unless the parsed default gateway equals <private-ip>, then selects <private-ip>/24. It writes a wlan1 pool from .2 to .254 and starts udhcpd; stop clears wlan1 address. This single gateway comparison is not exhaustive subnet-conflict detection. DHCP client init starts udhcpc on wlan0 with simple_dhcp.sh and a hostname option; renew signals the PID read from its PID file, falling back to restart only when cat fails. PID ownership is not verified in that path. P2P client uses wlan1 and simple_dhcp_p2p.sh. odhcp6c uses its separate script on wlan0; odhcpd registers a dhcp reload trigger. No current lease, route or hostname was read.

boot-test-start/end both return for release channel, otherwise chmod and background an existing respective /data/auto_test script. adbd has a similar release gate, but these source gates do not prove build channel or contents of those scripts. silentboot handles LX06 by clearing 18 LED positions at one of two I2C paths selected through module-name presence, then attempts silent flag clearing with bounded retries and still returns zero after logged failure. gpio_switch writes configured pin output values; actual sections were not inspected in this pass.
