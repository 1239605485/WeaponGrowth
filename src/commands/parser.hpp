#pragma once
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
namespace wg::commands {
enum class Kind { Ignore, Invalid, Help, Status, Draw, History };
struct Command {Kind kind=Kind::Ignore;int count=0,page=1;uint8_t mask=0;};
inline Command parse(const std::string& text){
 std::istringstream in(text);std::vector<std::string> words;std::string word;
 while(in>>word){words.push_back(word);if(words.size()>6)break;}
 if(words.empty())return {};
 const auto& root=words[0];
 bool chinese=root=="/随机单抽"||root=="/随机职业十连"||root=="/查看状态"||root=="/抽奖记录"||root.rfind("/指定",0)==0;
 if(chinese){
  Command c{Kind::Invalid};if(text.size()>160||text.find_first_of("\r\n")!=std::string::npos)return c;
  if(root=="/抽奖记录"){
   if(words.size()==1)c.kind=Kind::History;
   else if(words.size()==2&&words[1].size()==1&&words[1][0]>='1'&&words[1][0]<='7'){c.kind=Kind::History;c.page=words[1][0]-'0';}
   return c;
  }
  if(words.size()!=1)return c;
  if(root=="/查看状态"){c.kind=Kind::Status;return c;}
  if(root=="/随机单抽"||root=="/随机职业十连"){c.kind=Kind::Draw;c.count=root=="/随机单抽"?1:10;return c;}
  const char* classes[]={"近战","远程","魔法","召唤"};
  for(int i=0;i<4;++i){auto base=std::string("/指定")+classes[i];
   if(root==base+"单抽"||root==base+"职业十连"){c.kind=Kind::Draw;c.count=root==base+"单抽"?1:10;c.mask=uint8_t(1<<i);return c;}
  }
  return c;
 }
 if(root!="/wg"&&root!="/抽奖")return {};
 Command c{Kind::Invalid};if(text.size()>160||text.find_first_of("\r\n")!=std::string::npos)return c;
 size_t pos=1;bool alias=words[0]=="/抽奖";
 if(!alias){
  if(words.size()==1||words[1]=="help"){if(words.size()<=2)c.kind=Kind::Help;return c;}
  if(words[1]=="status"){if(words.size()==2)c.kind=Kind::Status;return c;}
  if(words[1]=="history"){
   if(words.size()==2){c.kind=Kind::History;return c;}
   if(words.size()==3&&words[2].size()==1&&words[2][0]>='1'&&words[2][0]<='7'){c.kind=Kind::History;c.page=words[2][0]-'0';}return c;
  }
  if(words[1]!="draw")return c;pos=2;
 }
 if(alias&&words.size()==1){c.kind=Kind::Draw;c.count=1;return c;}
 if(words.size()<pos+1||words.size()>pos+2)return c;
 if(words[pos]=="1")c.count=1;else if(words[pos]=="10")c.count=10;else return c;
 if(words.size()==pos+2){auto& category=words[pos+1];
  if(category=="melee"||category=="近战")c.mask=1;
  else if(category=="ranged"||category=="远程")c.mask=2;
  else if(category=="magic"||category=="魔法")c.mask=4;
  else if(category=="summon"||category=="召唤")c.mask=8;
  else if(category!="random"&&category!="随机")return c;
 }
 c.kind=Kind::Draw;return c;
}
// A single pending user action. Repeated processing cannot repeat a transaction.
class Inbox {
 std::optional<Command> pending;uint64_t session=0,at=0;
public:
 bool push(Command c,uint64_t profile,uint64_t now){if(pending)return false;pending=c;session=profile;at=now;return true;}
 std::optional<Command> take(uint64_t profile,uint64_t now){auto result=pending;pending.reset();if(profile!=session||now<at||now-at>5000)return {};return result;}
 void clear(){pending.reset();}
 bool busy()const{return pending.has_value();}
};
}
