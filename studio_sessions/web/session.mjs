export function parseInvitation(value, origin) {
  const url = new URL(value, origin);
  if (url.origin !== origin) throw new Error('Use an invitation from this IDW studio server.');
  const params = new URLSearchParams(url.hash.slice(1));
  const room = params.get('room'), ticket = params.get('ticket');
  if (!/^[A-Za-z0-9_-]{24}$/.test(room || '') || !/^[A-Za-z0-9_-]{43}$/.test(ticket || ''))
    throw new Error('This studio invitation is incomplete.');
  return {room, ticket};
}

export class StudioSession {
  constructor({mediaDevices, WebSocketClass, PeerClass, endpoint, iceServers = [], onState = () => {}, onLocal = () => {}, onRemote = () => {}}) {
    Object.assign(this, {mediaDevices, WebSocketClass, PeerClass, endpoint, iceServers, onState, onLocal, onRemote});
    this.epoch = 0;
  }
  async join(invitation, camera = true) {
    this.leave(false);
    const epoch = this.epoch;
    this.camera = camera;
    this.onState('Connecting to the studio…');
    const ws = this.ws = new this.WebSocketClass(this.endpoint);
    ws.onopen = () => { if (epoch === this.epoch) ws.send(JSON.stringify({type:'join', ...invitation})); };
    let chain = Promise.resolve();
    ws.onmessage = event => {
      chain = chain.then(async () => {
        if (epoch !== this.epoch) return;
        const data = JSON.parse(event.data);
        await this.handle(data, epoch);
      }).catch(() => { if (epoch === this.epoch) this.fail('The connection could not be completed. Leave and join again.'); });
    };
    ws.onclose = event => {
      if (epoch === this.epoch) this.fail(event.code === 1008 ? 'Room unavailable, invitation expired, or seat already occupied.' : 'Disconnected. Join again to reconnect.');
    };
    ws.onerror = () => { if (epoch === this.epoch) this.fail('Unable to reach the studio server.'); };
  }
  send(data) { if (this.ws?.readyState === 1) this.ws.send(JSON.stringify(data)); }
  async handle(data, epoch) {
    if (data.type === 'joined') {
      this.role = data.role;
      this.onState('Allow microphone' + (this.camera ? ' and camera' : '') + ' access to join.');
      let stream;
      try { stream = await this.mediaDevices.getUserMedia({audio:true, video:this.camera}); }
      catch { if (epoch === this.epoch) this.fail('Media access failed. Check permissions or try microphone-only mode.'); return; }
      if (epoch !== this.epoch) { stream.getTracks().forEach(t => t.stop()); return; }
      this.local = stream;
      stream.getTracks().forEach(track => { track.onended = () => { if (epoch === this.epoch) this.fail('A media device disconnected. Join again after checking it.'); }; });
      this.onLocal(stream, this.role);
      this.onState('Waiting for the other participant…');
      this.send({type:'ready'});
      return;
    }
    if (data.type === 'peer_joined') {
      this.resetPeer();
      this.onState('The other participant is joining…');
      if (this.local) this.send({type:'ready'});
      return;
    }
    if (data.type === 'peer_left' || data.type === 'peer_unavailable') {
      this.resetPeer();
      this.onState('The other participant left. Your microphone/camera remain active until Leave.');
      return;
    }
    if (data.type === 'ready') {
      if (this.local && this.role === 'host' && !this.pc) {
        const pc = this.makePeer(epoch);
        const offer = await pc.createOffer();
        if (epoch !== this.epoch || pc !== this.pc) return;
        await pc.setLocalDescription(offer);
        this.send({type:'offer',sdp:pc.localDescription.sdp});
        this.sdpSent = true; this.flushIce();
      }
      return;
    }
    if (data.type === 'offer' && this.role === 'guest' && this.local) {
      this.resetPeer();
      const pc = this.makePeer(epoch);
      await pc.setRemoteDescription({type:'offer',sdp:data.sdp});
      const answer = await pc.createAnswer();
      if (epoch !== this.epoch || pc !== this.pc) return;
      await pc.setLocalDescription(answer);
      this.send({type:'answer',sdp:pc.localDescription.sdp});
      this.sdpSent = true; this.flushIce();
      return;
    }
    if (data.type === 'answer' && this.pc && this.role === 'host') {
      await this.pc.setRemoteDescription({type:'answer',sdp:data.sdp});
      for (const candidate of this.candidates || []) await this.pc.addIceCandidate(candidate);
      this.candidates = [];
      return;
    }
    if (data.type === 'candidate' && this.pc) {
      if (this.pc.remoteDescription) await this.pc.addIceCandidate(data.candidate);
      else (this.candidates ||= []).push(data.candidate);
    }
  }
  makePeer(epoch) {
    const pc = this.pc = new this.PeerClass({iceServers:this.iceServers});
    for (const track of this.local.getTracks()) pc.addTrack(track, this.local);
    // Queue candidates until local SDP has been sent; otherwise a fast ICE event
    // can reach the guest before its offer creates a peer connection.
    this.pendingIce = [];
    pc.onicecandidate = event => {
      if (epoch !== this.epoch || pc !== this.pc) return;
      this.pendingIce.push(event.candidate?.toJSON() ?? null);
      this.flushIce();
    };
    pc.ontrack = event => { if (epoch === this.epoch && pc === this.pc) this.onRemote(event.streams[0]); };
    pc.onconnectionstatechange = () => {
      if (epoch !== this.epoch || pc !== this.pc) return;
      if (pc.connectionState === 'connected') this.onState('Connected · Two-person studio session');
      if (pc.connectionState === 'disconnected') this.onState('Connection interrupted…');
      if (pc.connectionState === 'failed') this.fail('Media connection failed. This network may need the studio relay service.');
    };
    return pc;
  }
  flushIce() {
    if (!this.sdpSent) return;
    for (const candidate of this.pendingIce || []) this.send({type:'candidate', candidate});
    this.pendingIce = [];
  }
  resetPeer() {
    this.sdpSent = false;
    this.pendingIce = [];
    this.candidates = [];
    if (this.pc) { this.pc.onconnectionstatechange = null; this.pc.ontrack = null; this.pc.onicecandidate = null; this.pc.close(); this.pc = null; }
    this.onRemote(null);
  }
  setMicrophone(enabled) { this.local?.getAudioTracks().forEach(t => {t.enabled = enabled;}); }
  setCamera(enabled) { this.local?.getVideoTracks().forEach(t => {t.enabled = enabled;}); }
  fail(message) { this.leave(false); this.onState(message); }
  leave(announce = true) {
    ++this.epoch;
    if (this.ws) { this.ws.onclose = null; this.ws.onerror = null; this.ws.onmessage = null; this.ws.onopen = null; this.ws.close(); this.ws = null; }
    this.resetPeer();
    this.local?.getTracks().forEach(t => {t.onended = null; t.stop();});
    this.local = null;
    this.onLocal(null);
    if (announce) this.onState('You left the session. Camera and microphone are released.');
  }
}
