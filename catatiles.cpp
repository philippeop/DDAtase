#ifdef TILES

#include <string>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include "catacurses.h"
#include "color.h" // for init_special_tiles
#include "game.h"

//----------------------------------------------------------------------------

static const struct special_tile_info
{
    int sym;
    int color;
    const char *name;
    bool bg;
} special_info [scid_total] =
{
    { '@', c_white,     "you", false },
    { '@', c_white,     "you female", false },
    { '@', c_magenta,   "npc", false },
    { '@', c_magenta,   "npc female", false },
    { '+', c_yellow,    "selection marker", false },
    { '*', c_red,       "aim marker", false },
    { '#', c_dkgray,    "fog of war", true },
    { '\'', c_white,    "bulb", false },
    { '*', c_red,       "bullet", false },
    { '#', c_red,       "flame", false }
};

//----------------------------------------------------------------------------

enum tile_overlap_conf
{
    tolc_w,
    tolc_n,
    tolc_e,
    tolc_s,
    tolc_nw,
    tolc_ne,
    tolc_se,
    tolc_sw,
    tolc_uw,
    tolc_un,
    tolc_ue,
    tolc_us,
    tolc_all,
    tolc_cnw,
    tolc_cne,
    tolc_cse,
    tolc_csw,

    tolc_total
};

// see usage
static const struct overlap_tile_info
{
    const char *name;
    int pattern;
    int mask;
} overlap_info[tolc_total] =
{
    { "w",      tcf_w,    tcf_w | tcf_n | tcf_s },
    { "n",      tcf_n,    tcf_n | tcf_w | tcf_e },
    { "e",      tcf_e,    tcf_e | tcf_n | tcf_s },
    { "s",      tcf_s,    tcf_s | tcf_w | tcf_e },
    { "nw",     tcf_n | tcf_w,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "ne",     tcf_n | tcf_e,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "se",     tcf_s | tcf_e,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "sw",     tcf_s | tcf_w,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "uw",     tcf_n | tcf_e | tcf_s,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "un",     tcf_e | tcf_w | tcf_s,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "ue",     tcf_n | tcf_w | tcf_s,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "us",     tcf_n | tcf_w | tcf_e,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "all",    tcf_n | tcf_w | tcf_s | tcf_e,    tcf_n | tcf_w | tcf_s | tcf_e },
    { "cnw",    tcf_nw,      tcf_w | tcf_nw | tcf_n },
    { "cne",    tcf_ne,      tcf_e | tcf_ne | tcf_n },
    { "cse",    tcf_se,      tcf_e | tcf_se | tcf_s },
    { "csw",    tcf_sw,      tcf_w | tcf_sw | tcf_s }
};

static const char *ter_conf_names[16] =
{
    "", "w", "e", "we", "n", "nw", "ne", "us",
    "s", "sw", "se", "un", "ns", "ue", "uw", "all"
};

//----------------------------------------------------------------------------

cata_tiles tiles;

//----------------------------------------------------------------------------

static char lowercase (char a)
{
    if (a >= 'A' && a <= 'Z')
        return a - 'A' + 'a';
    return a;
}

//----------------------------------------------------------------------------

static bool is_names_equal (std::string &str1, const char *str2)
{
    int len = str1.length();
    if (len != strlen(str2))
        return false;
    for (int i = 0; i < len; i++)
    {
        if (lowercase (str1[i]) != lowercase (str2[i]))
            return false;
    }
    return true;
}

//----------------------------------------------------------------------------

static unsigned long str_to_argb (std::string col)
{
    unsigned long res = 0;
    for (int i = 0; i < col.length() && i < 8; i++)
    {
        char c = lowercase (col[i]);
        unsigned long num;
        if (c >= 'a' && c <= 'f')
            num = c - 'a' + 10;
        else
        if (c >= 'A' && c <= 'F')
            num = c - 'A' + 10;
        else
        if (c >= '0' && c <= '9')
            num = c - '0';
        else
            return 0xffffffff;
        res = res * 16 + num;
    }
    return res;
}

