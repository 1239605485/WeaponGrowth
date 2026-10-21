#include "service.hpp"
#include <algorithm>
namespace wg {
DrawResult GachaService::draw(int count,uint8_t mask,int slots,uint64_t time,const std::function<bool(int)>& give){
 DrawResult r{};r.message="抽取条件未满足";
 if((count!=1&&count!=10)||slots<count||!ledger.available()||ledger.state().frozen||ledger.state().tickets<count)return r;
 std::vector<int> candidates;for(const auto& e:pool)if((!mask||(e.mask&mask))&&!ledger.state().owned.count(e.id))candidates.push_back(e.id);
 std::sort(candidates.begin(),candidates.end());candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
 if(int(candidates.size())<count){r.message="未收集奖池不足";return r;}
 for(int i=0;i<count;++i){std::uniform_int_distribution<size_t> pick(size_t(i),candidates.size()-1);std::swap(candidates[i],candidates[pick(rng)]);}
 candidates.resize(size_t(count));
 if(!ledger.prepare(candidates,time)){r.message="存档不可写，未发奖";return r;}
 for(int id:candidates){try{if(!give(id))break;r.ids.push_back(id);}catch(...){ledger.freeze();ledger.save();r.message="发奖状态不确定，已冻结，请检查存档";return r;}}
 if(!ledger.finish(r.ids,time)){r.message="发奖后存档失败，已冻结以防重复";return r;}
 r.ok=int(r.ids.size())==count;r.message=r.ok?"抽取成功":"部分发放，仅扣已发放数量";return r;
}
}
