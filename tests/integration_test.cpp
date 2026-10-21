#include "test.hpp"
#include "runtime/game.hpp"
#include "metadata_host.hpp"
#include "game_scene.hpp"
#include "runtime/network.hpp"
#include <thread>
#include <chrono>
#include <filesystem>
int main(){
 using namespace metadata_host;
 bind();
 auto main=get_type("Terraria.Main"),projectile=get_type("Terraria.Projectile");
 CHECK(!member(main,"myPlayer","fields"));CHECK(member(main,"myPlayer","properties"));
 CHECK(!member(projectile,"identity","fields"));CHECK(member(projectile,"key","fields"));
 auto path=std::filesystem::temp_directory_path()/("wg-native-strict-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 game_scene::bind((path/"player.plr").string());
 bool ready=wg::initialize_game(path.string());
 for(auto& name:missing)std::printf("metadata lookup: %s\n",name.c_str());
 CHECK(ready);CHECK(hooked("GetWeaponDamage"));CHECK(hooked("MouseText_DrawItemTooltip_GetLinesInfo"));CHECK(hooked("CreateOutgoingMessage"));
 game_scene::tick();std::this_thread::sleep_for(std::chrono::milliseconds(270));game_scene::tick();
 CHECK(wg::local_player_index()==0);
 for(int i=0;i<4;++i){std::this_thread::sleep_for(std::chrono::milliseconds(20));game_scene::tick();}
 int result=5;auto item=game_scene::weapon;void* args[]={&item};patch_method_signature_t signature{};signature.return_type=PATCH_INT32;
 game_scene::hook("GetWeaponDamage").post(game_scene::player,args,&result,&signature);CHECK(result>5);
 game_scene::command("/查看状态");CHECK(game_scene::said("角色账本=就绪"));CHECK(game_scene::said("世界阶段=10"));
 game_scene::command("/指定近战单抽");CHECK(game_scene::said("抽取成功"));CHECK(game_scene::clones==1);
 CHECK((*static_cast<std::vector<H>*>(game_scene::inventory))[0]!=game_scene::weapon);
 game_scene::command("/查看状态");CHECK(game_scene::said("抽奖券=9"));
 game_scene::command("/抽奖记录");CHECK(game_scene::said("(#3507)"));
 std::printf("game scene: damage 5 -> %d; Chinese command -> one clone, 10 -> 9 tickets, history confirmed\n",result);
 auto online_weapon=game_scene::object();*static_cast<game_scene::Object*>(online_weapon)=*static_cast<game_scene::Object*>(game_scene::weapon);
 game_scene::set(online_weapon,"type",3509);
 auto recipes=*static_cast<std::vector<H>*>(wg::engine::read<H>(member(main,"recipe","fields")));game_scene::set(recipes[0],"createItem",online_weapon);
 auto queued_text=game_scene::text("/查看状态"),queued_message=game_scene::object();
 static_cast<game_scene::Object*>(queued_message)->text=queued_text;void* queued_args[]={&queued_text};
 game_scene::hook("CreateOutgoingMessage").post(nullptr,queued_args,&queued_message,nullptr);
 auto reply_count=game_scene::replies.size();
 game_scene::global("Terraria.Main.netMode",1);game_scene::tick();
 CHECK(game_scene::replies.size()==reply_count);CHECK(game_scene::clones==1);CHECK(game_scene::syncs==0);
 std::this_thread::sleep_for(std::chrono::milliseconds(270));
 for(int i=0;i<10;++i){game_scene::tick();std::this_thread::sleep_for(std::chrono::milliseconds(25));}
 game_scene::command("/指定近战单抽");CHECK(game_scene::clones==2);CHECK(game_scene::syncs==1);
 CHECK(wg::playable_network(0,false));CHECK(!wg::playable_network(1,false));CHECK(wg::playable_network(1,true));CHECK(wg::playable_network(3,true));CHECK(!wg::playable_network(2,true));
 std::puts("network scene: local client draw synced once; dedicated server excluded");
 // Summons skip GetWeaponDamage; their bound projectile must receive growth.
 game_scene::set(game_scene::weapon,"melee",false);game_scene::set(game_scene::weapon,"summon",true);game_scene::tick();
 auto projectile_object=game_scene::object();
 game_scene::set(projectile_object,"active",true);game_scene::set(projectile_object,"friendly",true);
 game_scene::set(projectile_object,"type",1);game_scene::set(projectile_object,"key",uint32_t(123));game_scene::set(projectile_object,"damage",20);
 game_scene::global("Terraria.Main.projectile",game_scene::array({projectile_object}));
 void* shoot_args[]={nullptr,&item,nullptr,nullptr};
 game_scene::hook("ItemCheck_Shoot").pre(game_scene::player,shoot_args,nullptr,nullptr);
 game_scene::hook("NewProjectile").pre(nullptr,nullptr,nullptr,nullptr);
 int slot=0;game_scene::hook("NewProjectile").post(nullptr,nullptr,&slot,&signature);
 game_scene::hook("ItemCheck_Shoot").post(game_scene::player,nullptr,nullptr,nullptr);
 game_scene::hook("Damage").pre(projectile_object,nullptr,nullptr,nullptr);
 auto damage_field=member(projectile,"damage","fields");CHECK(wg::engine::read<int>(damage_field,projectile_object)>20);
 game_scene::hook("Damage").post(projectile_object,nullptr,nullptr,nullptr);CHECK(wg::engine::read<int>(damage_field,projectile_object)==20);
 auto child=game_scene::object();game_scene::set(child,"active",true);game_scene::set(child,"friendly",true);
 game_scene::set(child,"whoAmI",1);game_scene::set(child,"key",uint32_t(999));game_scene::set(child,"type",2);game_scene::set(child,"damage",20);
 game_scene::global("Terraria.Main.projectile",game_scene::array({projectile_object,child}));
 game_scene::hook("AI").pre(projectile_object,nullptr,nullptr,nullptr);
 game_scene::hook("NewProjectile").pre(nullptr,nullptr,nullptr,nullptr);slot=1;
 game_scene::hook("NewProjectile").post(nullptr,nullptr,&slot,&signature);
 game_scene::hook("AI").post(projectile_object,nullptr,nullptr,nullptr);
 game_scene::hook("Damage").pre(child,nullptr,nullptr,nullptr);CHECK(wg::engine::read<int>(damage_field,child)>20);
 game_scene::hook("Damage").post(child,nullptr,nullptr,nullptr);CHECK(wg::engine::read<int>(damage_field,child)==20);
 game_scene::set(projectile_object,"key",uint32_t(124)); // Reused slot must lose the old source.
 game_scene::hook("Damage").pre(projectile_object,nullptr,nullptr,nullptr);CHECK(wg::engine::read<int>(damage_field,projectile_object)==20);
 game_scene::hook("Damage").post(projectile_object,nullptr,nullptr,nullptr);
 std::puts("game scene: summon/AI child scaled/restored; reused ProjectileKey isolated");
 CHECK(wg::shutdown_game());CHECK(cleared());
 hooks.clear();omitted="MouseText_DrawItemTooltip_GetLinesInfo";
 CHECK(wg::initialize_game(path.string()));CHECK(hooked("GetWeaponDamage"));CHECK(!hooked("MouseText_DrawItemTooltip_GetLinesInfo"));
 CHECK(wg::shutdown_game());CHECK(cleared());
 hooks.clear();omitted="GetWeaponDamage";
 CHECK(!wg::initialize_game(path.string()));CHECK(cleared());CHECK(wg::shutdown_game());
 std::puts("strict 1.4.5.8 metadata/properties/overloads/rollback: passed");
}
