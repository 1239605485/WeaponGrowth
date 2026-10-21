#include "game.hpp"
#include "scheduler.hpp"
#include "growth/rules.hpp"
#include "catalog/recipe_index.hpp"
#include "catalog/generated.hpp"
#include "combat/sources.hpp"
#include "gacha/service.hpp"
#include "hooks/registry.hpp"
#include "commands/chat.hpp"
#include "diagnostics.hpp"
#include "network.hpp"
#include "../../vendor/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <ctime>
#include <unordered_map>
#include <cstdio>
#include <cstring>
#include <deque>
namespace wg {
using namespace engine;
namespace {
struct Fields {
 H critMethod=nullptr,knockMethod=nullptr,manaMethod=nullptr,scaleMethod=nullptr;
 H main=nullptr,player=nullptr,item=nullptr,proj=nullptr,npc=nullptr;
 H players=nullptr,projectiles=nullptr,net=nullptr,hard=nullptr;
 Accessor menu,world,file,local;
 H path=nullptr,cloud=nullptr,held=nullptr,name=nullptr,clone=nullptr,syncSlot=nullptr;
 H id=nullptr,damage=nullptr,ammo=nullptr,pick=nullptr,axe=nullptr,hammer=nullptr,melee=nullptr,ranged=nullptr,magic=nullptr,summon=nullptr;
 H prefix=nullptr,useTime=nullptr,useAmmo=nullptr,shoot=nullptr,channel=nullptr,noMelee=nullptr,consumable=nullptr,sentry=nullptr,shootSpeed=nullptr;
 H playerIndex=nullptr,animation=nullptr,ammo75=nullptr,ammo80=nullptr;
 H pslot=nullptr,pidentity=nullptr,powner=nullptr,ptype=nullptr,pactive=nullptr,pfriendly=nullptr,ptrap=nullptr,pnpc=nullptr,pdamage=nullptr;
 H nslot=nullptr,ntype=nullptr,life=nullptr,lifeMax=nullptr,boss=nullptr,friendly=nullptr,town=nullptr,immortal=nullptr,dont=nullptr,chase=nullptr,statue=nullptr,real=nullptr,generation=nullptr;
 H recipes=nullptr,recipeCount=nullptr,output=nullptr,inputs=nullptr,tile=nullptr,groups=nullptr,special=nullptr,inventory=nullptr,chestItems=nullptr;
 std::array<H,4> banks{};std::array<H,14> down{};
 H itemBombs=nullptr,projBombs=nullptr,whips=nullptr,uniqueTags=nullptr,tagDamage=nullptr;
} f;
struct Config {bool enabled=true,gacha=true,suppress=true,multiplayer=true;std::string mode="world",legacy;};
struct Sample {int recipe=0,slot=-1;uint8_t mask=0;};
struct HookFrame {H object=nullptr;float value=0;int integer=0;bool flag=false;};
template<class T,size_t N=128>struct Frames {
 std::array<T,N> v{};size_t size=0,overflow=0;
 T* push(){if(size==N){++overflow;return nullptr;}v[size]={};return &v[size++];}
 bool pop(T& out){if(overflow){--overflow;return false;}if(!size)return false;out=v[--size];return true;}
 void clear(){size=overflow=0;}
};
struct StrikeFrame {SourceToken source{};uint64_t generation=0,session=0;int slot=-1,max=0,bossId=0;bool boss=false,eligible=false;};
struct Tag {int id=0,base=0,applied=0;};
struct WeaponRead {H object=nullptr;Weapon w{};uint64_t frame=0;};
struct State {
 bool alive=false,in_world=false,profile_ready=false,catalog_ready=false,catalog_build_started=false,install_failed=false;
 uint64_t catalog_due=0;int panel_depth=0;
 uint64_t session=1,frame=0,last_online=0,request_processed=0,revision=1;
 int world=0,network=-1,stage=0,local=-1,recipe_cursor=0,recipe_count=0,collect_cursor=0;
 std::string directory,profile,config_text;
 Config config{};HookRegistry hooks;Scheduler scheduler;RecipeIndex index;
 Sources sources;SourceStack<> ai,shots,spawns,damage_sources,item_sources;
 Frames<HookFrame> damage_frames,shoot_frames,ammo_frames;Frames<StrikeFrame> strikes;
 std::array<uint64_t,20000> dead{};std::array<bool,20000> died{};
 std::array<WeaponRead,64> reads{};size_t read_cursor=0;
 std::vector<Recipe> recipe_rules;std::unordered_map<int,Sample> locations,building_locations;
 std::unordered_map<int,std::string> names;std::vector<Tag> tags;
 uint64_t fingerprint=1469598103934665603ull,tag_revision=0;
 std::vector<std::unique_ptr<Ledger>> unsaved;std::unique_ptr<Ledger> ledger;std::unique_ptr<GachaService> gacha;
 commands::Inbox inbox;bool announced=false;
 std::thread::id thread{};std::mt19937 rng{std::random_device{}()};
 std::array<int,128> pending_boss{};size_t bosses=0;
 uint64_t damage_calls=0,ai_calls=0,strike_calls=0,scan_calls=0,writes=0;
} s;
bool on_thread(){return s.alive&&s.thread==std::this_thread::get_id();}
bool playable(){return playable_network(read<int>(f.net),s.config.multiplayer);}
bool active(){return on_thread()&&s.in_world&&s.config.enabled&&playable()&&read<int>(f.net)==s.network;}
bool local_player(H p){return p&&read<int>(f.playerIndex,p)==s.local;}
bool growth_enabled(){return active()&&s.profile_ready&&(s.config.mode=="kill"||s.config.mode=="mixed");}
H player(){return at<H>(read<H>(f.players),size_t(std::max(0,s.local)));}
bool set_contains(H field,int id){auto arr=read<H>(field);return id>=0&&arr&&size_t(id)<patchlib_array_length(arr)&&at<bool>(arr,size_t(id));}
FileIO files(){return {
 [](const std::string& name)->std::optional<std::string>{auto path=std::filesystem::path(s.directory)/name;std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)return {};auto n=in.tellg();if(n<0||n>1048576)return std::string{};std::string text(size_t(n),'\0');in.seekg(0);if(!in.read(text.data(),n))return std::string{};return text;},
 [](const std::string& name,const std::string& text){auto path=std::filesystem::path(s.directory)/name;std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)return false;out.write(text.data(),std::streamsize(text.size()));out.flush();++s.writes;return bool(out);}
 };}
