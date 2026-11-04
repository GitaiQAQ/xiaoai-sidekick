# Report: Factory init script static census

Scope: all regular files immediately under extracted `etc/init.d`. No scripts executed and no configuration values read. Function names and literal assignment expressions are lexical evidence, not evaluated shell behavior. Shell includes, generated functions and dynamically assigned priorities require separate review.

The inittab names `/etc/init.d/rcS S boot` and `/etc/init.d/rcS K shutdown`, but rcS is absent from this extraction. `etc/rc.common` implements enable/disable through rc.d symlinks and sources each init script; USE_PROCD replaces start/stop with procd service calls. Because the extraction has no symlinks and normalized permissions, START/STOP values cannot prove enabled services or boot order. `etc/rc.local` contains only comments and exit 0.

Regular init files: 55.

| File | START expression | STOP expression | USE_PROCD expression | Lexical functions | SHA-256 |
| --- | --- | --- | --- | --- | --- |
| adbd | 50 | — | — | usb_rndis_init, usb_adbd_init, usb_net_ipconfig, start_usbgadget, start | 7a8a5dc084280d8f23c8ab2f8305681696d7f52ae636d6e6d64638495e04d514 |
| alarm | 70 | — | 1 | alarm_log, sync_failed, sync, config_prepare, start_service | f5a82d8d5284014eaa056a8db76e5f55ef511d5278b96375a56306685edb44a8 |
| alsa | 15 | 10 | — | boot, stop | 9b09168fced13a9030f32fd70218dfe185bb353fe863b19eba26039e37b256aa |
| bluetooth | 75 | 30 | 1 | bluealsa_start, bluez_mibt_classical_start, bluez_start, marvell_setup, start_service, stop_service | 2d4d07b39c0720b51469a3b80ada30d6c35694f75dbe64c6c4050017d0992673 |
| bluetooth_aplay | — | — | 1 | bluealsa_start, start_service | dc3a4cbf9a440af4cf831304ce3cdc388ba8e16cd59d33ab860544a57b61b338 |
| bluetooth_ble | — | — | 1 | bluez_mibt_ble_start, start_service | ac4c7d79f0009888ee5f0a52dc2e6b2f3b09e859f86c97ea33620a37a53163fb |
| bluetoothd | 62 | — | 1 | bluetoothd_start, start_marvell, start_marvell_8987, start_rtk, start_service, stop_service | 048a74b947386dc352602cb5d89fb1cae6f51a899fe404e11a04b209a9edb847 |
| boot | 10 | 98 | — | l06a_board_config_fix, binfo_create_lx06, rtl8821_cfg, wifi_bt_driver_prepare_lx06, tas5805_dts_compatible, boot | 8c404213a3ec2177e0f5b7cbc3eccd6bfd4dd9e754e809157f23a3016baad062 |
| boot-test-end | 99 | 1 | — | boot | 95c4ea18265f6a8a437a6717c985e76be39cedf2e77a5dc965a45f644f6fa608 |
| boot-test-start | 10 | 98 | — | boot | 066040b608a2918b448f89bba6b016eb61cea807cdab5997743b7f76b7dcbd0a |
| central_lite | — | — | 1 | start_service | 4a138c3edc15c98f3f5a76d9084c11c70b7843dc30c2caaf48cbed157f05c333 |
| coredump | 70 | 01 | — | start, stop | 90f0f2c9a16d98a086d203f1560621cf7c7830d150934fb62d9678d3c1d72fa8 |
| cron | 50 | — | 1 | validate_cron_section, start_service, service_triggers | 9dada5dee5d490e68d443e5c629cb3ac9132d19fef2def0d5b567143750c3402 |
| dbus | 60 | — | — | start, stop | 3bf94740a5f589c70486110cfa7db4ff6b9dbb32368ebe0bda41c1cd561f344b |
| dhcpc | — | — | 1 | mico_log, renew, start_service, stop_service, restart | aa42677baeecf618375fccd526671066ff57dfa781ffbb9b43195b39cfacf4cf |
| dhcpc_p2p | — | — | 1 | mico_log, start_service, stop_service | 16354a02bcf4e4f2868a18697b695d0f406c27a92419fffe79629c0c4a9876ba |
| dhcpd | — | — | 1 | mico_log, config_file_create, start_service, stop_service | 34b2a081293358ddc00fdc54ac5f97cb5e39388314314d77774b2db42e798f7d |
| dlnainit | 90 | — | 1 | config_device_file, ota_updata_deicie, start_service, restart | 1d5ec98eda0032a4ea4d2d1e737bf42dc180c6fc0d47c9241caf6579a9cd4e29 |
| dnsmasq | 62 | — | 1 | mico_log, service_triggers, boot, start_service, reload_service, stop_service | 44107a54bbff679615e1f1d38fb05948033cc49cd2ffa0878e7b66084310c718 |
| done | 95 | — | — | boot | a8c3fa577f0ce3f6f2990f3c4f9bc166953bea6e3088b2fc3d5b7b256ed1c57e |
| dropbear | 50 | 50 | 1 | append_ports, validate_section_dropbear, dropbear_instance, keygen, load_interfaces, start_service, service_triggers, killclients | cc64cf16f9bb6ca7ddd17843284dcfbf62b41449b08bf6cd0cfcbbb2e8d771b3 |
| gpio_switch | 98 | 10 | 1 | load_gpio_switch, service_triggers, start_service | 8d9bf93b0342db1b840ff1f33eb377373d75d5e3fcbeb5c71065cbe8e3b19b5b |
| idmruntime | 79 | — | 1 | start_service | 09b197c077c01b69bccdcca3ba73a72f3ef4cd1cd3a0c57644099215859b4a2a |
| led | 42 | — | 1 | nightmode_config_init, m03a_watchdog_info, start_service, service_started | d1804aba2d682f1fe43b72d752d720243e7642c6efef31d27bc05c61c552a770 |
| linein | 61 | — | 1 | start_service, service_started | fabdce2e7a1e8aa2767e724fd2aaa154be957a00f510f9b050eb135318fa045e |
| logrotate | — | 10 | — | stop | 2816e86d377b17c6b2a3eb16200503e1e8ca4336c276b36bff29c8e1c045b030 |
| mediaplayer | 70 | — | 1 | player_log, country_prepare, start_service, service_started | b190336455397795279313f703d7c68bec27da165e081d301f7328a7b0e751c0 |
| messagingagent | 70 | — | 1 | start_service, stop_service | 138728d7bb898bae7b2601e9e6baa8ac9d4185fc31be3acff097413afbeba904 |
| mibrain_service | 96 | — | 1 | start_service | 9862aed0f4f3e68b23d67441de73d5bcf832dcaf34e1c7dadea9b84e80ba9ad5 |
| mibt_mesh | — | — | 1 | mico_log, mesh_proc_choose, start_service | 6a6388c3f5376e0544d2cd3706be4a044c41bc034bda016e0c5b2ee74bc34943 |
| mico_ai_crontab | 96 | — | 1 | start_service | 66f782220014dcbff3444afd4cf8f31aab3d03d5dfff9aa06810fcea14a27dc3 |
| mico_aivs_lab | 85 | — | 1 | mico_aivs_log, start_service, stop_service | cc17eb3e7b4981098aa145c88804be57dbd931fb0cf7dd9ea19fafd7a432c819 |
| mico_helper | 70 | — | 1 | notify_cfg_try_create, start_service | a4d7fc2b63e1d6bb23f3c021405a1c4c9386772fa11779e1bfd6a153a4b84fba |
| mico_ir_agent | 96 | — | 1 | start_service | 1e60c81a956768a8b94657fc72543fab49e00ef3df41608ae96ad4d985ae6d76 |
| miio | 62 | — | 1 | mico_log, start_service, stop_service | a0eac13e559d642bf87ec09503e44decbee65142f0e37c0f6581b98de4e05c8e |
| miio_agent | 62 | — | 1 | mico_log, start_service | abba761f0b8dc53409cab1c584de78d35412bc76ba3abc00f23b3fc3d9252532 |
| miio_client | 62 | — | 1 | mico_log, start_service | 8e50efaf3a6c2422e1bd5113173cbc117b768ed76d231bcd1b012abc360388bf |
| miio_mpkg | 81 | — | 1 | start_service | e672a25efea809d7f0ae879890a21445b43d75d833b2b932c73841c62058d04d |
| miplay | — | — | 1 | mico_log, start_service, stop_service, service_started | 788136874ab88811c13e77ccdf42bf6dd48c7ad77d0d78b662e234fe3ca3b525 |
| nano_httpd | 96 | — | 1 | start_service | 4dd18911e10f4508adc16a698b42bf028fec6990a308dd76e533228a47ab5ef9 |
| odhcp6c | — | — | 1 | mico_log, start_service | a538d10ecf7161d5cf6c04603108c3c94eb3cc997cc5b25c5d5931cdee059b90 |
| odhcpd | 35 | 85 | 1 | start_service, reload_service, service_triggers | 71bb102004b83237111717ce7f8fdfce6fec0c863e31825d6bbd8a0db145df99 |
| pns | 55 | — | 1 | pns_log, _start_mipns_nuance, _start_mipns_soundai_prepare, _start_mipns_soundai, _start_mipns_xiaomi, _start_mipns_gmems, _start_mipns_soundiot, _start_mipns_aispeech, _start_mipns_horizon, start_service, stop_service, pns_running, service_running, _set_volume, mdplay_action_sync, service_mute, service_unmute, toggle, mic_on, mic_off, mic_open, mic_close, mic_get, refresh, mitv_on, mitv_off, mitv_get | 911e6326b7862f832fc037e04eb2236b1ed38968b417647ae00cbdad0052d0ac |
| pns_ubus_helper | 54 | — | 1 | start_service | e2f4c47158bf6f77c6908ab6111c57331fda275f6da9e128a0af78f4e142f2d8 |
| quickplayer | 41 | — | 1 | start_service | 92502cdfa42ceac99805a3497a51fd0089cc0aaaa6ed74466da50f7b5e240618 |
| silentboot | 99z | — | — | start | 044aab894f9458ec018a8c13699256fa74ece9e79fc144b7bf5e7018bce31b55 |
| sound_effect | 40 | — | 1 | start_service, service_started | 8ca9d7a14dedbc2cfe9c98409959784a6e2dbed32bfaf629584a027fa9c981be |
| statpoints_daemon | — | — | 1 | do_rotate, start_service | 8eb3db2686790e40116f47503aa1150c18da661458a80bce7ca77669f49d9db0 |
| sysctl | 11 | — | — | set_vm_min_free, start | cda625d111d3a38bedcc864d68e57709e8343abf96b31f1557e8f5f4b51b5de4 |
| syslog-ng | 50 | — | — | start, stop, reload | fcd7978af71d2f69fe1c6d9c4513ca190ff1f8823f45ea52ea0b7256586bd96b |
| touchpad | 92 | — | 1 | check_sensor_fw, start_service | b88a2496005ccb4c9e72a697219f5e499e0d09b2628a8a4cc710f2c46f741dd1 |
| umount | — | 99 | — | stop | 38e2453d73021489099cb17dbd9076b3096a12e401e7aeb123517f44f98c1bbc |
| urandom_seed | 99 | — | 1 | start_service | 9954389574f10d45d2f8f8d4f736cce55eae506c6ef2803c249bd864b734af05 |
| voip | 96 | — | 1 | start_service, stop_service, clear_data | d44677d34dce67b68d873a391ca1d62c19b270544c2f1ba2e7d4d81c117df68d |
| wireless | 61 | 01 | 1 | wireless_log, file2log, show_led, shut_led, qplay, miplay, miplay_bootup, file_create, file_delete, file_trycp, file_cp, file_mv, try_insmod, rtl8723ds_update_map, bes2600_is_char, mknod_bes2600_dev, firmware_prepare_ap, firmware_prepare_sta, firmware_stop, scan_prepare, scan_restore, player_pause, hostapd_start, hostapd_stop, hostapd_restart, app_restart_check, country_code_delete, country_code_update, config_start_sound, internet_check_others, gateway_check, internet_check, ap_start, conf_prepare, wps_cfg_file_create, wpa_check, sta_start, tcpdump_start, sta_stop, ap_stop, wireless_stop, config_clean_nosound, wait_dhcp_done, wifi, bootup_sound_custom, battery_capacity_broadcast, boot_done_led, boot_done, mico_ble_start, mico_ble_stop, config_mode, andlink_enable, is_bt_classical_discoverable, andlink_env_prepare, andlink_coap_online, connect_times_update, connect_times_get, miio_register_start, miio_internet_check, miio_register_success, ota, delay_show_light, start_service, stop_service, restart | 954ac4ee47b8fc49eda2725d00466f9740d903d67cef0409a9bbb6e86e0f7a24 |

Remaining gaps: source-ROM alias and rc.d metadata, actual init dispatcher, effective configuration, include resolution, all launch conditions and runtime effects. No feature-wide checkpoint is satisfied by this census.

Verification: all 55 recorded SHA-256 values independently matched the system shasum utility. Function-name extraction includes both `name()` and `function name()` forms; it remains lexical and does not evaluate includes or generated shell code.
