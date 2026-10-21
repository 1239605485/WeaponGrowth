#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace wg {
// One due task per frame; no catch-up burst after pause.
class Scheduler {
 struct Task { uint64_t interval=0,due=0;std::function<void()> run; };
 std::array<Task,16> tasks{}; size_t count=0,cursor=0;
public:
 void add(uint64_t interval,std::function<void()> fn){ if(count<tasks.size())tasks[count++]={interval,interval,std::move(fn)}; }
 void reset(uint64_t now){for(size_t i=0;i<count;++i)tasks[i].due=now+tasks[i].interval;cursor=0;}
 void tick(uint64_t now){ for(size_t i=0;i<count;++i){size_t n=(cursor+i)%count;auto& t=tasks[n];if(now>=t.due){t.due=now+t.interval;cursor=(n+1)%count;t.run();return;}} }
};
}
