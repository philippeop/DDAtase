#if (defined TILES || defined GFX_GL)

#include <GL/gl.h>
#include "catatex.h"
#if (defined _WIN32 || defined WINDOWS)
   #include "GL/glext.h"
#endif

bool use_texture_padding = false;

//---------------------------------------------------------------------------

Texture::Texture()
{
    m_bytes = 0;
    m_fmt = -1;
    m_ok = false;
    m_width = 0;
    m_height = 0;
    m_symbol_width = 0;
    m_symbol_height = 0;
    m_symbols_num = 0;
    m_file = 0;
    m_name = 0;
    m_name_mono = 0;
    m_char_id = 0;
    m_padx = 0;
    m_pady = 0;
    m_data = 0;
}

//---------------------------------------------------------------------------

Texture::~Texture()
{
    clear ();
}

//---------------------------------------------------------------------------

int Texture::width () const
{
    return m_width;
}

//---------------------------------------------------------------------------

int Texture::height () const
{
    return m_height;
}

//---------------------------------------------------------------------------

int Texture::symbol_width () const
{
    return m_symbol_width;
}

//---------------------------------------------------------------------------

int Texture::symbol_height () const
{
    return m_symbol_height;
}

//---------------------------------------------------------------------------

int Texture::symbols_num () const
{
    return m_symbols_num;
}

//---------------------------------------------------------------------------

bool Texture::ok () const
{
    return m_ok;
}

//---------------------------------------------------------------------------

void Texture::clear ()
{
    if (m_name)
    {
        glDeleteTextures (1, (GLuint *) (&m_name));
        m_name = 0;
    }
    if (m_name_mono)
    {
        glDeleteTextures (1, (GLuint *) (&m_name_mono));
        m_name_mono = 0;
    }
}

//---------------------------------------------------------------------------

unsigned char Texture::load_byte ()
{
    unsigned char res;
    if (fread (&res, 1, 1, m_file) == 1)
        return res;
    return 0;
}

//---------------------------------------------------------------------------

unsigned short Texture::load_word ()
{
    return ((unsigned short) load_byte ()) |
          (((unsigned short) load_byte ()) << 8);
}

//---------------------------------------------------------------------------

static int next_pow_2 (int v)
{
    int t = 1;
    while (t < v && t < 0x3fffffff)
        t <<= 1;

    return t;
}

//---------------------------------------------------------------------------

bool Texture::load_tga (const char *fnm)
{
    m_ok = false;
    m_width = 0;
    m_height = 0;
    m_symbol_width = 0;
    m_symbol_height = 0;
    m_symbols_num = 0;
    m_name = 0;
    m_name_mono = 0;

    if (m_data)
        delete[] m_data;
    m_data = 0;

    m_file = fopen (fnm, "rb");
    if (!m_file)
        return m_ok;

    int id_len = (int) load_byte ();
    int cmap_exist = load_byte (); // colormap included
    if (cmap_exist)
    {
        fclose (m_file);
        return m_ok;
    }

    int imtype = load_byte (); // image type
    if (imtype != 2)
    { // only uncompressed RGB supported
        fclose (m_file);
        return m_ok;
    }

    load_word (); // colormap offset
    load_word (); // colormap size
    load_byte (); // colormap enrtry size

    load_word (); // left
    load_word (); // top

    int w1 = (int) load_word ();
    int h1 = (int) load_word ();

    if (w1 * h1 < 1)
    {
        fclose (m_file);
        return m_ok;
    }

    int width = use_texture_padding? next_pow_2 (w1) : w1;
    int height = use_texture_padding? next_pow_2 (h1) : h1;
    m_padx = width - w1;
    m_pady = height - h1;

    if (width > 4096 || height > 4096)
    {
        fclose (m_file);
        return m_ok;
    }

    int nbts = (int) load_byte ();
    if (nbts != 24 && nbts != 32)
    {
        fclose (m_file);
        return m_ok;
    }
    m_bytes = nbts / 8;

    int imdesc = load_byte ();  // pixel description
    if (imdesc & 0xf)
        m_fmt = GL_BGRA_EXT;
    else
        m_fmt = GL_BGR_EXT;

    m_upper = (imdesc & 0x20) != 0;

    if (id_len)
        fseek (m_file, id_len, SEEK_CUR);

    int wh = width * height;
    int whb = wh * m_bytes;

    m_data = new unsigned char [whb];
    if (!m_data)
    {
        fclose (m_file);
        return m_ok;
    }

    for (int i = 0; i < h1; i++)
        if ((int) fread (m_data + (m_upper? m_bytes * i * width : m_bytes * (h1 - 1 - i) * width),
                          m_bytes, w1, m_file) != w1)
        {
             delete[] m_data;
             m_data = 0;
             fclose (m_file);
             return m_ok;
        }

    fclose (m_file);
    m_file = 0;

    m_width = width;
    m_height = height;
    m_ok = true;
    return m_ok;
}

//---------------------------------------------------------------------------

void Texture::apply ()
{
    if (!m_ok || !m_name)
        return;

    glBindTexture (GL_TEXTURE_2D, m_name);
}