//----------------------------------------------------------------------------

tile_layer_info::tile_layer_info ()
{
    id = -1;
    var = 0;
    mono = false;
}

//----------------------------------------------------------------------------

tile_layer_info::tile_layer_info (int aid, unsigned long avar, bool amono):
    id (aid), var (avar), mono (amono)
{
}

//----------------------------------------------------------------------------

bool complex_tile::add_layer (tile_layer_info &layer)
{
    layers.push_back (layer);
}

//----------------------------------------------------------------------------

void complex_tile::draw (int x, int y, t_feature feature, unsigned long tint, bool mono)
{
    for (int i = 0; i < layers.size(); i++)
    {
        unsigned int argb = 0xffffffff;
        if (layers[i].argb.size() > 0)
        {
            unsigned long fcol = ((((unsigned long)feature) & 0x555555) << 1) | ((((unsigned long)feature) & 0xaaaaaa) >> 1);
            argb = layers[i].argb[fcol % layers[i].argb.size()];
        }
        if (tint != 0xffffffff)
            argb = (argb & 0xff000000) | (tint & 0xffffff);
        tiles.draw (x, y, layers[i].id + (layers[i].var > 0? (feature % (layers[i].var + 1)) : 0),
                                           argb, layers[i].mono || mono);
    }
}

//----------------------------------------------------------------------------

cata_tiles::cata_tiles ()
{
    width = 0;
    height = 0;
}

//----------------------------------------------------------------------------

cata_tiles::~cata_tiles ()
{
    clear ();
}

//----------------------------------------------------------------------------

void cata_tiles::clear ()
{
    for (int i = 0; i < files.size(); i++)
        if (files[i].texture)
        {
            delete files[i].texture;
            files[i].texture = 0;
        }
    files.clear ();
    id_to_file.clear ();
    complex_tiles.clear ();
    named_cid_numbers.clear();
    special_cid.clear ();
    terrain_cid.clear ();
    terrain_conf_cid.clear ();
    monster_cid.clear ();
    item_cid.clear ();
    trap_cid.clear ();
    field_cid.clear ();
    worn_cid.clear ();
    overlap_cid.clear ();
    fieldlap_cid.clear();
    backup_font.clear ();
    over_terrains.clear ();
    over_terrain_names.clear ();
}

//----------------------------------------------------------------------------

