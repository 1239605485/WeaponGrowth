#include "test.hpp"
#include "runtime/scheduler.hpp"
#include "catalog/recipe_index.hpp"
#include "combat/sources.hpp"
int main(){
 using namespace wg;
 Scheduler s; int a=0,b=0; s.add(100,[&]{++a;});s.add(100,[&]{++b;});
 s.tick(100); CHECK(a==1&&b==0);s.tick(101);CHECK(b==1);s.tick(150);CHECK(a==1);
 s.tick(200);CHECK(a==2);s.reset(1000);s.tick(1000);CHECK(a==2);
 RecipeIndex idx; idx.seed(1,0);idx.seed(2,3);
 idx.begin({{3,-1,{1,2},false,false},{4,-1,{3},false,false}},11);
 CHECK(idx.origin(3)==-1);idx.step(1);CHECK(idx.work_last()<=1);
 while(!idx.ready())idx.step(1);CHECK(idx.origin(4)==3);
 auto rev=idx.revision();idx.begin({{3,-1,{1},false,false}},12);
 while(!idx.ready())idx.step(1);CHECK(idx.origin(3)==0);CHECK(idx.revision()>rev);
 SourceStack<2> stack; SourceToken t{};t.valid=true;t.weapon.id=4;t.session=1;
 stack.enter(t);stack.enter({});CHECK(!stack.current().valid);
 stack.enter(t);CHECK(!stack.current().valid);stack.leave();CHECK(!stack.current().valid);
 stack.leave();CHECK(stack.current().valid);stack.leave();CHECK(!stack.current().valid);
 Sources sources; t.slot=2;t.identity=7;t.owner=0;t.type=1;sources.bind(t);
 CHECK(sources.lookup(2,7,0,1,1).valid);CHECK(!sources.lookup(2,8,0,1,1).valid);
 sources.bind(t);CHECK(!sources.lookup(2,7,0,1,2).valid);
 std::puts("runtime/catalog/sources: passed");
}
