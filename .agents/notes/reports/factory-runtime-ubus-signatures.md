# Report: LX06 Runtime ubus Method Signatures

## Capture scope

2026-11-03，目标设备为 LX06 固件 1.94.13。通过 SSH 只读执行 `ubus list` 与 `ubus -v list <object>`，记录当时公开的对象、方法名、参数名和类型。没有调用任何方法，也没有读取账号、设备配置、认证信息或 Mesh 密钥。

本快照包含 26 个对象与 260 条方法签名。签名只描述 ubus 注册时公开的输入结构；它不说明参数是否必填、值域、调用权限、错误语义或副作用。`(unknown)` 表示 ubus introspection 没有给出类型。该快照仅适用于采集时的固件和运行状态。

## Captured signatures

```text
## ai_crontab
	"new":{"personal_skill":"String"}
	"notify":{"src":"String","event":"String","id":"String"}
	"remove_all":{}
	"dump":{"busy":"Integer","free":"Integer"}
## alarm
	"mibrain_operation":{"mibrain":"String"}
	"alarm_create":{"type":"String","timestamp":"Integer","circle":"Integer","volume":"Integer","event":"String","extra":"String","behavior":"String","ringtone_query":"String","ringtone_type":"String","display_txt":"String","time_reminder":"String"}
	"alarm_query":{"type":"String"}
	"alarm_delete":{"type":"String","id":"String"}
	"alarm_close":{"type":"String","id":"String"}
	"alarm_open":{"type":"String","id":"String"}
	"alarm_modify":{"type":"String","id":"String","timestamp":"Integer","circle":"Integer","volume":"Integer","event":"String","extra":"String","behavior":"String","ringtone_query":"String","ringtone_type":"String","display_txt":"String","time_reminder":"String"}
	"create":{"type":"String","timestamp":"Integer","circle":"Integer","volume":"Integer","event":"String","extra":"String","behavior":"String","ringtone_query":"String","ringtone_type":"String","display_txt":"String","time_reminder":"String"}
	"query":{"type":"String"}
	"delete":{"type":"String","id":"String"}
	"close":{"type":"String","id":"String"}
	"open":{"type":"String","id":"String"}
	"modify":{"type":"String","id":"String","timestamp":"Integer","circle":"Integer","volume":"Integer","event":"String","extra":"String","behavior":"String","ringtone_query":"String","ringtone_type":"String","display_txt":"String","time_reminder":"String"}
	"micmute":{"hour":"Integer","minute":"Integer","second":"Integer"}
	"micmute_remove":{}
	"chime_hourly_set":{"status":"String","chime_hourly":"Array"}
	"chime_hourly_get":{}
	"alarm_get_volume":{}
	"alarm_set_volume":{"volume":"Integer","isPreview":"Integer"}
	"alarm_set_work_mode":{"mode":"Integer"}
	"set_store_show_status":{"status":"Integer","auto_silent_status":"Integer","auto_default_volume_status":"Integer","silent_count_down_min":"Integer","default_volume":"Integer"}
	"get_store_show_status":{}
	"wakeup_notify":{}
	"touchpad_notify":{}
	"debug":{}
## dhcp
	"ipv4leases":{}
	"ipv6leases":{}
## ir_agent
	"play":{"async":"Integer","freq":"Integer","code":"String","head":"String"}
	"play_batch":{"async":"Integer","freq":"Integer","interval":"Integer","code":"String","head":"String"}
	"learn":{"key":"String"}
	"learn_stop":{"key":"String"}
	"read":{"key":"String"}
	"codec":{"type":"String","code":"String","length":"Integer"}
## led
	"show":{"L":"Integer","pos":"Integer","rgb":"String"}
	"shut":{"L":"Integer"}
	"status":{}
	"shut_all":{}
## linein
	"start":{}
	"stop":{}
	"play_status":{}
	"line_status":{}
	"set_soft_vol":{"vol":"Integer"}
	"get_soft_vol":{"(unknown)"}
	"notify_stereo_mode":{"mode":"String","output":"String","format":"String","channel":"Integer","samplerate":"Integer"}
## mdplay
	"get_mdplay_status":{}
	"get_miplay_delay":{}
	"notify_wakeup_status":{"status":"Integer"}
	"notify_nlp_result":{"method":"String","param":"String"}
	"player_set_volume":{"volume":"Integer","media":"String"}
	"player_play_operation":{"action":"String","param":"String"}
	"notify_tts_status":{"status":"Integer"}
	"notify_mdplay_play_list":{"method":"String","param":"String"}
	"notify_common_cmd":{"method":"String","param":"String"}
	"set_mdplay_time_offset":{"time_offset":"(unknown)"}
	"casting_status":{}
	"casting_quit":{}
	"casting_pause":{}
	"casting_resume":{}
	"casting_toggle":{}
	"casting_do":{"cmd":"String","args":"String"}
	"casting_set_volume":{"volume":"Integer"}
	"notify_mediaplayer_status":{"volume":"Integer","loop_type":"Integer","media_type":"Integer","audio_id":"String","status":"Integer","position":"Integer","duration":"Integer","cp_id":"String","cp_name":"String"}
	"notify_play_source":{"latency":"String"}
	"notify_play_samplerate":{"rate":"Integer"}
	"player_subs_notify":{"looptype":"Integer","playlist":"Integer"}
	"device_detect":{"refresh":"Integer","timeout":"Integer"}
	"change_speaker_group":{"groupInfo":"String"}
	"query_device_groupInfo":{}
	"speaker_network_online":{}
## mediaplayer
	"player_wakeup":{"action":"String","source":"String"}
	"player_play_operation":{"media":"String","action":"String","message":"String","group":"String","dialog_id":"String","instruction_id":"String"}
	"player_play_url":{"url":"String","type":"Integer","group":"String","media":"String","src":"String","dialog_id":"String","id":"String","instruction_id":"String","duration":"Integer","auth":"Boolean","tts_point":"String"}
	"set_playled_status":{"status":"Integer"}
	"set_playled_action":{"action":"String"}
	"set_dlna_status":{"status":"Integer"}
	"player_get_play_status":{}
	"player_play_status":{}
	"media_control_play_status":{}
	"player_play_private_fm":{}
	"player_get_latest_playlist":{}
	"get_playled_status":{}
	"get_dlna_status":{}
	"player_set_positon":{"position":"Integer","media":"String"}
	"player_set_loop":{"type":"Integer","media":"String"}
	"player_play_music":{"music":"String","startaudioid":"String","startOffset":"Integer","group":"String","media":"String","src":"String","dialog_id":"String","id":"String","instruction_id":"String","duration":"Integer"}
	"player_set_volume":{"volume":"Integer","media":"String","beep":"Integer","type":"String","unit":"String","tts_silence":"Boolean","lednum":"Integer"}
	"player_set_continuous_volume":{"volume":"Integer","media":"String"}
	"player_modify_volume":{"isVolumeUp":"Integer","value":"Integer","type":"String","unit":"String","tts_silence":"Boolean"}
	"player_get_context":{}
	"player_play_index":{"index":"Integer","media":"String"}
	"player_play_alarm_reminder":{"id":"String","type":"Integer","reminder":"String","volume":"Integer","timeReminder":"String","query":"String"}
	"player_play_resource":{"message":"String","src":"String","volume":"Integer"}
	"player_play_album_playlist":{"type":"Integer","id":"String","startOffset":"Integer","media":"String"}
	"player_play_commad":{"type":"String","dialogid":"String","messages":"String"}
	"media_control":{"player":"String","action":"String","volume":"Integer"}
	"player_set_shutdown_timer":{"action":"String","hour":"Integer","minute":"Integer","second":"Integer","media":"String"}
	"test":{}
	"get_shutdown_timer":{}
	"get_media_volume":{}
	"get_alarm_volume":{}
	"player_reset":{}
	"player_retore_last_volume":{}
	"set_voip_status":{"voip_status":"String"}
	"set_player_quiet":{"quiet":"Boolean"}
	"set_playrate":{"rate":"String"}
	"player_aux_operation":{"aux_operation":"String","aux_tip":"Integer"}
	"notify_mdplay_status":{"status":"Integer","type":"Integer"}
	"set_mdplay_info":{"volume":"Integer","play_status":"Integer","loop_type":"Integer","media_type":"Integer","audio_id":"String","audio_pos":"Integer","audio_length":"Integer","cp_name":"String","cp_id":"String"}
	"mdplay_action_sync":{"action":"String","source":"String"}
	"get_mdc_active_status":{}
	"miplay_set_subs":{"looptype":"Integer","playlist":"Integer"}
	"player_event":{"event":"Integer","ext1":"Integer","ext2":"Integer"}
## messagingagent
	"notify":{"payload":"String"}
	"identify_device":{}
	"miot_push":{"payload":"String"}
	"report_events":{"payload":"String"}
	"add_device_log":{"mac":"String","payload":"String"}
	"set_kv":{"key":"String","value":"String"}
	"close":{}
	"get_pid":{}
	"fault":{}
	"sync_admin":{}
	"get_ai_service_token":{}
	"keep_alive":{"set":"Boolean"}
## mibluealsa
	"notify_stereo_mode":{"mode":"String","output":"String","format":"String","channel":"Integer","samplerate":"Integer"}
	"notify_play_status":{"status":"String"}
## mibrain
	"nlp_result_get":{}
	"text_to_speech":{"text":"String","caller":"String","vendor":"String","codec":"String","volume":"Integer","save":"Integer","play":"Integer"}
	"vendor_switch":{"vendor_name":"String"}
	"vendor_who":{}
	"ai_service":{"bypass":"String","caller":"String","duration":"Integer","id":"String","asr":"Integer","nlp":"Integer","tts":"Integer","asr_audio":"String","nlp_text":"String","nlp_execute":"Integer","tts_text":"String","tts_type":"String","tts_vendor":"String","tts_volume":"Integer","tts_codec":"String","tts_save":"Integer","tts_play":"Integer"}
	"aivs_miio_token_update":{"token":"String","session_id":"String"}
	"aivs_authorization_get":{}
	"aivs_event_post":{"namespace":"String","name":"String","payload":"String"}
	"aivs_track_post":{"event":"String","dialog_id":"String","extend":"String"}
	"aivs_env_switch":{}
	"tts_vendor_switch":{"vendor_name":"String","language_name":"String","switch_src":"String","dialog_id":"String","tone_type":"Integer"}
	"tts_vendor_show":{}
	"get_lan_tone_status":{}
## mibt
	"wakeup_tv":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"connect":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"lock":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"status":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"unpair":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"enable":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"disconnect":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"volume":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"set_device_info":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"play":{"mac":"String","name":"String","recent":"String","action":"String","btmode":"String","volume":"Integer","connect":"Integer","discover":"Integer","btrole":"String","src":"String"}
	"notify_stereo_mode":{"mode":"String","output":"String","format":"String","channel":"Integer","samplerate":"Integer"}
	"miconnect_whitelist_add":{"mac":"String","expiry_date":"String","always_valid":"Boolean"}
	"miconnect_whitelist":{"enable":"Boolean"}
	"audio_relay":{"enable":"Boolean"}
	"notify_state":{"src":"String","event":"String","detail":"String"}
## mibt_mesh
	"mesh_status":{}
	"mesh_scan":{"caller":"String"}
	"mesh_bind":{"is_cancel":"Boolean"}
	"mesh_enable":{"caller":"String"}
	"mesh_disable":{"is_need_reboot":"Boolean"}
	"wakeup_tv":{"tv_mac":"String"}
	"miconnect_enable":{"interval":"Integer","action":"String"}
## miio
	"renew":{}
	"register":{"ssid":"String","psk":"String","identity":"String","uid":"String","bind_key":"String","country":"String","domain":"String","oauth_appid":"String","oauth_token":"String","from":"String"}
	"drop":{}
	"ap_connected":{}
	"config_mode":{}
	"token_write":{"token":"String","session_id":"String","ttl":"String"}
	"power_save":{"value":"Boolean"}
	"miot_rpc":{}
	"notify_mode":{"mode":"String","output":"String"}
	"connect":{"ssid":"String","password":"String","bssid":"String","identity":"String","ssid_5G":"String","password_5G":"String"}
	"enter_bt_mode":{}
	"exit_bt_mode":{}
	"scan_start":{"ssid":"String"}
	"dhcp":{}
## miplay
	"get_mdplay_status":{}
	"get_miplay_delay":{}
	"notify_wakeup_status":{"status":"Integer"}
	"notify_nlp_result":{"method":"String","param":"String"}
	"player_set_volume":{"volume":"Integer","media":"String"}
	"player_play_operation":{"action":"String","param":"String"}
	"notify_tts_status":{"status":"Integer"}
	"notify_mdplay_play_list":{"method":"String","param":"String"}
	"notify_common_cmd":{"method":"String","param":"String"}
	"set_mdplay_time_offset":{"time_offset":"(unknown)"}
	"casting_status":{}
	"casting_quit":{}
	"casting_pause":{}
	"casting_resume":{}
	"casting_toggle":{}
	"casting_do":{"cmd":"String","args":"String"}
	"casting_set_volume":{"volume":"Integer"}
	"notify_mediaplayer_status":{"volume":"Integer","loop_type":"Integer","media_type":"Integer","audio_id":"String","status":"Integer","position":"Integer","duration":"Integer","cp_id":"String","cp_name":"String"}
	"notify_play_source":{"latency":"String"}
	"notify_play_samplerate":{"rate":"Integer"}
	"player_subs_notify":{"looptype":"Integer","playlist":"Integer"}
	"device_detect":{"refresh":"Integer","timeout":"Integer"}
	"change_speaker_group":{"groupInfo":"String"}
	"query_device_groupInfo":{}
	"speaker_network_online":{}
## nightmode
	"set":{"total":"String","light":"String","volume":"String","start":"String","stop":"String"}
	"sleep_mode":{"value":"String","from":"String"}
	"status":{}
	"current":{}
## notify
	"create":{"id":"String","level":"Integer","expire":"Integer","volume":"Integer","text":"String"}
	"query":{"id":"String"}
	"delete":{"id":"String"}
	"play":{}
	"stop":{}
	"clean":{}
	"status":{"level":"Integer"}
	"set":{"level":"Integer","enable":"Integer","autoplay":"Integer","volume":"Integer"}
## path_child_mode
	"set":{"message":"String"}
	"status":{}
## pnshelper
	"event_notify":{"src":"Integer","event":"Integer","detail":"String"}
	"oneshot_set":{"open":"Boolean"}
	"oneshot_get":{}
	"environ_detect_set":{"config":"String"}
	"environ_detect_get":{}
	"db_detect_set":{"config":"String"}
	"db_detect_get":{}
## qplayer
	"play":{"play":"String","light":"Integer"}
## service
	"set":{"name":"String","script":"String","instances":"Table","triggers":"Array","validate":"Array","autostart":"Boolean"}
	"add":{"name":"String","script":"String","instances":"Table","triggers":"Array","validate":"Array","autostart":"Boolean"}
	"list":{"name":"String","verbose":"Boolean"}
	"delete":{"name":"String","instance":"String"}
	"signal":{"name":"String","instance":"String","signal":"Integer"}
	"update_start":{"name":"String"}
	"update_complete":{"name":"String"}
	"event":{"type":"String","data":"Table"}
	"validate":{"package":"String","type":"String","service":"String"}
	"get_data":{"name":"String","instance":"String","type":"String"}
	"state":{"spawn":"Boolean","name":"String"}
## sound_effect
	"set_mode":{"mode":"Integer","effect":"Integer"}
	"get_mode":{}
## system
	"board":{}
	"info":{}
	"reboot":{}
	"watchdog":{"frequency":"Integer","timeout":"Integer","magicclose":"Boolean","stop":"Boolean"}
	"signal":{"pid":"Integer","signum":"Integer"}
	"sysupgrade":{"path":"String","prefix":"String","command":"String"}
## touchpad
	"notify_alarm_status_set":{"status":"Integer"}
	"notify_alarm_status_get":{}
## voice_print
	"get_status":{}
	"get_network_info":{}
	"register_voice_recognize":{}
	"register_voice_payment":{}
	"register_voice_recognize_cancel":{}
	"register_voice_payment_cancel":{}
## voip
	"action":{"vender":"String","event":"String","contact_id":"String","number":"String","name":"String","string":"String"}
	"cmcc_register":{"msisdn":"String","type":"String","operation":"Integer"}
	"cmcc_unregister":{"msisdn":"String","type":"String","operation":"Integer"}
	"cmcc_miscellaneous":{"msisdn":"String","type":"String","operation":"Integer"}
	"query_carrier_type":{}
	"milink_enable":{}
	"milink_disable":{}
```
