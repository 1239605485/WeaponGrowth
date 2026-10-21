#include "rules.hpp"
#include <limits>
namespace wg {
namespace { bool positive(double x) { return std::isfinite(x)&&x>0; }
constexpr int caps[]={5,7,9,10,12,14,15,17,18,19,20};
constexpr double power[]={1,1.45,2.1,3.2,7,11,16,24,34,50,80};
constexpr double bonus[]={0,.04,.08,.12,.22,.27,.32,.37,.42,.46,.50};
bool stage_ok(int s) { return s>=0&&s<=10; }
}
int damage(double v,double m) {
 double n=std::floor(v*m+.500000001);
 if(std::isnan(n)) return 1;
 return static_cast<int>(std::clamp(n,1.,2147483647.));
}
double combine(double p,double s,double cap) {
 if(!positive(p)) p=1; if(!positive(s)) s=1;
 double r=p<1?p*std::min(1.2,s):1+std::max(0.,p-1)+std::max(0.,s-1);
 r=std::min(1.6,r); return positive(cap)?std::min(cap,r):r;
}
int stage_cap(int stage) { return caps[stage_ok(stage)?stage:0]; }
Level level(double xp,int stage) {
 Level r{}; r.cap=stage_cap(stage);
 if(!std::isfinite(xp)||xp<0) xp=0;
 while(r.stored<20&&xp>=100+50*r.stored) {xp-=100+50*r.stored; ++r.stored;}
 r.effective=std::min(r.stored,r.cap);
 r.current=r.stored>=r.cap?0:static_cast<int>(xp);
 r.need=r.stored>=r.cap?0:100+50*r.stored;
 return r;
}
int allowed(int xp,int amount,int stage) {
 int c=stage_cap(stage); return std::max(0,std::min(amount,100*c+25*c*(c-1)-xp));
}
int experience(double life,bool boss,int stage) {
 if(!positive(life)) return 0;
 double e=std::floor(std::sqrt(life)+.5),base=std::max(12.,std::floor(e/3+.5)*3);
 if(boss) return static_cast<int>(std::min(3000.,std::floor(base*(2+.25*std::clamp(stage,0,10))/3+.5)*3));
 return static_cast<int>(std::min(600.,base));
}
Attributes attributes(double n,const Weapon& w) {
 Attributes a{}; if(w.disabled) return a;
 double x=std::isfinite(n)?std::clamp(std::floor(n),0.,20.)/20:0;
 int crit=static_cast<int>(std::floor(3*x));
 switch(w.category) {
 case Category::Melee:
  if(w.channel){a.crit=crit;a.knockback=.8*x;}
  else if(!w.no_melee){a.attack_speed=.03*x;a.size=.04*x;}
  else {a.crit=crit;if(w.shoot>0)a.projectile_speed=.06*x;}
  break;
 case Category::Ranged:
  if(w.use_time>=25){a.crit=crit;a.knockback=.8*x;}
  else {a.use_time=.025*x;if(w.use_ammo>0)a.ammo_save=.1*x;}
  break;
 case Category::Magic:
  if(w.channel){a.crit=crit;a.mana_cost=.12*x;}
  else {a.cast_speed=.025*x;a.mana_cost=.1*x;}
  break;
 case Category::Summon:
  if(w.whip){a.attack_speed=.03*x;a.whip_range=.05*x;}
  else a.knockback=(w.sentry?1.5:1.2)*x;
  break;
 default: break;
 }
 return a;
}
GrowthResult evaluate_growth(const GrowthInput& in) {
 GrowthResult r{}; r.level=level(in.xp,in.stage);
 if(in.kill_growth&&!in.weapon.disabled&&in.weapon.category!=Category::Unknown) {
  r.skill=1+.01*r.level.effective; r.attrs=attributes(r.level.effective,in.weapon);
 }
 if(stage_ok(in.stage)&&stage_ok(in.origin)) {
  if(in.origin>in.stage&&in.suppress) {
   double base=power[in.stage]/power[in.origin]; double q=positive(in.q)&&in.q<=1?in.q:1;
   r.period=base*std::pow(q,1-base);
  } else if(in.origin<in.stage&&in.world_growth&&!in.weapon.disabled)
   r.period=1+bonus[in.stage]-bonus[in.origin];
 }
 r.multiplier=combine(r.period,r.skill,in.cap); return r;
}
}
