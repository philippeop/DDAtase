// Gremour
#if (defined TILES || defined GFX_GL)

#ifdef linux
    #define GLX_GLXEXT_PROTOTYPES
    #define GL_GLEXT_PROTOTYPES
    #include <GL/glx.h> // includes gl, glext
    #include <X11/keysym.h>
#else
#if (defined _WIN32 || defined WINDOWS)
    #include <GL/gl.h>
    #include "GL/glext.h"
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctime>
#include <string>
#include <vector>
#include "catatex.h"
#include "catacurses.h"

//----------------------------------------------------------------------------

static std::vector<std::string> extensions;
static GLuint fbo_id = 0;
static GLuint rbo_id = 0;

int window_width = 0;
int window_height = 0;
Texture font;               // textured font for textual display
std::string font_name ("data/catafont.tga");

WINDOW *mainwin = 0;
static int lastchar = 0;
static int inputdelay = -1;
pairs *colorpairs;          //storage for pair'ed colored, should be dynamic, meh

struct t_rgb_triplet
{
    unsigned char rgb[3];
    t_rgb_triplet () {};
    t_rgb_triplet (unsigned char r, unsigned char g, unsigned char b)
    { rgb[0] = r; rgb[1] = g; rgb[2] = b; }
};

struct t_rgb_triplet *console_palette = 0;
static const int max_color_pairs = 200;

#ifdef linux
static Display *display;
static GLXContext context;
static XSetWindowAttributes winattrs;
static Window window;
static bool key_found = false;

#else
#if (defined _WIN32 || defined WINDOWS)
static const WCHAR *szWindowClass = (L"CataCurseWindow");    //Class name :D
static HINSTANCE WindowINST;   //the instance of the window
static HWND WindowHandle;      //the handle of the window
static HDC WindowDC;           //Device Context of the window, used for backbuffer
static char szDirectory[MAX_PATH] = "";
static HGLRC context;
#endif
#endif
static bool mapped = false;
static bool destroyed = false;

#ifdef linux
//***********************************
// Xwindow-specific Functions       *
//***********************************

//----------------------------------------------------------------------------

static void process_events ()
{
    XEvent report;
    KeySym keysym;
    XComposeStatus compose;
    key_found = false;

    while (XCheckMaskEvent(display, winattrs.event_mask, &report))
        switch  (report.type)
        {
        case Expose:            // Need redraw
            if (report.xexpose.count)
                break;          // Skip intermediate exposures
            gfx_refresh_screen ();
            break;
        case ConfigureNotify:   // New window size
//            resize_window (report.xconfigure.width, report.xconfigure.height);
            break;
        case KeyPress:
            XLookupString(&report.xkey, NULL, 0, &keysym, &compose);
            if (keysym == XK_Shift_L || keysym == XK_Shift_R ||
                keysym == XK_Control_L || keysym == XK_Control_R ||
                keysym == XK_Alt_L || keysym == XK_Alt_R)
                break;
            lastchar = keysym;
            key_found = true;

            break;
        case KeyRelease:
//            XLookupString(&report.xkey, NULL, 0, &keysym, &compose);
            break;
        case MappingNotify:
            XRefreshKeyboardMapping(&report.xmapping);
            break;
        case MapNotify:         // Window is now mapped
            XSetInputFocus(display, window, RevertToParent, CurrentTime);
            mapped = true;
            break;
        case UnmapNotify:       // Window is now unmapped
            mapped = false;
            break;
        case DestroyNotify:
            destroyed = true;
            break;
        default:
            break;
        }
    if (destroyed)
        throw TerminationException();
}

//----------------------------------------------------------------------------

