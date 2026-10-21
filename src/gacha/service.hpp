#pragma once
#include "storage/ledger.hpp"
#include <random>
namespace wg {
struct PoolEntry {int id=0;uint8_t mask=0;};
struct DrawResult {bool ok=false;std::vector<int> ids;std::string message;};
class GachaService {
 Ledger& ledger;std::vector<PoolEntry> pool;std::mt19937 rng{std::random_device{}()};
public:
 explicit GachaService(Ledger& l):ledger(l){}
 void set_pool(std::vector<PoolEntry> input){pool=std::move(input);}
 size_t total()const{return pool.size();}
 DrawResult draw(int count,uint8_t mask,int slots,uint64_t time,const std::function<bool(int)>& give);
};
}
