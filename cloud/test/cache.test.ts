import test from 'node:test';
import assert from 'node:assert/strict';
import {Cache,ReplyBudget} from '../src/cache.ts';
import {encode,transition,type State} from '../src/protocol.ts';

function largeSnapshot(){
 let state:State={epoch:crypto.randomUUID(),revision:'0',peers:['a','b'].map(id=>({id,reachable:true,busy:false,wip:'',x:.5,pinned:false,inbox:[]}))};
 for(let n=0;n<76;n++)state=transition(state,'a',{requestId:crypto.randomUUID(),action:'note',targetId:'b',text:'x'.repeat(120),busy:false},()=> '').state!;
 return encode({v:1,op:'result',requestId:crypto.randomUUID(),body:state});
}
test('large snapshot replies stop admission within the per-cache budget without losing accepted retries',()=>{
 const budget=new ReplyBudget(),cache=new Cache(budget),reply=largeSnapshot();let accepted=0;
 for(let n=0;n<256;n++){
  const key=crypto.randomUUID();if(!cache.canPut(key,reply.length))break;
  cache.put(key,{digest:'d'.repeat(64),reply});accepted++;
  assert.ok(budget.used<=512*1024,'one cache exceeded 512 KiB of retained string storage');
 }
 assert.ok(accepted>0&&accepted<256,'large snapshots must have finite byte admission');
 assert.equal(cache.values.size,accepted,'accepted retries remain exact');
 assert.equal([...cache.values.values()][0].reply,reply);
 cache.clear();assert.equal(budget.used,0);
});
test('all 64 connections share one bounded budget; FIFO replacement and disposal release it',()=>{
 const budget=new ReplyBudget(),caches=Array.from({length:64},()=>new Cache(budget));const reply='x'.repeat(15000);let refused=false;
 for(let n=0;n<256&&!refused;n++)for(const cache of caches){
  const key=crypto.randomUUID();if(!cache.canPut(key,reply.length)){refused=true;break;}
  cache.put(key,{digest:'d'.repeat(64),reply});assert.ok(budget.used<=budget.limit);
 }
 assert.ok(refused);assert.ok(budget.used>0);
 for(const cache of caches)cache.clear();assert.equal(budget.used,0);
 const cache=new Cache(budget);for(let n=0;n<300;n++){assert.ok(cache.canPut(String(n),100));cache.put(String(n),{digest:'d',reply:'x'.repeat(100)});}
 assert.equal(cache.values.size,256);assert.equal(cache.values.has('0'),false);cache.clear();assert.equal(budget.used,0);
});
