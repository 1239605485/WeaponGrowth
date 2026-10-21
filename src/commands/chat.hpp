#pragma once
#include "parser.hpp"
#include <string>
namespace wg::commands {
using Handler=void(*)(const Command&);
bool install(Handler handler);
bool shutdown();
void reply(const std::string& message);
}