bool cata_tiles::init (game *g)
{
    clear ();

    if (!read_config ("data/graphics.cfg", false))
    {
        debugmsg ("Error reading config file, line %d: %s\n", error_line, error_text.c_str());
        return false;
    }

    bool print_list = false;
    std::ofstream fout;
    if (print_list)
    {
        fout.open("data/graphics_template");
        fout << "# Tilesets:" << std::endl;
        fout << "# Specials" << std::endl;
        fout << "spc=.tga" << std::endl;
        fout << "# Monsters" << std::endl;
        fout << "mon=.tga" << std::endl;
        fout << "# Items" << std::endl;
        fout << "itm=.tga" << std::endl;
        fout << "# Terrain" << std::endl;
        fout << "ter=.tga" << std::endl;
        fout << "# Terrain overlaps" << std::endl;
        fout << "ovl=.tga" << std::endl;
        fout << "# Traps" << std::endl;
        fout << "trp=.tga" << std::endl;
        fout << "# Fields" << std::endl;
        fout << "fld=.tga" << std::endl;
    }

	// list of terrains which could overlap others and be overlapped
	for (int i = 0; i < over_terrain_names.size(); i++)
		for (int j = 0; j < num_terrain_types; j++)
			if (over_terrain_names[i] == terlist[j].name)
			{
				over_terrains.push_back (j);
			}

				if (print_list)
        fout << std::endl << "# Specials:" << std::endl;
    for (int i = 0; i < scid_total; i++)
    {
        add_named_cid (special_cid,
            "special", std::string(special_info[i].name),
            special_info[i].sym,
            special_info[i].color,
            special_info[i].bg);
        if (print_list)
            fout << "special:" << std::string(special_info[i].name) << "=spc:" << i << std::endl;
    }

    if (print_list)
        fout << std::endl << "# Monsters:" << std::endl;
    for (int i = 0; i < g->mtypes.size(); i++)
    {
        add_named_cid (monster_cid, "monster", g->mtypes[i]->name, g->mtypes[i]->sym, g->mtypes[i]->color, false);
        if (print_list)
            fout << "monster:" << g->mtypes[i]->name << "=mon:" << i << std::endl;
    }

    if (print_list)
        fout << std::endl << "# Items:" << std::endl;
    int cnt = 0;
    for (int i = 0; i < g->itypes.size(); i++)
    {
        add_named_cid (item_cid, "item", g->itypes[i]->name, g->itypes[i]->sym, g->itypes[i]->color, false);
        add_named_cid (worn_cid, "worn", g->itypes[i]->name, g->itypes[i]->sym, g->itypes[i]->color, false);
        if (print_list && i > itm_toolset && i < num_items)
        {
            fout << "item:" << g->itypes[i]->name << "=itm:" << cnt << std::endl;
            cnt++;
        }
    }

    if (print_list)
        fout << std::endl << "# Terrain:" << std::endl;
    for (int i = 0; i < num_terrain_types; i++)
    {
        add_named_cid (terrain_cid, "terrain", terlist[i].name, terlist[i].sym, terlist[i].color, true);
        if (print_list && i)
            fout << "terrain:" << terlist[i].name << "=ter:" << (i-1) << std::endl;
        // terrain configurations
        for (int j = 1; j < 16; j++)
            add_named_cid (terrain_conf_cid, "terrain", std::string(ter_conf_names[j]) + ":" + terlist[i].name,
                           terlist[i].sym, terlist[i].color, true);
    }

    if (print_list)
        fout << std::endl << "# Overlaps:" << std::endl;
    for (int i = 0; i < over_terrains.size(); i++)
    {
        for (int j = 0; j < tolc_total; j++)
        {
            std::string onm =
                    std::string (overlap_info[j].name) + ":" +
                    terlist[over_terrains[i]].name;
            add_named_cid (overlap_cid, "overlap", onm, 0, 0, false);
            if (print_list && i > 0)
                fout << "overlap:" << onm << "=ovl:" << ((i-1) * tolc_total + j) << std::endl;
        }
    }

    if (print_list)
        fout << std::endl << "# Traps:" << std::endl;
    for (int i = 0; i < g->traps.size(); i++)
    {
        add_named_cid (trap_cid, "trap", g->traps[i]->name, g->traps[i]->sym, g->traps[i]->color, false);
        if (print_list && i)
            fout << "trap:" << g->traps[i]->name << "=trp:" << (i-1) << std::endl;
    }

    if (print_list)
        fout << std::endl << "# Fields:" << std::endl;
    char ss[64];
    for (int i = 0; i < num_fields; i++)
    {
        if (print_list && i)
            fout << "# " << fieldlist[i].name[0] << ", " << fieldlist[i].name[1] << ", " << fieldlist[i].name[2] << std::endl;
        for (int j = 0; j < 3; j++)
        {
            sprintf (ss, "%d", i);
            add_named_cid (field_cid, "field", ss,
                                 fieldlist[i].sym, fieldlist[i].color[j], !fieldlist[i].transparent[j]);
            if (print_list && i)
                fout << "field:" << std::string(ss) << "=fld:" << ((i-1)*3 + j) << std::endl;
            // fieldlaps
            for (int k = 0; k < tolc_total; k++)
            {
                std::string flnm =
                    std::string (overlap_info[k].name) + ":" + ss;
                add_named_cid (fieldlap_cid, "fieldlap", flnm, 0, 0, false);
            }

        }
    }

    if (print_list)
        fout.close();

    return true;
}

