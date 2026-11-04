# Report: Native reset independent ARM review

## Scope and evidence

Independent read-only review of extracted `usr/bin/miio_helper` and `usr/lib/libmico-common.so`, using host objdump ARM disassembly/relocations plus raw little-endian word, PT_LOAD and PC-relative literal reconstruction. No factory code, device command or reset method was executed; no actual account, configuration, log, key or user data was read. Required project references and prove-it instructions were consulted. This report does not promote any feature-wide facet.

Hashes agree with the main report: miio_helper `2780bb24bed5cfc3216f638ad074422d753d77a4715bdc7e36bd4008d2a9d5da`; libmico-common.so `a8fe45c6b7f5a20e348223e3f359265eba86d3520b0cfc5dc0ad1e2d488cf5ae`.

## Independently confirmed reset chain

The 12-byte record at VMA `0x3fb68` (file `0x1fb68`, second LOAD mapping) contains name pointer `0x29ddf` (`miIO.clear_user_data`), flag word `0x10000`, handler `0x20580`. The handler requires state word +0x59b0 nonzero and byte +0x5c24 equal to one. Its successful branch calls user_data_clean through PLT 0x14f68/GOT relocation 0x3f4ec, then sets JSON result to zero without checking the call result. The rejected branch constructs -1. The JSON contains id from argument2 and result, and is serialized/passed to 0x18a54. Remote delivery remains unproved.

Library user_data_clean 0x1a70c independently resolves to three dir_remove calls (alarm, timer, chimehourly directories), two unlink calls (recognition record, old mediaplayer config path), player_pause, sleep(1), sync, sys_reboot, return0. Individual results are ignored. This is distinct from the shell reset script: it additionally removes chimehourly and does not recursively remove the old player directory through unlink.

The dir_remove 0x138a4 wrapper at 0x2d5d0 passes version3 to __lxstat (PLT0xe164), not __xstat. It skips exact dot/dotdot names, constructs paths, recursively removes directory-mode entries, unlinks others and stops on error. Thus symlink entries are not deliberately traversed as directories by this test. Races and actual filesystem effects are untested. sys_reboot0x1a6fc tail-calls system with literal /usr/bin/reboot.sh; the script pauses playback, cuts logs, marks upload_log, requests shutdown sound, signals udhcpc and reboots without guarding earlier failures.

## Newly recovered cloud-state gate lineage

The 12-byte native method table also maps `local.status` to handler `0x1c7c0` (record VMA0x3f9e8, raw flags0). Handler0x1c7c0 converts its fourth argument using json_object_get_string at0x1c7cc. Null conversion returns -1 without the status writes described below.

Exact comparisons at0x1c7f4/0x1c804/0x1c818 inspect device_init, didkey_done and token_done while state+0x59e0 is not7; none of those names is an observed runtime value. These branches can update that separate initialization state. The handler then compares against cloud_connected (literal0x2ac98) at0x1c838–0x1c848. That match eventually stores1 into state+0x59b0 at0x1c8ec–0x1c8f8, unless it was already1 (an ancillary context field may be cleared in that case).

At0x1c900–0x1c910 it compares against wifi_connected (literal0x2b1a5). **A wifi_connected match preserves the cloud-state word and returns; it does not clear it.** Every remaining non-null/non-cloud_connected/non-wifi_connected string follows0x1c940–0x1c9a8 and writes0. No exact cloud_disconnected discriminator is required by this default branch. Therefore the reset word is supported as a cloud-connectivity state lineage, not as authenticated-account authorization. This corrects the initial abbreviated agent message which had grouped the wifi_connected branch with the clearing path.

## Native lookup and flag consumption

The table is58 contiguous 12-byte entries starting0x3f9c4. Lookup0x18404 uses index×12, compares the request method to each name, returns the matching record and stops after58 (0x18460). Dispatcher0x19bd0 extracts method (key literal at0x19ee0), calls lookup at0x19c70, extracts params (literal at0x19ef0), and ultimately loads record+8 and BLX-calls it at0x19eac–0x19ebc with context, id, _from and params/whole-request arguments.

