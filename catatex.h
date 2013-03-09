#ifndef _CATATEX_H_
#define _CATATEX_H_

#if (defined TILES || defined GFX_GL)

//---------------------------------------------------------------------------

#include <stdio.h>

//---------------------------------------------------------------------------

class Texture
{
private:
    bool m_ok;
    FILE *m_file;
    int m_bytes;
    int m_fmt;
    int m_upper;
    unsigned char *m_data;
    int m_width;
    int m_height;
    int m_symbol_width;
    int m_symbol_height;
    int m_symbols_num;
    int m_padx;
    int m_pady;
    unsigned int m_name;
    unsigned int m_name_mono;
    unsigned int m_char_id;
    unsigned char load_byte ();
    unsigned short load_word ();
    bool setup_texture (bool mono);
public:
    Texture ();
    ~Texture ();
    bool load_tga (const char *fnm);
    int width () const;
    int height () const;
    int symbol_width () const;
    int symbol_height () const;
    int symbols_num () const;
    bool ok () const;
    void clear ();
    void apply ();
    void apply_mono ();
    bool setup (const char *fnm = 0);
    bool setup_symbols (const char *fnm = 0, int sym_width = 0, int sym_height = 0, bool mono = false);
    void draw_symbol (int x, int y, int symbol, bool mono = false);
};

//---------------------------------------------------------------------------

extern bool use_texture_padding;

#endif

#endif
