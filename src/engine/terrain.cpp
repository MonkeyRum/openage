#include "terrain.h"

#include <map>

#include "terrain_chunk.h"
#include "engine.h"
#include "texture.h"
#include "log.h"
#include "util/error.h"
#include "util/misc.h"
#include "util/strings.h"
#include "coord/camgame.h"
#include "coord/chunk.h"

namespace engine {

//TODO: get that from the convert script!
coord::camgame_delta tile_halfsize = {48, 24};


Terrain::Terrain(size_t terrain_meta_count,
                 terrain_type *terrain_meta,
                 size_t blending_meta_count,
                 blending_mode *blending_meta) {
	//maps chunk position to chunks
	this->chunks = std::map<coord::chunk, TerrainChunk *, coord_chunk_compare>{};
	//activate blending
	this->blending_enabled = true;

	this->terrain_type_count = terrain_meta_count;
	this->blendmode_count    = blending_meta_count;
	this->textures       = new engine::Texture*[this->terrain_type_count];
	this->blending_masks = new engine::Texture*[this->blendmode_count];
	this->terrain_id_priority_map  = new int[terrain_type_count];
	this->terrain_id_blendmode_map = new int[terrain_type_count];

	log::dbg("terrain prefs: %lu tiletypes, %lu blendmodes", this->terrain_type_count, this->blendmode_count);

	//create tile textures (snow, ice, grass, whatever)
	for (size_t i = 0; i < this->terrain_type_count; i++) {
		auto line = &terrain_meta[i];
		this->terrain_id_priority_map[i] = line->blend_priority;
		this->terrain_id_blendmode_map[i] = line->blend_mode;

		char *terraintex_filename = util::format("age/raw/Data/terrain.drs/%d.slp.png", line->slp_id);
		auto new_texture = new Texture(terraintex_filename, true, ALPHAMASKED);
		new_texture->fix_hotspots(48, 24);
		this->textures[i] = new_texture;
		delete[] terraintex_filename;
	}

	//create blending masks (see doc/media/blendomatic)
	for (size_t i = 0; i < this->blendmode_count; i++) {
		auto line = &blending_meta[i];

		char *mask_filename = util::format("age/alphamask/mode%02d.png", line->mode_id);
		auto new_texture = new Texture(mask_filename, true);
		new_texture->fix_hotspots(48, 24);
		this->blending_masks[i] = new_texture;
		delete[] mask_filename;
	}

}

Terrain::~Terrain() {
	for (size_t i = 0; i < this->terrain_type_count; i++) {
		delete this->textures[i];
	}
	for (size_t i = 0; i < this->blendmode_count; i++) {
		delete this->blending_masks[i];
	}

	delete[] this->blending_masks;
	delete[] this->textures;
	delete[] this->terrain_id_priority_map;
}

void Terrain::attach_chunk(TerrainChunk *new_chunk, coord::chunk position) {
	new_chunk->set_terrain(this);
	this->chunks[position] = new_chunk;

	struct chunk_neighbors neigh = this->get_chunk_neighbors(position);
	for (int i = 0; i < 8; i++) {
		TerrainChunk *neighbor = neigh.neighbor[i];
		if (neighbor != nullptr) {
			neighbor->neighbors.neighbor[(i+4) % 8] = new_chunk;
		}
		else {
			log::dbg("neighbor %d not found.", i);
		}
	}
}

TerrainChunk *Terrain::get_chunk(coord::chunk position) {
	//is this chunk stored?
	if (this->chunks.find(position) == this->chunks.end()) {
		return nullptr;
	}
	else {
		return this->chunks[position];
	}
}

void Terrain::draw() {
	for (auto &chunk : this->chunks) {
		coord::chunk pos = chunk.first;
		chunk.second->draw(pos);
	}
}

bool Terrain::valid_terrain(size_t terrain_id) {
	if (terrain_id >= this->terrain_type_count) {
		throw Error("requested terrain_id is out of range: %lu", terrain_id);
	}
	else {
		return true;
	}
}

bool Terrain::valid_mask(size_t mask_id) {
	if (mask_id >= this->blendmode_count) {
		throw Error("requested mask_id is out of range: %lu", mask_id);
	}
	else {
		return true;
	}
}

int Terrain::priority(size_t terrain_id) {
	this->valid_terrain(terrain_id);
	return this->terrain_id_priority_map[terrain_id];
}

int Terrain::blendmode(size_t terrain_id) {
	this->valid_terrain(terrain_id);
	return this->terrain_id_blendmode_map[terrain_id];
}

Texture *Terrain::texture(size_t terrain_id) {
	this->valid_terrain(terrain_id);
	return this->textures[terrain_id];
}

Texture *Terrain::blending_mask(size_t mask_id) {
	this->valid_mask(mask_id);
	return this->blending_masks[mask_id];
}

/**
returns the terrain subtexture id for a given position.

this function returns always the right value, so that neighbor tiles
of the same terrain (like grass-grass) are matching (without blendomatic).
*/
unsigned Terrain::get_subtexture_id(coord::tile pos, unsigned atlas_size, coord::chunk chunk_pos) {
	unsigned result = 0;
	pos = chunk_pos.to_tile(pos.get_pos_on_chunk());

	result += util::mod<coord::tile_t>(pos.se, atlas_size);
	result *= atlas_size;
	result += util::mod<coord::tile_t>(pos.ne, atlas_size);

	return result;
}


/**
get the adjacent chunk neighbors.

chunk neighbor ids:
      0   / <- ne
    7   1
  6   @   2
    5   3
      4   \ <- se

   ne se
0:  1 -1
1:  1  0
2:  1  1
3:  0  1
4: -1  1
5: -1  0
6: -1 -1
7:  0 -1

*/
struct chunk_neighbors Terrain::get_chunk_neighbors(coord::chunk position) {
	struct chunk_neighbors ret;
	coord::chunk tmp_pos;