Flags are consumed separately by byte/halfword: record byte+4 equal1 triggers the context-pointer comparison and logging at0x19cec–0x19d08; the special context is global-state+0x21d0. That branch goes to logging then rejoins dispatch, so it is not by itself a demonstrated rejection. Record halfword+4 equal0x101, that special context, and cloud word0 trigger the rejection path0x19dcc–0x19e18. Record byte+6 equal1 controls automatic completion/error reply paths at0x19e1c onward. Reset flag0x10000 has bytes0,0,1, so it receives the completion convention but does not match the0x101 cloud-gated case. A single word named auth flag would misdescribe these independently consumed bytes.

A separate 0x18474 table loop processes only entries with nonzero byte+4 and passes names to0x271d0; reset byte+4 is zero and is skipped. This is a registration/filter path, but the helper's outer wire semantics remain open.

## Restore-policy cross-check and initialization owner

Setter0x204f8 gets params array index0, converts to integer, normalizes nonzero to1, calls iot_restore_set and updates state+0x5c24 only on zero return. Library iot_restore_set0x1925c writes enable for argument1 and disable otherwise to /data/miio/miio_restore through file_write_string; positive write result maps to0, other results to-1. Getter0x19204 compares a zero-initialized64-byte read buffer exactly with enable. Query0x2061c returns whether policy byte==1. No actual policy file was read.

The main routine at0x15010 allocates0x5cc0 bytes at0x15080 and stores the state pointer through global0x400c8 at0x1508c. At0x150a4–0x150ac it constructs argument1=state+0x59fc and calls helper0x22a94 at0x150b0. That helper calls iot_restore_get at0x22bb8 and stores its return at argument1-object+0x228 at0x22bd0. Since0x59fc+0x228=0x5c24, this closes startup initialization of the exact policy byte used by the native reset handler. The actual policy value and helper I/O success remain unobserved.

## Remaining evidence gaps and facet recommendation

The native reset subchain now has a statically registered ubus miio.miot_rpc entry and a concrete miio_agent socket-client receive entry, both traced to dispatcher0x19bd0 and the reset handler; startup policy initialization is closed. These facts do not establish successful registration/connection, effective ubus/agent caller permissions, actual cloud or policy values, cleanup completion, reply delivery, full user-data-reset feature entrypoints or recovery. Keep feature-wide facets open; use this report as independent cross-check evidence for the named native subchain only.

## Outer ubus entry registration closed

Independent object decode confirms ubus_add_object at0x21fd0 is passed object0x3fd78; its +0x1c name is miio, +0x34 table0x2c250, +0x38 count14. Table index7 (0-based), record0x2c2f8, maps miot_rpc to0x20b38. Its raw24-byte record has mask0, tags0, n_policy0; a nonnull policy pointer with count0 is not proof of field validation. Existing read-only runtime signature report separately lists miot_rpc:{}.

Handler0x20b38 takes blob payload from the fifth callback argument (stack), serializes through blobmsg_format_json_with_cb, parses using json_tokener_parse, and invokes dispatcher19bd0 at0x20c18. The dispatcher context is fixed global-state+0x4424; a raw word from original callback context+0x30 is copied to state+0x44c8. Its exact semantic identity is unknown; it must not be labelled peer UID, authenticated principal or socket identity.

Dispatcher19bd0 extracts id using json_get_int and method through json_get_string; lookup0x18404 searches58 records, params comes from json_get_obj with whole-request fallback when absent. At0x19eac–0x19ebc it passes (fixed context,id,_from,params-or-request) to the record's handler. This closes a static ubus miio.miot_rpc→native reset dispatch path, subject to object-registration success, ubus caller permissions and reset's own cloud/policy gates. It is not end-to-end or authorization acceptance.

