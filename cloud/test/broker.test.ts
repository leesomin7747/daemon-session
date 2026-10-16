import test from 'node:test';import assert from 'node:assert/strict';import {Miniflare,Log,LogLevel,convertV4MiniflareOptions} from 'miniflare';import {mkdtemp,rm,readdir,readFile} from 'node:fs/promises';import {tmpdir} from 'node:os';import {join} from 'node:path';
const encoder=new TextEncoder();
function server(path?:string){return new Miniflare(convertV4MiniflareOptions({name:'daemon-session-tests',modules:true,scriptPath:'dist/index.js',compatibilityDate:'2026-10-12',compatibilityFlags:['nodejs_compat'],bindings:{OFFICE_ID:'test-office'},durableObjects:{OFFICE:{className:'Office',useSQLite:true}},resourcePersistencePath:path,unsafeInspectDurableObjects:true,logRequests:false,telemetry:{enabled:false},log:new Log(LogLevel.ERROR)}));}
async function until(check:()=>boolean){const end=Date.now()+3000;while(!check()&&Date.now()<end)await new Promise(r=>setTimeout(r,5));assert.ok(check(),'Expected protocol message not received');}
async function peer(mf:Miniflare){const response=await mf.dispatchFetch('http://office/',{headers:{Upgrade:'websocket'}});assert.equal(response.status,101);const ws=response.webSocket!;const messages:any[]=[];ws.accept();ws.addEventListener('message',event=>messages.push(JSON.parse(event.data as string)));let closed=false;ws.addEventListener('close',()=>closed=true);
 return {ws,messages,get closed(){return closed;},send(op:string,body:object={},id=crypto.randomUUID()){ws.send(JSON.stringify({v:1,op,requestId:id,body}));return id;},async result(id:string){await until(()=>messages.some(m=>m.requestId===id));return messages.find(m=>m.requestId===id);},async auth(token?:string){let peerId='';if(!token){const r=await this.result(this.send('device.register'));token=r.body.token;peerId=r.body.peerId;}const r=await this.result(this.send('auth',{token}));assert.equal(r.op,'result');return {token:token!,id:peerId||r.body.peerId};}};
}
async function pair(mf:Miniflare){const a=await peer(mf),b=await peer(mf);const ca=await a.auth(),cb=await b.auth();const code=(await a.result(a.send('invite.create'))).body.invite;const request=(await b.result(b.send('invite.join',{invite:code}))).body.joinRequestId;assert.ok(!a.messages.some(m=>m.op==='session.snapshot'));await a.result(a.send('invite.approve',{joinRequestId:request}));await until(()=>a.messages.some(m=>m.op==='session.snapshot')&&b.messages.some(m=>m.op==='session.snapshot'));return {a,b,ca,cb};}
function command(peer:any,target:string,action:string,text='',busy=false,id=crypto.randomUUID()){peer.send('session.command',{requestId:id,action,targetId:target,text,busy},id);return id;}
test('handshake requires exact path, no Origin/query and WebSocket upgrade',async()=>{const mf=server();try{assert.equal((await mf.dispatchFetch('http://office/')).status,426);assert.equal((await mf.dispatchFetch('http://office/?token=no',{headers:{Upgrade:'websocket'}})).status,400);assert.equal((await mf.dispatchFetch('http://office/',{headers:{Upgrade:'websocket',Origin:'https://other'}})).status,403);}finally{await mf.dispose();}});
test('heartbeat requires authentication and an empty body without producing scene events',async()=>{const mf=server();try{
 const a=await peer(mf);assert.equal((await a.result(a.send('session.heartbeat'))).body.code,'unauthenticated');
 await a.auth();assert.equal((await a.result(a.send('session.heartbeat',{unexpected:true}))).body.code,'invalid');
 assert.deepEqual((await a.result(a.send('session.heartbeat'))).body,{});
 assert.equal(a.messages.filter(m=>m.op==='session.event').length,0);
 }finally{await mf.dispose();}});
test('large snapshots retire a full control cache; accepted command replay remains exact after reconnect',async()=>{const mf=server();try{
 const {a,b,ca,cb}=await pair(mf);let last='';
 for(let n=0;n<76;n++){last=command(a,cb.id,'note','x'.repeat(120));assert.equal((await a.result(last)).op,'result');await new Promise(r=>setTimeout(r,120));}
 const receipt=a.messages.find(m=>m.requestId===last);let full=false;
 for(let n=0;n<30;n++){const r=await a.result(a.send('session.snapshot'));if(r.body.code==='capacity'){full=true;break;}assert.equal(r.op,'result');await new Promise(r=>setTimeout(r,120));}
 assert.ok(full);await until(()=>a.closed);
 const back=await peer(mf);await back.auth(ca.token);command(back,cb.id,'note','x'.repeat(120),false,last);
 assert.deepEqual(await back.result(last),receipt);
 assert.equal(b.messages.filter(m=>m.op==='session.event'&&m.body.requestId===last).length,1);
 }finally{await mf.dispose();}});
test('a silent recipient expires, cannot receive offline mail, and reconnect does not replay it',async()=>{const mf=server();let keep:ReturnType<typeof setInterval>|undefined;try{
 const {a,b,cb}=await pair(mf);
 keep=setInterval(()=>a.send('session.snapshot'),5000);
 await new Promise(r=>setTimeout(r,31000));
 const receipt=await a.result(command(a,cb.id,'note','must not become offline mail'));
 assert.equal(receipt.body.code,'offline');assert.ok(b.closed);
 const back=await peer(mf);await back.auth(cb.token);await until(()=>back.messages.some(m=>m.op==='session.snapshot'));
 assert.equal(back.messages.find(m=>m.op==='session.snapshot').body.peers.find((p:any)=>p.id===cb.id).inbox.length,0);
 assert.equal(back.messages.filter(m=>m.op==='session.event').length,0);
 }finally{if(keep)clearInterval(keep);await mf.dispose();}});
