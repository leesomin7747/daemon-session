import test from 'node:test';import assert from 'node:assert/strict';
import {trim,frame,command,transition} from '../src/protocol.ts';
const state=()=>({epoch:'epoch',revision:'0',peers:[{id:'a',reachable:true,busy:false,wip:'',x:.25,pinned:false,inbox:[]},{id:'b',reachable:true,busy:false,wip:'',x:.75,pinned:false,inbox:[]}]});
const cmd=(action:string,text='',busy=false)=>({requestId:'r',action,targetId:'b',text,busy});
test('Qt trim preserves BOM but removes NEL whitespace',()=>{assert.equal(trim('\u0085한글\u0085'),'한글');assert.equal(trim('\ufeff한글\ufeff'),'\ufeff한글\ufeff');});
test('wire rejects spoofed versions, arrays, unknown and malformed IDs',()=>{
 const good={v:1,op:'session.command',requestId:'r',body:{}};assert.equal(frame(JSON.stringify(good))?.op,'session.command');
 for(const bad of [{...good,v:2},{...good,body:[]},{...good,extra:1},{...good,requestId:'\ud800'},[]])assert.equal(frame(JSON.stringify(bad)),null);
});
test('Unicode limits are code points, malformed surrogates rejected',()=>{
 assert.ok(command(cmd('note','😀'.repeat(120)),'r'));assert.equal(command(cmd('note','😀'.repeat(121)),'r'),null);
 assert.equal(command(cmd('note','\ud800'),'r'),null);assert.ok(command(cmd('setWip',''),'r'));assert.equal(command(cmd('note',' '),'r'),null);
});
test('busy blocks rope, queues note without bubble and explicit ack only',()=>{
 const s=state();s.peers[1].busy=true;assert.equal(transition(s,'a',cmd('rope'),()=> 'copy').error,'busy');
 const result=transition(s,'a',cmd('note','<b>한글</b>\n😀'),()=> 'copy');assert.equal(result.event.queued,true);assert.equal(result.event.bubble,'');assert.equal(result.state.peers[1].inbox[0].text,'<b>한글</b>\n😀');assert.equal(s.peers[1].inbox.length,0);
});
test('rope and lure positions persist after unrelated actions',()=>{
 const rope=transition(state(),'a',cmd('rope'),()=> 'copy');assert.equal(rope.state.peers[1].x,.37);assert.equal(rope.state.peers[1].pinned,true);
 const lure=transition(rope.state,'b',{...cmd('lure'),targetId:'a'},()=> 'copy');assert.ok(Math.abs(lure.state.peers[0].x-.21)<1e-12);
 const ping=transition(lure.state,'a',cmd('ping'),()=> 'copy');assert.equal(ping.state.peers[0].x,lure.state.peers[0].x);assert.equal(ping.state.peers[1].x,.37);
});
test('revision exhaustion and spoofed ownership reject without mutation',()=>{
 const s=state();s.revision='18446744073709551615';assert.equal(transition(s,'a',cmd('ping'),()=> 'copy').error,'revision_exhausted');
 assert.equal(transition(state(),'a',cmd('setWip','owned'),()=> 'copy').error,'forbidden');
});
