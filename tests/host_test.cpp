#include "test.hpp"
#include "hooks/registry.hpp"
static int installed=0,removed=0;static bool fail=false;
static int argc_(void*){return 1;}static bool static_(void*){return false;}
static short install_(void*,prefix_callback_t,postfix_callback_t){return installed++;}
static bool remove_(short){++removed;return !fail;}
int main(){
 patchlib_method_get_param_count=argc_;patchlib_method_is_static=static_;patchlib_install_prepost_hook=install_;patchlib_uninstall_hook=remove_;
 wg::HookRegistry r;CHECK(!r.add(nullptr,1,false,nullptr,nullptr));CHECK(!r.add((void*)1,2,false,nullptr,nullptr));CHECK(installed==0);
 CHECK(r.add((void*)1,1,false,nullptr,nullptr));CHECK(r.count()==1);fail=true;CHECK(!r.clear());CHECK(r.count()==1);fail=false;CHECK(r.clear());CHECK(r.count()==0);CHECK(removed==2);
 std::puts("hook registry: passed");
}
