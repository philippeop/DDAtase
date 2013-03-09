/*#ifndef _CATACURSES_H_
#define _CATACURSES_H_

#if (defined TILES || defined GFX_GL)
    #include "catacursegl.h"
    #ifdef TILES
        #include "catatiles.h"
    #endif
#else
    #if (defined _WIN32 || defined WINDOWS)
        #include "catacurse.h"
    #else
        #include <curses.h>
    #endif
#endif

class TerminationException
{
};

#endif*/
