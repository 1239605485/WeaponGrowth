#include "ledger.hpp"
#include "../../vendor/json.hpp"
#include "catalog/generated.hpp"
#include <algorithm>
#include <sstream>
#include <limits>
namespace wg {
using Json=nlohmann::json;
uint32_t checksum(const std::string& s){uint64_t n=0;for(unsigned char c:s)n=(n*131+c)%2147483647;return uint32_t(n);}
std::string hex(const std::string& s){const char* h="0123456789abcdef";std::string r;r.reserve(s.size()*2);for(unsigned char c:s){r+=h[c>>4];r+=h[c&15];}return r;}
namespace {
bool bounded(int n,int hi){return n>=0&&n<=hi;}
std::string encode(const Account& a){
 Json j={{"tickets",a.tickets},{"pity",a.pity},{"day",a.day},{"daily",a.daily},{"seconds",a.seconds},{"newgift",a.newgift},{"frozen",a.frozen},{"generation",a.generation},{"request",a.request},{"xp",a.xp},{"owned",a.owned},{"bosses",a.bosses},{"pending",a.pending}};
 j["identity"]=a.identity;j["legacy_weapons"]=a.legacy_weapons;
 j["history"]=Json::array();for(const auto& r:a.history)j["history"].push_back({{"time",r.time},{"ids",r.ids}});
 auto body=j.dump();return "WGNATIVE|1|"+std::to_string(checksum(body))+"\n"+body;
}
bool decode(const std::string& text,Account& a){
 if(text.size()>1048576)return false;
 try{
  auto nl=text.find('\n');if(nl==std::string::npos||text.rfind("WGNATIVE|1|",0)!=0)return false;
  auto body=text.substr(nl+1);if(text.substr(11,nl-11)!=std::to_string(checksum(body)))return false;
  auto j=Json::parse(body);a={};a.tickets=j.at("tickets").get<int>();a.pity=j.at("pity").get<int>();a.day=j.at("day").get<int>();a.daily=j.at("daily").get<int>();a.seconds=j.at("seconds").get<int>();
  a.newgift=j.at("newgift").get<bool>();a.frozen=j.at("frozen").get<bool>();a.generation=j.at("generation").get<uint64_t>();a.request=j.at("request").get<uint64_t>();
  a.xp=j.at("xp").get<std::array<int,4>>();a.owned=j.at("owned").get<std::set<int>>();a.bosses=j.at("bosses").get<std::set<int>>();a.pending=j.at("pending").get<std::vector<int>>();
  a.identity=j.value("identity",std::string{});if(j.contains("legacy_weapons"))a.legacy_weapons=j.at("legacy_weapons").get<std::map<int,int>>();
  if(a.legacy_weapons.size()>4096)return false;for(auto [id,xp]:a.legacy_weapons)if(id<1||xp<0||xp>11500)return false;
  if(!bounded(a.tickets,10000)||!bounded(a.pity,100000)||!bounded(a.day,99999999)||!bounded(a.daily,20)||!bounded(a.seconds,119)||a.generation>1000000000||a.pending.size()>10)return false;
  for(int x:a.xp)if(!bounded(x,11500))return false;
  for(int x:a.owned)if(x<1||x>20000)return false;
  for(int x:a.bosses)if(x<1||x>20000)return false;
  for(int x:a.pending)if(x<1||x>20000)return false;
  if(j.at("history").size()>20)return false;
  for(auto& r:j.at("history")){DrawRecord d{r.at("time").get<uint64_t>(),r.at("ids").get<std::vector<int>>()};if(d.time>9999999999||d.ids.empty()||d.ids.size()>10)return false;for(int x:d.ids)if(x<1||x>20000)return false;a.history.push_back(std::move(d));}
  if(!a.pending.empty())a.frozen=true;
  return true;
 }catch(...){return false;}
}
bool old_body(const std::string& text,const std::string& magic,uint64_t& gen,std::string& body){
 if(text.size()>1048576)return false;auto nl=text.find('\n');if(nl==std::string::npos||text.rfind(magic,0)!=0)return false;
 auto h=text.substr(magic.size(),nl-magic.size());auto sep=h.find('|');if(sep==std::string::npos)return false;
 try{size_t used=0;gen=std::stoull(h.substr(0,sep),&used);if(used!=sep||gen>1000000000)return false;body=text.substr(nl+1);return h.substr(sep+1)==std::to_string(checksum(std::to_string(gen)+"\n"+body));}catch(...){return false;}
}
}
bool Ledger::open(const std::string& basename,const std::string& identity){
 key=basename;data={};slot=-1;blocked=true;dirty=false;last_save=0;rev=0;
 if(key.empty()||key.size()>8192)return false;
 bool found=false;for(int i=0;i<2;++i){auto t=io.read(key+(i?"_b.dat":"_a.dat"));if(!t)continue;found=true;Account a;if(decode(*t,a)&&(slot<0||a.generation>data.generation)){data=std::move(a);slot=i;}}
 blocked=(found&&slot<0)||(!data.identity.empty()&&data.identity!=identity);if(slot<0)data.identity=identity;return !blocked;
}
bool Ledger::save(){
 if(blocked)return false;if(!dirty)return true;if(data.generation>=1000000000)return false;
 auto next=data;next.generation++;auto text=encode(next);int target=slot==0?1:0;auto name=key+(target?"_b.dat":"_a.dat");
 if(!io.write(name,text)||io.read(name)!=std::optional<std::string>(text))return false;
 data.generation=next.generation;slot=target;dirty=false;return true;
}
bool Ledger::checkpoint(uint64_t now){if(now<last_save||now-last_save>=30000){if(!save())return false;last_save=now;}return true;}
bool Ledger::gift(){if(blocked||data.frozen)return false;if(data.newgift)return true;auto old=data;bool was_dirty=dirty;data.newgift=true;data.tickets=std::min(10000,data.tickets+10);dirty=true;if(!save()){data=old;dirty=was_dirty;return false;}++rev;return true;}
bool Ledger::boss(int id){if(blocked||data.frozen||id<1||id>20000)return false;if(data.bosses.count(id))return true;auto old=data;bool was_dirty=dirty;data.bosses.insert(id);data.tickets=std::min(10000,data.tickets+10);dirty=true;if(!save()){data=old;dirty=was_dirty;return false;}++rev;return true;}
void Ledger::online(int day,int elapsed){
 if(blocked||data.frozen)return;
 if(day>data.day){data.day=day;data.daily=0;data.seconds=0;dirty=true;++rev;return;}
 if(day<data.day||data.daily>=20||elapsed<=0||elapsed>5)return;
 data.seconds+=elapsed;dirty=true;++rev;
 if(data.seconds>=120){data.seconds-=120;++data.daily;data.tickets=std::min(10000,data.tickets+1);save();}
}
bool Ledger::add_owned(int id){if(blocked||id<1||id>20000)return false;if(!data.owned.insert(id).second)return false;dirty=true;++rev;return true;}
void Ledger::award(int cls,int amount){if(blocked||cls<0||cls>=4||amount<=0)return;data.xp[cls]=std::min(11500,data.xp[cls]+amount);dirty=true;++rev;}
void Ledger::migrate_weapon(int id,int cls){if(blocked||cls<0||cls>=4)return;auto it=data.legacy_weapons.find(id);if(it==data.legacy_weapons.end())return;data.xp[cls]=std::max(data.xp[cls],it->second);data.legacy_weapons.erase(it);dirty=true;++rev;}
bool Ledger::prepare(const std::vector<int>& ids,uint64_t request){
 if(blocked||data.frozen||!data.pending.empty()||ids.empty()||ids.size()>10||data.tickets<int(ids.size()))return false;
 for(int id:ids)if(id<1||id>20000||data.owned.count(id))return false;
 auto old=data;bool was_dirty=dirty;data.pending=ids;data.request=request;dirty=true;
 if(!save()){data=old;dirty=was_dirty;return false;}return true;
}
bool Ledger::finish(const std::vector<int>& granted,uint64_t time){
 if(blocked||data.pending.empty()||granted.size()>data.pending.size())return false;
 for(size_t i=0;i<granted.size();++i)if(granted[i]!=data.pending[i])return false;
 data.tickets-=int(granted.size());data.pity+=int(granted.size());for(int id:granted)data.owned.insert(id);
 if(!granted.empty()){data.history.push_back({time,granted});if(data.history.size()>20)data.history.erase(data.history.begin());}
 data.pending.clear();dirty=true;++rev;
 if(!save()){data.frozen=true;dirty=true;return false;}return true;
}
bool Ledger::import_legacy(const std::string& profile,const std::string& prefix){
 if(blocked||slot>=0)return !blocked;
 Account imported=data;bool any=false;
 uint64_t best=0;bool seen=false,valid=false;
 for(const char* suffix:{"a","b"}){
  auto t=io.read(prefix+"weapon_gacha_"+suffix+"_"+hex(profile)+".dat");if(!t)continue;seen=true;uint64_t gen=0;std::string body;if(!old_body(*t,"WGACHA|1|",gen,body))continue;
  try{Account candidate{};std::istringstream in(body);std::string line;while(std::getline(in,line)){auto p=line.find('|');if(p==std::string::npos)throw 1;auto k=line.substr(0,p),v=line.substr(p+1);if(k=="tickets")candidate.tickets=std::stoi(v);else if(k=="newgift")candidate.newgift=v=="1";else if(k=="weapon")candidate.owned.insert(std::stoi(v));else if(k=="boss")candidate.bosses.insert(std::stoi(v));else if(k=="pity")candidate.pity=std::stoi(v);else if(k=="online"){char c;std::istringstream ss(v);ss>>candidate.day>>c>>candidate.daily>>c>>candidate.seconds;if(!ss)throw 1;}else if(k=="draw"){auto c=v.find(':');DrawRecord r{std::stoull(v.substr(0,c)),{}};std::istringstream ids(v.substr(c+1));std::string id;while(std::getline(ids,id,','))r.ids.push_back(std::stoi(id));candidate.history.push_back(std::move(r));}}
   Account checked;candidate.generation=gen;if(!decode(encode(candidate),checked))continue;
   if(!valid||gen>best){imported=std::move(checked);best=gen;valid=true;any=true;}
  }catch(...){continue;}
 }
 if(seen&&!valid){blocked=true;return false;}
 // v1/v2/v3 progress: latest checksum-valid slot; v2 migrated totals are reset before per-weapon max migration.
 best=0;seen=false;valid=false;
 for(const char* suffix:{"a","b"}){
  auto t=io.read(prefix+"weapon_progress_"+suffix+".dat");if(!t)continue;seen=true;
  for(int version=1;version<=3;++version){uint64_t gen=0;std::string body;if(!old_body(*t,"WGPROGRESS|"+std::to_string(version)+"|",gen,body)|| (valid&&gen<=best))continue;
   bool ok=true;std::map<int,int> weapons;std::array<int,4> xp{};std::istringstream in(body);std::string line;
   while(std::getline(in,line)){std::vector<std::string> parts;std::istringstream row(line);std::string p;while(std::getline(row,p,'|'))parts.push_back(p);if(!line.empty()&&line.back()=='|')parts.emplace_back();if(parts.size()<5){ok=false;break;}if(parts[0]!=hex(profile))continue;try{for(int i=0;i<4;++i){xp[i]=std::stoi(parts[i+1]);if(!bounded(xp[i],11500))ok=false;}if(version<3&&parts.size()>6){int mask=std::stoi(parts[5]);if(mask<0||mask>15)throw 1;for(int i=0;i<4;++i)if(mask&(1<<i))xp[i]=0;}
    if(version>=2&&parts.size()>5){std::istringstream entries(parts.size()>6?parts[6]:(parts[5].find(':')!=std::string::npos?parts[5]:std::string{}));std::string entry;while(std::getline(entries,entry,',')){auto colon=entry.find(':');if(colon==std::string::npos)throw 1;int id=std::stoi(entry.substr(0,colon)),value=std::stoi(entry.substr(colon+1));if(id<1||value<0||value>11500||weapons.count(id)||weapons.size()>=4096)throw 1;weapons[id]=value;}}
    for(auto it=weapons.begin();it!=weapons.end();){int cls=0;for(auto entry:generated::legacy_classes)if(entry.id==it->first){cls=entry.value;break;}for(const auto& sample:generated::samples)if(!cls&&sample.id==it->first){cls=sample.category;break;}if(cls>=1&&cls<=4){xp[cls-1]=std::max(xp[cls-1],it->second);it=weapons.erase(it);}else ++it;}
   }catch(...){ok=false;}}
   if(ok){best=gen;valid=true;imported.xp=xp;imported.legacy_weapons=std::move(weapons);any=true;}
  }
 }
 if(seen&&!valid){blocked=true;return false;}
 auto gift=io.read(prefix+"weapon_gacha_gift_"+hex(profile)+".txt");if(gift){auto first=gift->find_first_not_of(" \r\n\t"),last=gift->find_last_not_of(" \r\n\t");if(first!=std::string::npos)*gift=gift->substr(first,last-first+1);if(*gift=="1\n"||*gift=="1")imported.newgift=true;else if(*gift!="0\n"&&*gift!="0"){blocked=true;return false;}}
 if(any||gift){imported.identity=data.identity;data=std::move(imported);data.generation=0;dirty=true;return save();}return true;
}
}