//----------------------------------------------------------------------------

bool cata_tiles::load_file (std::string alias, std::string fname)
{
    Texture *tex = new Texture;
    if (!tex || !tex->setup_symbols (fname.c_str(), width, height, true))
        return false;
    tile_file_info finf;
    finf.alias = alias;
    finf.filename = fname;
    finf.texture = tex;
    finf.num_start = id_to_file.size();
    finf.num_tiles = tex->symbols_num();
    for (int i = 0; i < finf.num_tiles; i++)
        id_to_file.push_back (files.size());
    files.push_back (finf);
    return true;
}

//----------------------------------------------------------------------------

extern std::string font_name;

bool cata_tiles::parse_line (std::string line, bool preinit)
{
    error_text.clear();
    if (line.length() <= 0)
        return true;
    if (line[0] == '#')
        return true;

    int neq = line.find ('=');
    if (neq == std::string::npos)
    {
        error_text = "missing '=' token";
        return false;
    }
    std::string key = line.substr (0, neq);
    std::string val = line.substr (neq + 1, line.length() - (neq + 1));

    if (is_names_equal (key, "tile_width"))
    {
        if (preinit)
            width = atoi (val.c_str());
        return true;
    }
    else
    if (is_names_equal (key, "tile_height"))
    {
        if (preinit)
            height = atoi (val.c_str());
        return true;
    }
    else
    if (is_names_equal (key, "font"))
    {
        if (preinit)
            font_name = std::string("data/") + val;
        return true;
    }
    if (preinit)
        return true;

    if (is_names_equal (key, "backup_font"))
    {
        backup_font_name = std::string("data/") + val;
        backup_font.setup_symbols (backup_font_name.c_str());//, width, height);
        return true;
    }
	else
    if (is_names_equal (key, "overlap_list"))
	{
		over_terrain_names.push_back (val);
		return true;
	}

    int ntga = val.rfind (".tga");
    if (ntga == val.length() - 4)
    { // alias = filename case
        bool load_ok = load_file (key, std::string("data/") + val);
        if (!load_ok)
        {
            error_text = "error loading file ";
            error_text += val;
            return false;
        }
        else
            return true;
    }

    std::vector<std::string> vlist;
    int nspc = 0, nnext = 0;
    while (std::string::npos != (nspc = val.find (' ', nnext)))
    {
        vlist.push_back (val.substr(nnext, nspc - nnext));
        nnext = nspc + 1;
    }
    vlist.push_back (val.substr(nnext, val.length() - nnext));

    complex_tile ct;
    ct.name = key;
    for (int i = 0; i < vlist.size(); i++)
    {
        int nsc = vlist[i].find (':');
        if (nsc == std::string::npos)
        {
            error_text = "missing ':' token";
            return false;
        }
        std::string alias = vlist[i].substr (0, nsc);
        std::string left = vlist[i].substr (nsc + 1, line.length() - (nsc + 1));
        int ncl1 = left.find ('&');
        int ncl2 = left.find ('*');
        int mcl = 0;
        if (ncl1 != std::string::npos)
            mcl += 1;
        if (ncl2 != std::string::npos)
            mcl += 2;
        if (mcl > 2)
        {
            error_text = "both '&' and '*' tokens in same expression";
            return false;
        }
        bool mono = false;
        int var = 0;
        std::vector<unsigned long> argbl;
        if (mcl)
        {
            if (mcl == 2)
                ncl1 = ncl2;
            std::string col = left.substr (ncl1 + 1, left.length() - (ncl1 + 1));

            std::vector<std::string> clist;
            nspc = 0;
            nnext = 0;
            while (std::string::npos != (nspc = col.find ('+', nnext)))
            {
                clist.push_back (col.substr(nnext, nspc - nnext));
                nnext = nspc + 1;
            }
            clist.push_back (col.substr(nnext, val.length() - nnext));

            for (int i = 0; i < clist.size(); i++)
            {
                unsigned long argb = str_to_argb (clist[i]);
                argbl.push_back (argb);
            }
            mono = mcl == 1;
            left = left.substr (0, ncl1);
        }
        int nadd = left.find ('+');
        if (nadd != std::string::npos)
        {
            var = atoi (left.substr (nadd + 1, left.length() - (nadd + 1)).c_str());
            left = left.substr (0, nadd);
        }
        int num = atoi (left.c_str());
        int id = tile_id (alias, num);
        if (id < 0)
        {
            error_text = "requested tile number in given tileset not found";
            return false;
        }
        tile_layer_info layer (id, var, mono);
        layer.argb = argbl;
        ct.add_layer (layer);
    }
    add_complex_tile (ct);
    return true;
}

