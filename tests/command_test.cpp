#include "test.hpp"
#include "commands/parser.hpp"
int main(){
 using namespace wg::commands;
 CHECK(parse("hello").kind==Kind::Ignore);
 CHECK(parse("/wgood draw 10").kind==Kind::Ignore);
 CHECK(parse("/wgdraw 10").kind==Kind::Ignore);
 CHECK(parse("/wg").kind==Kind::Help);
 CHECK(parse("  /wg status  ").kind==Kind::Status);
 auto one=parse("/wg draw 1");CHECK(one.kind==Kind::Draw&&one.count==1&&one.mask==0);
 auto ten=parse("/wg draw 10 summon");CHECK(ten.kind==Kind::Draw&&ten.count==10&&ten.mask==8);
 CHECK(parse("/wg draw 1 近战").mask==1);
 CHECK(parse("/wg draw 1 ranged").mask==2);
 CHECK(parse("/wg draw 1 magic").mask==4);
 CHECK(parse("/抽奖 10 召唤").mask==8);
 CHECK(parse("/抽奖").count==1);
 CHECK(parse("/wg draw 0").kind==Kind::Invalid);
 CHECK(parse("/wg draw 11").kind==Kind::Invalid);
 CHECK(parse("/wg draw 1 bogus").kind==Kind::Invalid);
 CHECK(parse("/wg draw 10 melee extra").kind==Kind::Invalid);
 CHECK(parse("/wg history").page==1);
 CHECK(parse("/wg history 7").page==7);
 CHECK(parse("/wg history 8").kind==Kind::Invalid);
 CHECK(parse("/wg history 2oops").kind==Kind::Invalid);
 CHECK(parse("/wg status extra").kind==Kind::Invalid);
 CHECK(parse("/wg\ndraw 10").kind==Kind::Invalid);
 CHECK(parse("/随机单抽").count==1);
 CHECK(parse("/随机职业十连").count==10);
 CHECK(parse("/指定近战单抽").mask==1);
 CHECK(parse("/指定远程单抽").mask==2);
 CHECK(parse("/指定魔法职业十连").mask==4);
 CHECK(parse("/指定召唤职业十连").mask==8);
 CHECK(parse("/查看状态").kind==Kind::Status);
 CHECK(parse("/抽奖记录").kind==Kind::History);
 CHECK(parse("/抽奖记录 2").page==2);
 CHECK(parse("/指定未知单抽").kind==Kind::Invalid);
 CHECK(parse("/随机单抽 extra").kind==Kind::Invalid);
 CHECK(parse("/查看状态\n").kind==Kind::Invalid);
 Inbox inbox;Command command=one;CHECK(inbox.push(command,10,100));
 CHECK(!inbox.push(command,10,101));
 auto next=inbox.take(11,102);CHECK(!next); // Session changed: never draw for another character.
 CHECK(inbox.push(command,11,103));CHECK(!inbox.take(11,6000));
 CHECK(inbox.push(command,11,6001));next=inbox.take(11,6002);CHECK(next&&next->count==1);
 CHECK(!inbox.take(11,6003));
 std::puts("chat commands: passed");
}
