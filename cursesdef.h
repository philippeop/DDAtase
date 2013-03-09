#ifndef _CURSES_DEF_H_
#define _CURSES_DEF_H_

#if (defined TILES || defined GFX_GL)
    #include "catacursegl.h"
    #ifdef TILES
        #include "catatiles.h"
    #endif
#else
    #if (defined _WIN32 || defined WINDOWS)
        #include "catacurse.h"
    #elif (defined __CYGWIN__)
           #include "ncurses/curses.h"
    #else
        #include <curses.h>
    #endif
#endif

class TerminationException {};

#endif // CURSES_DEF_H
