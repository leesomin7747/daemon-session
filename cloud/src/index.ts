import {Cache,ReplyBudget} from './cache';
import {DurableObject} from 'cloudflare:workers';
import copy from '../../core/copy/ko.json';
import {MAX_FRAME,bytes,encode,frame,command,id,keys,transition,type Frame,type State,type Event} from './protocol';
const uuid=()=>crypto.randomUUID();
const secret=()=>{const data=crypto.getRandomValues(new Uint8Array(32));return btoa(String.fromCharCode(...data)).replaceAll('+','-').replaceAll('/','_').replace(/=+$/,'');};
function token(value:unknown):value is string {
 if(typeof value!=='string'||!/^[A-Za-z0-9_-]{43}$/.test(value))return false;
 try{const raw=atob(value.replaceAll('-','+').replaceAll('_','/')+'=');return raw.length===32&&btoa(raw).replaceAll('+','-').replaceAll('/','_').replace(/=+$/,'')===value;}catch{return false;}
}
const digest=async(text:string)=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(text))),x=>x.toString(16).padStart(2,'0')).join('');
function stable(value:unknown,depth=0):unknown {
 if(depth>16)throw Error('invalid');
 if(Array.isArray(value))return value.map(v=>stable(v,depth+1));
 if(value!==null&&typeof value==='object')return Object.fromEntries(Object.keys(value).sort().map(k=>[k,stable((value as Record<string,unknown>)[k],depth+1)]));
 return value;
}
class Bucket {
 tokens:number;at=Date.now();constructor(readonly capacity=20,readonly perSecond=10){this.tokens=capacity;}
 take(){const now=Date.now();this.tokens=Math.min(this.capacity,this.tokens+Math.max(0,now-this.at)*this.perSecond/1000);this.at=Math.max(now,this.at);if(this.tokens<1)return false;this.tokens--;return true;}
}
interface Connection {peer:string;ip:string;registered:boolean;controls:Cache;frames:Bucket;queued:number;leaseUntil:number;deadline:ReturnType<typeof setTimeout>}
interface Room {state:State;commands:Cache;copy:Map<string,string>;departures:Map<string,ReturnType<typeof setTimeout>>}
type Invite={code_hash:string;inviter_id:string;expires_at:number;request_id:string|null;applicant_id:string|null};
export class Office extends DurableObject<Env> {
 private replyBudget=new ReplyBudget();private connections=new Map<WebSocket,Connection>();private active=new Map<string,WebSocket>();private rooms=new Map<string,Room>();
 private rates=new Map<string,Bucket>();private ips=new Map<string,{at:number;used:number}>();private queued=0;private queue=Promise.resolve();
 constructor(ctx:DurableObjectState,env:Env){
  super(ctx,env);
  ctx.storage.transactionSync(()=>{
   ctx.storage.sql.exec(`CREATE TABLE IF NOT EXISTS devices(id TEXT PRIMARY KEY NOT NULL,token_hash TEXT UNIQUE NOT NULL CHECK(length(token_hash)=64));
    CREATE TABLE IF NOT EXISTS pairs(id TEXT PRIMARY KEY NOT NULL);
    CREATE TABLE IF NOT EXISTS members(peer_id TEXT PRIMARY KEY NOT NULL REFERENCES devices(id),pair_id TEXT NOT NULL REFERENCES pairs(id) ON DELETE CASCADE,slot INTEGER NOT NULL CHECK(slot IN (0,1)),UNIQUE(pair_id,slot));
    CREATE TABLE IF NOT EXISTS invites(code_hash TEXT PRIMARY KEY NOT NULL CHECK(length(code_hash)=64),inviter_id TEXT UNIQUE NOT NULL REFERENCES devices(id),expires_at INTEGER NOT NULL,request_id TEXT UNIQUE,applicant_id TEXT UNIQUE REFERENCES devices(id),CHECK((request_id IS NULL AND applicant_id IS NULL) OR (request_id IS NOT NULL AND applicant_id IS NOT NULL)));`);
  });
 }
 private rows<T extends Record<string,SqlStorageValue>>(query:string,...bindings:SqlStorageValue[]):T[]{return this.ctx.storage.sql.exec<T>(query,...bindings).toArray();}
 private pair(peer:string){return this.rows<{pair_id:string}>('SELECT pair_id FROM members WHERE peer_id=?',peer)[0]?.pair_id;}
 private members(pair:string){return this.rows<{peer_id:string}>('SELECT peer_id FROM members WHERE pair_id=? ORDER BY slot',pair).map(r=>r.peer_id);}
 private permit(ip:string){const now=Date.now();for(const [key,b] of this.ips)if(now-b.at>=60000)this.ips.delete(key);let bucket=this.ips.get(ip);if(!bucket){if(this.ips.size>=1024)return false;bucket={at:now,used:0};this.ips.set(ip,bucket);}return ++bucket.used<=10;}
 private pick(room:Room,key:string){const choices=(copy as Record<string,string[]>)[key];if(!choices?.length)throw Error('copy');const available=choices.filter(v=>v!==room.copy.get(key));const pool=available.length?available:choices;const value=pool[crypto.getRandomValues(new Uint32Array(1))[0]%pool.length];room.copy.set(key,value);return value;}
 private send(ws:WebSocket,data:string){if(bytes(data)>MAX_FRAME||ws.readyState!==1){this.retire(ws,'connection_lost');return;}try{ws.send(data);}catch{this.retire(ws,'connection_lost');}}
 private push(ws:WebSocket,op:string,body:unknown){this.send(ws,encode({v:1,op,requestId:'',body}));}
 private broadcast(pair:string,op:string,body:unknown){for(const peer of this.members(pair)){const ws=this.active.get(peer);if(ws)this.push(ws,op,body);}}
 private reply(f:Frame,code?:string,body:unknown={}){return encode({v:1,op:code?'error':'result',requestId:f.requestId,body:code?{code}:body});}
 private replay(ws:WebSocket,f:Frame,hash:string,cache:Cache,key:string){const cached=cache.values.get(key);if(!cached)return false;this.send(ws,cached.digest===hash?cached.reply:this.reply(f,'request_conflict'));return true;}
 private bump(state:State){if(BigInt(state.revision)>=18446744073709551615n)return false;state.revision=(BigInt(state.revision)+1n).toString();return true;}
 private drop(ws:WebSocket){
  const c=this.connections.get(ws);if(!c)return;this.connections.delete(ws);clearTimeout(c.deadline);c.controls.clear();
  if(!c.peer||this.active.get(c.peer)!==ws)return;this.active.delete(c.peer);
  const pair=this.pair(c.peer),room=pair?this.rooms.get(pair):undefined;if(!room||!pair)return;
  const peer=room.state.peers.find(p=>p.id===c.peer)!;if(peer.reachable&&this.bump(room.state)){peer.reachable=false;this.broadcast(pair,'session.snapshot',room.state);}
  const prior=room.departures.get(c.peer);if(prior)clearTimeout(prior);
  room.departures.set(c.peer,setTimeout(()=>{
   room.departures.delete(c.peer);if(this.rooms.get(pair)!==room)return;
   const coffee=room.state.peers.some(p=>p.inbox.some(i=>i.kind==='coffee'));
   this.broadcast(pair,'session.notice',{text:this.pick(room,coffee?'caption.leaveCoffee':'caption.leave')});
   if(room.departures.size===0&&room.state.peers.every(p=>!p.reachable)){room.commands.clear();this.rooms.delete(pair);}
  },15000));
 }
 private expire(){const now=Date.now();for(const [ws,c] of this.connections)if(c.peer&&now>=c.leaseUntil)this.retire(ws,'heartbeat_timeout');}
 private touch(ws:WebSocket,c:Connection){clearTimeout(c.deadline);c.leaseUntil=Date.now()+30000;c.deadline=setTimeout(()=>this.expire(),30000);}
 private capacity(ws:WebSocket,f:Frame){this.send(ws,this.reply(f,'capacity'));this.retire(ws,'capacity');}
 private retire(ws:WebSocket,reason:string){this.drop(ws);try{ws.close(1008,reason);}catch{/* already closed */}}
 private attach(peer:string){
  const pending=this.rows<Invite>('SELECT * FROM invites WHERE inviter_id=? AND request_id IS NOT NULL AND expires_at>?',peer,Date.now())[0];const own=this.active.get(peer);
  if(pending&&own)this.push(own,'join.requested',{joinRequestId:pending.request_id,peerId:pending.applicant_id});
  const pair=this.pair(peer);if(!pair)return;let room=this.rooms.get(pair);
  if(!room){const peers=this.members(pair);if(peers.length!==2)throw Error('storage');room={state:{epoch:uuid(),revision:'0',peers:peers.map((id,i)=>({id,reachable:false,busy:false,wip:'',x:i===0?.25:.75,pinned:false,inbox:[]})).sort((a,b)=>a.id.localeCompare(b.id))},commands:new Cache(this.replyBudget),copy:new Map(),departures:new Map()};this.rooms.set(pair,room);}
  let changed=false;for(const p of room.state.peers){if(this.active.has(p.id)&&!p.reachable&&this.bump(room.state)){p.reachable=true;changed=true;const timer=room.departures.get(p.id);if(timer)clearTimeout(timer);room.departures.delete(p.id);}}
  this.broadcast(pair,'session.snapshot',room.state);
  if(changed&&room.state.peers.every(p=>p.reachable))this.broadcast(pair,'session.notice',{text:this.pick(room,'caption.join')});
 }
 private act(ws:WebSocket,c:Connection,f:Frame,hash:string){
  const pair=this.pair(c.peer),room=pair?this.rooms.get(pair):undefined;if(!pair||!room){this.send(ws,this.reply(f,'not_paired'));return;}
  const key=c.peer+':'+f.requestId;if(this.replay(ws,f,hash,room.commands,key))return;
  if(!room.commands.canPut(key,512)){this.capacity(ws,f);return;}
  const cmd=command(f.body,f.requestId);let error:string|undefined;let event:Event|undefined;
  if(!this.rates.get(c.peer)?.take())error='rate_limited';else if(!cmd)error='invalid';else{
   const target=room.state.peers.find(p=>p.id===cmd.targetId),add=cmd.action==='note'||(['coffee','energy'].includes(cmd.action)&&target?.busy);
   if(add&&target&&target.inbox.length>=100)error='inbox_full';else{const result=transition(room.state,c.peer,cmd,key=>this.pick(room!,key));error=result.error;if(result.state)room.state=result.state;event=result.event;}
  }
  const reply=this.reply(f,error,event?{eventId:event.eventId,epoch:event.epoch,revision:event.revision}:{});room.commands.put(key,{digest:hash,reply});
  if(event)this.broadcast(pair,'session.event',event);this.send(ws,reply);
 }
 private async handle(ws:WebSocket,c:Connection,f:Frame){
  let hash:string,capability:string|undefined,newSecret:string|undefined;
  try{hash=await digest(encode(stable(f)));if(f.op==='auth'&&token(f.body.token))capability=await digest(f.body.token);if(f.op==='invite.join'&&token(f.body.invite))capability=await digest(f.body.invite);if(f.op==='device.register'||f.op==='invite.create'){newSecret=secret();capability=await digest(newSecret);}}
  catch{this.retire(ws,'invalid');return;}
  this.expire();if(this.connections.get(ws)!==c)return;
  if(c.peer)this.touch(ws,c);
  if(f.op==='session.command'&&c.peer){this.act(ws,c,f,hash);return;}
  if(f.op==='session.heartbeat'){this.send(ws,this.reply(f,!c.peer?'unauthenticated':!keys(f.body,[])?'invalid':!this.rates.get(c.peer)?.take()?'rate_limited':undefined));return;}
  if(this.replay(ws,f,hash,c.controls,f.requestId))return;
  if(!c.controls.canPut(f.requestId,MAX_FRAME)){this.capacity(ws,f);return;}
  if(c.peer&&!this.rates.get(c.peer)?.take()){const reply=this.reply(f,'rate_limited');c.controls.put(f.requestId,{digest:hash,reply});this.send(ws,reply);return;}
  if(c.peer&&['invite.create','invite.join','invite.approve'].includes(f.op))this.ctx.storage.sql.exec('DELETE FROM invites WHERE expires_at<=?',Date.now());
  let error:string|undefined,body:unknown={},attach=false,revoke:string[]=[];
  if(f.op==='device.register'){
   if(!keys(f.body,[]))error='invalid';else if(c.peer||c.registered)error='forbidden';else if(!this.permit(c.ip))error='rate_limited';else if(this.rows<{n:number}>('SELECT count(*) AS n FROM devices')[0].n>=4096)error='capacity';else{const peer=uuid();this.ctx.storage.sql.exec('INSERT INTO devices VALUES(?,?)',peer,capability!);c.registered=true;body={peerId:peer,token:newSecret};}
  }else if(f.op==='auth'){
   if(c.peer||!keys(f.body,['token'])||!capability){this.retire(ws,'unauthenticated');return;}
   const peer=this.rows<{id:string}>('SELECT id FROM devices WHERE token_hash=?',capability)[0]?.id;
   if(!peer){this.retire(ws,'unauthenticated');return;}
   const old=this.active.get(peer);if(old)this.retire(old,'superseded');c.peer=peer;this.touch(ws,c);this.active.set(peer,ws);if(!this.rates.has(peer))this.rates.set(peer,new Bucket());body={peerId:peer};attach=true;
  }else if(!c.peer)error='unauthenticated';
  else if(f.op==='invite.create'){
   if(!keys(f.body,[]))error='invalid';else if(!this.permit(c.ip))error='rate_limited';else if(this.pair(c.peer))error='refused';else{
    this.ctx.storage.transactionSync(()=>{this.ctx.storage.sql.exec('DELETE FROM invites WHERE inviter_id=? OR expires_at<=?',c.peer,Date.now());this.ctx.storage.sql.exec('INSERT INTO invites(code_hash,inviter_id,expires_at) VALUES(?,?,?)',capability!,c.peer,Date.now()+600000);});body={invite:newSecret,expiresIn:600};
   }
  }else if(f.op==='invite.join'){
   if(!keys(f.body,['invite'])||typeof f.body.invite!=='string')error='invalid';else{
    const invitation=capability?this.rows<Invite>('SELECT * FROM invites WHERE code_hash=? AND expires_at>?',capability,Date.now())[0]:undefined;
    if(!invitation||invitation.request_id||invitation.inviter_id===c.peer||this.pair(c.peer)||this.pair(invitation.inviter_id)||this.rows<Invite>('SELECT * FROM invites WHERE applicant_id=?',c.peer).length)error='refused';else{
     const request=uuid();this.ctx.storage.sql.exec('UPDATE invites SET request_id=?,applicant_id=? WHERE code_hash=?',request,c.peer,capability!);body={joinRequestId:request};const owner=this.active.get(invitation.inviter_id);if(owner)this.push(owner,'join.requested',{joinRequestId:request,peerId:c.peer});
    }
   }
  }else if(f.op==='invite.approve'){
   if(!keys(f.body,['joinRequestId'])||!id(f.body.joinRequestId))error='invalid';else{
    const invitation=this.rows<Invite>('SELECT * FROM invites WHERE inviter_id=? AND request_id=? AND expires_at>?',c.peer,f.body.joinRequestId,Date.now())[0];
    if(!invitation||!invitation.applicant_id||this.pair(c.peer)||this.pair(invitation.applicant_id))error='refused';else{
     const pair=uuid();this.ctx.storage.transactionSync(()=>{this.ctx.storage.sql.exec('INSERT INTO pairs VALUES(?)',pair);this.ctx.storage.sql.exec('INSERT INTO members VALUES(?,?,0),(?,?,1)',c.peer,pair,invitation.applicant_id!,pair);this.ctx.storage.sql.exec('DELETE FROM invites WHERE inviter_id IN (?,?) OR applicant_id IN (?,?)',c.peer,invitation.applicant_id!,c.peer,invitation.applicant_id!);});body={pairId:pair};attach=true;
    }
   }
  }else if(f.op==='pair.revoke'){
   if(!keys(f.body,[]))error='invalid';else{
    const pair=this.pair(c.peer);revoke=pair?this.members(pair):[c.peer];
    this.ctx.storage.transactionSync(()=>{for(const peer of revoke)this.ctx.storage.sql.exec('DELETE FROM invites WHERE inviter_id=? OR applicant_id=?',peer,peer);if(pair)this.ctx.storage.sql.exec('DELETE FROM pairs WHERE id=?',pair);});
    if(pair){const room=this.rooms.get(pair);if(room){for(const timer of room.departures.values())clearTimeout(timer);room.commands.clear();}this.rooms.delete(pair);}
   }
  }else if(f.op==='session.snapshot'){
   if(!keys(f.body,[]))error='invalid';else{const pair=this.pair(c.peer),room=pair?this.rooms.get(pair):undefined;if(!room)error='not_paired';else body=room.state;}
  }else error='invalid';
  const reply=this.reply(f,error,body);c.controls.put(f.requestId,{digest:hash,reply});this.send(ws,reply);
  if(attach&&this.connections.has(ws))this.attach(c.peer);
  for(const peer of revoke){const active=this.active.get(peer);if(active)this.retire(active,'pair_revoked');}
 }
 fetch(request:Request):Response {
  for(const [peer,bucket] of this.rates)if(!this.active.has(peer)&&Date.now()-bucket.at>=60000)this.rates.delete(peer);
  if(this.connections.size>=64)return new Response('Connection capacity',{status:503});
  const pair=new WebSocketPair(),client=pair[0],ws=pair[1];ws.accept();
  const connection:Connection={peer:'',ip:request.headers.get('CF-Connecting-IP')??'local',registered:false,controls:new Cache(this.replyBudget),frames:new Bucket(40,20),queued:0,leaseUntil:0,deadline:setTimeout(()=>this.retire(ws,'auth_timeout'),5000)};this.connections.set(ws,connection);
  ws.addEventListener('close',()=>this.drop(ws));ws.addEventListener('error',()=>this.drop(ws));
  ws.addEventListener('message',event=>{
   if(!this.connections.has(ws))return;const f=frame(event.data);
   if(!f||!f.requestId||['result','error','session.event','session.notice','join.requested'].includes(f.op)||!connection.frames.take()||connection.queued>=32||this.queued>=256){this.retire(ws,'invalid');return;}
   connection.queued++;this.queued++;
   this.queue=this.queue.then(async()=>{if(this.connections.get(ws)===connection)await this.handle(ws,connection,f);}).catch(()=>{this.send(ws,this.reply(f,'storage_unavailable'));this.retire(ws,'storage_unavailable');}).finally(()=>{connection.queued--;this.queued--;});
  });
  return new Response(null,{status:101,webSocket:client});
 }
}
export default {
 async fetch(request:Request,env:Env):Promise<Response>{
  const url=new URL(request.url);
  if(request.method==='GET'&&url.pathname==='/healthz'&&!url.search)return Response.json({status:'ok'});
  if(request.method!=='GET'||url.pathname!=='/'||url.search||url.username||url.password)return new Response('Invalid request',{status:400});
  if(request.headers.get('Origin'))return new Response('Native clients only',{status:403});
  if(request.headers.get('Upgrade')?.toLowerCase()!=='websocket')return new Response('WebSocket required',{status:426});
  try{return await env.OFFICE.getByName(env.OFFICE_ID).fetch(request);}catch{return new Response('Service unavailable',{status:503});}
 }
} satisfies ExportedHandler<Env>;
