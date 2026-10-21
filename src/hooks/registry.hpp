#pragma once
#include "engine/patch_api.hpp"
#include <array>
namespace wg {
class HookRegistry {
 std::array<patch_hook_id_t,32> hooks{};size_t used=0;
public:
 bool add(engine::H m,int argc,bool is_static,prefix_callback_t pre,postfix_callback_t post){
  if(!m||used==hooks.size()||patchlib_method_get_param_count(m)!=argc||patchlib_method_is_static(m)!=is_static)return false;
  auto id=patchlib_install_prepost_hook(m,pre,post);if(id==PATCH_HOOK_INVALID_ID)return false;hooks[used++]=id;return true;
 }
 bool clear(){bool ok=true;for(size_t i=used;i>0;--i){auto& h=hooks[i-1];if(h!=PATCH_HOOK_INVALID_ID){if(patchlib_uninstall_hook(h))h=PATCH_HOOK_INVALID_ID;else ok=false;}}if(ok)used=0;return ok;}
 size_t count()const{return used;}
};
}
