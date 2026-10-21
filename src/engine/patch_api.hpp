#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <string>
#include <initializer_list>
#include "tefkernel/patchlib/type.h"
#include "tefkernel/patchlib/field.h"
#include "tefkernel/patchlib/property.h"
#include "tefkernel/patchlib/method.h"
#include "tefkernel/patchlib/struct/array.h"
#include "tefkernel/patchlib/struct/string.h"
namespace wg::engine {
using H=patch_handle_t;
inline H type(const char* ns,const char* n){return patchlib_type_get_type(ns,n);}
inline H field(H t,const char* n){return t?patchlib_type_get_field(t,n):nullptr;}
inline H property(H t,const char* n){auto p=t?patchlib_type_get_property(t,n):nullptr;return p?patchlib_property_get_get_method(p):nullptr;}
inline H method(H t,const char* n,int count){return t?patchlib_type_get_method_by_param_count(t,n,count):nullptr;}
inline H named(H t,const char* n,std::initializer_list<const char*> names){return t?patchlib_type_get_method_by_param_names(t,n,int(names.size()),const_cast<const char**>(names.begin())):nullptr;}
inline H typed(H t,const char* n,std::initializer_list<H> types){return t?patchlib_type_get_method_by_param_types(t,n,int(types.size()),types.begin()):nullptr;}
// The supplied Android PatchLib reports pointer width for value-type fields,
// including the four-byte ProjectileKey. Reserve that width and copy only T.
template<class T>T read(H f,H o=nullptr){
 static_assert(std::is_trivially_copyable_v<T>);
 alignas(std::max_align_t) unsigned char storage[sizeof(T)>sizeof(uint64_t)?sizeof(T):sizeof(uint64_t)]{};
 T v{};if(f){patchlib_field_get_value(f,o,storage);std::memcpy(&v,storage,sizeof(T));}return v;
}
template<class T>void write(H f,H o,T value){if(f)patchlib_field_set_value(f,o,&value);}
// PatchLib Android forwards this buffer directly to libffi. Small integer
// results may occupy a machine word; never pass a one-byte bool or four-byte int.
template<class T>T invoke(H m,H o=nullptr,void** args=nullptr){
 static_assert(std::is_trivially_copyable_v<T>);
 alignas(std::max_align_t) unsigned char storage[sizeof(T)>sizeof(uint64_t)?sizeof(T):sizeof(uint64_t)]{};
 T v{};if(m&&patchlib_method_invoke_args(m,o,storage,args))std::memcpy(&v,storage,sizeof(T));return v;
}
template<class T>T at(H arr,size_t i){T v{};if(arr&&i<patchlib_array_length(arr))patchlib_array_at(arr,i,&v);return v;}
struct Accessor {H f=nullptr,get=nullptr,owner=nullptr;template<class T>T value()const{H o=owner?read<H>(owner):nullptr;return f?read<T>(f,o):invoke<T>(get);}bool valid()const{return f||get;}};
inline Accessor accessor(H t,const char* name){Accessor a{};a.f=field(t,name);if(a.f&&patchlib_field_is_instance(a.f))a.owner=field(t,"instance");if(!a.f)a.get=property(t,name);return a;}
inline std::string string(H s){if(!s)return {};auto p=patchlib_string_cstr(s);if(!p)return {};std::string result(p);std::free(p);return result;}
const char* missing_api();
bool available();
}
