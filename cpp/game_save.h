// Copyright 2015-2015 the openage authors. See copying.md for legal info.

#ifndef OPENAGE_GAME_SAVE_H_
#define OPENAGE_GAME_SAVE_H_

#include <string>

namespace openage {

class GameMain;
class Terrain;

namespace gameio {

/**
 * a game save function that sometimes works
 */
void save(openage::GameMain *, std::string fname);

/**
 * a game load function that sometimes works
 */
void load(openage::GameMain *, std::string fname);

} //namespace gameio
} //namespace openage

#endif
