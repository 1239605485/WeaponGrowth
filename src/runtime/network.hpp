#pragma once
namespace wg {
// The local player owns client-side growth and inventory. Never repeat it on
// a dedicated server (netMode 2). Mobile host-and-play uses netMode 3.
inline bool playable_network(int mode,bool multiplayer){return mode==0||(multiplayer&&(mode==1||mode==3));}
}
