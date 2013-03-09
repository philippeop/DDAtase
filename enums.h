#ifndef _ENUMS_H_
#define _ENUMS_H_

#ifndef sgn
#define sgn(x) (((x) < 0) ? -1 : 1)
#endif

enum material {
MNULL = 0,
//Food Materials
LIQUID, VEGGY, FLESH, POWDER,
//Clothing
COTTON, WOOL, LEATHER, KEVLAR,
//Other
STONE, PAPER, WOOD, PLASTIC, GLASS, IRON, STEEL, SILVER
};

struct point {
 int x;
 int y;
 point(int X = 0, int Y = 0) : x (X), y (Y) {}
 point(const point &p) : x (p.x), y (p.y) {}
 ~point(){}
};

struct tripoint {
 int x;
 int y;
 int z;
 tripoint(int X = 0, int Y = 0, int Z = 0) : x (X), y (Y), z (Z) {}
 tripoint(const tripoint &p) : x (p.x), y (p.y), z (p.z) {}
 ~tripoint(){}
};

////////TILES STUFF
// if changing type, check
// map::ter_conf, map::set_ter_conf, complex_tile::draw functions,
// they're working with bits
typedef unsigned short t_feature;

enum ter_config
{
    tcf_w = 0x1,
    tcf_e = 0x2,
    tcf_n = 0x4,
    tcf_s = 0x8,
    tcf_uw = tcf_e | tcf_n | tcf_s,
    tcf_ue = tcf_w | tcf_n | tcf_s,
    tcf_un = tcf_e | tcf_w | tcf_s,
    tcf_us = tcf_e | tcf_n | tcf_w
};

enum ter_config_diag
{
    tcf_nw = 0x10,
    tcf_ne = 0x20,
    tcf_sw = 0x40,
    tcf_se = 0x80
};

#endif