//----------------------------------------------------------------------------

bool cata_tiles::read_config (const char *fname, bool preinit)
{
    std::ifstream confin;
    confin.open (fname);
    error_line = -1;
    if (!confin.is_open())
    {
        error_text = "opening config file ";
        error_text += fname;
        return false;
    }
    std::string line;
    while (!confin.eof())
    {
        error_line++;
        getline (confin, line);
        if (!parse_line (line, preinit))
            return false;
    }
    return true;
}

//----------------------------------------------------------------------------

void cata_tiles::add_complex_tile (complex_tile &ct)
{
    complex_tiles.push_back (ct);
}

//----------------------------------------------------------------------------

int cata_tiles::add_named_cid (cid_with_backup_vector &vec, const char *prefix,
                               std::string name, int bk_sym, int bk_attrs, bool bk_use_bg)
{
    cid_with_backup rec;
    std::string prefname(prefix);
    if (prefname.length())
        prefname += std::string (":") + name;
    else
        prefname = name;
    int num = named_cid_numbers[prefname];
    rec.cid = complex_id (prefname, num);
    named_cid_numbers[prefname] = ++num;
    rec.bk_attrs = bk_attrs;
    rec.bk_sym = bk_sym;
    rec.bk_use_bg = bk_use_bg;
    vec.push_back (rec);
}

//----------------------------------------------------------------------------

int cata_tiles::tile_id (std::string alias, int num)
{
    for (int i = 0; i < files.size(); i++)
        if (is_names_equal (files[i].alias, alias.c_str()))
            return files[i].num_start + num;
    return -1;
}

//----------------------------------------------------------------------------

int cata_tiles::complex_id (std::string name, int count)
{
    int num = 0;
    for (int i = 0; i < complex_tiles.size(); i++)
        if (is_names_equal(complex_tiles[i].name, name.c_str()))
        {
            if (num < count)
                num++;
            else
                return i;
        }
    return -1;
}

//----------------------------------------------------------------------------

void cata_tiles::draw (int x, int y, int id, unsigned long argb, bool mono)
{
    if (id < 0 || id >= id_to_file.size())
        return;
    int fi = id_to_file[id];
    if (!files[fi].texture)
        return;
    gfx_set_color_argb (argb);
    gfx_draw_symbol (*files[fi].texture, x, y, id - files[fi].num_start, true, mono);
}

//----------------------------------------------------------------------------

void cata_tiles::draw_complex (int x, int y, int cid, t_feature feature, unsigned long tint, bool mono)
{
    if (cid < 0 || cid >= complex_tiles.size ())
        return;
    complex_tiles[cid].draw (x, y, feature, tint, mono);
}

//----------------------------------------------------------------------------

void cata_tiles::draw_backup (int x, int y, int attrs, int sym, bool bg, unsigned long tint, bool mono)
{
    if (!backup_font.ok())
        return;

    if (bg)
    {
        //gfx_set_color_bg(attrs);
        gfx_set_color_argb(0);
        gfx_draw_rectangle (x, y, width, height);
    }
    if (tint != 0xffffffff)
        gfx_set_color_argb (tint);
    else
        gfx_set_color_fg(attrs);
    if (sym >= 4194410 && sym <= 4194424)
        sym = (sym - 4194410) + 1;
    backup_font.draw_symbol (x, y, sym);
}

