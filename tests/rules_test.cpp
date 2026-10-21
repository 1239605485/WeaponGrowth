#include "test.hpp"
#include "growth/rules.hpp"
#include <limits>
int main() {
 using namespace wg;
 CHECK(damage(100,.5)==50); CHECK(damage(1,.001)==1);
 CHECK(damage(2147483647.,100)==2147483647);
 NEAR(combine(.5,1.5,1.6),.6); NEAR(combine(1.5,1.2,1.6),1.6);
 NEAR(combine(std::numeric_limits<double>::quiet_NaN(),1,1.6),1);
 CHECK(level(100).stored==1); CHECK(level(249).stored==1); CHECK(level(250).stored==2);
 CHECK(level(11500).stored==20); CHECK(level(11500,0).effective==5);
 CHECK(level(11500,0).need==0); CHECK(allowed(999,100,0)==1);
 CHECK(experience(100,false,0)==12); CHECK(experience(90000,false,0)==300);
 CHECK(experience(90000,true,10)==1350); CHECK(experience(-1,false,0)==0);
 Weapon w{}; w.category=Category::Melee; w.no_melee=false;
 NEAR(attributes(20,w).attack_speed,.03); NEAR(attributes(20,w).size,.04);
 w.no_melee=true; w.shoot=1; CHECK(attributes(20,w).crit==3);
 w.category=Category::Ranged; w.use_time=20; w.use_ammo=1;
 NEAR(attributes(20,w).ammo_save,.1); w.disabled=true; NEAR(attributes(20,w).ammo_save,0);
 GrowthInput in{}; in.weapon.category=Category::Melee; in.origin=0; in.stage=10;
 NEAR(evaluate_growth(in).multiplier,1.5);
 in.kill_growth=true; in.xp=11500; NEAR(evaluate_growth(in).multiplier,1.6);
 in.stage=0; in.origin=10; NEAR(evaluate_growth(in).multiplier,.0125*1.05);
 in.origin=-1; NEAR(evaluate_growth(in).multiplier,1.05);
 std::puts("rules: passed");
}