## Stream transport owner: connector rather than listener

The special transport branch of connector0x186d0 tests raw context flags bit0x8000, then calls socket(1,5,0) at0x18704 (AF_UNIX/SOCK_SEQPACKET raw values), copies context+0xb8 into sun_path, and calls connect at0x18738. It does not bind/listen in this branch. The generic branch calls usock with context flags/host/service fields. After success,0x189c8 initializes ustream at context+0x18 and0x189e0 assigns read callback0x19f1c. Callback reads using ustream_read, uses a streaming JSON parser and calls19bd0 at0x1a0d0, subtracting0x18 from the ustream base to recover context. A second connector initialization sequence also assigns19f1c at0x1a680.

The concrete agent connector construction is closed below. No listener is established by these functions. Server-side permissions, peer authentication and the full network/local transport census remain open.

## Complete native table coordinates (name-based triage only)

These58 records are statically traversed by lookup18404. The category is lexical triage, not an assertion of implemented effects, external reachability or authorization. No method was invoked.

| Record VMA | Method | Raw flags | Handler | Name-based review bucket |
| --- | --- | --- | --- | --- |
| 0x3f9c4 | miIO.restore | 0x10000 | 0x1c0a4 | destructive/policy candidate; static only |
| 0x3f9d0 | miIO.reboot | 0x10000 | 0x1c15c | destructive/policy candidate; static only |
| 0x3f9dc | miIO.info | 0x0 | 0x1c16c | read/query candidate; side effects unproven |
| 0x3f9e8 | local.status | 0x0 | 0x1c7c0 | local/status/event candidate |
| 0x3f9f4 | local.time | 0x0 | 0x1c9f4 | local/status/event candidate |
| 0x3fa00 | local.bind | 0x0 | 0x1cb90 | local/status/event candidate |
| 0x3fa0c | bind_key | 0x0 | 0x1cb90 | other native request |
| 0x3fa18 | local.reg_dev | 0x1 | 0x1c504 | local/status/event candidate |
| 0x3fa24 | local.unreg_dev | 0x1 | 0x1c6f4 | local/status/event candidate |
| 0x3fa30 | set_microphone_MicrophoneMute | 0x10001 | 0x1ad7c | state/control candidate |
| 0x3fa3c | set_speaker_SpeakerMute | 0x10001 | 0x1ae5c | state/control candidate |
| 0x3fa48 | set_speaker_SpeakerVolume | 0x10001 | 0x1af08 | state/control candidate |
| 0x3fa54 | set_speaker_up | 0x10001 | 0x1afc8 | state/control candidate |
| 0x3fa60 | set_speaker_down | 0x10001 | 0x1b00c | state/control candidate |
| 0x3fa6c | set_speaker_SpeakerRate | 0x10001 | 0x1b050 | state/control candidate |
| 0x3fa78 | set_IntelligentSpeaker_SleepMode | 0x10001 | 0x1b174 | state/control candidate |
| 0x3fa84 | set_NoDisturb_NoDisturbSwitch | 0x10001 | 0x1b20c | state/control candidate |
| 0x3fa90 | set_NoDisturb_EnableTimePeriod | 0x10001 | 0x1b29c | state/control candidate |
| 0x3fa9c | get_prop | 0x1 | 0x1b514 | read/query candidate; side effects unproven |
| 0x3faa8 | wakeup | 0x10001 | 0x1b73c | other native request |
| 0x3fab4 | play_pause | 0x10001 | 0x1b76c | state/control candidate |
| 0x3fac0 | play_fm | 0x10101 | 0x1b838 | state/control candidate |
| 0x3facc | play_music | 0x10101 | 0x1b838 | state/control candidate |
| 0x3fad8 | play_next | 0x10101 | 0x1b87c | state/control candidate |
| 0x3fae4 | play_prev | 0x10101 | 0x1b8c0 | state/control candidate |
| 0x3faf0 | tts | 0x10101 | 0x1b9e4 | other native request |
| 0x3fafc | mic_on | 0x10001 | 0x1bb10 | other native request |
| 0x3fb08 | mic_off | 0x10001 | 0x1bb54 | other native request |
| 0x3fb14 | alarm_stop | 0x10001 | 0x1bb98 | other native request |
| 0x3fb20 | tv_switch_on | 0x10001 | 0x1baa4 | other native request |
| 0x3fb2c | custom_directives | 0x10101 | 0x1b904 | other native request |
| 0x3fb38 | action | 0x1 | 0x196f0 | state/control candidate |
| 0x3fb44 | get_properties | 0x0 | 0x18f78 | read/query candidate; side effects unproven |
| 0x3fb50 | set_properties | 0x0 | 0x18fac | state/control candidate |
| 0x3fb5c | miIO.disable_local_restore | 0x10000 | 0x204f8 | destructive/policy candidate; static only |
| 0x3fb68 | miIO.clear_user_data | 0x10000 | 0x20580 | destructive/policy candidate; static only |
| 0x3fb74 | miIO.get_disable_local_restore | 0x10000 | 0x2061c | decoded read/query of policy byte; no persistent write on reviewed path |
| 0x3fb80 | miIO.ir_play | 0x0 | 0x24490 | state/control candidate |
| 0x3fb8c | miIO.ir_play_batch | 0x0 | 0x2472c | state/control candidate |
| 0x3fb98 | miIO.ir_read | 0x0 | 0x249f8 | other native request |
| 0x3fba4 | miIO.ir_learn | 0x0 | 0x24d94 | other native request |
| 0x3fbb0 | miIO.ir_learn_stop | 0x0 | 0x24eb0 | other native request |
| 0x3fbbc | bt_gateway_status | 0x0 | 0x2611c | other native request |
| 0x3fbc8 | bt_gateway_enable | 0x10000 | 0x26610 | other native request |
| 0x3fbd4 | bt_gateway_disable | 0x10000 | 0x26518 | other native request |
| 0x3fbe0 | _local.miot_device_scan_result | 0x10000 | 0x25bb8 | other native request |
| 0x3fbec | _local.miot_bind_device_result | 0x10000 | 0x25d44 | other native request |
| 0x3fbf8 | _internal.info | 0x0 | 0x1cf7c | read/query candidate; side effects unproven |
| 0x3fc04 | _internal.req_didkeymac2 | 0x0 | 0x1e530 | other native request |
| 0x3fc10 | _internal.req_wifi_conf_status | 0x0 | 0x1d9a4 | other native request |
| 0x3fc1c | _internal.wifi_start | 0x0 | 0x1e450 | other native request |
| 0x3fc28 | _internal.request_dinfo | 0x0 | 0x1e5c0 | read/query candidate; side effects unproven |
| 0x3fc34 | _internal.request_dtoken | 0x0 | 0x1e81c | other native request |
| 0x3fc40 | _internal.request_ot_config | 0x0 | 0x1e9a4 | other native request |
| 0x3fc4c | _internal.update_dtoken | 0x0 | 0x1eb20 | other native request |
| 0x3fc58 | _internal.record_offline | 0x0 | 0x1ec7c | other native request |
| 0x3fc64 | _internal.wifi_scan_req | 0x0 | 0x1ecd8 | other native request |
| 0x3fc70 | _internal.wifi_disconnect_req | 0x0 | 0x1f044 | other native request |