//----------------------------------------------------------------------------

void cata_tiles::draw_cid (int x, int y, int num, cid_with_backup_vector &vec, t_feature feature, unsigned long tint, bool mono)
{
    if (num < 0 || num >= vec.size())
        return;
    if (vec[num].cid < 0)
        draw_backup (x, y, vec[num].bk_attrs, vec[num].bk_sym, vec[num].bk_use_bg, tint, mono);
    else
        draw_complex (x, y, vec[num].cid, feature, tint, mono);
}

//----------------------------------------------------------------------------

void cata_tiles::draw_terrain (int x, int y, map *m, int mx, int my, bool base, unsigned long tint, bool mono)
{
    ter_id t = m->ter(mx, my);
    if (base)
        t = terlist[t].base_terrain;
    t_feature ft = m->ter_feature (mx, my);
    int cfg = m->ter_conf (mx, my) & 0xf;
    int num = t * 15 + cfg - 1;
    if (cfg > 0 && num >= 0 && num < terrain_conf_cid.size())
    {
        if (terrain_conf_cid[num].cid < 0)
        {
            // if we don't have needed conf tile, but have a w-e, then use it
            // unless that's vertical conf and we have a n-s
            if (((cfg == tcf_e) || (cfg == tcf_w) || (cfg == tcf_w | tcf_e)) &&
                terrain_conf_cid[t * 15 + (tcf_n | tcf_s) - 1].cid >= 0)
                cfg = tcf_n | tcf_s;
            else
            if (terrain_conf_cid[t * 15 + (tcf_e | tcf_w) - 1].cid >= 0)
                cfg = tcf_e | tcf_w;
        }
        num = t * 15 + cfg - 1;
        if (terrain_conf_cid[num].cid > 0)
        {
            draw_cid (x, y, num, terrain_conf_cid, ft, tint, mono);
            return;
        }
    }
    if (terrain_cid[t].cid < 0) // a little hack to display correct walls
        draw_backup (x, y, terrain_cid[t].bk_attrs, m->ter_sym(mx, my), terrain_cid[t].bk_use_bg, tint, mono);
    else
        draw_cid (x, y, t, terrain_cid, ft, tint, mono);
}

//----------------------------------------------------------------------------