static bool init_window (int width, int height)
{
    int attributes[] =
    {
      GLX_RGBA,
      GLX_DOUBLEBUFFER,
      GLX_RED_SIZE, 8,
      GLX_GREEN_SIZE, 8,
      GLX_BLUE_SIZE, 8,
      GLX_DEPTH_SIZE, 16,
      0
    };
    display = XOpenDisplay(0);
    if (!display)
    {
        printf ("Fatal error: can't open display.\n");
        return false;
    }
    XVisualInfo *vinfo = glXChooseVisual(display, DefaultScreen(display), attributes);
    if (!vinfo)
    {
        printf ("Fatal error: can't get visual.\n");
        return false;
    }
    context = glXCreateContext(display, vinfo, 0, GL_TRUE);
    window_width = width;
    window_height = height;

    winattrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                          ButtonPressMask | ButtonReleaseMask | //PointerMotionMask |
                          StructureNotifyMask | SubstructureNotifyMask;
    winattrs.border_pixel = 0;
    winattrs.override_redirect = False;  // Windowed
    int winmask           = CWColormap | CWEventMask |  CWBorderPixel | CWOverrideRedirect;// | CWBitGravity ;

    Window rw = DefaultRootWindow(display);
    winattrs.colormap = XCreateColormap(display, rw, vinfo->visual, AllocNone);
    window = XCreateWindow(display, rw,
                            0, 0, window_width, window_height, 0, vinfo->depth
                            , InputOutput,
                            vinfo->visual
                            ,winmask
                            ,&winattrs);
    XSetStandardProperties(display, window, "Cataclysm", 0, None, 0, 0, 0);
    int rw_w = XDisplayWidth(display, 0);
    int rw_h = XDisplayHeight(display, 0);
    int d_w = (rw_w - window_width) / 2;
    int d_h = (rw_h - window_height) / 2;
    if (d_w < 0) d_w = 0;
    if (d_h < 0) d_h = 0;

    XMapWindow(display, window);
    XMoveWindow (display, window, d_w, d_h);
    glXMakeCurrent(display, window, context);

    mapped = false;
    clock_t map_start = clock ();
    while (!mapped && clock() - map_start < CLOCKS_PER_SEC * 5)
        process_events ();

    if (!mapped)
    {
        printf ("Fatal error: failed to map window.\n");
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------

static void destroy_window ()
{
    glXDestroyContext (display, context);
    XDestroyWindow (display, window);
}

//----------------------------------------------------------------------------
// keyboard handling
//----------------------------------------------------------------------------

typedef struct
{
    int letter;
    int keysym;
} t_keysym_to_char;

//----------------------------------------------------------------------------

static const t_keysym_to_char translate_keys[] =
{
    { '\n', XK_Return },
    { '\n', XK_KP_Enter },
    { 0x08, XK_BackSpace },
    { 0x1b, XK_Escape },
    { 0x09, XK_Tab },

    { '0',  XK_KP_0 },
    { '1',  XK_KP_1 },
    { KEY_DOWN,  XK_KP_2 },
    { '3',  XK_KP_3 },
    { KEY_LEFT,  XK_KP_4 },
    { '5',  XK_KP_5 },
    { KEY_RIGHT,  XK_KP_6 },
    { '7',  XK_KP_7 },
    { KEY_UP,  XK_KP_8 },
    { '9',  XK_KP_9 },
    { '.',  XK_KP_Decimal },
    { '0',  XK_KP_Insert },
    { '1',  XK_KP_End },
    { KEY_DOWN,  XK_KP_Down },
    { '3',  XK_KP_Page_Down },
    { KEY_LEFT,  XK_KP_Left },
    { '5',  XK_KP_Begin },
    { KEY_RIGHT,  XK_KP_Right },
    { '7',  XK_KP_Home },
    { KEY_UP,  XK_KP_Up },
    { '9',  XK_KP_Page_Up },
    { KEY_DOWN,  XK_Down },
    { KEY_LEFT,  XK_Left },
    { KEY_RIGHT, XK_Right },
    { KEY_UP,    XK_Up },

//    { '0',  XK_KP_0 },
//    { '1',  XK_KP_1 },
//    { '2',  XK_KP_2 },
//    { '3',  XK_KP_3 },
//    { '4',  XK_KP_4 },
//    { '5',  XK_KP_5 },
//    { '6',  XK_KP_6 },
//    { '7',  XK_KP_7 },
//    { '8',  XK_KP_8 },
//    { '9',  XK_KP_9 },
//    { '.',  XK_KP_Decimal },
//    { '0',  XK_KP_Insert },
//    { '1',  XK_KP_End },
//    { '2',  XK_KP_Down },
//    { '3',  XK_KP_Page_Down },
//    { '4',  XK_KP_Left },
//    { '5',  XK_KP_Begin },
//    { '6',  XK_KP_Right },
//    { '7',  XK_KP_Home },
//    { '8',  XK_KP_Up },
//    { '9',  XK_KP_Page_Up },
//    { '8',  XK_Up },
//    { '4',  XK_Left },
//    { '6',  XK_Right },
//    { '2',  XK_Down },

    { '.',  XK_KP_Delete },
    { '*',  XK_KP_Multiply },
    { '/',  XK_KP_Divide },
    { '+',  XK_KP_Add },
    { '-',  XK_KP_Subtract },
    { '4',  XK_Left },
    { '6',  XK_Right },
    { '8',  XK_Up },
    { '2',  XK_Down },

    { 0,  0 }
};

#else
#if (defined _WIN32 || defined WINDOWS)

//***********************************
// Windows-specific Functions       *
//***********************************

//Check for any window messages (keypress, paint, mousemove, etc)
void CheckMessages()
{
    MSG msg;
    while (PeekMessage(&msg, 0 , 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (destroyed)
        throw TerminationException();
};

//This function processes any Windows messages we get. Keyboard, OnClose, etc
LRESULT CALLBACK ProcessMessages(HWND__ *hWnd,unsigned int Msg,
                                 WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
        case WM_CHAR:               //This handles most key presses
            lastchar=(int)wParam;
            switch (lastchar)
            {
                case 13:            //Reroute ENTER key for compatilbity purposes
                    lastchar=10;
                    break;
                case 8:             //Reroute BACKSPACE key for compatilbity purposes
                    lastchar=127;
                    break;
            };
            break;
        case WM_KEYDOWN:                //Here we handle non-character input
            switch (wParam)
            {
                case VK_LEFT:
                    lastchar = KEY_LEFT;
                    break;
                case VK_RIGHT:
                    lastchar = KEY_RIGHT;
                    break;
                case VK_UP:
                    lastchar = KEY_UP;
                    break;
                case VK_DOWN:
                    lastchar = KEY_DOWN;
                    break;
                default:
                    break;
            };
        case WM_ERASEBKGND:
            return 1;               //We don't want to erase our background
        case WM_PAINT:              //Pull from our backbuffer, onto the screen
            gfx_refresh_screen ();
            ValidateRect(WindowHandle,NULL);
            break;
        case WM_DESTROY:
            destroyed = true;
            break;
//            exit(0);
        default://If we didnt process a message, return the default value for it
            return DefWindowProcW(hWnd, Msg, wParam, lParam);
    };
    return 0;
};

static bool init_window (int width, int height)
{
    int success;
    WNDCLASSEXW WindowClassType;
    int WinBorderHeight;
    int WinBorderWidth;
    int WinTitleSize;
    unsigned int WindowStyle;
    int rw_w = GetSystemMetrics(SM_CXSCREEN);
    int rw_h = GetSystemMetrics(SM_CYSCREEN);
    int d_w = (rw_w - width) / 2;
    int d_h = (rw_h - height) / 2;
    if (d_w < 0) d_w = 0;
    if (d_h < 0) d_h = 0;
    const WCHAR *szTitle=  (L"Cataclysm");
    WinTitleSize = GetSystemMetrics(SM_CYCAPTION);      //These lines ensure
    WinBorderWidth = GetSystemMetrics(SM_CXDLGFRAME) * 2;  //that our window will
    WinBorderHeight = GetSystemMetrics(SM_CYDLGFRAME) * 2; // be a perfect size
    WindowClassType.cbSize = sizeof(WNDCLASSEXW);
    WindowClassType.style = 0;//No point in having a custom style, no mouse, etc
    WindowClassType.lpfnWndProc = ProcessMessages;//the procedure that gets msgs
    WindowClassType.cbClsExtra = 0;
    WindowClassType.cbWndExtra = 0;
    WindowClassType.hInstance = WindowINST;// hInstance
    WindowClassType.hIcon = LoadIcon(WindowINST, IDI_APPLICATION);//Default Icon
    WindowClassType.hIconSm = LoadIcon(WindowINST, IDI_APPLICATION);//Default Icon
    WindowClassType.hCursor = LoadCursor(NULL, IDC_ARROW);//Default Pointer
    WindowClassType.lpszMenuName = NULL;
    WindowClassType.hbrBackground = 0;//Thanks jday! Remove background brush
    WindowClassType.lpszClassName = szWindowClass;
    success = RegisterClassExW(&WindowClassType);
    if (success== 0)
    {
        printf ("Fatal error: registering window class.\n");
        return false;
    }
    WindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME) & ~(WS_MAXIMIZEBOX);
    WindowHandle = CreateWindowExW(WS_EX_APPWINDOW || WS_EX_TOPMOST * 0,
                                   szWindowClass , szTitle,WindowStyle, d_w,
                                   d_h, width + (WinBorderWidth) * 1,
                                   height + (WinBorderHeight +
                                   WinTitleSize) * 1, 0, 0, WindowINST, NULL);
    if (WindowHandle == 0)
    {
       printf ("Fatal error: creating window.\n");
       return false;
    }
    ShowWindow (WindowHandle, 5);
    CheckMessages ();

    WindowDC = GetDC(WindowHandle);
    if (!WindowDC)
    {
        printf ("Fatal error: getting window DC.\n");
        return false;
    }

	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize  = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion   = 1;
	pfd.dwFlags    = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 0;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int nPixelFormat = ChoosePixelFormat(WindowDC , &pfd);

	if (nPixelFormat == 0)
	{
	    printf ("Fatal error: choosing pixel format.\n");
	    return false;
	}

	BOOL bResult = SetPixelFormat (WindowDC, nPixelFormat, &pfd);

	if (!bResult)
	{
	    printf ("Fatal error: setting pixel format.\n");
	    return false;
	}

	context = wglCreateContext(WindowDC);
	wglMakeCurrent(WindowDC, context);
//	wglSwapIntervalEXT (1);
    window_width = width;
    window_height = height;

    mapped = true;
    return true;
}

static void destroy_window ()
{
    wglMakeCurrent (WindowDC, 0);
    wglDeleteContext (context);
    if ((WindowDC > 0) && (ReleaseDC(WindowHandle, WindowDC) == 0))
    {
        WindowDC = 0;
    }
    if ((!WindowHandle == 0) && (!(DestroyWindow(WindowHandle))))
    {
        WindowHandle = 0;
    }
    if (!(UnregisterClassW(szWindowClass, WindowINST)))
    {
        WindowINST = 0;
    }
};

#endif
#endif

//----------------------------------------------------------------------------

static bool is_extension_supported (const char *ext)
{
    for (int i = 0; i < extensions.size(); i++)
        if (extensions[i] == ext)
            return true;
    return false;
}

//----------------------------------------------------------------------------

static bool init_graphics ()
{
    // get all extensions as a string
    char *str = (char *) glGetString(GL_EXTENSIONS);
    char *tok;

    // split extensions
    if(str)
    {
        tok = strtok((char*)str, " ");
        while(tok)
        {
            extensions.push_back(tok);    // put a extension into struct
            tok = strtok(0, " ");         // next token
        }
    }
    else
    {
        printf ("Error getting extensions.\n");
        return false;
    }

    use_texture_padding = !is_extension_supported ("GL_ARB_texture_non_power_of_two");

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, window_width, window_height, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    font.setup_symbols (font_name.c_str());
    if (!font.ok())
    {
        printf ("Error loading font %s.\n", font_name.c_str());
        return false;
    }

//#ifdef TILES
//    if (!tiles.read_config ("data/graphics.cfg", false))
//    {
//        printf ("Error reading config file, line %d: %s\n", tiles.error_line, tiles.error_text.c_str());
//        return 0;
//    }
//#endif

    glClearColor (0.0, 0.0, 0.0, 0.0);
    glClear (GL_COLOR_BUFFER_BIT);
    return true;
}

//----------------------------------------------------------------------------

void gfx_set_color_pal (int index)
{
    if (console_palette && index >= 0 && index < 16)
        glColor3ubv (console_palette[index].rgb);
}

//----------------------------------------------------------------------------

void gfx_set_color_argb (unsigned long argb)
{
    GLubyte alpha = (GLubyte) ((argb >> 24) & 0xff);
    glColor4ub ((GLubyte) ((argb >> 16) & 0xff),
                (GLubyte) ((argb >> 8) & 0xff),
                (GLubyte) ((argb >> 0) & 0xff),
                alpha? alpha : (GLubyte) (0xff));
}

//----------------------------------------------------------------------------

unsigned long gfx_fg_to_argb (int attrs)
{
    int npair = (attrs & A_COLOR) >> 17;
    if (console_palette && npair >= 0 && npair < max_color_pairs)
    {
        int num = colorpairs[npair].FG;
        if (attrs & A_BOLD) num += 8;
        if (num >= 0 && num <= 16)
            return (console_palette[num].rgb[2]) |
                   (console_palette[num].rgb[1] << 8) |
                   (console_palette[num].rgb[0] << 16) |
                   0xff000000;
    }
    return 0xffffffff;
}

//----------------------------------------------------------------------------

unsigned long gfx_bg_to_argb (int attrs)
{
    int npair = (attrs & A_COLOR) >> 17;
    if (console_palette && npair >= 0 && npair < max_color_pairs)
    {
        int num = colorpairs[npair].BG;
        if (attrs & A_BLINK) num += 8;
        if (num >= 0 && num <= 16)
            return (console_palette[num].rgb[2]) |
                   (console_palette[num].rgb[1] << 8) |
                   (console_palette[num].rgb[0] << 16) |
                   0xff000000;
    }
    return 0xffffffff;
}

//----------------------------------------------------------------------------

void gfx_set_color_fg (int attrs)
{
    int npair = (attrs & A_COLOR) >> 17;
    if (console_palette && npair >= 0 && npair < max_color_pairs)
    {
        int num = colorpairs[npair].FG;
        if (attrs & A_BOLD) num += 8;
        if (num >= 0 && num <= 16)
            glColor3ubv (console_palette[num].rgb);
    }
}

//----------------------------------------------------------------------------

void gfx_set_color_bg (int attrs)
{
    int npair = (attrs & A_COLOR) >> 17;
    if (console_palette && npair >= 0 && npair < max_color_pairs)
    {
        int num = colorpairs[npair].BG;
        if (attrs & A_BLINK) num += 8;
        if (num >= 0 && num <= 16)
            glColor3ubv (console_palette[num].rgb);
    }
}

//----------------------------------------------------------------------------

void gfx_draw_symbol (Texture &tex, int x, int y, int sym, bool tinted, bool mono)
{
    if (!tinted)
        glColor3f(1.0, 1.0, 1.0);
    tex.draw_symbol (x, y, sym, mono);
}

//----------------------------------------------------------------------------

void gfx_draw_rectangle (int x, int y, int w, int h)
{
    glBegin (GL_POLYGON);
    {
        glVertex2i(x, y);
        glVertex2i(x + w, y);
        glVertex2i(x + w, y + h);
        glVertex2i(x, y + h);
    }
    glEnd();
}

//----------------------------------------------------------------------------

void gfx_draw_symbol_with_bg (Texture &tex, int x, int y, int attrs, int sym)
{
    gfx_set_color_bg(attrs);
    gfx_draw_rectangle (x, y, tex.symbol_width(), tex.symbol_height());
    gfx_set_color_fg(attrs);
    tex.draw_symbol (x, y, sym);
}

//----------------------------------------------------------------------------

void gfx_refresh_screen ()
{
    if (mapped)
    {
        glFlush();
#ifdef linux
        glXSwapBuffers (display, window);
#else
#if (defined _WIN32 || defined WINDOWS)
        SwapBuffers (WindowDC);
#endif
#endif
        // now copy front buffer into backbuffer as background for next frame
        glReadBuffer (GL_FRONT);
        glDrawBuffer (GL_BACK);
        glCopyPixels (0, 0, window_width, window_height, GL_COLOR);
    }
}


//----------------------------------------------------------------------------

static void draw_window (WINDOW *win)
{
    for (int j = 0; j < win->height; j++)
    {
        if (win->line[j].touched)
        {
            win->line[j].touched = false;
            for (int i = 0; i < win->width; i++)
            {
                int x = win->offset_x + (win->x + i) * font.symbol_width();
                int y = win->offset_y + (win->y + j) * font.symbol_height();
                gfx_set_color_pal (win->line[j].BG[i]);
                gfx_draw_rectangle (x, y, font.symbol_width(), font.symbol_height());
                gfx_set_color_pal (win->line[j].FG[i]);
                font.draw_symbol (x, y, win->line[j].chars[i]);
            };
        }
    };
    win->draw = false;
};

//----------------------------------------------------------------------------

int getch ()
{
    refresh();
    if (inputdelay != 0)
        gfx_refresh_screen ();

#ifdef linux
    clock_t key_start = clock();
    do
        process_events ();
    while (!key_found && (inputdelay < 0 || (clock() - key_start < CLOCKS_PER_SEC / 1000 * inputdelay)));
    if (!key_found)
        return -1;
    key_found = false;
//    printf ("key %x pressed (%c)\n", lastchar, (char) lastchar);
    int i = 0;
    while (translate_keys[i].keysym)
    {
        if (translate_keys[i].keysym == lastchar)
        {
            lastchar = translate_keys[i].letter;
            break;
        }
        i++;
    }
//    if (lastchar < 0 || lastchar > 0x7f)
//        lastchar = -1;
    return lastchar;

#else
#if (defined _WIN32 || defined WINDOWS)

    lastchar=ERR;//ERR=-1
    if (inputdelay < 0)
    {
        do
        {
            CheckMessages();
            if (lastchar!=ERR) break;
            MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLEVENTS);//low cpu wait!
        }
        while (lastchar==ERR);
    }
    else if (inputdelay > 0)
    {
        unsigned long starttime=GetTickCount();
        unsigned long endtime;
        do
        {
            CheckMessages();        //MsgWaitForMultipleObjects won't work very good here
            endtime=GetTickCount(); //it responds to mouse movement, and WM_PAINT, not good
            if (lastchar!=ERR) break;
            Sleep(2);
        }
        while (endtime<(starttime+inputdelay));
    }
    else
    {
        CheckMessages();
    };
    Sleep(25);
    return lastchar;
#endif
#endif
}

//***********************************
// Pseudo-Curses Functions          *
//***********************************


//----------------------------------------------------------------------------

static bool get_font_wh (const char *fnm, int &fw, int &fh)
{
    FILE *f = fopen (fnm, "rb");
    if (!f)
    {
        printf ("Can't open file %s\n", fnm);
        return false;
    }
    unsigned char buf[20];
    if (fread (buf, 1, 16, f) != 16)
    {
        printf ("fread error\n");
        fclose (f);
        return false;
    }
    fw = ((unsigned short) (buf[12]) | ((unsigned short) (buf[13]) << 8)) / 16;
    fh = ((unsigned short) (buf[14]) | ((unsigned short) (buf[15]) << 8)) / 16;
    fclose (f);
    return true;
}

//----------------------------------------------------------------------------

//Basic Init, create the font, backbuffer, etc
WINDOW *initscr(void)
{
#ifdef TILES
    if (!tiles.read_config ("data/graphics.cfg", true))
    {
        printf ("Error reading config file, line %d: %s\n", tiles.error_line, tiles.error_text.c_str());
        return 0;
    }
#endif
    int fw, fh;
    if (!get_font_wh (font_name.c_str(), fw, fh))
    {
        printf ("Error opening font file %s.\n", font_name.c_str());
        return 0;
    }
    if (fw > 100 || fh > 100 || fw < 1 || fh < 1)
    {
        printf ("Bad font size (%dx%d)!", fw, fh);
        return 0;
    }

#ifdef TILES
    int ww = tiles.width * 25 + fw * 55;
    int wh = fh * 25;
    if (wh < tiles.height * 25)
        wh = tiles.height * 25;
//    printf ("tw=%d ww=%d wh=%d fw=%d fh=%d fname=%s\n", tiles.width, ww, wh, fw, fh, font_name.c_str());
#else
    int ww = fw * 80;
    int wh = fh * 25;
#endif
    if (!init_window (ww, wh))
        return 0;
    if (!init_graphics ())
        return 0;

    lastchar = 0;
    inputdelay = -1;

    mainwin = newwin (25, 80, 0, 0);
    gfx_refresh_screen ();
    return mainwin;   //create the 'stdscr' window and return its ref
};

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x, int offset_x_pix, int offset_y_pix)
{
    int i,j;
    WINDOW *newwindow = new WINDOW;
    newwindow->offset_x=offset_x_pix;
    newwindow->offset_y=offset_y_pix;
    newwindow->x=begin_x;
    newwindow->y=begin_y;
    newwindow->width=ncols;
    newwindow->height=nlines;
    newwindow->inuse=true;
    newwindow->draw=false;
    newwindow->BG=0;
    newwindow->FG=8;
    newwindow->cursorx=0;
    newwindow->cursory=0;
    newwindow->line = new curseline[nlines];

    for (j=0; j<nlines; j++)
    {
        newwindow->line[j].chars= new char[ncols];
        newwindow->line[j].FG= new char[ncols];
        newwindow->line[j].BG= new char[ncols];
        newwindow->line[j].touched=true;//Touch them all !?
        for (i=0; i<ncols; i++)
        {
          newwindow->line[j].chars[i]=0;
          newwindow->line[j].FG[i]=0;
          newwindow->line[j].BG[i]=0;
        }
    }
    //WindowCount++;
    return newwindow;
};