Weapon identify(H object){
 Weapon w{};if(!object)return w;
 for(const auto& cached:s.reads)if(cached.object==object&&cached.frame==s.frame)return cached.w;
 w.id=read<int>(f.id,object);w.damage=read<int>(f.damage,object);w.prefix=read<uint8_t>(f.prefix,object);
 if(w.id>0&&w.damage>0&&read<int>(f.ammo,object)==0&&read<int>(f.pick,object)==0&&read<int>(f.axe,object)==0&&read<int>(f.hammer,object)==0&&!set_contains(f.itemBombs,w.id)){
  bool flags[]={read<bool>(f.melee,object),read<bool>(f.ranged,object),read<bool>(f.magic,object),read<bool>(f.summon,object)};int count=0;
  for(int i=0;i<4;++i)if(flags[i]){++count;w.category=Category(i+1);w.mask|=uint8_t(1<<i);}
  if(count!=1)w.category=Category::Unknown;
  w.use_time=read<int>(f.useTime,object);w.use_ammo=read<int>(f.useAmmo,object);w.shoot=read<int>(f.shoot,object);
  w.channel=read<bool>(f.channel,object);w.no_melee=read<bool>(f.noMelee,object);w.sentry=read<bool>(f.sentry,object);w.whip=set_contains(f.whips,w.shoot);
  w.disabled=read<bool>(f.consumable,object)&&(w.category==Category::Melee||w.category==Category::Ranged);
 }
 s.reads[s.read_cursor++%s.reads.size()]={object,w,s.frame};return w;
}
GrowthResult growth(const Weapon& w){GrowthInput in{};in.weapon=w;in.stage=s.stage;in.origin=s.index.origin(w.id);in.suppress=s.config.suppress;in.kill_growth=growth_enabled();in.world_growth=s.config.mode!="kill";if(in.kill_growth&&w.category!=Category::Unknown)in.xp=s.ledger->state().xp[int(w.category)-1];return evaluate_growth(in);}
SourceToken capture(H p,H it){SourceToken t{};if(!active()||!local_player(p))return t;t.weapon=identify(it);if(t.weapon.category==Category::Unknown)return t;t.valid=true;t.session=s.session;t.owner=s.local;t.embedded=t.weapon.category==Category::Summon?1:growth(t.weapon).multiplier;return t;}
SourceToken lookup(H p){if(!p||!read<bool>(f.pactive,p))return {};return s.sources.lookup(read<int>(f.pslot,p),read<uint32_t>(f.pidentity,p),read<int>(f.powner,p),read<int>(f.ptype,p),s.session);}
void bind(SourceToken t,H p,int slot){s.sources.erase(slot);if(!t.valid||!p||!read<bool>(f.pactive,p)||!read<bool>(f.pfriendly,p)||read<bool>(f.ptrap,p)||read<bool>(f.pnpc,p)||read<int>(f.powner,p)!=t.owner)return;t.slot=slot;t.identity=read<uint32_t>(f.pidentity,p);t.type=read<int>(f.ptype,p);s.sources.bind(t);}
void restore_tags(){auto array=read<H>(f.uniqueTags);for(const auto& t:s.tags){auto obj=at<H>(array,size_t(t.id));if(obj&&read<int>(f.tagDamage,obj)==t.applied)write(f.tagDamage,obj,t.base);}s.tags.clear();s.tag_revision=0;}
void clear_session(){
 if(s.ledger&&s.ledger->needs_save()&&!s.ledger->save())s.unsaved.push_back(std::move(s.ledger));restore_tags();++s.session;++s.revision;s.sources.clear();s.ai.clear();s.shots.clear();s.spawns.clear();s.damage_sources.clear();s.item_sources.clear();s.damage_frames.clear();s.shoot_frames.clear();s.ammo_frames.clear();s.strikes.clear();s.dead={};s.died={};s.reads={};s.inbox.clear();s.announced=false;s.bosses=0;s.profile.clear();s.profile_ready=false;s.ledger.reset();s.gacha.reset();s.last_online=0;s.collect_cursor=0;s.catalog_due=0;s.catalog_ready=false;s.catalog_build_started=false;s.recipe_cursor=0;s.recipe_rules.clear();s.building_locations.clear();s.fingerprint=1469598103934665603ull;
}
void retry_saves(){for(auto it=s.unsaved.begin();it!=s.unsaved.end();){if((*it)->save())it=s.unsaved.erase(it);else ++it;}}
void config_task(){
 retry_saves();
 auto text=files().read("config.json");if(!text||*text==s.config_text)return;
 try{auto j=nlohmann::json::parse(*text);Config c{};c.enabled=j.value("enabled",true);c.gacha=j.value("gacha_enabled",true);c.multiplayer=j.value("multiplayer_enabled",true);c.suppress=j.value("suppress_ahead_weapons",true);c.mode=j.value("growth_mode",std::string("world"));c.legacy=j.value("legacy_directory",std::string{});if(c.mode!="world"&&c.mode!="kill"&&c.mode!="mixed")return;s.config=std::move(c);s.config_text=*text;++s.revision;if(!s.config.enabled)restore_tags();}catch(...){return;}
}
void session_task(){
 bool menu=f.menu.value<bool>();int net=read<int>(f.net),id=f.world.value<int>();
 if(menu||!playable_network(net,s.config.multiplayer)){if(s.in_world)clear_session();s.in_world=false;return;}
 int local=f.local.value<int>();if(local<0||local>=255)return;
 if(!s.in_world||id!=s.world||s.local!=local||net!=s.network){clear_session();s.world=id;s.local=local;s.network=net;s.in_world=true;}
 auto file=f.file.value<H>();auto path=string(invoke<H>(f.path,file));if(path.empty()){if(s.profile_ready)clear_session();return;}auto key=(invoke<bool>(f.cloud,file)?"cloud:":"local:")+path;
 if(key!=s.profile){clear_session();retry_saves();if(!s.unsaved.empty())return;s.profile=key;s.profile_ready=false;auto io=files();s.ledger=std::make_unique<Ledger>(io);std::string base="account_"+std::to_string(checksum(key));if(s.ledger->open(base,key)&&s.ledger->import_legacy(key,s.config.legacy.empty()?std::string{}:s.config.legacy+"/")){s.profile_ready=true;s.gacha=std::make_unique<GachaService>(*s.ledger);if(s.config.gacha)s.ledger->gift();}diagnostics::record(s.profile_ready?"profile.ready":"profile.failed: ledger open/import failed",!s.profile_ready);}
 int stage=0;for(int i=0;i<3;++i)if(read<bool>(f.down[i]))stage=i+1;if(read<bool>(f.hard))stage=4;bool a=read<bool>(f.down[3]),b=read<bool>(f.down[4]),c=read<bool>(f.down[5]);if(a||b||c)stage=5;if(a&&b&&c)stage=6;if(read<bool>(f.down[6]))stage=7;if(read<bool>(f.down[7]))stage=8;for(int i=9;i<14;++i)if(read<bool>(f.down[i]))stage=std::max(stage,9);if(read<bool>(f.down[8]))stage=10;if(s.stage!=stage){s.stage=stage;++s.revision;}
}
void hash(int value){s.fingerprint^=uint32_t(value);s.fingerprint*=1099511628211ull;}
H sample_at(const Sample& loc){auto recipe=at<H>(read<H>(f.recipes),size_t(loc.recipe));if(!recipe)return nullptr;return loc.slot<0?read<H>(f.output,recipe):at<H>(read<H>(f.inputs,recipe),size_t(loc.slot));}
void sample(H it,int recipe,int slot){if(!it)return;auto w=identify(it);if(w.mask){s.building_locations[w.id]={recipe,slot,w.mask};}}
void catalog_task(){
 if(!s.in_world)return;
 auto now=monotonic_ms();if(s.catalog_ready){if(now<s.catalog_due)return;s.catalog_ready=false;s.catalog_build_started=false;s.recipe_cursor=0;s.recipe_rules.clear();s.building_locations.clear();s.fingerprint=1469598103934665603ull;}
 if(s.catalog_build_started){if(s.index.step(64)){s.catalog_ready=true;s.catalog_due=now+30000;s.locations.swap(s.building_locations);s.building_locations.clear();s.names.clear();++s.revision;}return;}
 if(s.recipe_cursor==0&&s.recipe_rules.empty()){s.recipe_count=read<int>(f.recipeCount);if(s.recipe_count<=0||s.recipe_count>10000)return;auto arr=read<H>(f.recipes);if(!arr||patchlib_array_length(arr)<size_t(s.recipe_count))return;s.recipe_rules.reserve(size_t(s.recipe_count));}
 auto arr=read<H>(f.recipes);if(!arr)return;
 int end=std::min(s.recipe_count,s.recipe_cursor+32);auto start=monotonic_ms();
 for(;s.recipe_cursor<end;++s.recipe_cursor){++s.scan_calls;auto r=at<H>(arr,size_t(s.recipe_cursor));if(!r)break;auto output=read<H>(f.output,r);int id=read<int>(f.id,output);Recipe rule{};rule.output=id;rule.tile=read<int>(f.tile,r);rule.special=read<bool>(f.special,r);hash(id);hash(rule.tile);hash(rule.special);sample(output,s.recipe_cursor,-1);
  auto inputs=read<H>(f.inputs,r);size_t length=inputs?patchlib_array_length(inputs):0;if(length>100){s.recipe_rules.clear();s.recipe_cursor=0;return;}
  for(size_t i=0;i<length;++i){auto it=at<H>(inputs,i);int item=read<int>(f.id,it);hash(item);if(item>0){rule.inputs.push_back(item);sample(it,s.recipe_cursor,int(i));}}
  auto groups=read<H>(f.groups,r);if(groups){size_t n=patchlib_array_length(groups);if(n>100)return;for(size_t i=0;i<n;++i){int g=at<int>(groups,i);hash(g);if(g>=0)rule.groups=true;}}
  s.recipe_rules.push_back(std::move(rule));if(monotonic_ms()-start>=2){++s.recipe_cursor;break;}
 }
 if(s.recipe_cursor>=s.recipe_count){s.index.begin(std::move(s.recipe_rules),s.fingerprint);s.recipe_rules.clear();s.catalog_build_started=true;}
}
void pool(){if(!s.gacha)return;std::vector<PoolEntry> p;p.reserve(s.locations.size());for(auto& [id,loc]:s.locations)p.push_back({id,loc.mask});s.gacha->set_pool(std::move(p));}
void collection_task(){
 if(!s.in_world||!s.profile_ready||!s.config.gacha)return;auto p=player();if(!p)return;
 std::array<H,5> arrays{};arrays[0]=read<H>(f.inventory,p);for(size_t i=0;i<4;++i){auto chest=read<H>(f.banks[i],p);arrays[i+1]=chest?read<H>(f.chestItems,chest):nullptr;}
 int total=0;for(auto arr:arrays)if(arr)total+=int(patchlib_array_length(arr));if(!total)return;
 int quota=32;
 while(quota-->0){int slot=s.collect_cursor++%total;for(auto arr:arrays){int n=arr?int(patchlib_array_length(arr)):0;if(slot<n){auto it=at<H>(arr,size_t(slot));if(it){auto w=identify(it);if(w.mask){s.ledger->add_owned(w.id);if(w.category!=Category::Unknown)s.ledger->migrate_weapon(w.id,int(w.category)-1);}}break;}slot-=n;}}
}
void collect_all(){int loops=16;while(loops-->0)collection_task();}
void economy_task(){
 if(!s.in_world||!s.profile_ready)return;
 auto now=monotonic_ms();if(s.config.gacha){time_t wall=std::time(nullptr);std::tm tm{};
#if defined(_WIN32)
 localtime_s(&tm,&wall);
#else
 localtime_r(&wall,&tm);
#endif
 int elapsed=s.last_online&&now>=s.last_online?int((now-s.last_online)/1000):0;s.last_online=s.last_online&&elapsed>0&&elapsed<=5?s.last_online+uint64_t(elapsed)*1000:now;s.ledger->online((tm.tm_year+1900)*10000+(tm.tm_mon+1)*100+tm.tm_mday,elapsed);
 s.ledger->gift();size_t retained=0;for(size_t i=0;i<s.bosses;++i)if(!s.ledger->boss(s.pending_boss[i]))s.pending_boss[retained++]=s.pending_boss[i];s.bosses=retained;
 }else s.last_online=0;
 s.ledger->checkpoint(now);
}
void submit_command(const commands::Command& command){
 if(!on_thread())return;
 if(!s.inbox.push(command,s.session,monotonic_ms()))commands::reply("上一条指令正在处理，请稍后再试");
}
bool sync_inventory_slot(int slot){
 if(read<int>(f.net)==0)return true;if(!f.syncSlot)return false;
 int packet=5,remote=-1,ignore=-1,who=s.local,zero=0;H text=nullptr;float index=float(slot),fzero=0;
 void* args[]={&packet,&remote,&ignore,&text,&who,&index,&fzero,&fzero,&zero,&zero,&zero};
 return patchlib_method_invoke_args(f.syncSlot,nullptr,nullptr,args);
}
void process_draw(const commands::Command& r){
 if(read<int>(f.net)!=0&&!f.syncSlot){commands::reply("联机背包同步接口不可用，未扣券未发奖");return;}

 if(!s.in_world||!playable()||!s.profile_ready||!s.gacha||!s.config.gacha||!s.catalog_ready){commands::reply("抽奖未就绪：请进入世界，启用抽奖并等待奖池加载；/查看状态 查看详情");return;}
 collect_all();pool();auto p=player();auto inv=read<H>(f.inventory,p);std::vector<int> slots;size_t n=inv?std::min<size_t>(50,patchlib_array_length(inv)):0;for(size_t i=0;i<n;++i){auto it=at<H>(inv,i);if(it&&read<int>(f.id,it)==0)slots.push_back(int(i));}
 size_t used=0;auto result=s.gacha->draw(r.count,r.mask,int(slots.size()),uint64_t(std::time(nullptr)),[&](int id){auto loc=s.locations.find(id);if(loc==s.locations.end()||used>=slots.size())return false;auto source=sample_at(loc->second);if(!source||read<int>(f.id,source)!=id)return false;auto clone=invoke<H>(f.clone,source);if(!clone||clone==source||read<int>(f.id,clone)!=id)return false;auto stack=field(f.item,"stack");write(stack,clone,1);if(!patchlib_array_set(inv,size_t(slots[used]),&clone))return false;auto inserted=at<H>(inv,size_t(slots[used]));if(inserted!=clone||read<int>(f.id,inserted)!=id)throw std::runtime_error("inventory verification");if(!sync_inventory_slot(slots[used]))throw std::runtime_error("inventory sync failed");++used;return true;});
 std::string message=result.message;for(int id:result.ids)message+=" | "+item_name(id)+" (#"+std::to_string(id)+")";commands::reply(message);
 if(s.ledger)commands::reply("剩余抽奖券："+std::to_string(s.ledger->state().tickets));
}
void process_command(){
 auto command=s.inbox.take(s.session,monotonic_ms());if(!command)return;
 using commands::Kind;switch(command->kind){
 case Kind::Help:case Kind::Invalid:
  commands::reply("/随机单抽；/指定近战单抽；/随机职业十连；/指定召唤职业十连；/查看状态；/抽奖记录 [1-7]。指定职业可用近战、远程、魔法、召唤。");break;
 case Kind::Status:{
  commands::reply("2.1.0 | 成长开关="+std::string(s.config.enabled?"开":"关")+" | 模式="+s.config.mode+" | 世界阶段="+std::to_string(s.stage)+" | 单人世界="+(s.in_world&&read<int>(f.net)==0?"是":"否")+" | 角色账本="+(s.profile_ready?"就绪":"未就绪")+" | 奖池="+(s.catalog_ready?std::to_string(s.locations.size()):"加载中")+" | 网络模式="+std::to_string(read<int>(f.net))+" | 联机="+(s.config.multiplayer?"开":"关")+" | 抽奖开关="+(s.config.gacha?"开":"关"));
  if(s.profile_ready&&s.ledger){auto& a=s.ledger->state();commands::reply("抽奖券="+std::to_string(a.tickets)+" | 今日在线券="+std::to_string(a.daily)+"/20 | 冻结="+(a.frozen?"是":"否")+" | 职业经验="+std::to_string(a.xp[0])+"/"+std::to_string(a.xp[1])+"/"+std::to_string(a.xp[2])+"/"+std::to_string(a.xp[3]));}
  commands::reply("伤害调用="+std::to_string(s.damage_calls)+" | 射弹AI="+std::to_string(s.ai_calls)+" | 命中="+std::to_string(s.strike_calls)+" | 配方扫描="+std::to_string(s.scan_calls));break;}
 case Kind::Draw:process_draw(*command);break;
 case Kind::History:{
  if(!s.profile_ready||!s.ledger){commands::reply("角色账本尚未就绪");break;}
  auto& history=s.ledger->state().history;int first=int(history.size())-1-(command->page-1)*3;
  if(first<0){commands::reply("本页没有抽奖记录");break;}
  commands::reply("抽奖记录第 "+std::to_string(command->page)+" 页");
  for(int row=0;row<3&&first-row>=0;++row){auto& record=history[size_t(first-row)];std::string line=std::to_string(record.time);for(int id:record.ids)line+=" | "+item_name(id)+" (#"+std::to_string(id)+")";commands::reply(line);}break;}
 default:break;
 }
}
void update_tags(){
 if(!active()||read<int>(f.net)!=0){if(!s.tags.empty())restore_tags();return;}
 uint64_t rev=s.revision+(s.ledger?s.ledger->revision():0);if(s.tag_revision==rev||!f.uniqueTags||!f.tagDamage)return;auto arr=read<H>(f.uniqueTags);if(!arr)return;
 for(const auto& sample:generated::samples){if(std::strcmp(sample.reference,"summon_whip"))continue;auto obj=at<H>(arr,size_t(sample.id));if(!obj)continue;int current=read<int>(f.tagDamage,obj);if(current<=0)continue;auto it=std::find_if(s.tags.begin(),s.tags.end(),[&](const Tag& t){return t.id==sample.id;});if(it==s.tags.end()){s.tags.push_back({sample.id,current,current});it=std::prev(s.tags.end());}if(current!=it->applied)it->base=current;Weapon w{};w.id=sample.id;w.category=Category::Summon;w.whip=true;int value=damage(it->base,growth(w).multiplier);write(f.tagDamage,obj,value);it->applied=value;}
 s.tag_revision=rev;
}
void maintenance(H,void**,void*,const patch_method_signature_t*){
 if(!s.alive)return;if(s.thread==std::thread::id{}){s.thread=std::this_thread::get_id();diagnostics::record("heartbeat.enter: UpdateTime callback reached");}if(!on_thread())return;++s.frame;
 // Live mode/menu guards precede cached session; never apply stale single-player state online.
 if(f.menu.value<bool>()||!playable()||(s.in_world&&read<int>(f.net)!=s.network)){if(s.in_world)clear_session();s.in_world=false;}
 s.scheduler.tick(monotonic_ms());if(s.in_world&&!s.announced){commands::reply("Native 2.1.0 已运行，输入 /查看状态；/随机单抽 或 /随机职业十连 抽奖");diagnostics::record("heartbeat.active: game-thread maintenance running");s.announced=true;}process_command();
}
void weapon_damage(H p,void** args,void* result,const patch_method_signature_t* sig){
 if(s.panel_depth||!active()||!local_player(p)||!args||!result||sig->return_type!=PATCH_INT32)return;++s.damage_calls;auto w=identify(*static_cast<H*>(args[0]));if(w.category==Category::Unknown||w.category==Category::Summon)return;*static_cast<int*>(result)=damage(*static_cast<int*>(result),growth(w).multiplier);
}
void attribute(H p,void** args,void* result,const patch_method_signature_t* sig){
 if(!growth_enabled()||!args||!result||(!sig->is_instance?false:!local_player(p)))return;auto w=identify(*static_cast<H*>(args[0]));auto a=growth(w).attrs;auto target=sig->method;
 if(target==f.critMethod&&sig->return_type==PATCH_INT32)*static_cast<int*>(result)+=a.crit;
 else if(target==f.knockMethod&&sig->return_type==PATCH_FLOAT)*static_cast<float*>(result)+=float(a.knockback);
 else if(target==f.manaMethod&&sig->return_type==PATCH_INT32)*static_cast<int*>(result)=std::max(0,int(std::floor(*static_cast<int*>(result)*(1-a.mana_cost)+.5)));
 else if(target==f.scaleMethod&&sig->return_type==PATCH_FLOAT)*static_cast<float*>(result)*=float(1+a.size);
}
void speed(H p,void**,void* result,const patch_method_signature_t* sig){if(!growth_enabled()||!local_player(p)||!result||sig->return_type!=PATCH_FLOAT)return;auto a=growth(identify(invoke<H>(f.held,p))).attrs;if(a.attack_speed>0)*static_cast<float*>(result)*=float(1+a.attack_speed);else *static_cast<float*>(result)/=float(1-a.use_time-a.cast_speed);}
bool shoot_pre(H p,void** args,const patch_method_signature_t*,void*){
 if(!on_thread())return false;H item=args&&args[1]?*static_cast<H*>(args[1]):nullptr;auto source=capture(p,item);s.shots.enter(source);auto frame=s.shoot_frames.push();if(frame&&source.valid&&growth_enabled()){auto a=growth(source.weapon).attrs;if(a.projectile_speed>0&&f.shootSpeed){frame->object=item;frame->value=read<float>(f.shootSpeed,item);write(f.shootSpeed,item,frame->value*float(1+a.projectile_speed));}}return false;
}
void shoot_post(H,void**,void*,const patch_method_signature_t*){if(!on_thread())return;s.shots.leave();HookFrame frame;if(s.shoot_frames.pop(frame)&&frame.object)write(f.shootSpeed,frame.object,frame.value);}
bool ai_pre(H p,void**,const patch_method_signature_t*,void*){if(!on_thread())return false;++s.ai_calls;s.ai.enter(active()?lookup(p):SourceToken{});return false;}
void ai_post(H,void**,void*,const patch_method_signature_t*){if(on_thread())s.ai.leave();}
bool spawn_pre(H,void**,const patch_method_signature_t*,void*){if(!on_thread())return false;auto t=s.ai.active()?s.ai.current():s.shots.current();auto applied=s.damage_sources.current();if(t.valid&&applied.valid&&t.slot==applied.slot&&t.identity==applied.identity)t.embedded=applied.embedded;s.spawns.enter(t);return false;}
void spawn_post(H,void**,void* result,const patch_method_signature_t* sig){if(!on_thread())return;auto t=s.spawns.current();s.spawns.leave();if(!result||sig->return_type!=PATCH_INT32)return;int slot=*static_cast<int*>(result);if(slot<0||slot>=5000)return;auto obj=at<H>(read<H>(f.projectiles),size_t(slot));bind(t,obj,slot);}
bool projectile_pre(H p,void**,const patch_method_signature_t*,void*){
 if(!on_thread())return false;auto t=active()?lookup(p):SourceToken{};HookFrame* frame=s.damage_frames.push();if(t.valid){double applied=growth(t.weapon).multiplier/t.embedded;int original=read<int>(f.pdamage,p);if(frame&&original>0){int adjusted=damage(original,applied);frame->object=p;frame->integer=original;write(f.pdamage,p,adjusted);t.embedded*=applied;}}s.damage_sources.enter(t);return false;
}
void projectile_post(H,void**,void*,const patch_method_signature_t*){if(!on_thread())return;s.damage_sources.leave();HookFrame frame;if(s.damage_frames.pop(frame)&&frame.object)write(f.pdamage,frame.object,frame.integer);}
bool item_pre(H p,void**,const patch_method_signature_t*,void*){if(on_thread())s.item_sources.enter(read<int>(f.animation,p)>0?capture(p,invoke<H>(f.held,p)):SourceToken{});return false;}
void item_post(H,void**,void*,const patch_method_signature_t*){if(on_thread())s.item_sources.leave();}
bool strike_pre(H p,void** args,const patch_method_signature_t*,void*){
 if(!on_thread())return false;auto frame=s.strikes.push();if(!frame||!active()||!args||!args[4]||*static_cast<bool*>(args[4]))return false;++s.strike_calls;
 frame->slot=read<int>(f.nslot,p);frame->session=s.session;frame->boss=read<bool>(f.boss,p);frame->bossId=read<int>(f.ntype,p);frame->max=read<int>(f.lifeMax,p);int life=read<int>(f.life,p);
 frame->generation=invoke<uint64_t>(f.generation,p);frame->source=s.damage_sources.active()?s.damage_sources.current():s.item_sources.current();
 auto& t=frame->source;
 frame->eligible=t.valid&&t.session==s.session&&t.owner==s.local&&!t.weapon.disabled&&life>0&&frame->max>5&&frame->slot>=0&&frame->slot<20000&&read<int>(f.real,p)<0&&read<bool>(f.chase,p)&&!read<bool>(f.friendly,p)&&!read<bool>(f.town,p)&&!read<bool>(f.immortal,p)&&!read<bool>(f.dont,p)&&!read<bool>(f.statue,p);
 if(!s.damage_sources.active()&&(t.weapon.category!=Category::Melee||t.weapon.no_melee))frame->eligible=false;
 if(s.damage_sources.active()&&set_contains(f.projBombs,t.type))frame->eligible=false;
 if(!args[0]||*static_cast<int*>(args[0])<=0||!args[5]||(*static_cast<int*>(args[5])>=0&&*static_cast<int*>(args[5])!=s.local))frame->eligible=false;
 frame->boss=frame->boss&&life>0&&read<int>(f.real,p)<0;
 return false;
}
void strike_post(H p,void**,void* result,const patch_method_signature_t* sig){
 if(!on_thread())return;StrikeFrame frame;if(!s.strikes.pop(frame)||!active()||frame.session!=s.session||frame.slot<0||frame.slot>=20000||frame.generation!=invoke<uint64_t>(f.generation,p)||read<int>(f.life,p)>0)return;
 if(s.died[frame.slot]&&s.dead[frame.slot]==frame.generation)return;
 bool hit=result&&((sig->return_type==PATCH_DOUBLE&&*static_cast<double*>(result)>0)||(sig->return_type==PATCH_INT32&&*static_cast<int*>(result)>0)||(sig->return_type==PATCH_FLOAT&&*static_cast<float*>(result)>0));if(!hit)return;
 if(frame.boss&&frame.source.valid&&frame.source.owner==s.local&&s.config.gacha&&s.bosses<s.pending_boss.size())s.pending_boss[s.bosses++]=frame.bossId;
 if(frame.eligible&&growth_enabled()){int cls=int(frame.source.weapon.category)-1;if(cls>=0){int xp=s.ledger->state().xp[cls];s.ledger->award(cls,allowed(xp,experience(frame.max,frame.boss,s.stage),s.stage));++s.revision;}}
 s.died[frame.slot]=true;s.dead[frame.slot]=frame.generation;
}
bool ammo_pre(H p,void** args,const patch_method_signature_t*,void*){
 if(!on_thread())return false;auto frame=s.ammo_frames.push();if(!frame||!growth_enabled()||!local_player(p)||!args||!args[0]||(args[7]&&*static_cast<bool*>(args[7])))return false;
 auto a=growth(identify(*static_cast<H*>(args[0]))).attrs;if(a.ammo_save<=0||read<bool>(f.ammo75,p)||read<bool>(f.ammo80,p))return false;
 if(std::generate_canonical<double,32>(s.rng)<a.ammo_save/.25){frame->object=p;write(f.ammo75,p,true);}return false;
}
void ammo_post(H,void**,void*,const patch_method_signature_t*){if(!on_thread())return;HookFrame frame;if(s.ammo_frames.pop(frame)&&frame.object)write(f.ammo75,frame.object,false);}
void reset_post(H p,void**,void*,const patch_method_signature_t*){if(!on_thread()||!local_player(p))return;update_tags();}
void tooltip(H,void** args,void*,const patch_method_signature_t*){
 if(!active()||!args||!args[0]||!args[6])return;auto it=*static_cast<H*>(args[0]);auto lines=*static_cast<H*>(args[6]);auto w=identify(it);if(w.category==Category::Unknown||!lines)return;auto g=growth(w);size_t n=std::min<size_t>(100,patchlib_array_length(lines));
 for(size_t i=0;i<n;++i){auto line=string(at<H>(lines,i));auto marker=line.find("（成长×");if(marker==std::string::npos)marker=line.find("（压制×");if(marker!=std::string::npos)line.erase(marker);auto first=line.find_first_not_of(" \t");if(first==std::string::npos||line[first]<'0'||line[first]>'9'||line.find("标记")!=std::string::npos||line.find("tag damage")!=std::string::npos)continue;if(line.find("伤害")==std::string::npos&&line.find("damage")==std::string::npos&&line.find("ダメージ")==std::string::npos)continue;size_t end=first;while(end<line.size()&&line[end]>='0'&&line[end]<='9')++end;void* damage_args[]={&it};++s.panel_depth;int base=invoke<int>(typed(f.player,"GetWeaponDamage",{f.item}),player(),damage_args);--s.panel_depth;if(base<=0)base=std::atoi(line.substr(first,end-first).c_str());line.replace(first,end-first,std::to_string(damage(base,g.multiplier)));char buffer[256];std::snprintf(buffer,sizeof buffer,"（%s×%.2f）\n武器阶段 %d｜世界阶段 %d\n职业 Lv.%d/%d｜经验 %d/%d",g.multiplier<1?"压制":"成长",g.multiplier,s.index.origin(w.id),s.stage,g.level.effective,g.level.cap,g.level.current,g.level.need);line+=buffer;auto str=patchlib_string_create(line.c_str());patchlib_array_set(lines,i,&str);patchlib_free(str);break;}
}
bool setup_fields(){
 f.main=type("Terraria","Main");f.player=type("Terraria","Player");f.item=type("Terraria","Item");f.proj=type("Terraria","Projectile");f.npc=type("Terraria","NPC");if(!f.main||!f.player||!f.item||!f.proj||!f.npc)return false;
#define FIELD(member,t,name) f.member=field(t,name);if(!f.member)diagnostics::record("metadata.field.missing: " #member " (" name ")")
 FIELD(players,f.main,"player");FIELD(projectiles,f.main,"projectile");FIELD(net,f.main,"netMode");f.local=accessor(f.main,"myPlayer");FIELD(hard,f.main,"hardMode");f.menu=accessor(f.main,"gameMenu");f.world=accessor(f.main,"worldID");f.file=accessor(f.main,"ActivePlayerFileData");
 auto integer=patchlib_get_basic_type(PATCH_INT32),real=patchlib_get_basic_type(PATCH_FLOAT);
 f.syncSlot=typed(type("Terraria","NetMessage"),"SendData",{integer,integer,integer,type("Terraria.Localization","NetworkText"),integer,real,real,real,integer,integer,integer});
 if(!f.syncSlot)diagnostics::record("network.sync.unavailable: online draws disabled; growth still available");
 auto file=type("Terraria.IO","FileData");f.path=property(file,"Path");f.cloud=property(file,"IsCloudSave");f.held=property(f.player,"HeldItem");f.name=property(f.item,"Name");f.clone=typed(f.item,"Clone",{});
 FIELD(id,f.item,"type");FIELD(damage,f.item,"damage");FIELD(ammo,f.item,"ammo");FIELD(pick,f.item,"pick");FIELD(axe,f.item,"axe");FIELD(hammer,f.item,"hammer");FIELD(melee,f.item,"melee");FIELD(ranged,f.item,"ranged");FIELD(magic,f.item,"magic");FIELD(summon,f.item,"summon");FIELD(prefix,f.item,"prefix");FIELD(useTime,f.item,"useTime");FIELD(useAmmo,f.item,"useAmmo");FIELD(shoot,f.item,"shoot");FIELD(channel,f.item,"channel");FIELD(noMelee,f.item,"noMelee");FIELD(consumable,f.item,"consumable");FIELD(sentry,f.item,"sentry");FIELD(shootSpeed,f.item,"shootSpeed");
 FIELD(playerIndex,f.player,"whoAmI");FIELD(animation,f.player,"itemAnimation");FIELD(ammo75,f.player,"ammoCost75");FIELD(ammo80,f.player,"ammoCost80");FIELD(inventory,f.player,"inventory");
 FIELD(pslot,f.proj,"whoAmI");f.pidentity=field(f.proj,"identity");if(!f.pidentity)f.pidentity=field(f.proj,"key");FIELD(powner,f.proj,"owner");FIELD(ptype,f.proj,"type");FIELD(pactive,f.proj,"active");FIELD(pfriendly,f.proj,"friendly");FIELD(ptrap,f.proj,"trap");FIELD(pnpc,f.proj,"npcProj");FIELD(pdamage,f.proj,"damage");
 FIELD(nslot,f.npc,"whoAmI");FIELD(ntype,f.npc,"type");FIELD(life,f.npc,"life");FIELD(lifeMax,f.npc,"lifeMax");FIELD(boss,f.npc,"boss");FIELD(friendly,f.npc,"friendly");FIELD(town,f.npc,"townNPC");FIELD(immortal,f.npc,"immortal");FIELD(dont,f.npc,"dontTakeDamage");FIELD(chase,f.npc,"chaseable");FIELD(statue,f.npc,"SpawnedFromStatue");FIELD(real,f.npc,"realLife");f.generation=property(f.npc,"generation");
 auto recipe=type("Terraria","Recipe");FIELD(recipes,f.main,"recipe");FIELD(recipeCount,recipe,"numRecipes");FIELD(output,recipe,"createItem");FIELD(inputs,recipe,"requiredItem");FIELD(tile,recipe,"requiredTile");FIELD(groups,recipe,"acceptedGroups");FIELD(special,recipe,"needMechdusa");FIELD(chestItems,type("Terraria","Chest"),"item");
 const char* banks[]={"bank","bank2","bank3","bank4"};for(size_t i=0;i<4;++i)f.banks[i]=field(f.player,banks[i]);
 const char* down[]={"downedBoss1","downedBoss2","downedBoss3","downedMechBoss1","downedMechBoss2","downedMechBoss3","downedPlantBoss","downedGolemBoss","downedMoonlord","downedAncientCultist","downedTowerSolar","downedTowerVortex","downedTowerNebula","downedTowerStardust"};for(size_t i=0;i<14;++i)f.down[i]=field(f.npc,down[i]);
 auto itemsets=patchlib_type_get_inner_type(type("Terraria.ID","ItemID"),"Sets");auto projsets=patchlib_type_get_inner_type(type("Terraria.ID","ProjectileID"),"Sets");FIELD(itemBombs,itemsets,"ItemsThatCountAsBombsForDemolitionistToSpawn");FIELD(whips,projsets,"IsAWhip");FIELD(projBombs,projsets,"IsABombWithFuse");FIELD(uniqueTags,itemsets,"UniqueTagEffects");FIELD(tagDamage,type("Terraria.GameContent.Items","WhipTagEffect"),"TagDamage");
#undef FIELD
 bool ok=true;
 const std::pair<const char*,H> required[]={
  {"FileData.Path",f.path},{"FileData.IsCloudSave",f.cloud},{"Player.HeldItem",f.held},{"Item.Clone",f.clone},
  {"Main.player",f.players},{"Main.projectile",f.projectiles},{"Main.netMode",f.net},{"Main.hardMode",f.hard},
  {"Item.type",f.id},{"Item.damage",f.damage},{"Item.ammo",f.ammo},{"Item.pick",f.pick},{"Item.axe",f.axe},{"Item.hammer",f.hammer},
  {"Item.melee",f.melee},{"Item.ranged",f.ranged},{"Item.magic",f.magic},{"Item.summon",f.summon},{"Item.prefix",f.prefix},
  {"Item.useTime",f.useTime},{"Item.useAmmo",f.useAmmo},{"Item.shoot",f.shoot},{"Item.channel",f.channel},{"Item.noMelee",f.noMelee},{"Item.consumable",f.consumable},{"Item.sentry",f.sentry},
  {"Player.whoAmI",f.playerIndex},{"Player.inventory",f.inventory},{"Player.itemAnimation",f.animation},
  {"Projectile.identity/key",f.pidentity},{"Projectile.whoAmI",f.pslot},{"Projectile.owner",f.powner},{"Projectile.type",f.ptype},{"Projectile.active",f.pactive},{"Projectile.friendly",f.pfriendly},{"Projectile.trap",f.ptrap},{"Projectile.npcProj",f.pnpc},{"Projectile.damage",f.pdamage},
  {"NPC.generation",f.generation},{"NPC.whoAmI",f.nslot},{"NPC.type",f.ntype},{"NPC.life",f.life},{"NPC.lifeMax",f.lifeMax},{"NPC.boss",f.boss},{"NPC.friendly",f.friendly},{"NPC.townNPC",f.town},{"NPC.immortal",f.immortal},{"NPC.dontTakeDamage",f.dont},{"NPC.chaseable",f.chase},{"NPC.SpawnedFromStatue",f.statue},{"NPC.realLife",f.real},
  {"Main.recipe",f.recipes},{"Recipe.numRecipes",f.recipeCount},{"Recipe.createItem",f.output},{"Recipe.requiredItem",f.inputs},{"Recipe.requiredTile",f.tile},{"Recipe.acceptedGroups",f.groups},{"Recipe.needMechdusa",f.special},
  {"ItemID.Sets.bombs",f.itemBombs},{"ProjectileID.Sets.bombs",f.projBombs},{"ProjectileID.Sets.IsAWhip",f.whips}
 };
 for(auto& entry:required)if(!entry.second){diagnostics::record(std::string("metadata.required.missing: ")+entry.first,true);ok=false;}
 if(!f.menu.valid()||!f.world.valid()||!f.file.valid()||!f.local.valid()){diagnostics::record("metadata.required.missing: Main.gameMenu/worldID/ActivePlayerFileData/myPlayer",true);ok=false;}
 return ok;
}
bool install_hooks(){
 auto add=[&](H m,int n,bool st,prefix_callback_t pre,postfix_callback_t post){bool ok=s.hooks.add(m,n,st,pre,post);if(!ok){std::string name=m&&patchlib_method_get_name?patchlib_method_get_name(m):"method-not-found";diagnostics::record("hook.failed: "+name+" expected argc="+std::to_string(n)+" static="+std::to_string(st),true);}return ok;};
 if(!add(typed(f.main,"UpdateTime",{}),0,true,nullptr,maintenance))return false;
 if(!add(typed(f.player,"GetWeaponDamage",{f.item}),1,false,nullptr,weapon_damage))return false;
 if(!add(f.critMethod=named(f.player,"GetWeaponCrit",{"sItem"}),1,false,nullptr,attribute))diagnostics::record("optional.attribute.disabled",true);
 if(!add(f.knockMethod=named(f.player,"GetWeaponKnockback",{"sItem","KnockBack"}),2,false,nullptr,attribute))diagnostics::record("optional.attribute.disabled",true);
 if(!add(f.manaMethod=named(f.player,"ItemCheck_PayMana_GetManaCostToPay",{"sItem","altFire"}),2,true,nullptr,attribute))diagnostics::record("optional.mana.disabled",true);
 if(!add(f.scaleMethod=named(f.player,"GetAdjustedItemScale",{"item"}),1,false,nullptr,attribute)){if(!add(f.scaleMethod=typed(f.player,"GetAdjustedItemScale",{f.item}),1,false,nullptr,attribute))diagnostics::record("optional.attribute.disabled",true);}
 if(!add(named(f.player,"TurnAttackSpeedToUseTimeMultiplier",{"speed"}),1,false,nullptr,speed))diagnostics::record("optional.speed.disabled",true);
 if(!add(named(f.player,"ItemCheck_Shoot",{"i","sItem","weaponDamage","withAudioVisualFeedback"}),4,false,shoot_pre,shoot_post))return false;
 if(!add(named(f.player,"PickAmmo",{"sItem","projToShoot","speed","canShoot","Damage","KnockBack","usedAmmoItemId","dontConsume"}),8,false,ammo_pre,ammo_post))diagnostics::record("optional.ammo.disabled",true);
 if(!add(typed(f.proj,"AI",{}),0,false,ai_pre,ai_post)||!add(typed(f.proj,"Damage",{}),0,false,projectile_pre,projectile_post))return false;
 auto integer=patchlib_get_basic_type(PATCH_INT32),real=patchlib_get_basic_type(PATCH_FLOAT),boolean=patchlib_get_basic_type(PATCH_BOOL),source=type("Terraria.DataStructures","IEntitySource");H spawn=nullptr;int n=0;
 auto modifier=type("Terraria","NewProjectileModifier");if(modifier){spawn=typed(f.proj,"NewProjectile",{source,real,real,real,real,integer,integer,real,integer,real,real,real,modifier});n=13;}
 if(!spawn){spawn=typed(f.proj,"NewProjectile",{source,real,real,real,real,integer,integer,real,integer,real,real,real});n=12;}
 if(!spawn){spawn=typed(f.proj,"NewProjectile",{source,real,real,real,real,integer,integer,real,integer,real,real});n=11;}
 if(!add(spawn,n,true,spawn_pre,spawn_post))return false;
 auto itemcheck=named(f.player,"ItemCheck",{"i"});int ic=1;if(!itemcheck){itemcheck=typed(f.player,"ItemCheck",{});ic=0;}if(!add(itemcheck,ic,false,item_pre,item_post))return false;
 auto strike=typed(f.npc,"StrikeNPC",{integer,real,integer,boolean,boolean,integer});if(!add(strike,6,false,strike_pre,strike_post))return false;
 if(!add(typed(f.player,"ResetEffects",{}),0,false,nullptr,reset_post))diagnostics::record("optional.whip_tags.disabled",true);
 if(!add(method(f.main,"MouseText_DrawItemTooltip_GetLinesInfo",12),12,true,nullptr,tooltip))diagnostics::record("tooltip.disabled: growth core remains active",true);
 return true;
}
}
uint64_t monotonic_ms(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
int local_player_index(){return s.local;}
std::string item_name(int id){auto fnd=s.names.find(id);if(fnd!=s.names.end())return fnd->second;auto loc=s.locations.find(id);if(loc!=s.locations.end()){auto obj=sample_at(loc->second);auto name=string(invoke<H>(f.name,obj));if(!name.empty()){s.names[id]=name;return name;}}return "武器#"+std::to_string(id);}
bool initialize_game(const std::string& directory){
 if(s.alive)return !s.install_failed;if(s.install_failed)return false;diagnostics::configure(directory);diagnostics::record("init.enter: WeaponGrowth 2.1.0 command edition");if(auto missing=missing_api()){diagnostics::record(std::string("api.missing: ")+missing,true);return false;}s.scheduler=Scheduler{};s.thread={};s.catalog_ready=false;s.catalog_build_started=false;s.recipe_cursor=0;s.recipe_rules.clear();s.building_locations.clear();s.fingerprint=1469598103934665603ull;s.directory=directory;s.install_failed=false;std::filesystem::create_directories(directory);if(!setup_fields()){diagnostics::record("metadata.failed: required game fields/getters missing",true);return false;}diagnostics::record("metadata.ready");
 for(auto p:generated::anchors)s.index.seed(p.id,p.value);for(auto p:generated::tiles)s.index.tile(p.id,p.value);
 s.alive=true;config_task();s.scheduler.add(250,session_task);s.scheduler.add(16,catalog_task);s.scheduler.add(500,collection_task);s.scheduler.add(1000,economy_task);s.scheduler.add(2000,config_task);s.scheduler.reset(monotonic_ms());
 if(!install_hooks()||!commands::install(submit_command)){s.install_failed=true;s.alive=false;commands::shutdown();s.hooks.clear();diagnostics::record("init.failed: hook or chat entry unavailable; partial hooks removed",true);return false;}diagnostics::record("init.ready: native growth + chat commands, no renderer dependency");return true;
}
bool shutdown_game(){s.alive=false;bool ok=commands::shutdown();ok=s.hooks.clear()&&ok;if(ok){clear_session();retry_saves();s.in_world=false;ok=s.unsaved.empty();}if(!ok)s.install_failed=true;return ok;}
}
