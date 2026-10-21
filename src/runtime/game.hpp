#pragma once
#include "engine/patch_api.hpp"
#include <string>
namespace wg {
bool initialize_game(const std::string& directory);
bool shutdown_game();
uint64_t monotonic_ms();
int local_player_index();
std::string item_name(int id);
}
