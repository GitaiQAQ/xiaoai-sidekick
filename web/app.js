'use strict';
const $=id=>document.getElementById(id);
let snapshot=null,csrf='',online=false,busy=false,stream=null,lastSeen=0,qrTimer=null,qrUntil=0,serial=0;
let mode=0,modeLoaded=false,dragging=false,seeking=false;
const pb={position:0,duration:0,status:0,at:0};
const phaseNames={stopped:'已停止',standby:'本地待机',connecting:'正在连接',listening:'正在收音',thinking:'正在思考',speaking:'正在回答',followup:'等待追问',stopping:'正在停止',error:'运行异常',running:'运行中'};
const labels={voice_start:'启动豆包助手',voice_listen:'开始一次对话',voice_stop:'停止豆包助手',music_pause:'暂停音乐',music_resume:'继续播放',music_stop:'停止音乐',music_play:'播放音频链接',music_seek:'调整播放进度',music_mode_get:'读取播放模式',music_mode_set:'切换播放模式',volume:'调整音量',lamp_clear:'清除会话灯效',netease_login:'获取登录二维码',netease_check:'查询扫码状态',netease_refresh:'刷新网易云权益',netease_logout:'退出网易云账号',netease_search:'搜索网易云歌曲',netease_play:'播放网易云歌曲',netease_charts:'读取榜单',netease_chart_tracks:'读取榜单曲目',netease_chart_play:'播放榜单',netease_playlists:'读取我的歌单',netease_playlist_play:'播放歌单'};
const errors={not_logged_in:'请先登录网易云账号',login_required:'请先验证登录状态',qr_key_rejected:'网易云未返回有效二维码，请重试',network_or_tls_failed:'连接网易云失败，请稍后重试',http_or_tls_rejected:'网易云暂未接受请求',session_not_confirmed:'登录状态未能确认，请刷新或重新扫码',login_cookie_missing:'扫码成功但未收到登录会话，请重试',session_save_failed:'登录会话保存失败',preview_only:'这首歌仅返回试听，未开始播放',stream_unavailable:'当前账号或所选音质没有可用音源',playback_not_confirmed:'未能确认播放成功',search_rejected:'搜索暂不可用',operation_not_confirmed:'操作未能确认，请查看最新状态',origin_or_token_rejected:'页面会话已变化，请刷新后重试',invalid_position_or_no_track:'当前没有可调整进度的歌曲',seek_readback_mismatch:'进度调整未生效，请重试',invalid_playback_mode:'不支持的播放模式',mode_readback_mismatch:'播放模式未生效，请重试'};
function toast(message,error=false){$('toast').textContent=message;$('toast').className='toast'+(error?' error':'');clearTimeout(toast.timer);toast.timer=setTimeout(()=>$('toast').classList.add('hidden'),4500);}
function paint(el){const min=Number(el.min)||0,max=Number(el.max)||100,value=Number(el.value);el.style.setProperty('--p',max>min?((value-min)/(max-min)*100)+'%':'0%');}
function duration(ms){if(!(ms>=0))return '--:--';const n=Math.floor(ms/1000);return `${Math.floor(n/60).toString().padStart(2,'0')}:${(n%60).toString().padStart(2,'0')}`;}
function mib(n){return Number.isFinite(n)?`${(n/1024).toFixed(1)} MB`:'—';}
function renderAvatar(img,text,n,available){
 if(!img.dataset.bound){img.dataset.bound='1';img.addEventListener('error',()=>{img.hidden=true;text.hidden=false;});}
 if(available&&n.avatar){const key=String(n.avatar_key||0);if(img.dataset.key!==key){img.src='/api/v1/avatar?v='+key;img.dataset.key=key;}img.hidden=false;text.hidden=true;}
 else{img.hidden=true;img.removeAttribute('src');img.dataset.key='';text.hidden=false;}
}
function icon(el,name){const use=el&&el.querySelector('use');if(use)use.setAttribute('href','#'+name);}
function setConnected(value){online=value;$('connection').classList.toggle('online',value);$('connection').lastElementChild.textContent=value?'音箱已连接':'连接已中断';$('sideDot').classList.toggle('online',value);$('offlineBanner').classList.toggle('hidden',value);setButtons();}
function musicStatus(){const m=snapshot&&snapshot.music;return m&&m.ok?m.status:-1;}
function setButtons(){const voice=snapshot&&snapshot.voice,m=snapshot&&snapshot.music||{},active=m.ok&&(m.status===1||m.status===2),hasTrack=m.ok&&active;
 document.querySelectorAll('[data-method]').forEach(b=>{const method=b.dataset.method||'';b.disabled=!online||busy||(method.indexOf('music.')===0&&!hasTrack);});
 $('voiceListen').disabled=!online||busy||!!(voice&&voice.running);$('voiceStart').disabled=!online||busy||!!(voice&&voice.running);$('voiceStop').disabled=!online||busy||!(voice&&voice.running);$('lampClear').disabled=!online||busy||!!(voice&&voice.running);$('loginButton').disabled=!online||busy;
 $('musicToggle').disabled=!online||busy||!hasTrack;$('musicStop').disabled=!online||busy||!active;$('musicMode').disabled=!online||busy||!m.ok||!modeLoaded||seeking;
 $('seek').disabled=!online||busy||!m.ok||!(m.duration_ms>0)||!active;$('volume').disabled=!online;
}
function renderMode(){const repeat=modeLoaded&&mode===1;const text=!modeLoaded?'播放模式读取中':repeat?'列表循环':'顺序播放';$('musicMode').classList.toggle('active',repeat);$('musicMode').title=text;$('musicMode').setAttribute('aria-label','播放模式：'+text);}
function renderPlayer(m){const playing=m.ok&&m.status===1,paused=m.ok&&m.status===2;
 $('musicPill').textContent=!m.ok?'状态未知':playing?'播放中':paused?'已暂停':'未播放';
 $('vinyl').classList.toggle('playing',playing);
 $('trackTitle').textContent=m.title||(!m.ok?'暂时无法读取状态':playing?'正在音箱播放':paused?'当前曲目已暂停':'尚未开始播放');
 $('trackSource').textContent=!m.ok?'等待音箱状态':m.playlist||'原厂播放器 · 流式播放';
 $('trackSubtitle').textContent=m.artist||(m.ok?'搜索歌曲，或从榜单和歌单开始播放。':'播放状态会在这里实时显示。');
 $('trackQueue').classList.toggle('hidden',!(m.queue_total>0));if(m.queue_total>0)$('trackQueue').textContent=`${m.queue_index||1} / ${m.queue_total}`;
 $('barTitle').textContent=$('trackTitle').textContent;
 $('barArtist').textContent=m.artist||(!m.ok?'音箱状态不可用':playing?'正在播放':paused?'已暂停':'选择歌曲开始');
 $('barCover').classList.toggle('playing',playing);
 icon($('musicToggle'),playing?'i-pause':'i-play');$('musicToggle').title=playing?'暂停':'播放';$('musicToggle').setAttribute('aria-label',playing?'暂停':'播放');$('musicToggle').classList.toggle('playing',playing);
 if(m.ok&&Number.isInteger(m.loop_type)){mode=m.loop_type;modeLoaded=true;renderMode();}
 pb.duration=m.ok&&m.duration_ms>0?m.duration_ms:0;
 pb.status=m.ok?m.status:0;
 if(!dragging){pb.position=m.ok?Math.max(0,m.position_ms||0):0;pb.at=Date.now();
  $('musicDuration').textContent=pb.duration>0?duration(pb.duration):'--:--';
  $('musicPosition').textContent=duration(pb.position);
  $('seek').value=pb.duration>0?Math.round(pb.position/pb.duration*1000):0;paint($('seek'));}
}
function render(s){snapshot=s;csrf=s.csrf;lastSeen=Date.now();setConnected(true);const d=s.device,v=s.voice,m=s.music,l=s.lamp,n=s.netease;
 $('deviceAddress').textContent=location.host;$('voiceState').textContent=phaseNames[v.state]||'运行中';$('voicePill').textContent=v.running?'运行中':'已停止';$('voiceDot').classList.toggle('active',!!v.running);$('speakerRing').classList.toggle('lit',!!l.on);
 $('voiceDescription').textContent=!v.credentials_loaded?'可启动本地唤醒；未配置语音凭据时，音箱会回应并提示配置。':v.state==='standby'?'在音箱旁说“豆包豆包”，即可开始对话。':v.state==='followup'?'直接说出下一个问题，无需重复唤醒。':v.state==='stopped'?'唤醒词仍待补录校准；现在可点击「开始一次对话」，无需唤醒词。':v.state==='speaking'?'正在播放回答，播完后恢复收音。':'助手正在处理这次会话。';
 $('memoryValue').textContent=mib(d.memory_available_kib);$('memoryBar').style.width=`${Math.max(0,Math.min(100,d.memory_available_kib/d.memory_total_kib*100))}%`;$('storageValue').textContent=mib(d.storage_free_kib);$('storageBar').style.width=`${Math.max(0,Math.min(100,d.storage_free_kib/d.storage_total_kib*100))}%`;
 $('meshState').textContent=d.mesh_known?(d.mesh_running?'正常运行':'未运行'):'状态未知';$('lampState').textContent=l.on?({1:'收音灯',2:'思考灯',3:'播报灯',8:'自定义灯'}[l.effect]||'灯效开启'):'已熄灭';
 renderPlayer(m||{ok:0});
 if(document.activeElement!==$('volume')&&Date.now()-volumeHold>1500&&Number.isInteger(m&&m.volume)&&m.volume>=0){$('volume').value=m.volume;$('volumeValue').textContent=m.volume+'%';paint($('volume'));}
 const saved=n.state==='logged_in'||n.state==='session_saved';$('accountEmpty').classList.toggle('hidden',saved);$('accountInfo').classList.toggle('hidden',!saved);$('accountPill').textContent=n.authenticated?'已登录':saved?'待验证':'未登录';$('accountName').textContent=n.nickname||'网易云账号';$('accountAvatarText').textContent=(n.nickname||'云').slice(0,1);
 const vip=!!n.authenticated&&((n.vip_type>0)||(n.vip_level>0)),level=n.vip_level>0?n.vip_level:0;
 $('vipBadge').classList.toggle('hidden',!vip);$('vipLevel').textContent=level>0?' V'+level:'';
 $('levelChip').classList.toggle('hidden',!(n.authenticated&&n.level>0));$('levelChip').textContent='Lv.'+(n.level>0?n.level:'—');
 $('memberStatus').textContent=!n.authenticated?'已保存会话，正在验证会员状态':level>0?`黑胶会员 · V${level}`:vip?'黑胶会员':'普通账号 · 歌曲权限逐曲校验';
 $('listenSongs').textContent=n.listen_songs>0?Number(n.listen_songs).toLocaleString('zh-CN'):'—';
 $('accountLevel').textContent=n.level>0?'Lv.'+n.level:'—';
 $('vipTypeText').textContent=level>0?'V'+level:vip?'会员':'普通';
 renderAvatar($('accountAvatarImg'),$('accountAvatarText'),n,n.authenticated);
 $('sideName').textContent=saved?(n.nickname||'网易云账号'):'未登录网易云';
 $('sideVip').textContent=saved?(vip?'黑胶VIP'+(level>0?' V'+level:''):'已连接网易云'):'点击连接账号';
 renderAvatar($('sideAvatarImg'),$('sideAvatarText'),n,saved);
 $('uptime').textContent=`${Math.floor(d.uptime_seconds/86400)} 天 ${Math.floor(d.uptime_seconds%86400/3600)} 小时 ${Math.floor(d.uptime_seconds%3600/60)} 分`;$('webMemory').textContent=mib(d.web_rss_kib);$('lastUpdate').textContent=new Date().toLocaleTimeString('zh-CN',{hour12:false});
 const events=$('events');events.replaceChildren();if(!s.events.length){const p=document.createElement('p');p.className='empty-note';p.textContent='还没有操作记录。';events.append(p);}else for(const e of s.events){if(e.action==='netease_check')continue;const row=document.createElement('div');row.className='event-row'+(!e.ok?' failed':'');const dot=document.createElement('i');dot.className='dot';const text=document.createElement('span');text.textContent=(labels[e.action]||e.action)+(e.ok?'':' · 未确认');const time=document.createElement('time');time.textContent=e.age_seconds<60?'刚刚':`${Math.floor(e.age_seconds/60)} 分钟前`;row.append(dot,text,time);events.append(row);}
 markActive();setButtons();library.auto(s);
}
function markActive(){const title=snapshot&&snapshot.music&&snapshot.music.title;document.querySelectorAll('.track-row').forEach(r=>r.classList.toggle('active',!!title&&r.dataset.title===title));}
async function refresh(){const ctrl=new AbortController(),timer=setTimeout(()=>ctrl.abort(),12000);try{const r=await fetch('/api/v1/state',{cache:'no-store',signal:ctrl.signal});if(!r.ok)throw Error('连接音箱失败');render(await r.json());}catch(e){setConnected(false);}finally{clearTimeout(timer);}}
function startEvents(){if(stream)stream.close();if(document.hidden)return;stream=new EventSource('/api/v1/events');stream.addEventListener('state',e=>{try{render(JSON.parse(e.data));}catch{}});stream.onerror=()=>{if(Date.now()-lastSeen>12000)setConnected(false);};}
async function send(method,params){const ctrl=new AbortController(),timer=setTimeout(()=>ctrl.abort(),10000);try{let response;try{response=await fetch('/api/v1/command',{method:'POST',headers:{'Content-Type':'application/json','X-LX06-Token':csrf},body:JSON.stringify({v:1,id:`web-${Date.now()}-${++serial}`,method,params:params||{}}),signal:ctrl.signal});}catch(e){throw Error(e.name==='AbortError'?'音箱响应超时':'无法连接音箱');}const envelope=await response.json();const data=envelope.result||envelope;if(!response.ok||!data.ok)throw Error(errors[data.error]||data.error||'操作未完成');return data;}finally{clearTimeout(timer);}}
async function command(method,params={}){if(busy)throw Error('请等待当前操作完成');if(!csrf)await refresh();if(!csrf)throw Error('音箱未连接');busy=true;setButtons();const ctrl=new AbortController(),timer=setTimeout(()=>ctrl.abort(),22000);try{let r;try{r=await fetch('/api/v1/command',{method:'POST',headers:{'Content-Type':'application/json','X-LX06-Token':csrf},body:JSON.stringify({v:1,id:`web-${Date.now()}-${++serial}`,method,params}),signal:ctrl.signal});}catch(e){throw Error(e.name==='AbortError'?'音箱响应超时':'无法连接音箱');}const envelope=await r.json();const data=envelope.result||envelope;if(!r.ok||!data.ok)throw Error(errors[data.error]||data.error||'操作未完成');return data;}finally{clearTimeout(timer);busy=false;setButtons();refresh();}}
async function silent(method,params={}){if(!csrf)return null;try{return await send(method,params);}catch(e){return null;}}
/* Read-only list calls run outside the busy lock: they can take many seconds on
   large playlists and must not block playback controls. */