//Deletes the window and marks it as free. Clears it just in case.
int delwin(WINDOW *win)
{
    int j;
    win->inuse=false;
    win->draw=false;
    for (j=0; j<win->height; j++){
        delete win->line[j].chars;
        delete win->line[j].FG;
        delete win->line[j].BG;
        }
    delete win->line;
    delete win;
    return 1;
};

inline int newline(WINDOW *win){
    if (win->cursory < win->height - 1){
        win->cursory++;
        win->cursorx=0;
        return 1;
    }
return 0;
};

inline void addedchar(WINDOW *win){
    win->cursorx++;
    win->line[win->cursory].touched=true;
    if (win->cursorx > win->width)
        newline(win);
};


//Borders the window with fancy lines!
int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl, chtype br)
{

    int i, j;
    int oldx=win->cursorx;//methods below move the cursor, save the value!
    int oldy=win->cursory;//methods below move the cursor, save the value!
    if (ls>0)
        for (j=1; j<win->height-1; j++)
            mvwaddch(win, j, 0, ls);
    if (rs>0)
        for (j=1; j<win->height-1; j++)
            mvwaddch(win, j, win->width-1, rs);
    if (ts>0)
        for (i=1; i<win->width-1; i++)
            mvwaddch(win, 0, i, ts);
    if (bs>0)
        for (i=1; i<win->width-1; i++)
            mvwaddch(win, win->height-1, i, bs);
    if (tl>0)
        mvwaddch(win,0, 0, tl);
    if (tr>0)
        mvwaddch(win,0, win->width-1, tr);
    if (bl>0)
        mvwaddch(win,win->height-1, 0, bl);
    if (br>0)
        mvwaddch(win,win->height-1, win->width-1, br);
    wmove(win,oldy,oldx);
    return 1;
};