## Concrete agent context and startup transport construction

Main0x15010 constructs the agent connection at0x1514c–0x15174: first argument=state+0x21d0, second literal agent, third literal mijia_agent, raw flags fourth argument0x8000, fifth stack argument `/tmp/miio_agent.socket` (literal at0x15284), sixth stack argument0. Constructor0x1a2d8 saves the fifth argument then strdup-copies it into context+0xb8 at0x1a388–0x1a394; it saves flags atcontext+0xb0. Its socket1/5/connect path and stream-read callback are as traced above. Thus received frames from this client connector reach native dispatcher with context=state+0x21d0, exactly matching the dispatcher's special-context comparison. This is a static client path to the independently identified miio_agent listener, not a claim that the current device connection is established or authenticated.

The same main also initializes state and state+0x10e8 contexts at0x15128 and0x15148 with raw flags0x4800, fixed host127.0.0.1 and service54322, named internal/mico and ot_client. They use the generic usock path. They are distinct transport contexts and must not be silently collapsed into the agent socket. The later reconnect routine0x1a720 independently passes state+0x21d0 into connector0x186d0 at0x1a748, corroborating the context arithmetic. Full protocol and authorization for each context remain open.

## Distinct native restore and reboot handlers

The table's first two entries map miIO.restore→0x1c0a4 and miIO.reboot→0x1c15c, both rawflags0x10000. These are distinct from miIO.clear_user_data, not aliases of its gate or file scope. MSTC_EnterFactory is not a name in this58-entry table; its separate manufacturing path is not closed here.

