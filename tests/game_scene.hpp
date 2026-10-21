#pragma once
#include "metadata_host.hpp"
#include "test.hpp"
#include <array>
#include <cstring>
namespace game_scene {
using namespace metadata_host;
using Bytes=std::array<unsigned char,16>;
struct Object {std::map<std::string,Bytes> fields;bool consumed=false;H text=nullptr;};
inline std::vector<std::unique_ptr<Object>> objects;
inline std::vector<std::unique_ptr<std::vector<H>>> arrays;
inline std::vector<std::unique_ptr<std::string>> strings;
inline std::map<std::string,Bytes> statics;
inline std::vector<std::string> replies;
inline H player=nullptr,weapon=nullptr,inventory=nullptr,file=nullptr;
inline int clones=0,syncs=0;
inline H object(){objects.push_back(std::make_unique<Object>());return objects.back().get();}
inline H array(std::vector<H> values){arrays.push_back(std::make_unique<std::vector<H>>(std::move(values)));return arrays.back().get();}
inline H text(const std::string& value){strings.push_back(std::make_unique<std::string>(value));return strings.back().get();}
template<class T>void set(H obj,const std::string& field,T value){auto& bytes=static_cast<Object*>(obj)->fields[field];std::memcpy(bytes.data(),&value,sizeof value);}
template<class T>void global(const std::string& field,T value){auto& bytes=statics[field];std::memcpy(bytes.data(),&value,sizeof value);}
// Mirror PatchLib common/field.c, including its pointer-sized value-type copy.
inline size_t field_size(const Json& data){auto type=data["type"].get<std::string>();if(type=="bool"||type=="byte")return 1;if(type=="int"||type=="uint"||type=="float")return 4;return sizeof(H);}
inline void bind(const std::string& identity){
 player=object();weapon=object();file=object();
 set(weapon,"type",3507);set(weapon,"damage",5);set(weapon,"melee",true);set(weapon,"useTime",13);
 std::vector<H> slots(50);for(auto& slot:slots)slot=object();inventory=array(slots);set(player,"inventory",inventory);
 global("Terraria.Main.player",array({player}));global("Terraria.Main.projectile",array({}));
 global("Terraria.NPC.downedMoonlord",true);global("Terraria.Main.hardMode",true);
 auto recipe=object();set(recipe,"createItem",weapon);set(recipe,"requiredItem",array({}));set(recipe,"acceptedGroups",array({}));
 global("Terraria.Main.recipe",array({recipe}));global("Terraria.Recipe.numRecipes",1);
 static std::string profile;profile=identity;
 patchlib_field_get_value=[](H h,H obj,void* result){auto& m=*static_cast<Meta*>(h);auto& bytes=m.data["static"].get<bool>()?statics[m.owner+"."+m.name]:static_cast<Object*>(obj)->fields[m.name];std::memcpy(result,bytes.data(),field_size(m.data));};
 patchlib_field_set_value=[](H h,H obj,void* value){auto& m=*static_cast<Meta*>(h);auto& bytes=m.data["static"].get<bool>()?statics[m.owner+"."+m.name]:static_cast<Object*>(obj)->fields[m.name];std::memcpy(bytes.data(),value,field_size(m.data));};
 patchlib_array_length=[](H arr)->size_t{return static_cast<std::vector<H>*>(arr)->size();};
 patchlib_array_at=[](H arr,size_t index,void* out){auto& values=*static_cast<std::vector<H>*>(arr);if(index>=values.size())return false;std::memcpy(out,&values[index],sizeof(H));return true;};
 patchlib_array_set=[](H arr,size_t index,void* value){auto& values=*static_cast<std::vector<H>*>(arr);if(index>=values.size())return false;values[index]=*static_cast<H*>(value);return true;};
 patchlib_string_create=[](const char* value)->H{return text(value);};
 patchlib_string_cstr=[](H h)->char*{auto& s=*static_cast<std::string*>(h);auto copy=static_cast<char*>(std::malloc(s.size()+1));std::memcpy(copy,s.c_str(),s.size()+1);return copy;};
 patchlib_method_invoke_args=[](H h,H obj,void* result,void** args){
  auto& m=*static_cast<Meta*>(h);uint64_t value=0;
  if(m.name=="get_myPlayer")value=0;
  else if(m.name=="get_worldID")value=123;
  else if(m.name=="get_ActivePlayerFileData")value=reinterpret_cast<uintptr_t>(file);
  else if(m.name=="get_Path")value=reinterpret_cast<uintptr_t>(text(profile));
  else if(m.name=="get_HeldItem")value=reinterpret_cast<uintptr_t>(weapon);
  else if(m.name=="get_Name")value=reinterpret_cast<uintptr_t>(text("Test blade"));
  else if(m.name=="get_Text")value=reinterpret_cast<uintptr_t>(static_cast<Object*>(obj)->text);
  else if(m.name=="get_IsConsumed")value=static_cast<Object*>(obj)->consumed;
  else if(m.name=="Consume")static_cast<Object*>(obj)->consumed=true;
  else if(m.name=="Clone"){auto copy=object();*static_cast<Object*>(copy)=*static_cast<Object*>(obj);++clones;value=reinterpret_cast<uintptr_t>(copy);}
  else if(m.name=="SendData"){CHECK(*static_cast<int*>(args[0])==5);CHECK(*static_cast<int*>(args[4])==0);++syncs;}
  else if(m.name=="NewText"){CHECK(*static_cast<bool*>(args[4]));replies.push_back(*static_cast<std::string*>(*static_cast<H*>(args[0])));}
  // Match libffi's machine-word writes for narrow integer/bool results.
  if(result)std::memcpy(result,&value,sizeof(value));return true;
 };
}
inline Hook& hook(const char* name){for(auto& h:hooks)if(!h.removed&&h.meta->name==name)return h;std::abort();}
inline void tick(){hook("UpdateTime").post(nullptr,nullptr,nullptr,nullptr);}
inline void command(const std::string& value){
 H raw=text(value),msg=object();static_cast<Object*>(msg)->text=raw;void* args[]={&raw};
 auto callback=hook("CreateOutgoingMessage").post;callback(nullptr,args,&msg,nullptr);
 CHECK(static_cast<Object*>(msg)->consumed);
 // A second delivery of the same consumed ChatMessage must be idempotent.
 callback(nullptr,args,&msg,nullptr);tick();
}
inline bool said(const std::string& value){return std::any_of(replies.begin(),replies.end(),[&](auto& s){return s.find(value)!=std::string::npos;});}
}