//Refreshes a window, causing it to redraw on top.
int wrefresh(WINDOW *win)
{
    if (win == 0) // mainwin
        win = mainwin;

    if (win->draw)
        draw_window (win);

    return 1;
};

//Refreshes window 0 (stdscr), causing it to redraw on top.
int refresh(void)
{
    return wrefresh(mainwin);
};

//The core printing function, prints characters to the array, and sets colors
inline int printstring(WINDOW *win, char *fmt)
{
 int size = strlen(fmt);
 int j;
 for (j=0; j<size; j++){
  if (!(fmt[j]==10)){//check that this isnt a newline char
   if (win->cursorx <= win->width - 1 && win->cursory <= win->height - 1) {
    win->line[win->cursory].chars[win->cursorx]=fmt[j];
    win->line[win->cursory].FG[win->cursorx]=win->FG;
    win->line[win->cursory].BG[win->cursorx]=win->BG;
    win->line[win->cursory].touched=true;
    addedchar(win);
   } else
   return 0; //if we try and write anything outside the window, abort completely
} else // if the character is a newline, make sure to move down a line
  if (newline(win)==0){
      return 0;
      }
 }
 win->draw=true;
 return 1;
}

//Prints a formatted string to a window at the current cursor, base function
int wprintw(WINDOW *win, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2048];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    return printstring(win,printbuf);
};