Restore handler0x1c0a4 reads params string field deviceID using json_get_string at0x1c0b0. If no string pointer is returned, it directly takes the restore branch0x1c11c. Otherwise it compares against state+0x5a50 through str_case_equal at0x1c0cc; return0 takes restore, nonzero logs and returns-1. Independently inspected str_case_equal export0x14700 requires equal lengths and delegates to strncasecmp, normalizing equality to0 and mismatch to-1. No current device identifier was read. Missing field therefore is not an automatic rejection in this local code. This is a conditional identity-comparison contract, not proven caller authentication. The handler does not itself read the cloud/policy gates used by clear_user_data; dispatcher rawflags0x10000 also does not match its special0x101 cloud restriction.

On the restore branch0x1c13c calls dev_restore through PLT0x14980 then returns0 without checking the helper return. Library dev_restore0x1a49c calls pipe_exe with null output, length0 and literal `/bin/unbind_device.sh &`, then returns0. pipe_exe0x10e34 formats the command, popens it and, for null output, jumps to pclose at0x10f58. The ampersand requests shell background execution; completion of the helper does not establish completion of the background unbind script or its reboot. No runtime timing is measured.

The unbind script checks a temporary in-progress marker and attempts an early top-level return, then creates the marker. It conditionally requests carrier unregister for a non-mico register type when the service script exists. In an andlink branch it fsyncs and temporarily moves the carrier directory. It pauses playback, stops Bluetooth for board IDs3/6/9, stops wireless and requests a shutdown sound. It issues two literal recursive deletion commands with `/data/*` and `/data/.*`, suppressing their errors, conditionally restores the carrier directory, removes the marker, syncs, sleeps1 and calls reboot -f. No checked backup/rollback or deletion-result gate is present. Exact shell handling of dot entries, mounted subtrees, interruptions and actual storage effects remains unverified. This broad source path is distinct from selective native clear_user_data and from overlay reset.

Reboot handler0x1c15c directly calls sys_reboot through PLT0x14bfc and only then returns0. sys_reboot uses system(`/usr/bin/reboot.sh`), with no background ampersand in that literal. The reviewed handler therefore does not establish a reply-before-reboot or deferred asynchronous reboot guarantee. It also does not itself check the clear-user-data cloud/policy gates. Actual shutdown or reply delivery remains untested.

Source SHA-256 `bin/unbind_device.sh`: `6b9794a31479264301bac5558af2cfd1dd585d0e2f3da70dcd44007f429223e2`.

Source SHA-256 `usr/bin/reboot.sh`: `8bb1d6a5d279781e160a58ef4fa73c5ca0c62cf5952b97bcef2bb78861ca2faf`.
