// Account UTF-16 strings plus a conservative fixed allowance per map entry.
// Admission happens before side effects; exhaustion rejects and retires that socket.
export class ReplyBudget {used=0;readonly limit=16*1024*1024;}
type Reply={digest:string;reply:string};
export class Cache {
 readonly budget:ReplyBudget;readonly limit=512*1024;private used=0;
 values=new Map<string,Reply>();
 constructor(budget:ReplyBudget){this.budget=budget;}
 private size(key:string,value:Reply){return 128+2*(key.length+value.digest.length+value.reply.length);}
 private replaced(key:string){const old=this.values.get(key);if(old)return this.size(key,old);if(this.values.size<256)return 0;const first=this.values.entries().next().value!;return this.size(first[0],first[1]);}
 canPut(key:string,replyChars:number){const delta=128+2*(key.length+64+replyChars)-this.replaced(key);return this.used+delta<=this.limit&&this.budget.used+delta<=this.budget.limit;}
 private remove(key:string){const value=this.values.get(key);if(!value)return;const size=this.size(key,value);this.used-=size;this.budget.used-=size;this.values.delete(key);}
 put(key:string,value:Reply){
  if(!this.canPut(key,value.reply.length))throw Error('reply_capacity');
  this.remove(key);if(this.values.size>=256)this.remove(this.values.keys().next().value!);
  const size=this.size(key,value);this.values.set(key,value);this.used+=size;this.budget.used+=size;
 }
 clear(){this.budget.used-=this.used;this.used=0;this.values.clear();}
}