//Prints a formatted string to a window, moves the cursor
int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2048];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    if (wmove(win,y,x)==0) return 0;
    return printstring(win,printbuf);
};

//Prints a formatted string to window 0 (stdscr), moves the cursor
int mvprintw(int y, int x, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2048];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    if (move(y,x)==0) return 0;
    return printstring(mainwin,printbuf);
};

//Prints a formatted string to window 0 (stdscr) at the current cursor
int printw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2078];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    return printstring(mainwin,printbuf);
};

//erases a window of all text and attributes
int werase(WINDOW *win)
{
    int j,i;
    for (j=0; j<win->height; j++)
    {
     for (i=0; i<win->width; i++)   {
     win->line[j].chars[i]=0;
     win->line[j].FG[i]=0;
     win->line[j].BG[i]=0;
     }
        win->line[j].touched=true;
    }
    win->draw=true;
    wmove(win,0,0);
    wrefresh(win);
    return 1;
};

//erases window 0 (stdscr) of all text and attributes
int erase(void)
{
    glClear (GL_COLOR_BUFFER_BIT);
    return werase(mainwin);
};

//pairs up a foreground and background color and puts it into the array of pairs
int init_pair(short pair, short f, short b)
{
    colorpairs[pair].FG=f;
    colorpairs[pair].BG=b;
    return 1;
};

