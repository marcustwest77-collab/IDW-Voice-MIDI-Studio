import {StudioSession, parseInvitation} from './session.mjs';
const $ = id => document.getElementById(id);
let invitation = null, micOn = true, cameraOn = true;
try { if(location.hash) invitation = parseInvitation(location.href, location.origin); }
catch { $('status').textContent = 'This studio invitation is invalid.'; }
// Avoid retaining tickets in navigation history. No localStorage or analytics.
if(location.hash) history.replaceState(null, '', location.pathname);
if(invitation) $('invitation').placeholder = 'Invitation loaded — ready to join';
const client = new StudioSession({
  mediaDevices:navigator.mediaDevices,
  WebSocketClass:WebSocket,
  PeerClass:RTCPeerConnection,
  endpoint:(location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/v1/connect',
  onState:message => { $('status').textContent = message; updateControls(); },
  onLocal:(stream,role) => {
    $('local').srcObject = stream;
    $('local-empty').hidden = !!stream?.getVideoTracks().length && cameraOn;
    $('local-name').textContent = role ? `You · ${role === 'host' ? 'Host' : 'Guest'}` : 'You';
    $('local-state').textContent = stream ? 'Microphone active' : 'Devices released';
    updateControls();
  },
  onRemote:stream => {
    $('remote').srcObject = stream || null;
    $('remote-empty').hidden = !!stream?.getVideoTracks().length;
    $('remote-state').textContent = stream ? 'Media received' : 'No media received';
    $('hear').hidden = true;
    if(stream) $('remote').play().catch(() => {$('hear').hidden = false;});
  }
});
function updateControls() {
  $('join').disabled = !!client.ws;
  $('mode').disabled = !!client.ws;
  $('invitation').disabled = !!client.ws;
  $('leave').disabled = !client.ws;
  $('mic').disabled = !client.local;
  $('camera').disabled = !client.local?.getVideoTracks().length;
}
$('join').onclick = async () => {
  try {
    if($('invitation').value.trim()) invitation = parseInvitation($('invitation').value.trim(),location.origin);
    if(!invitation) throw new Error('Paste your IDW studio invitation first.');
    if(!navigator.mediaDevices?.getUserMedia) throw new Error('Camera access requires a supported browser and a secure connection.');
    $('invitation').value = '';
    micOn = cameraOn = true;
    $('mic').textContent = 'Mute microphone'; $('mic').setAttribute('aria-pressed','false');
    $('camera').textContent = 'Turn camera off'; $('camera').setAttribute('aria-pressed','false');
    await client.join(invitation, $('mode').value === 'video');
    updateControls();
  } catch(error) { $('status').textContent = error.message; }
};
$('mic').onclick = () => {
  client.setMicrophone(micOn = !micOn);
  $('mic').textContent = micOn ? 'Mute microphone' : 'Unmute microphone';
  $('mic').setAttribute('aria-pressed',String(!micOn));
  $('local-state').textContent = micOn ? 'Microphone active' : 'Microphone muted';
};
$('camera').onclick = () => {
  client.setCamera(cameraOn = !cameraOn);
  $('camera').textContent = cameraOn ? 'Turn camera off' : 'Turn camera on';
  $('camera').setAttribute('aria-pressed',String(!cameraOn));
  $('local-empty').textContent = 'Camera off'; $('local-empty').hidden = cameraOn;
};
$('leave').onclick = () => client.leave();
$('hear').onclick = () => $('remote').play().then(() => {$('hear').hidden=true;}).catch(() => {$('status').textContent='Call audio could not start. Check your output device.';});
window.addEventListener('pagehide',() => client.leave(false));
