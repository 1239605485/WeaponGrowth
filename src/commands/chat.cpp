#include "chat.hpp"
#include "engine/patch_api.hpp"
#include "hooks/registry.hpp"
#include "runtime/game.hpp"
#include "runtime/diagnostics.hpp"
namespace wg::commands {
namespace {
using namespace engine;
HookRegistry hooks;Handler dispatch=nullptr;
H text_getter=nullptr,consumed_getter=nullptr,consume=nullptr,new_text=nullptr;
bool enabled=false;
void handle(H message,const Command& command){
 if(!message||invoke<bool>(consumed_getter,message))return;
 patchlib_method_invoke_args(consume,message,nullptr,nullptr);
 if(invoke<bool>(consumed_getter,message)&&dispatch)dispatch(command);
}
void outgoing(H,void** args,void* result,const patch_method_signature_t*){
 if(!enabled||!args||!args[0]||!result)return;
 auto command=parse(string(*static_cast<H*>(args[0])));if(command.kind==Kind::Ignore)return;
 handle(*static_cast<H*>(result),command);
}
bool incoming(H,void** args,const patch_method_signature_t*,void*){
 if(!enabled||!args||!args[0])return false;auto message=*static_cast<H*>(args[0]);if(!message)return false;
 auto command=parse(string(invoke<H>(text_getter,message)));if(command.kind==Kind::Ignore)return false;
 // Remote clients can never invoke this local single-player economy.
 if(args[1]&&*static_cast<int*>(args[1])!=local_player_index())return false;
 handle(message,command);return true;
}
bool sending(H,void** args,const patch_method_signature_t*,void*){
 if(!enabled||!args||!args[0])return false;auto message=*static_cast<H*>(args[0]);if(!message)return false;
 // Consume only our commands; keep ordinary chat and commands from other mods intact.
 return parse(string(invoke<H>(text_getter,message))).kind!=Kind::Ignore;
}
}
void reply(const std::string& message){
 if(!new_text||!patchlib_string_create)return;
 auto text=patchlib_string_create(("[全武器成长] "+message).c_str());if(!text)return;
 uint8_t r=245,g=215,b=130;bool local=true;void* args[]={&text,&r,&g,&b,&local};
 patchlib_method_invoke_args(new_text,nullptr,nullptr,args);patchlib_free(text);
}
bool install(Handler handler){
 if(enabled)return true;
 auto processor=type("Terraria.Chat","ChatCommandProcessor"),message=type("Terraria.Chat","ChatMessage");
 text_getter=property(message,"Text");consumed_getter=property(message,"IsConsumed");consume=typed(message,"Consume",{});
 auto str=type("System","String"),byte=patchlib_get_basic_type(PATCH_UINT8),boolean=patchlib_get_basic_type(PATCH_BOOL),integer=patchlib_get_basic_type(PATCH_INT32);
 new_text=typed(type("Terraria","Main"),"NewText",{str,byte,byte,byte,boolean});
 bool ready=true;
 for(auto entry:{std::pair<const char*,H>{"ChatMessage.Text",text_getter},{"ChatMessage.IsConsumed",consumed_getter},{"ChatMessage.Consume()",consume},{"Main.NewText(string,byte,byte,byte,bool)",new_text}}){
  if(!entry.second){diagnostics::record(std::string("chat.metadata.missing: ")+entry.first,true);ready=false;}
 }
 if(!ready)return false;
 auto add=[&](const char* name,H method,int argc,bool stat,prefix_callback_t pre,postfix_callback_t post){
  bool ok=hooks.add(method,argc,stat,pre,post);if(!ok)diagnostics::record(std::string("chat.hook.failed: ")+name,true);return ok;
 };
 if(!add("ChatCommandProcessor.CreateOutgoingMessage(string)",typed(processor,"CreateOutgoingMessage",{str}),1,false,nullptr,outgoing)||
    !add("ChatCommandProcessor.ProcessIncomingMessage(ChatMessage,int)",typed(processor,"ProcessIncomingMessage",{message,integer}),2,false,incoming,nullptr)||
    !add("ChatHelper.SendChatMessageFromClient(ChatMessage)",typed(type("Terraria.Chat","ChatHelper"),"SendChatMessageFromClient",{message}),1,true,sending,nullptr)){
  hooks.clear();return false;
 }
 dispatch=handler;enabled=true;return true;
}
bool shutdown(){enabled=false;dispatch=nullptr;bool ok=hooks.clear();if(ok){text_getter=consumed_getter=consume=new_text=nullptr;}return ok;}
}