//moves the cursor in a window
int wmove(WINDOW *win, int y, int x)
{
    if (x>=win->width)
     {return 0;}//FIXES MAP CRASH -> >= vs > only
    if (y>=win->height)
     {return 0;}// > crashes?
    if (y<0)
     {return 0;}
    if (x<0)
     {return 0;}
    win->cursorx=x;
    win->cursory=y;
    return 1;
};

//Clears windows 0 (stdscr)     I'm not sure if its suppose to do this?
int clear(void)
{
    glClear(GL_COLOR_BUFFER_BIT);
    return wclear(mainwin);
};

//Ends the terminal, destroy everything
int endwin(void)
{
    destroy_window();
    return 1;
};

//adds a character to the window
int mvwaddch(WINDOW *win, int y, int x, const chtype ch)
{
   if (wmove(win,y,x)==0) return 0;
   return waddch(win, ch);
};

//clears a window
int wclear(WINDOW *win)
{
    werase(win);
    wrefresh(win);
    return 1;
};

//gets the max x of a window (the width)
int getmaxx(WINDOW *win)
{
    if (win==0) return mainwin->width;     //StdScr
    return win->width;
};

//gets the max y of a window (the height)
int getmaxy(WINDOW *win)
{
    if (win==0) return mainwin->height;     //StdScr
    return win->height;
};

