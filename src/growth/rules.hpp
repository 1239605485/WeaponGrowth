#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace wg {
enum class Category : uint8_t { Unknown, Melee, Ranged, Magic, Summon };
struct Weapon {
 int id=0,damage=0,use_time=0,use_ammo=0,shoot=0;
 uint8_t prefix=0,mask=0;
 Category category=Category::Unknown;
 bool channel=false,no_melee=true,sentry=false,whip=false,disabled=false;
};
struct Attributes {
 double attack_speed=0,size=0,projectile_speed=0,knockback=0,use_time=0,ammo_save=0,cast_speed=0,mana_cost=0,whip_range=0;
 int crit=0;
};
struct Level { int stored=0,effective=0,current=0,need=100,cap=20; };
struct GrowthInput {
 Weapon weapon{}; int stage=0,origin=-1,xp=0;
 bool kill_growth=false,world_growth=true,suppress=true;
 double cap=1.6,q=1;
};
struct GrowthResult { double multiplier=1,period=1,skill=1; Level level{}; Attributes attrs{}; };
int damage(double original,double multiplier);
double combine(double period,double skill,double cap);
Level level(double xp,int stage=10);
int stage_cap(int stage);
int allowed(int xp,int amount,int stage);
int experience(double life,bool boss,int stage);
Attributes attributes(double level,const Weapon& weapon);
GrowthResult evaluate_growth(const GrowthInput& input);
}
