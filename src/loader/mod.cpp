#include "kernel/mod_core.h"
#include "kernel/mod_logger.h"
#include "runtime/game.hpp"
#include "runtime/diagnostics.hpp"
#include <exception>
namespace {
kernel_mod_info_t info{"liuxin.weapon_growth.kernel",20100,1,"2.1.0"};
void init(kernel_mod_handle_t* h){
 try{if(!h||!h->private_dir||!wg::initialize_game(h->private_dir)){if(mod_logger_write)mod_logger_write(MOD_LOG_LEVEL_ERROR,"WeaponGrowth","Native initialization failed; hooks disabled. Check game/Kernel API compatibility.");return;}if(mod_logger_write)mod_logger_write(MOD_LOG_LEVEL_INFO,"WeaponGrowth","Native growth and chat commands initialized. Enter /查看状态 in single-player chat.");}
 catch(const std::exception& e){wg::diagnostics::record(std::string("init.exception: ")+e.what(),true);wg::shutdown_game();}
 catch(...){wg::diagnostics::record("init.exception: unknown native exception",true);wg::shutdown_game();}
}
void cleanup(kernel_mod_handle_t*){try{if(!wg::shutdown_game()&&mod_logger_write)mod_logger_write(MOD_LOG_LEVEL_CRITICAL,"WeaponGrowth","Hook removal failed; do not unload/reload this library. Restart game.");}catch(...){if(mod_logger_write)mod_logger_write(MOD_LOG_LEVEL_CRITICAL,"WeaponGrowth","Cleanup exception; restart game before unloading.");}}
kernel_mod_info_t* get_info(){return &info;}
kernel_mod_ops_t ops{init,cleanup,get_info};
}
#if defined(_WIN32)
#define WG_EXPORT __declspec(dllexport)
#else
#define WG_EXPORT __attribute__((visibility("default")))
#endif
extern "C" WG_EXPORT kernel_mod_ops_t* create_kernel_mod(){return &ops;}
