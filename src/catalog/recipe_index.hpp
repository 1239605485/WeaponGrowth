#pragma once
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>
namespace wg {
struct Recipe {int output=0,tile=-1;std::vector<int> inputs;bool groups=false,special=false;};
class RecipeIndex {
 std::unordered_map<int,int> anchors,tiles{{-1,0}},published,building;
 std::unordered_map<int,std::vector<size_t>> dependents;
 std::vector<Recipe> recipes;
 std::deque<size_t> queue;std::vector<bool> queued;
 uint64_t fingerprint=0,rev=0;bool complete=false;size_t last=0;
public:
 void seed(int id,int stage){anchors[id]=stage;published[id]=stage;}
 void tile(int id,int stage){tiles[id]=stage;}
 void begin(std::vector<Recipe> input,uint64_t hash);
 bool step(size_t budget);
 bool ready()const{return complete;}
 size_t work_last()const{return last;}
 int origin(int id)const {auto i=published.find(id);return i==published.end()?-1:i->second;}
 uint64_t revision()const{return rev;}
};
}
