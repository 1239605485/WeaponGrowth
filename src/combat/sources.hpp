#pragma once
#include "growth/rules.hpp"
#include <array>
namespace wg {
struct SourceToken {
 Weapon weapon{};uint64_t session=0;int slot=-1,owner=-1,type=0;uint32_t identity=0;
 double embedded=1; bool valid=false;
};
template<size_t Capacity=128> class SourceStack {
 std::array<SourceToken,Capacity> data{};size_t used=0,overflow=0;
public:
 void enter(const SourceToken& t){if(used==Capacity){++overflow;return;}data[used++]=t;}
 void leave(){if(overflow){--overflow;return;}if(used)--used;}
 SourceToken current()const{return overflow||!used?SourceToken{}:data[used-1];}
 bool active()const{return used||overflow;}
 void clear(){used=overflow=0;}
};
class Sources {
 std::array<SourceToken,5000> data{};
public:
 void clear(){data={};}
 void erase(int slot){if(slot>=0&&slot<int(data.size()))data[slot]={};}
 void bind(const SourceToken& t){if(t.slot>=0&&t.slot<int(data.size()))data[t.slot]=t;}
 SourceToken lookup(int slot,uint32_t identity,int owner,int type,uint64_t session){
  if(slot<0||slot>=int(data.size()))return {};
  auto& t=data[slot];if(t.valid&&t.identity==identity&&t.owner==owner&&t.type==type&&t.session==session)return t;
  t={};return {};
 }
};
}