test('approval, busy queue, dedup/conflict, offline rejection and revoke use real SQLite/WebSockets',async()=>{const mf=server();try{
 const {a,b,ca,cb}=await pair(mf);let id=command(b,cb.id,'setBusy','',true);assert.equal((await b.result(id)).op,'result');
 id=command(a,cb.id,'rope');assert.equal((await a.result(id)).body.code,'busy');
 id=command(a,cb.id,'note','\ufeff<b>한글</b>\n😀');assert.equal((await a.result(id)).op,'result');await until(()=>b.messages.some(m=>m.op==='session.event'&&m.body.requestId===id));
 const received=b.messages.find(m=>m.op==='session.event'&&m.body.requestId===id).body;assert.equal(received.text,'\ufeff<b>한글</b>\n😀');assert.equal(received.queued,true);assert.equal(received.bubble,'');
 command(a,cb.id,'note','\ufeff<b>한글</b>\n😀',false,id);await new Promise(r=>setTimeout(r,30));assert.equal(b.messages.filter(m=>m.op==='session.event'&&m.body.requestId===id).length,1);
 a.messages.length=0;command(a,cb.id,'note','changed',false,id);assert.equal((await a.result(id)).body.code,'request_conflict');
 b.ws.close();await until(()=>a.messages.some(m=>m.op==='session.snapshot'&&m.body.peers.some((p:any)=>p.id===cb.id&&!p.reachable)));
 id=command(a,cb.id,'note','offline');assert.equal((await a.result(id)).body.code,'offline');
 const back=await peer(mf);await back.auth(cb.token);await until(()=>back.messages.some(m=>m.op==='session.snapshot'));assert.equal(back.messages.find(m=>m.op==='session.snapshot').body.peers.find((p:any)=>p.id===cb.id).inbox.length,1);
 await a.result(a.send('pair.revoke'));await until(()=>a.closed&&back.closed);
 }finally{await mf.dispose();}});
test('same identity replaces old socket before it can act',async()=>{const mf=server();try{const {a,b,ca,cb}=await pair(mf);const back=await peer(mf);await back.auth(ca.token);await until(()=>a.closed);const id=command(back,cb.id,'ping');assert.equal((await back.result(id)).op,'result');await until(()=>b.messages.some(m=>m.op==='session.event'&&m.body.requestId===id));}finally{await mf.dispose();}});
test('auth and pair survive runtime restart without persisting note bodies or bearer tokens',async()=>{
 const path=await mkdtemp(join(tmpdir(),'daemon-cloud-'));let mf=server(path);let tokenA='',tokenB='',idB='';const note='private-fixture-'+crypto.randomUUID();
 try{const {a,b,ca,cb}=await pair(mf);tokenA=ca.token;tokenB=cb.token;idB=cb.id;await a.result(command(a,cb.id,'note',note));a.ws.close();b.ws.close();await mf.dispose();
 async function files(p:string):Promise<string[]>{const entries=await readdir(p,{withFileTypes:true});return (await Promise.all(entries.map(e=>e.isDirectory()?files(join(p,e.name)):[join(p,e.name)]))).flat();}
 for(const f of await files(path)){const bytes=await readFile(f);assert.equal(bytes.includes(encoder.encode(note)),false);assert.equal(bytes.includes(encoder.encode(tokenA)),false);assert.equal(bytes.includes(encoder.encode(tokenB)),false);}
 mf=server(path);const backA=await peer(mf),backB=await peer(mf);await backA.auth(tokenA);await backB.auth(tokenB);await until(()=>backB.messages.some(m=>m.op==='session.snapshot'));assert.equal(backB.messages.find(m=>m.op==='session.snapshot').body.peers.find((p:any)=>p.id===idB).inbox.length,0);
 }finally{await mf.dispose();await rm(path,{recursive:true,force:true});}
});
test('device rate budget cannot be reset by reconnecting',async()=>{const mf=server();try{const {a,ca}=await pair(mf);let limited=false;for(let i=0;i<25;i++){const reply=await a.result(a.send('session.snapshot'));limited ||= reply.body.code==='rate_limited';}assert.ok(limited);const back=await peer(mf);await back.auth(ca.token);assert.equal((await back.result(back.send('session.snapshot'))).body.code,'rate_limited');}finally{await mf.dispose();}});
test('expired pending request cannot be approved and cannot block a fresh invitation',async()=>{const mf=server();try{
 const a=await peer(mf),b=await peer(mf),c=await peer(mf);await a.auth();const cb=await b.auth();await c.auth();
 const old=(await a.result(a.send('invite.create'))).body.invite;const pending=(await b.result(b.send('invite.join',{invite:old}))).body.joinRequestId;
 const fresh=(await c.result(c.send('invite.create'))).body.invite;
 const storage=await mf.unsafeGetDurableObjectStorage('daemon-session-tests','Office',{name:'test-office'});
 await storage.exec('UPDATE invites SET expires_at=0 WHERE applicant_id=?',cb.id);
 assert.equal((await a.result(a.send('invite.approve',{joinRequestId:pending}))).body.code,'refused');
 assert.equal((await b.result(b.send('invite.join',{invite:fresh}))).op,'result');
 }finally{await mf.dispose();}});
