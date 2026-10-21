#include "recipe_index.hpp"
#include <algorithm>
namespace wg {
void RecipeIndex::begin(std::vector<Recipe> input,uint64_t hash){
 if(complete&&hash==fingerprint)return;
 fingerprint=hash;complete=false;recipes=std::move(input);building=anchors;dependents.clear();queue.clear();queued.assign(recipes.size(),true);
 for(size_t i=0;i<recipes.size();++i){queue.push_back(i);for(int id:recipes[i].inputs)dependents[id].push_back(i);}
}
bool RecipeIndex::step(size_t budget){
 last=0;if(complete)return true;
 while(last<budget&&!queue.empty()){
  auto i=queue.front();queue.pop_front();queued[i]=false;++last;const auto& r=recipes[i];
  auto tile=tiles.find(r.tile);if(tile==tiles.end()||r.inputs.empty()||r.special||anchors.count(r.output))continue;
  int stage=tile->second;bool known=true,s1=false,s2=false,s3=false;
  for(int id:r.inputs){auto f=building.find(id);if(f==building.end()){known=false;break;}stage=std::max(stage,f->second);s1|=id==547;s2|=id==548;s3|=id==549;}
  if(s1&&s2&&s3)stage=std::max(stage,6);
  if(!known||(r.groups&&stage!=0))continue;
  auto old=building.find(r.output);if(old!=building.end()&&old->second<=stage)continue;
  building[r.output]=stage;
  auto dep=dependents.find(r.output);if(dep!=dependents.end())for(auto n:dep->second)if(!queued[n]){queued[n]=true;queue.push_back(n);}
 }
 if(queue.empty()){published.swap(building);complete=true;++rev;}
 return complete;
}
}