int start_color(void)
{
    colorpairs = new pairs[max_color_pairs];
    console_palette = new t_rgb_triplet [16];
    console_palette[0] = t_rgb_triplet (0, 0, 0);
    console_palette[1] = t_rgb_triplet (196, 0, 0);
    console_palette[2] = t_rgb_triplet (0, 196, 0);
    console_palette[3] = t_rgb_triplet (196, 180, 30);
    console_palette[4] = t_rgb_triplet (0, 0, 196);
    console_palette[5] = t_rgb_triplet (196, 0, 180);
    console_palette[6] = t_rgb_triplet (0, 170, 200);
    console_palette[7] = t_rgb_triplet (196, 196, 196);
    console_palette[8] = t_rgb_triplet (96, 96, 96);
    console_palette[9] = t_rgb_triplet (255, 64, 64);
    console_palette[10] = t_rgb_triplet (0, 240, 0);
    console_palette[11] = t_rgb_triplet (255, 255, 0);
    console_palette[12] = t_rgb_triplet (96, 96, 255);
    console_palette[13] = t_rgb_triplet (255, 96, 240);
    console_palette[14] = t_rgb_triplet (0, 240, 255);
    console_palette[15] = t_rgb_triplet (255, 255, 255);
};

int keypad(WINDOW *faux, bool bf)
{
return 1;
};

