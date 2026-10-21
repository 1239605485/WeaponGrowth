#pragma once
#include "kernel/mod_logger.h"
#include <fstream>
#include <string>
namespace wg::diagnostics {
inline std::string directory;
inline void configure(const std::string& path){directory=path;}
inline void record(const std::string& message,bool error=false){
 if(mod_logger_write)mod_logger_write(error?MOD_LOG_LEVEL_ERROR:MOD_LOG_LEVEL_INFO,"WeaponGrowth","%s",message.c_str());
 // Only lifecycle and failure events use this file; never called from AI/damage hot paths.
 if(!directory.empty())try{std::ofstream out(directory+"/native-runtime.log",std::ios::app);out<<message<<'\n';}catch(...){}
}
}
