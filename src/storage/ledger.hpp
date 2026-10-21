#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <map>
#include <string>
#include <vector>
namespace wg {
struct FileIO {
 std::function<std::optional<std::string>(const std::string&)> read;
 std::function<bool(const std::string&,const std::string&)> write;
};
struct DrawRecord {uint64_t time=0;std::vector<int> ids;};
struct Account {
 int tickets=0,pity=0,day=0,daily=0,seconds=0;
 bool newgift=false,frozen=false;
 uint64_t generation=0,request=0;
 std::array<int,4> xp{};
 std::set<int> owned,bosses;
 std::map<int,int> legacy_weapons;
 std::string identity;
 std::vector<DrawRecord> history;
 std::vector<int> pending;
};
class Ledger {
 FileIO io;Account data{};std::string key;int slot=-1;bool dirty=false,blocked=true;uint64_t last_save=0,rev=0;
public:
 explicit Ledger(FileIO files):io(std::move(files)){}
 bool open(const std::string& basename,const std::string& identity={});
 bool save();bool checkpoint(uint64_t now);
 bool gift();bool boss(int id);void online(int day,int elapsed);
 bool add_owned(int id);void award(int cls,int amount);
 void migrate_weapon(int id,int cls);
 bool prepare(const std::vector<int>& ids,uint64_t request);
 bool finish(const std::vector<int>& granted,uint64_t time);
 void freeze(){data.frozen=true;dirty=true;}
 bool import_legacy(const std::string& profile,const std::string& directory_prefix);
 const Account& state()const{return data;}
 bool available()const{return !blocked;}
 bool needs_save()const{return !blocked&&dirty;}
 uint64_t revision()const{return rev;}
};
std::string hex(const std::string& s);
uint32_t checksum(const std::string& s);
}
