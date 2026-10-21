#include "test.hpp"
#include "commands/chat.hpp"
#include "engine/patch_api.hpp"
#include <map>
#include <memory>
#include <cstring>
namespace wg {int local_player_index(){return 0;}}
namespace {
using H=patch_handle_t;
struct Meta {std::string name;int argc;bool stat;};
struct Message {std::string text;bool consumed=false;};
struct Hook {prefix_callback_t pre;postfix_callback_t post;};
std::map<std::string,std::unique_ptr<Meta>> metadata;
std::map<std::string,Hook> callbacks;std::string response;int calls=0,removed=0;
H token(const std::string& name,int count=0){auto& m=metadata[name];if(!m)m=std::make_unique<Meta>(Meta{name,count,name=="SendChatMessageFromClient"});return m.get();}
void dispatch(const wg::commands::Command& c){CHECK(c.kind==wg::commands::Kind::Draw);CHECK(c.count==1);++calls;}
}
int main(){
 patchlib_type_get_type=[](const char*,const char* n)->H{return token(n);};
 patchlib_get_basic_type=[](patch_type_t)->H{return token("basic");};
 patchlib_type_get_property=[](H,const char* n)->H{return token(n);};
 patchlib_property_get_get_method=[](H p)->H{return p;};
 patchlib_type_get_method_by_param_types=[](H,const char* n,int count,const H*)->H{return token(n,count);};
 patchlib_method_get_param_count=[](H m){return static_cast<Meta*>(m)->argc;};
 patchlib_method_is_static=[](H m){return static_cast<Meta*>(m)->stat;};
 patchlib_install_prepost_hook=[](H m,prefix_callback_t pre,postfix_callback_t post)->patch_hook_id_t{callbacks[static_cast<Meta*>(m)->name]={pre,post};return patch_hook_id_t(callbacks.size());};
 patchlib_uninstall_hook=[](patch_hook_id_t){++removed;return true;};
 patchlib_string_cstr=[](H s)->char*{auto& value=*static_cast<std::string*>(s);auto p=static_cast<char*>(std::malloc(value.size()+1));std::memcpy(p,value.c_str(),value.size()+1);return p;};
 patchlib_string_create=[](const char* s)->H{return new std::string(s);};patchlib_free=[](H s){delete static_cast<std::string*>(s);};
 patchlib_method_invoke_args=[](H m,H instance,void* result,void** args){
  auto& name=static_cast<Meta*>(m)->name;
  if(name=="Text")*static_cast<H*>(result)=&static_cast<Message*>(instance)->text;
  else if(name=="IsConsumed")*static_cast<bool*>(result)=static_cast<Message*>(instance)->consumed;
  else if(name=="Consume")static_cast<Message*>(instance)->consumed=true;
  else if(name=="NewText"){
   CHECK(*static_cast<bool*>(args[4]));CHECK(*static_cast<uint8_t*>(args[1])==245);
   response=*static_cast<std::string*>(*static_cast<H*>(args[0]));
  }
  return true;
 };
 CHECK(wg::commands::install(dispatch));CHECK(callbacks.size()==3);
 std::string raw="/随机单抽";H raw_handle=&raw;Message message{raw};H handle=&message;void* args[]={&raw_handle};
 callbacks.at("CreateOutgoingMessage").post(nullptr,args,&handle,nullptr);CHECK(calls==1&&message.consumed);
 int local=0;void* incoming[]={&handle,&local};
 CHECK(callbacks.at("ProcessIncomingMessage").pre(nullptr,incoming,nullptr,nullptr));CHECK(calls==1);
 void* sending[]={&handle};CHECK(callbacks.at("SendChatMessageFromClient").pre(nullptr,sending,nullptr,nullptr));
 Message ordinary{"normal chat"};handle=&ordinary;raw="normal chat";
 callbacks.at("CreateOutgoingMessage").post(nullptr,args,&handle,nullptr);CHECK(calls==1&&!ordinary.consumed);
 CHECK(!callbacks.at("ProcessIncomingMessage").pre(nullptr,incoming,nullptr,nullptr));
 CHECK(!callbacks.at("SendChatMessageFromClient").pre(nullptr,sending,nullptr,nullptr));
 Message remote{"/随机单抽"};handle=&remote;int other=1;incoming[1]=&other;
 CHECK(!callbacks.at("ProcessIncomingMessage").pre(nullptr,incoming,nullptr,nullptr));CHECK(calls==1&&!remote.consumed);
 // Fallback platforms can enter directly through ProcessIncomingMessage.
 incoming[1]=&local;CHECK(callbacks.at("ProcessIncomingMessage").pre(nullptr,incoming,nullptr,nullptr));CHECK(calls==2&&remote.consumed);
 wg::commands::reply("ok");CHECK(response.find("ok")!=std::string::npos);
 CHECK(wg::commands::shutdown());CHECK(removed==3);
 std::puts("chat transport/consume/local-only reply: passed");
}
