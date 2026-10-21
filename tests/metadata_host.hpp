#pragma once
#include "engine/patch_api.hpp"
#include "fixtures/game_metadata.hpp"
#include "../vendor/json.hpp"
#include <map>
#include <memory>
#include <vector>
#include <algorithm>
// Only actual declarations resolve; unknown members never get fake handles.
namespace metadata_host {
using H=patch_handle_t;using Json=nlohmann::json;
inline Json schema=Json::parse(game_metadata_json);
struct Meta {std::string owner,name;Json data;};
struct Hook {Meta* meta;prefix_callback_t pre;postfix_callback_t post;bool removed=false;};
inline std::map<std::string,std::unique_ptr<Meta>> handles;
inline std::vector<Hook> hooks;
inline std::string omitted;inline std::vector<std::string> missing;
inline H token(std::string owner,std::string name,std::string kind,Json data={}){
 auto key=owner+":"+kind+":"+name+":"+data.dump();auto& h=handles[key];
 if(!h)h=std::make_unique<Meta>(Meta{owner,name,std::move(data)});return h.get();
}
inline std::string canonical(std::string s){
 static const std::map<std::string,std::string> aliases={{"int","System.Int32"},{"bool","System.Boolean"},{"byte","System.Byte"},{"float","System.Single"},{"string","System.String"},{"uint","System.UInt32"},{"void","System.Void"}};
 auto it=aliases.find(s);if(it!=aliases.end())return it->second;
 for(const auto& entry:schema.items())if(entry.key().substr(entry.key().find_last_of('.')+1)==s)return entry.key();return s;
}
inline H get_type(const std::string& full){
 if(schema.contains(full)||full.rfind("System.",0)==0)return token(full,full,"type");missing.push_back("type:"+full);return nullptr;
}
inline H member(H t,const char* name,const char* kind){
 if(!t)return nullptr;auto owner=static_cast<Meta*>(t)->owner;
 for(int depth=0;depth<8&&schema.contains(owner);++depth){auto& type=schema[owner];if(type[kind].contains(name))return token(owner,name,kind,type[kind][name]);owner=canonical(type["base"].get<std::string>());}return nullptr;
}
inline H get_method(H t,const char* name,int count,const H* types=nullptr,const char** names=nullptr){
 if(!t||omitted==name)return nullptr;auto owner=static_cast<Meta*>(t)->owner;
 for(int depth=0;depth<8&&schema.contains(owner);++depth){
  for(const auto& m:schema[owner]["methods"]){if(m["name"]!=name||int(m["params"].size())!=count)continue;
   bool matches=true;for(int i=0;i<count;++i){if(names&&m["params"][i]["name"]!=names[i])matches=false;if(types&&(!types[i]||canonical(m["params"][i]["type"])!=static_cast<Meta*>(types[i])->owner))matches=false;}
   if(matches)return token(owner,name,"method",m);
  }owner=canonical(schema[owner]["base"].get<std::string>());
 }missing.push_back("method:"+static_cast<Meta*>(t)->owner+"."+name+"/"+std::to_string(count));return nullptr;
}
inline void bind(){
 patchlib_type_get_type=[](const char* ns,const char* n)->H{return get_type(std::string(ns)+"."+n);};
 patchlib_get_basic_type=[](patch_type_t t)->H{switch(t){case PATCH_INT32:return get_type("System.Int32");case PATCH_FLOAT:return get_type("System.Single");case PATCH_BOOL:return get_type("System.Boolean");case PATCH_UINT8:return get_type("System.Byte");default:return nullptr;}};
 patchlib_type_get_inner_type=[](H t,const char* n)->H{return t?get_type(static_cast<Meta*>(t)->owner+"+"+n):nullptr;};
 patchlib_type_get_field=[](H t,const char* n)->H{return member(t,n,"fields");};
 patchlib_type_get_property=[](H t,const char* n)->H{return member(t,n,"properties");};
 patchlib_type_get_method_by_param_count=[](H t,const char* n,int c)->H{return get_method(t,n,c);};
 patchlib_type_get_method_by_param_types=[](H t,const char* n,int c,const H* ts)->H{return get_method(t,n,c,ts);};
 patchlib_type_get_method_by_param_names=[](H t,const char* n,int c,const char** ns)->H{return get_method(t,n,c,nullptr,ns);};
 patchlib_property_get_get_method=[](H p)->H{if(!p)return nullptr;auto& m=*static_cast<Meta*>(p);return token(m.owner,"get_"+m.name,"method",{{"static",m.data["static"]},{"params",Json::array()},{"return",m.data["type"]}});};
 patchlib_field_is_instance=[](H p){return !static_cast<Meta*>(p)->data["static"].get<bool>();};
 patchlib_method_get_param_count=[](H p){return int(static_cast<Meta*>(p)->data["params"].size());};
 patchlib_method_is_static=[](H p){return static_cast<Meta*>(p)->data["static"].get<bool>();};
 patchlib_method_get_name=[](H p){return static_cast<Meta*>(p)->name.c_str();};patchlib_method_get_return_type=nullptr;
 patchlib_install_prepost_hook=[](H p,prefix_callback_t pre,postfix_callback_t post)->patch_hook_id_t{hooks.push_back({static_cast<Meta*>(p),pre,post});return patch_hook_id_t(hooks.size()-1);};
 patchlib_uninstall_hook=[](patch_hook_id_t id){hooks.at(size_t(id)).removed=true;return true;};
 patchlib_field_get_value=[](H,H,void*){};patchlib_field_set_value=[](H,H,void*){};patchlib_method_invoke_args=[](H,H,void*,void**){return true;};
 patchlib_array_at=[](H,size_t,void*){return false;};patchlib_array_set=[](H,size_t,void*){return false;};patchlib_array_length=[](H)->size_t{return 0;};
 patchlib_string_create=[](const char*)->H{return nullptr;};patchlib_string_cstr=[](H)->char*{return nullptr;};patchlib_free=[](H){};
}
inline bool hooked(const char* name){return std::any_of(hooks.begin(),hooks.end(),[&](const Hook& h){return !h.removed&&h.meta->name==name;});}
inline bool cleared(){return std::all_of(hooks.begin(),hooks.end(),[](const Hook& h){return h.removed;});}
}