//---------------------------------------------------------------------------

void Texture::apply_mono ()
{
    if (!m_ok || !(m_name_mono? m_name_mono : m_name))
        return;

    glBindTexture (GL_TEXTURE_2D, m_name_mono? m_name_mono : m_name);
}

//---------------------------------------------------------------------------

bool Texture::setup_texture (bool mono)
{
    if (!m_data)
        return false;

    if (mono)
    {
        glGenTextures (1, (GLuint*) &m_name_mono);
        apply_mono ();
    }
    else
    {
        glGenTextures (1, (GLuint*) &m_name);
        apply ();
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);//GL_LINEAR);

    if (mono)
    {
        int msz = m_width * m_height;
        int mal = m_bytes == 4? 1 : 0;
        unsigned char *monodata = new unsigned char [msz * (1 + mal)];
        for (int i = 0; i < msz; i++)
        {
            double b = (double) m_data[i * (3 + mal)];
            double g = (double) m_data[i * (3 + mal) + 1];
            double r = (double) m_data[i * (3 + mal) + 2];
            int mlum = (int) (0.299 * r + 0.587 * g + 0.114 * b);
            if (mlum > 255)
                mlum = 255;
            monodata[i * (1 + mal)] = (unsigned char) mlum;
            if (mal) // alpha
                monodata[i * (1 + mal) + 1] = m_data[i * (3 + mal) + 3];
        }

        glTexImage2D(GL_TEXTURE_2D, 0, mal? GL_LUMINANCE_ALPHA : GL_LUMINANCE,
                     m_width, m_height, 0, mal? GL_LUMINANCE_ALPHA : GL_LUMINANCE, GL_UNSIGNED_BYTE, monodata);

        delete[] monodata;
        monodata = 0;
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, m_bytes == 4? GL_RGBA : GL_RGB,
                     m_width, m_height, 0, m_fmt, GL_UNSIGNED_BYTE, m_data);

    }

    GLenum err = glGetError ();
    if (err != GL_NO_ERROR)
    {
        printf ("Error %d setting texture\n", err);
        m_ok = false;
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------

bool Texture::setup (const char *fnm)
{
    clear ();
    if (fnm)
        load_tga (fnm);

    if (!m_ok || !m_data)
        return false;

    return setup_texture (false);
}

//---------------------------------------------------------------------------

bool Texture::setup_symbols (const char *fnm, int sym_width, int sym_height, bool mono)
{
    clear ();
    if (fnm)
        load_tga (fnm);

    if (!m_ok || !m_data)
        return false;

    bool res = setup_texture (false);
    if (res && mono)
        res = setup_texture (true);
    else
        m_name_mono = 0;
    delete[] m_data;
    m_data = 0;

    if (!res)
        return false;

    if (sym_width < 1)
        m_symbol_width = (m_width - m_padx) / 16;
    else
        m_symbol_width = sym_width;
    if (sym_height < 1)
        m_symbol_height = (m_height - m_pady) / 16;
    else
        m_symbol_height = sym_height;

    int nx = ((m_width - m_padx) / m_symbol_width);
    int ny = ((m_height - m_pady) / m_symbol_height);
    m_symbols_num = nx * ny;
    m_char_id = glGenLists (m_symbols_num);
    for (int i = 0; i < m_symbols_num; i++)
    {
        int numy = i / nx;
        GLfloat x1 = (GLfloat) ((i % nx) * m_symbol_width) / (GLfloat) m_width;
        GLfloat y1 = (GLfloat) ((numy) * m_symbol_height) / (GLfloat) m_height;
        GLfloat x2 = x1 + (GLfloat) m_symbol_width / (GLfloat) m_width;
        GLfloat y2 = y1 + (GLfloat) m_symbol_height / (GLfloat) m_height;

        glNewList (m_char_id + i, GL_COMPILE);
        {
            glBegin (GL_QUADS);
            {
                glTexCoord2f (x1, y1);
                glVertex2f   (0.0, 0.0);

                glTexCoord2f (x2, y1);
                glVertex2f   (1.0, 0.0);

                glTexCoord2f (x2, y2);
                glVertex2f   (1.0, 1.0);

                glTexCoord2f (x1, y2);
                glVertex2f   (0.0, 1.0);

            }
            glEnd ();
        }
        glEndList ();
    }
    return true;
}

//---------------------------------------------------------------------------

void Texture::draw_symbol (int x, int y, int symbol, bool mono)
{
    if (symbol < 0 || symbol >= m_symbols_num)
        return;

    glPushAttrib (GL_ENABLE_BIT);
    {
        glEnable (GL_TEXTURE_2D);
        glEnable (GL_BLEND);
        if (mono)
            apply_mono ();
        else
            apply ();

        glPushMatrix ();
        {
            glTranslatef ((GLfloat)x, (GLfloat)y, 0);
            glScalef (m_symbol_width, m_symbol_height, 1.0);
            glCallList (m_char_id + symbol);
        }
        glPopMatrix ();
    }
    glPopAttrib ();
}

//---------------------------------------------------------------------------

#endif
