#ifndef _CATATILES_H_
#define _CATATILES_H_

#ifdef TILES

#include <vector>
#include <string>
#include <map>
#include "enums.h"
#include "catatex.h"

//----------------------------------------------------------------------------

class tile_layer_info
{
public:
    int id;             // number of tile in cata_tiles full list
    unsigned long var;            // number of tile variations, that follow immediatly after id
    std::vector<unsigned long> argb; // color effect variations
    bool mono;          // monochromise tile
    tile_layer_info ();
    tile_layer_info (int aid, unsigned long avar, bool amono);
};

//----------------------------------------------------------------------------

class complex_tile
{
private:
public:
    std::vector<tile_layer_info> layers;
    std::string name;   // name of complex tile
    bool add_layer (tile_layer_info &layer);
    void draw (int x, int y, t_feature feature = 0, unsigned long tint = 0xffffffff, bool mono = false);
};

//----------------------------------------------------------------------------

class tile_file_info
{
public:
    std::string alias;      // tileset file alias. i.e  mon=monsters.tga <- alias=filename.tga
    std::string filename;   // tileset filename
    Texture *texture;       // loaded texure
    int num_start;          // number of first tile in full tiles id list
    int num_tiles;          // number of tiles in file
};

//----------------------------------------------------------------------------

struct cid_with_backup
{
    int cid;
    int bk_attrs;
    int bk_sym;
    bool bk_use_bg;
};

typedef std::vector<cid_with_backup> cid_with_backup_vector;
class game;
class map;

//----------------------------------------------------------------------------

class cata_tiles
{
private:
    std::vector<tile_file_info> files;      // loaded files info
    std::vector<int> id_to_file;            // table of fast file lookup based on id
    std::vector<complex_tile> complex_tiles;  // list of tiles with their effects
public:
    std::string backup_font_name;
    std::string error_text;
    int error_line;
    Texture backup_font;
    int width;
    int height;

    // indexed lists of game entities tied to corresponding complex tiles/backup display info
    // when adding another index, do not forget to include it into "init" and "clear" functions
    cid_with_backup_vector special_cid;
    cid_with_backup_vector terrain_cid;
    cid_with_backup_vector monster_cid;
    cid_with_backup_vector item_cid;
    cid_with_backup_vector trap_cid;
    cid_with_backup_vector field_cid;
    cid_with_backup_vector worn_cid;

    // overlaps index: this one is special,
    // it contains tolc_total entries for one terrain
    // listed in over_terrains. total
    // number of elements = over_terrains.size() * tolc_total;
    cid_with_backup_vector overlap_cid;
    // same as above, the size is num_fields * tolc_total
    cid_with_backup_vector fieldlap_cid;

    // terrain configurations go here, special, too
    cid_with_backup_vector terrain_conf_cid;

    // a map to track a number of add_named_cid accesses to each name
    std::map<std::string, int> named_cid_numbers;
    // a list of terrains which can be overlapped (base terrains)
    // in the order of overlapping
    std::vector<int> over_terrains;
	// as above, but ingame names. used at catatiles initialization
    std::vector<std::string> over_terrain_names;

    cata_tiles ();
    ~cata_tiles ();
    bool init (game *g);
    void clear ();
    bool load_file (std::string alias, std::string fname);
    void add_complex_tile (complex_tile &ct);
    bool parse_line (std::string line, bool preinit = false);
    bool read_config (const char *fname, bool preinit = false);

    int tile_id (std::string alias, int num);

    // get id of the complex tile (cid) for given name. count is number of
    // names to skip (if there are several same names exist)
    int complex_id (std::string name, int count = 0);

    // adds a record in the the given list of cid
    int add_named_cid (cid_with_backup_vector &vec, const char *prefix, std::string name, int bk_sym, int bk_attrs, bool bk_use_bg = true);

    // draws a tile with given id (get id by tile_id())
    void draw (int x, int y, int id, unsigned long argb = 0xffffffff, bool mono = false);

    // draws a complex tile with given complex id (get it by complex_id)
    void draw_complex (int x, int y, int cid, t_feature feature = 0, unsigned long tint = 0xffffffff, bool mono = false);

    // draws a symbol using backup font (if there's no tile)
    void draw_backup (int x, int y, int attrs, int sym, bool bg = true, unsigned long tint = 0xffffffff, bool mono = false);

    // draws a complex tile from given cid list
    void draw_cid (int x, int y, int num, cid_with_backup_vector &vec,t_feature feature = 0, unsigned long tint = 0xffffffff, bool mono = false);

    // draws overlaps for one tile
    // x, y -- posiotion on screen in pixels
    // mx, my -- map coordinates
    void draw_overlaps (int x, int y, map *m, int mx, int my, unsigned long tint = 0xffffffff, bool mono = false);

    // draws terrain (using proper ter_confs)
    void draw_terrain (int x, int y, map *m, int mx, int my, bool base = false, unsigned long tint = 0xffffffff, bool mono = false);

    void draw_fieldlaps (int x, int y, map *m, int mx, int my, unsigned long tint = 0xffffffff, bool mono = false);

    // check, if terrain t1 should connect to terrain t2 (using ter_conf_omni)
    bool is_ter_connects (int t1, int t2);
};

//----------------------------------------------------------------------------

enum special_cids
{
    scid_you,
    scid_you_female,
    scid_npc,
    scid_npc_female,
    scid_select,
    scid_aim,
    scid_fog,
    scid_bulb,
    scid_bullet,
    scid_flame,

    scid_total
};

//----------------------------------------------------------------------------

extern cata_tiles tiles;

//----------------------------------------------------------------------------

#endif

#endif

