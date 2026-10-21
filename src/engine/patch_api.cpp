#define TEF_API_IMPL 1
#include "patch_api.hpp"
namespace wg::engine {
const char* missing_api(){
#define REQUIRE_API(name) if(!name)return #name
#if !defined(__ANDROID__)
 REQUIRE_API(patchlib_free);
#endif
 REQUIRE_API(patchlib_type_get_type);REQUIRE_API(patchlib_type_get_field);REQUIRE_API(patchlib_field_get_value);REQUIRE_API(patchlib_field_set_value);
 REQUIRE_API(patchlib_type_get_method_by_param_count);REQUIRE_API(patchlib_type_get_method_by_param_types);REQUIRE_API(patchlib_type_get_method_by_param_names);
 REQUIRE_API(patchlib_method_get_param_count);REQUIRE_API(patchlib_method_is_static);REQUIRE_API(patchlib_install_prepost_hook);REQUIRE_API(patchlib_uninstall_hook);
 REQUIRE_API(patchlib_method_invoke_args);REQUIRE_API(patchlib_array_at);REQUIRE_API(patchlib_array_set);REQUIRE_API(patchlib_array_length);
 REQUIRE_API(patchlib_string_create);REQUIRE_API(patchlib_string_cstr);REQUIRE_API(patchlib_type_get_property);REQUIRE_API(patchlib_property_get_get_method);
 REQUIRE_API(patchlib_field_is_instance);REQUIRE_API(patchlib_type_get_inner_type);REQUIRE_API(patchlib_get_basic_type);
#undef REQUIRE_API
 return nullptr;
}
bool available(){return missing_api()==nullptr;}
}