int noecho(void)
{
    return 1;
};
int cbreak(void)
{
    return 1;
};
int keypad(int faux, bool bf)
{
    return 1;
};
int curs_set(int visibility)
{
    return 1;
};

int mvaddch(int y, int x, const chtype ch)
{
    return mvwaddch(mainwin,y,x,ch);
};

int wattron(WINDOW *win, int attrs)
{
    bool isBold = !!(attrs & A_BOLD);
    bool isBlink = !!(attrs & A_BLINK);
    int pairNumber = (attrs & A_COLOR) >> 17;
    win->FG=colorpairs[pairNumber].FG;
    win->BG=colorpairs[pairNumber].BG;
    if (isBold) win->FG += 8;
    if (isBlink) win->BG += 8;
    return 1;
};

int wattroff(WINDOW *win, int attrs)
{
    win->FG = 8;                                  //reset to white
    win->BG = 0;                                  //reset to black
    return 1;
};

int attron(int attrs)
{
    return wattron(mainwin, attrs);
};

int attroff(int attrs)
{
    return wattroff(mainwin,attrs);
};

int waddch(WINDOW *win, const chtype ch)
{
    char charcode;

    if (ch >= 4194410 && ch <= 4194424)
        charcode = (char) (ch - 4194410) + 1;
    else
        charcode = (char) ch;

    int curx=win->cursorx;
    int cury=win->cursory;

    win->line[cury].chars[curx]=charcode;
    win->line[cury].FG[curx]=win->FG;
    win->line[cury].BG[curx]=win->BG;

    win->draw=true;
    addedchar(win);
    return 1;
};

//Move the cursor of windows 0 (stdscr)
int move(int y, int x)
{
    return wmove(mainwin,y,x);
};

//Set the amount of time getch waits for input
void timeout(int delay)
{
    inputdelay=delay;
};

#endif // (defined TILES || defined GFX_GL)


