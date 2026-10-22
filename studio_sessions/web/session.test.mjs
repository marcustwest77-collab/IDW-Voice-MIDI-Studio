import test from 'node:test';
import assert from 'node:assert/strict';
import {StudioSession,parseInvitation} from './session.mjs';
const room='a'.repeat(24),ticket='b'.repeat(43),invite={room,ticket};
const tick=()=>new Promise(resolve=>setImmediate(resolve));
class Socket {
  static all=[];
  constructor(){this.sent=[];this.readyState=1;Socket.all.push(this);}
  send(data){this.sent.push(JSON.parse(data));}
  close(){this.readyState=3;}
  message(data){this.onmessage?.({data:JSON.stringify(data)});}
}
class Peer {
  constructor(){this.tracks=[];this.candidates=[];}
  addTrack(track){this.tracks.push(track);}
  async createOffer(){return {type:'offer',sdp:'offer'};}
  async createAnswer(){return {type:'answer',sdp:'answer'};}
  async setLocalDescription(value){this.localDescription=value;this.onicecandidate?.({candidate:{toJSON:()=>({candidate:'candidate'})}});}
  async setRemoteDescription(value){this.remoteDescription=value;}
  async addIceCandidate(value){this.candidates.push(value);}
  close(){this.closed=true;}
}
function stream(){
  const audio={enabled:true,stop(){this.stopped=true;}},video={enabled:true,stop(){this.stopped=true;}};
  return {getTracks:()=>[audio,video],getAudioTracks:()=>[audio],getVideoTracks:()=>[video]};
}
function setup(getUserMedia){
  const states=[];
  const client=new StudioSession({mediaDevices:{getUserMedia},WebSocketClass:Socket,PeerClass:Peer,endpoint:'ws://localhost/v1/connect',onState:s=>states.push(s)});
  return {client,states};
}
async function accepted(client,role='host'){
  await client.join(invite);
  const ws=client.ws;ws.onopen();ws.message({type:'joined',role,peer_present:false});await tick();return ws;
}
test('invitation only permits the current server and exact tickets',()=>{
  assert.deepEqual(parseInvitation(`https://idw.test/#room=${room}&ticket=${ticket}`,'https://idw.test'),invite);
  for(const value of ['https://evil.test/#room='+room+'&ticket='+ticket,'https://idw.test/#room=x','javascript:alert(1)'])
    assert.throws(()=>parseInvitation(value,'https://idw.test'));
});
test('permission is requested only after admission, and leave stops all devices',async()=>{
  let calls=0;const media=stream();const {client}=setup(async()=>{calls++;return media;});
  await client.join(invite);assert.equal(calls,0);client.ws.onopen();
  assert.deepEqual(client.ws.sent[0],{type:'join',...invite});
  client.ws.message({type:'joined',role:'host'});await tick();assert.equal(calls,1);
  client.setMicrophone(false);client.setCamera(false);
  assert.equal(media.getAudioTracks()[0].enabled,false);assert.equal(media.getVideoTracks()[0].enabled,false);
  client.leave();assert.ok(media.getTracks().every(t=>t.stopped));assert.equal(client.ws,null);
});
test('late permission acceptance after leave releases the resulting stream',async()=>{
  let resolve;const media=stream();const {client}=setup(()=>new Promise(r=>{resolve=r;}));
  await client.join(invite);client.ws.message({type:'joined',role:'host'});await tick();
  client.leave();resolve(media);await tick();assert.ok(media.getTracks().every(t=>t.stopped));assert.equal(client.local,null);
});
test('an old rejected permission request cannot cancel a newer join',async()=>{
  let reject;const {client}=setup(()=>new Promise((_,r)=>{reject=r;}));
  await client.join(invite);client.ws.message({type:'joined',role:'host'});await tick();
  await client.join(invite);const newest=client.ws;reject(new Error('denied'));await tick();
  assert.equal(client.ws,newest);client.leave();
});
test('host sends SDP before ICE and buffers candidates until the answer',async()=>{
  const {client}=setup(async()=>stream());const ws=await accepted(client);
  ws.message({type:'ready'});await tick();
  const types=ws.sent.map(x=>x.type);assert.ok(types.indexOf('offer')<types.indexOf('candidate'));
  ws.message({type:'candidate',candidate:{candidate:'remote'}});await tick();assert.equal(client.pc.candidates.length,0);
  ws.message({type:'answer',sdp:'answer'});await tick();assert.equal(client.pc.candidates.length,1);client.leave();
});
test('guest answers an offer and a departed peer is discarded',async()=>{
  const {client}=setup(async()=>stream());const ws=await accepted(client,'guest');
  ws.message({type:'offer',sdp:'offer'});await tick();assert.ok(ws.sent.some(x=>x.type==='answer'));
  const old=client.pc;ws.message({type:'peer_left'});await tick();assert.ok(old.closed);assert.equal(client.pc,null);
  assert.ok(client.local);client.leave();
});
test('lost signalling releases media and invalidates pending work',async()=>{
  const media=stream();const {client,states}=setup(async()=>media);const ws=await accepted(client);
  ws.onclose({code:1008});assert.ok(media.getTracks().every(t=>t.stopped));assert.equal(client.ws,null);
  assert.match(states.at(-1),/unavailable/);
});
test('permission denial closes the signalling seat',async()=>{
  const {client,states}=setup(async()=>{throw new Error('denied');});
  await accepted(client);assert.equal(client.ws,null);assert.match(states.at(-1),/Media access failed/);
});
