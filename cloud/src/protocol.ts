export const MAX_FRAME=16*1024;
export const actions=['ping','rope','lure','blanket','plane','coffee','energy','note','setBusy','setWip','ackInbox'] as const;
export type Action=typeof actions[number];
export interface Command {requestId:string;action:Action;targetId:string;text:string;busy:boolean}
export interface Peer {id:string;reachable:boolean;busy:boolean;wip:string;x:number;pinned:boolean;inbox:{eventId:string;kind:Action;text:string}[]}
export interface State {epoch:string;revision:string;peers:Peer[]}
export interface Event {eventId:string;requestId:string;epoch:string;revision:string;action:Action;actorId:string;targetId:string;caption:string;bubble:string;finalX:number;queued:boolean;text:string;busy:boolean}
export interface Frame {v:1;op:string;requestId:string;body:Record<string,unknown>}
const spaces='\\u0009-\\u000D\\u0020\\u0085\\u00A0\\u1680\\u2000-\\u200A\\u2028\\u2029\\u202F\\u205F\\u3000';
const surrounding=new RegExp(`^[${spaces}]+|[${spaces}]+$`,'g');
export const trim=(text:string)=>text.replace(surrounding,'');
export const record=(value:unknown):value is Record<string,unknown>=>typeof value==='object'&&value!==null&&!Array.isArray(value);
export const keys=(value:Record<string,unknown>,names:string[])=>Object.keys(value).length===names.length&&names.every(key=>Object.hasOwn(value,key));
export const fits=(value:unknown,limit:number,empty=true):value is string=>typeof value==='string'&&!/[\uD800-\uDFFF]/u.test(value)&&Array.from(value).length<=limit&&(empty||trim(value)!=='');
export const id=(value:unknown):value is string=>fits(value,128,false)&&trim(value)===value&&!/\p{Cc}/u.test(value);
export const encode=(value:unknown)=>JSON.stringify(value).replaceAll('\ufeff','\\ufeff');
export const bytes=(value:string)=>new TextEncoder().encode(value).length;
export function frame(data:unknown):Frame|null {
 if(typeof data!=='string'||data.length===0||data.length>MAX_FRAME||bytes(data)>MAX_FRAME)return null;
 let value:unknown;try{value=JSON.parse(data);}catch{return null;}
 if(!record(value)||!keys(value,['v','op','requestId','body'])||value.v!==1||typeof value.op!=='string'||!record(value.body)||typeof value.requestId!=='string')return null;
 const ops=['device.register','auth','invite.create','invite.join','invite.approve','pair.revoke','session.command','session.snapshot','session.heartbeat','result','error','session.event','session.notice','join.requested'];
 if(!ops.includes(value.op))return null;
 const push=['session.event','session.notice','join.requested'].includes(value.op)||(value.op==='session.snapshot'&&value.requestId==='');
 if(push?value.requestId!=='':!id(value.requestId))return null;
 return {v:1,op:value.op,requestId:value.requestId,body:value.body};
}
export function command(body:unknown,requestId:string):Command|null {
 if(!record(body)||!keys(body,['requestId','action','targetId','text','busy'])||body.requestId!==requestId||!id(requestId)||!id(body.targetId)||typeof body.action!=='string'||!actions.includes(body.action as Action)||typeof body.text!=='string'||typeof body.busy!=='boolean')return null;
 const action=body.action as Action,text=trim(body.text);
 if(action!=='setBusy'&&body.busy)return null;
 if(action==='note'?!fits(text,120,false):action==='setWip'?!fits(text,80):text!=='')return null;
 return {requestId,action,targetId:body.targetId,text,busy:body.busy};
}
export function transition(state:State,actor:string,c:Command,pick:(key:string)=>string):{state?:State;event?:Event;error?:string} {
 if(BigInt(state.revision)>=18446744073709551615n)return {error:'revision_exhausted'};
 const source=state.peers.find(p=>p.id===actor),target=state.peers.find(p=>p.id===c.targetId);
 if(!source||!target)return {error:'forbidden'};
 const own=['setBusy','setWip','ackInbox'].includes(c.action),gift=['coffee','energy'].includes(c.action),note=c.action==='note';
 if(own!==(actor===c.targetId))return {error:'forbidden'};
 if(!source.reachable||(!own&&!target.reachable))return {error:'offline'};
 if(!own&&target.busy&&!gift&&!note)return {error:'busy'};
 const next:State=structuredClone(state),recipient=next.peers.find(p=>p.id===c.targetId)!;
 next.revision=(BigInt(next.revision)+1n).toString();
 const event:Event={eventId:crypto.randomUUID(),requestId:c.requestId,epoch:state.epoch,revision:next.revision,action:c.action,actorId:actor,targetId:c.targetId,caption:'',bubble:'',finalX:target.x,queued:target.busy&&(gift||note),text:note||c.action==='setWip'?c.text:'',busy:target.busy};
 let caption='caption.'+c.action;
 if(c.action==='setBusy'){recipient.busy=c.busy;caption=c.busy?'caption.busy':'caption.ready';}
 else if(c.action==='setWip')recipient.wip=c.text;
 else if(c.action==='ackInbox')recipient.inbox=[];
 else if(c.action==='rope'||c.action==='lure'){
  const gap=c.action==='rope'?.12:.16,direction=target.x>=source.x?1:-1;let destination=source.x+direction*gap;
  if(destination<0||destination>1)destination=source.x-direction*gap;
  recipient.x=Math.max(0,Math.min(1,destination));recipient.pinned=true;
 }
 if(note||event.queued){const text=note?c.text:c.action==='coffee'?'친구가 커피 한 잔을 두고 갔어요.':'친구가 에너지음료를 두고 갔어요.';recipient.inbox.push({eventId:event.eventId,kind:c.action,text});event.text=text;}
 const limit=MAX_FRAME-1024-((note||event.queued||c.action==='setWip')?64:0);
 if(bytes(encode(next))>limit)return {error:'session_full'};
 if(event.queued)caption=gift?'caption.queuedGift':'caption.queuedNote';
 event.caption=pick(caption);if(!own&&!note&&!event.queued)event.bubble=pick('bubble.'+c.action);
 event.finalX=recipient.x;event.busy=recipient.busy;return {state:next,event};
}