async function fetchList(method,params={}){if(!csrf)await refresh();if(!csrf)throw Error('音箱未连接');const ctrl=new AbortController(),timer=setTimeout(()=>ctrl.abort(),30000);try{let r;try{r=await fetch('/api/v1/command',{method:'POST',headers:{'Content-Type':'application/json','X-LX06-Token':csrf},body:JSON.stringify({v:1,id:`list-${Date.now()}-${++serial}`,method,params}),signal:ctrl.signal});}catch(e){throw Error(e.name==='AbortError'?'音箱响应超时':'无法连接音箱');}const data=(await r.json()).result;if(!r.ok||!data.ok)throw Error(errors[data.error]||data.error||'读取失败');return data;}finally{clearTimeout(timer);}}
document.querySelectorAll('[data-method]').forEach(b=>b.addEventListener('click',async()=>{try{await command(b.dataset.method);}catch(e){toast(e.message,true);}}));
$('refresh').onclick=refresh;
$('musicToggle').onclick=async()=>{const status=musicStatus();if(status!==1&&status!==2){toast('当前没有可播放的歌曲',true);return;}try{if(status===1){await command('music.pause');toast('已暂停');}else{await command('music.resume');toast('继续播放');}}catch(e){toast(e.message,true);}};
$('musicMode').onclick=async()=>{const next=mode===1?2:1;try{const d=await command('music.mode.set',{mode:next===1?'repeat_all':'sequential'});if(Number.isInteger(d.loop_type))mode=d.loop_type;renderMode();toast(mode===1?'已切换为列表循环':'已切换为顺序播放');}catch(e){toast(e.message,true);}};
function previewSeek(){const value=Number($('seek').value);$('musicPosition').textContent=duration(pb.duration>0?pb.duration*value/1000:0);}
$('seek').addEventListener('pointerdown',()=>{dragging=true;});
$('seek').addEventListener('input',()=>{dragging=true;previewSeek();paint($('seek'));});
$('seek').addEventListener('pointerup',()=>{if(dragging)previewSeek();});
$('seek').addEventListener('pointercancel',()=>{dragging=false;});
$('seek').addEventListener('change',async()=>{
 const value=Number($('seek').value),target=Math.round(pb.duration*value/1000);
 dragging=false;if(!(pb.duration>0))return;
 seeking=true;setButtons();
 try{const d=await command('music.seek',{position_ms:target});pb.position=Number.isFinite(d.position_ms)?d.position_ms:target;pb.at=Date.now();$('musicPosition').textContent=duration(pb.position);$('seek').value=Math.round(pb.position/pb.duration*1000);paint($('seek'));}
 catch(e){toast(e.message,true);}
 finally{seeking=false;setButtons();}
});
let volumeTimer=null,volumeHold=0;
$('volume').oninput=()=>{$('volumeValue').textContent=$('volume').value+'%';paint($('volume'));volumeHold=Date.now();clearTimeout(volumeTimer);const value=Number($('volume').value);volumeTimer=setTimeout(async()=>{try{await send('device.volume.set',{value});volumeHold=Date.now();}catch(e){toast(e.message,true);setTimeout(refresh,600);}},300);};
$('streamForm').onsubmit=async e=>{e.preventDefault();try{await command('music.stream.play',{url:$('streamUrl').value.trim()});toast('已开始播放音频流');}catch(x){toast(x.message,true);}};
function stopQr(){clearTimeout(qrTimer);qrTimer=null;}
async function pollQr(){if(!$('loginDialog').open)return;if(Date.now()>qrUntil){$('qrStatus').textContent='二维码已过期，请刷新';return;}try{const data=await command('accounts.netease.login.check');if(data.authenticated){$('qrStatus').textContent='登录成功';toast('网易云账号已连接');setTimeout(()=>$('loginDialog').close(),700);return;}$('qrStatus').textContent=data.qr_code===802?'已扫码，请在手机上确认':data.qr_code===803?'正在验证账号':'等待扫码';}catch(e){$('qrStatus').textContent=e.message;}if($('loginDialog').open)qrTimer=setTimeout(pollQr,3000);}
async function showQr(){stopQr();if(!$('loginDialog').open)$('loginDialog').showModal();$('qrCode').replaceChildren();const label=document.createElement('span');label.textContent='正在获取二维码…';$('qrCode').append(label);$('qrStatus').textContent='正在连接网易云';try{const data=await command('accounts.netease.login.start');const qr=qrcode(0,'M');qr.addData(data.url,'Byte');qr.make();const canvas=document.createElement('canvas');const n=qr.getModuleCount(),scale=6,border=4;canvas.width=canvas.height=(n+border*2)*scale;const ctx=canvas.getContext('2d');ctx.fillStyle='#fff';ctx.fillRect(0,0,canvas.width,canvas.height);ctx.fillStyle='#183428';for(let y=0;y<n;y++)for(let x=0;x<n;x++)if(qr.isDark(y,x))ctx.fillRect((x+border)*scale,(y+border)*scale,scale,scale);canvas.setAttribute('aria-label','网易云音乐登录二维码');$('qrCode').replaceChildren(canvas);qrUntil=Date.now()+data.expires_in*1000;$('qrStatus').textContent='等待网易云 App 扫码';qrTimer=setTimeout(pollQr,3000);}catch(e){label.textContent='暂时无法获取二维码';$('qrStatus').textContent=e.message;}}
$('loginButton').onclick=showQr;$('retryQr').onclick=showQr;$('closeLogin').onclick=()=>$('loginDialog').close();$('loginDialog').addEventListener('close',stopQr);
$('voiceListen').onclick=async()=>{try{await command('voice.listen');toast('已建立会话，请直接说指令，无需唤醒词');}catch(e){toast(e.message,true);}};
$('accountRefresh').onclick=async()=>{try{await command('accounts.netease.refresh');toast('账号权益已刷新');}catch(e){toast(e.message,true);}};$('logoutButton').onclick=async()=>{if(!confirm('退出网易云账号并清除音箱上的登录会话？'))return;try{await command('accounts.netease.logout');library.reset();listTracks($('searchResults'),[]);listTracks($('chartTracks'),[]);listTracks($('playlistTracks'),[]);toast('已退出网易云');}catch(e){toast(e.message,true);}};
function hint(container,text){container.replaceChildren();const p=document.createElement('p');p.className='list-hint';p.textContent=text;container.append(p);}
function listTracks(container,items,emptyText){container.replaceChildren();if(!items.length){hint(container,emptyText||'暂无歌曲');return;}for(const item of items){const row=document.createElement('button');row.type='button';row.className='track-row';row.dataset.title=item.title||'';const index=document.createElement('span');index.className='row-index';index.textContent=item.rank??'';const copy=document.createElement('span');copy.className='row-copy';const title=document.createElement('strong');title.textContent=item.title;const artist=document.createElement('small');artist.textContent=item.artist||'未知歌手';copy.append(title,artist);const glyph=document.createElement('span');glyph.className='row-play';glyph.innerHTML='<svg><use href="#i-play"/></svg>';row.append(index,copy,glyph);row.onclick=()=>playTrack(item);container.append(row);}markActive();}
async function playTrack(item){try{await command('music.netease.play',{id:item.id,title:item.title,artist:item.artist||'',quality:$('quality').value});toast(`正在播放 ${item.title}`);}catch(e){toast(e.message,true);}}
function showLibraryPanel(id){
 const panels={panelCharts:'tabCharts',panelPlaylists:'tabPlaylists',panelSearch:'tabSearch'};
 for(const [panel,tab] of Object.entries(panels)){const on=panel===id;$(panel).classList.toggle('hidden',!on);$(tab).classList.toggle('active',on);$(tab).setAttribute('aria-selected',on?'true':'false');}
}
function setupLibrary(){
 let chartsLoaded=false,playlistsLoaded=false,chartsLoading=false,playlistsLoading=false,retryAt=0;
 $('tabCharts').onclick=()=>showLibraryPanel('panelCharts');
 $('tabPlaylists').onclick=()=>showLibraryPanel('panelPlaylists');
 $('tabSearch').onclick=()=>showLibraryPanel('panelSearch');
 function setCount(el,text){el.textContent=text;el.hidden=!text;}
 async function loadChartTracks(){const id=$('chartSelect').value;if(!id||id==='0')return;hint($('chartTracks'),'正在读取完整曲目…');try{const d=await fetchList('music.netease.chart.tracks',{id});const items=d.items||[];listTracks($('chartTracks'),items,'这个榜单暂无歌曲');$('playChart').disabled=!items.length;setCount($('chartCount'),items.length?`${items.length} 首`:'')}catch(e){hint($('chartTracks'),e.message);}}
 async function loadPlaylistTracks(){const id=$('playlistSelect').value;if(!id||id==='0')return;hint($('playlistTracks'),'正在读取完整曲目…');try{const d=await fetchList('music.netease.playlist.tracks',{id});const items=d.items||[];listTracks($('playlistTracks'),items,'这个歌单暂无歌曲');$('playPlaylist').disabled=!items.length;setCount($('playlistCount'),items.length?`${items.length} 首`:'')}catch(e){hint($('playlistTracks'),e.message);}}
 async function loadCharts(){if(chartsLoading)return;chartsLoading=true;try{const d=await fetchList('music.netease.charts');const select=$('chartSelect');select.replaceChildren();let preferred=0;for(const x of d.items){const option=document.createElement('option');option.value=x.id;option.textContent=x.name;if(x.name==='热歌榜')preferred=select.options.length;select.append(option);}select.selectedIndex=preferred;select.disabled=!d.items.length;chartsLoaded=!!d.items.length;setCount($('chartCount'),chartsLoaded?`${d.items.length} 个榜单`:'');if(chartsLoaded)await loadChartTracks();else hint($('chartTracks'),'暂无可用榜单');}catch(e){retryAt=Date.now()+30000;hint($('chartTracks'),e.message);}finally{chartsLoading=false;}}
 async function loadPlaylists(){if(playlistsLoading)return;playlistsLoading=true;try{const d=await fetchList('music.netease.playlists');const select=$('playlistSelect');select.replaceChildren();for(const x of d.items){const option=document.createElement('option');option.value=x.id;option.textContent=`${x.name} (${x.track_count||0})`;select.append(option);}select.disabled=!d.items.length;playlistsLoaded=!!d.items.length;if(playlistsLoaded)await loadPlaylistTracks();else hint($('playlistTracks'),'这个账号还没有歌单');}catch(e){retryAt=Date.now()+30000;hint($('playlistTracks'),e.message);}finally{playlistsLoading=false;}}
 $('loadCharts').onclick=async()=>{try{await loadCharts();toast('榜单与曲目已更新');}catch(e){toast(e.message,true);}};
 $('chartSelect').onchange=()=>{if(chartsLoaded)loadChartTracks();};
 $('playChart').onclick=async()=>{try{await command('music.netease.chart.play',{id:$('chartSelect').value,quality:$('quality').value});toast('榜单已加入音箱队列');}catch(e){toast(e.message,true);}};
 $('loadPlaylists').onclick=async()=>{try{await loadPlaylists();toast('歌单与曲目已更新');}catch(e){toast(e.message,true);}};
 $('playlistSelect').onchange=()=>{if(playlistsLoaded)loadPlaylistTracks();};
 $('playPlaylist').onclick=async()=>{try{await command('music.netease.playlist.play',{id:$('playlistSelect').value,quality:$('quality').value});toast('歌单已加入音箱队列');}catch(e){toast(e.message,true);}};
 return {auto(s){if(Date.now()<retryAt||!s||!s.netease||!s.netease.authenticated)return;if(!chartsLoaded&&!chartsLoading)loadCharts();if(!playlistsLoaded&&!playlistsLoading)loadPlaylists();},reset(){chartsLoaded=playlistsLoaded=false;retryAt=0;setCount($('chartCount'),'');setCount($('playlistCount'),'');}};
}
$('searchForm').onsubmit=async e=>{e.preventDefault();showLibraryPanel('panelSearch');const list=$('searchResults');hint(list,'正在搜索…');try{const data=await fetchList('music.netease.search',{query:$('searchQuery').value.trim()});listTracks(list,data.items||[],'没有找到匹配的歌曲');}catch(x){hint(list,x.message);}};
const library=setupLibrary();
setInterval(()=>{if(pb.status===1&&!dragging&&!seeking&&pb.duration>0){const position=Math.min(pb.duration,pb.position+(Date.now()-pb.at));$('seek').value=Math.round(position/pb.duration*1000);$('musicPosition').textContent=duration(position);paint($('seek'));}},500);
document.addEventListener('visibilitychange',()=>{if(document.hidden){if(stream)stream.close();}else{refresh();startEvents();}});document.querySelectorAll('.nav-item').forEach(link=>link.onclick=()=>{document.querySelectorAll('.nav-item').forEach(x=>x.classList.remove('active'));link.classList.add('active');});setInterval(()=>{if(!document.hidden&&lastSeen&&Date.now()-lastSeen>16000)setConnected(false);},5000);renderMode();refresh();startEvents();
