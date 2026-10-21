#include "test.hpp"
#include "gacha/service.hpp"
#include <unordered_map>
int main(){
 using namespace wg;std::unordered_map<std::string,std::string> files;bool writable=true;int writes=0;
 FileIO io{[&](const std::string& p)->std::optional<std::string>{auto f=files.find(p);if(f==files.end())return {};return f->second;},[&](const std::string& p,const std::string& s){++writes;if(!writable)return false;files[p]=s;return true;}};
 Ledger l(io);CHECK(l.open("p"));CHECK(l.gift());CHECK(l.state().tickets==10);CHECK(l.gift());CHECK(l.state().tickets==10);
 GachaService g(l);g.set_pool({{4,1},{39,2},{64,4}});int delivered=0;
 auto r=g.draw(1,1,1,100,[&](int){++delivered;return true;});CHECK(r.ok&&delivered==1&&l.state().tickets==9);
 CHECK(!g.draw(10,0,10,101,[](int){return true;}).ok);
 writable=false;CHECK(!g.draw(1,2,1,102,[&](int){++delivered;return true;}).ok);CHECK(delivered==1);
 writable=true;Ledger loaded(io);CHECK(loaded.open("p"));CHECK(loaded.state().tickets==9);
 CHECK(loaded.state().owned.count(4)==1);
 // Establish the first day before measuring 30 minutes; entering/crossing day discards the previous interval.
 loaded.online(20261021,0);auto before=writes;
 for(int i=0;i<900;++i){loaded.online(20261021,2);loaded.checkpoint(uint64_t(i+1)*2000);}
 CHECK(loaded.state().daily==15);CHECK(loaded.state().tickets==24);CHECK(writes-before<=80);
 loaded.online(20261022,2);CHECK(loaded.state().daily==0);
 CHECK(loaded.boss(50));CHECK(loaded.state().tickets==34);CHECK(loaded.boss(50));CHECK(loaded.state().tickets==34);
 Ledger partial(io);CHECK(partial.open("partial"));CHECK(partial.gift());GachaService pg(partial);pg.set_pool({{1,1},{2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{8,1},{9,1},{10,1}});
 int n=0;auto pr=pg.draw(10,0,10,200,[&](int){return ++n<=2;});CHECK(!pr.ok);CHECK(partial.state().tickets==8);CHECK(partial.state().owned.size()==2);
 Ledger corrupt(io);files["bad_a.dat"]="broken";files["bad_b.dat"]="broken";CHECK(!corrupt.open("bad"));CHECK(!corrupt.gift());CHECK(!corrupt.needs_save());CHECK(files["bad_a.dat"]=="broken");
 // Interrupted transactions must freeze on reload, never silently redeliver.
 Ledger pending(io);CHECK(pending.open("pending"));CHECK(pending.gift());CHECK(pending.prepare({1},300));
 Ledger recovered(io);CHECK(recovered.open("pending"));CHECK(recovered.state().frozen);GachaService rg(recovered);rg.set_pool({{1,1}});CHECK(!rg.draw(1,0,1,301,[](int){return true;}).ok);
 // A failed transaction must not discard earlier unsaved XP.
 Ledger rollback(io);CHECK(rollback.open("rollback"));CHECK(rollback.gift());rollback.award(0,123);writable=false;CHECK(!rollback.prepare({4},900));writable=true;CHECK(rollback.save());Ledger again(io);CHECK(again.open("rollback"));CHECK(again.state().xp[0]==123);
 Ledger healthy(io);CHECK(healthy.open("healthy"));CHECK(healthy.gift());CHECK(!healthy.needs_save());
 Ledger identity(io);CHECK(identity.open("identity","first"));CHECK(identity.gift());Ledger collision(io);CHECK(!collision.open("identity","second"));
 auto body=hex("legacy")+"|900|0|0|0|1|4:450,19999:600\n";files["weapon_progress_a.dat"]="WGPROGRESS|2|1|"+std::to_string(checksum("1\n"+body))+"\n"+body;
 Ledger legacy(io);CHECK(legacy.open("legacy"));CHECK(legacy.import_legacy("legacy",""));CHECK(legacy.state().xp[0]==450);CHECK(legacy.state().legacy_weapons.at(19999)==600);legacy.migrate_weapon(19999,1);CHECK(legacy.state().xp[1]==600);
 std::puts("ledger/gacha: passed");
}