void cata_tiles::draw_overlaps (int x, int y, map *m, int mx, int my, unsigned long tint, bool mono)
{
    //       x  y
    ter_id t[3][3]; // nearby tiles
    int   ot[3][3]; // over_terrains index of nearby tiles ground
    int   ot1[3][3]; // over_terrains index of nearby tiles terrain objects: trees, etc. (what's not ground)
    for (int iy = 0; iy < 3; iy++)
        for (int ix = 0; ix < 3; ix++)
        {
            t[ix][iy] = m->ter(mx + ix - 1, my + iy - 1);
            ot[ix][iy] = -1;
            ot1[ix][iy] = -1;
            for (int i = 0; i < over_terrains.size(); i++)
            {
                if (over_terrains[i] == terlist[t[ix][iy]].base_terrain)
                    ot[ix][iy] = i;
                if (over_terrains[i] == t[ix][iy])
                    ot1[ix][iy] = i;
            }
        }
    // if central square can't be overlapped, we're done
    if (ot[1][1] < 0)
        return;
    // here's how patterns and masks work:
    // every direction is assigned a bit
    // bits:
    //  1  2  3  +-> X
    //  0     4  |
    //  7  6  5  v Y
    // first, we construct terrain bits layout
    // which has 1's set for every direction where
    // terrain of interest (the one which overlaps our current tile) is.
    // then we seek what overlap_info number it matches
    // by applying info mask on terrain bits and
    // comparing it to info pattern.
    // we have to draw all overmaps that match.
    for (int i = 0; i < over_terrains.size(); i++)
    {
        // overlap only if terrain of interest is lower in list than
        // terrain of central tile
        if (ot[1][1] >= i)
            continue;
        int ter_bits = 0;
        if (ot[0][1] == i) ter_bits |= tcf_w;
        if (ot[0][0] == i) ter_bits |= tcf_nw;
        if (ot[1][0] == i) ter_bits |= tcf_n;
        if (ot[2][0] == i) ter_bits |= tcf_ne;
        if (ot[2][1] == i) ter_bits |= tcf_e;
        if (ot[2][2] == i) ter_bits |= tcf_se;
        if (ot[1][2] == i) ter_bits |= tcf_s;
        if (ot[0][2] == i) ter_bits |= tcf_sw;
        for (int j = 0; j < tolc_total; j++)
        {
            int num = i * tolc_total + j;
            if (((ter_bits & overlap_info[j].mask) == overlap_info[j].pattern) &&
                overlap_cid[num].cid >= 0)
                draw_cid (x, y, num, overlap_cid, m->ter_feature(mx, my), tint, mono);

        }
    }
    // and another loop for non-ground terrain
    for (int i = 0; i < over_terrains.size(); i++)
    {
        // overlap only if terrain of interest is lower in list than
        // terrain of central tile
        if (ot1[1][1] >= i)
            continue;
        int ter_bits = 0;
        if (ot1[0][1] == i) ter_bits |= tcf_w;
        if (ot1[0][0] == i) ter_bits |= tcf_nw;
        if (ot1[1][0] == i) ter_bits |= tcf_n;
        if (ot1[2][0] == i) ter_bits |= tcf_ne;
        if (ot1[2][1] == i) ter_bits |= tcf_e;
        if (ot1[2][2] == i) ter_bits |= tcf_se;
        if (ot1[1][2] == i) ter_bits |= tcf_s;
        if (ot1[0][2] == i) ter_bits |= tcf_sw;
        for (int j = 0; j < tolc_total; j++)
        {
            int num = i * tolc_total + j;
            if (((ter_bits & overlap_info[j].mask) == overlap_info[j].pattern) &&
                overlap_cid[num].cid >= 0)
                draw_cid (x, y, num, overlap_cid, m->ter_feature(mx, my), tint, mono);

        }
    }
}

//----------------------------------------------------------------------------

void cata_tiles::draw_fieldlaps (int x, int y, map *m, int mx, int my, unsigned long tint, bool mono)
{
    //       x  y
    int ot[3][3]; // nearby fields types * 3 + density
    for (int iy = 0; iy < 3; iy++)
        for (int ix = 0; ix < 3; ix++)
        {
            field fld = m->field_at (mx + ix - 1, my + iy - 1);
            ot[ix][iy] = fld.type * 3 + fld.density-1;
        }

    for (int i = 0; i < num_fields * 3; i++)
    {
        // overlap only if field of interest is lower in list than
        // field of central tile
        if (ot[1][1] >= i)
            continue;
        int ter_bits = 0;
        if (ot[0][1] == i) ter_bits |= tcf_w;
        if (ot[0][0] == i) ter_bits |= tcf_nw;
        if (ot[1][0] == i) ter_bits |= tcf_n;
        if (ot[2][0] == i) ter_bits |= tcf_ne;
        if (ot[2][1] == i) ter_bits |= tcf_e;
        if (ot[2][2] == i) ter_bits |= tcf_se;
        if (ot[1][2] == i) ter_bits |= tcf_s;
        if (ot[0][2] == i) ter_bits |= tcf_sw;
        for (int j = 0; j < tolc_total; j++)
        {
            int num = i * tolc_total + j;
            if (((ter_bits & overlap_info[j].mask) == overlap_info[j].pattern) &&
                fieldlap_cid[num].cid >= 0)
            {
                t_feature ft = fieldlist[i]._volatile? gen_feature() : m->ter_feature(mx, my);
                draw_cid (x, y, num, fieldlap_cid, ft, tint, mono);
            }
        }
    }
}

//----------------------------------------------------------------------------

#endif //TILES