	//TODO: use chunk_delta
	constexpr int neighbor_pos_delta[8][2] = {
		{  1, -1},
		{  1,  0},
		{  1,  1},
		{  0,  1},
		{ -1,  1},
		{ -1,  0},
		{ -1, -1},
		{  0, -1},
	};

	for (int i = 0; i < 8; i++) {
		tmp_pos = position;
		//TODO: use the overloaded operators..
		tmp_pos.ne += neighbor_pos_delta[i][0];
		tmp_pos.se += neighbor_pos_delta[i][1];
		ret.neighbor[i] = this->get_chunk(tmp_pos);
	}

	return ret;
}

/**
return the blending mode id for two given neighbor ids.
*/
int Terrain::get_blending_mode(size_t base_id, size_t neighbor_id) {
	int base_mode     = this->blendmode(base_id);
	int neighbor_mode = this->blendmode(neighbor_id);
	if (neighbor_mode > base_mode) {
		return neighbor_mode;
	} else {
		return base_mode;
	}
}



/**
parse and store a given line of a texture meta file.

this is used for reading all the lines of a .docx meta file
generated by the convert script.
*/
int terrain_type::fill(const char *by_line) {
	if (sscanf(by_line, "%u=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
	           &this->id,
	           &this->terrain_id,
	           &this->slp_id,
	           &this->sound_id,
	           &this->blend_mode,
	           &this->blend_priority,
	           &this->angle_count,
	           &this->frame_count,
	           &this->terrain_dimensions0,
	           &this->terrain_dimensions1,
	           &this->terrain_replacement_id
	           )) {
		return 0;
	}
	else {
		return -1;
	}
}

/**
parse and store a blending mode description line.
*/
int blending_mode::fill(const char *by_line) {
	if (sscanf(by_line, "%u=%d",
	           &this->id,
	           &this->mode_id
	           )) {
		return 0;
	}
	else {
		return -1;
	}
}


} //namespace engine
