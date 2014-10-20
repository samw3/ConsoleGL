// ConsoleGL
//
// Licensed public domain

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <GLFW/glfw3.h>
#include "font.h"
#include "ConsoleGL.h"

static const GLubyte sPalette[] = {
    0x00, 0x00, 0x00, // 0 Black
    0xaa, 0x00, 0x00, // 1 Red
    0x00, 0xaa, 0x00, // 2 Green
    0xaa, 0x55, 0x00, // 3 Yellow
    0x00, 0x00, 0xaa, // 4 Blue
    0xaa, 0x00, 0xaa, // 5 Magenta
    0x00, 0xaa, 0xaa, // 6 Cyan
    0xaa, 0xaa, 0xaa, // 7 White
    0x55, 0x55, 0x55, // 8 Br. Black
    0xff, 0x55, 0x55, // 9 Br. Red
    0x55, 0xff, 0x55, // A Br. Green
    0xff, 0xff, 0x55, // B Br. Yellow
    0x55, 0x55, 0xff, // C Br. Blue
    0xff, 0x55, 0xff, // D Br. Magenta
    0x55, 0xff, 0xff, // E Br. Cyan
    0xff, 0xff, 0xff, // F Br. White
};

#define BLINK_SPEED (30)

static void (*sTickCallback)(void);
static GLboolean sDirty = GL_TRUE;
static GLushort *sChars;
static GLubyte *sTexture;
static int sCharsWidth;
static int sCharsHeight;
static int sCharsArea;
static int sCharsXPos = 0;
static int sCharsYPos = 0;
static GLshort sLastAttrib = (128 + 31) << 8;
static char* sPrintFBuffer;

static GLuint *sIndexBuffer;
static GLfloat *sVertexBuffer;
static GLshort *sTextureCoordBuffer;
static GLubyte *sFgColorBuffer;
static GLubyte *sBgColorBuffer;

static int sBlinkTimer = BLINK_SPEED;
static GLboolean sBlinkState = GL_FALSE;

const char* _CGLerror(const char* _msg)
{
    glfwTerminate();
    return _msg;
}

/**
 *  Opens a window
 */
const char* CGLmain(const char* _windowTitle, int _columns, int _rows, void (*_callback)(void))
{
    sCharsWidth = _columns;
    sCharsHeight = _rows;
    sCharsArea = _columns * _rows;
    
    sPrintFBuffer = malloc(sizeof(char) * sCharsArea);
    if (!sPrintFBuffer) return _CGLerror("ERROR: Cannot allocate printf buffer.");
    
    sTickCallback = _callback;
    GLFWwindow* window;
    
    /* Initialize the library */
    if (!glfwInit()) return _CGLerror("ERROR: glfwInit failed.");
    
    /* Create a windowed mode window and its OpenGL context */
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    window = glfwCreateWindow(_columns * 8 * 2, _rows * 8 * 2, _windowTitle, NULL, NULL);
    if (!window) return _CGLerror("ERROR: glfwCreateWindow failed.");
    
    // Allocate screen buffer
    sChars = malloc(_columns * _rows * sizeof(unsigned short));
    if (!sChars) return _CGLerror("ERROR: Cannot allocate screen buffer.");
    memset(sChars, 0, sCharsArea * sizeof(GLushort));
    
    // Allocate texture
    sTexture = malloc(128 * 128);
    if (!sTexture) return _CGLerror("ERROR: Cannot allocate texture.");
    memset(sTexture, 0, 128 * 128);
    // Copy font to texture
    {
        for (int x = 0; x < 16; x++)
        {
            for (int y = 0; y < 16; y++)
            {
                for (int yy = 0; yy < 8; yy++)
                {
                    GLubyte row = font_data[((y * 16 + x) * 8) + yy];
                    for (int xx = 0; xx < 8; xx++)
                    {
                        GLubyte alpha = ((row >> (7 - xx)) & 1) * 255;
                        sTexture[((y * 8 + yy) * 128) + (x * 8 + xx)] = alpha;
                    }
                }
            }
        }
    }
    
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(1.0f / 128.0f, 1.0f / 128.0f, 1.0f);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, _columns * 8.0f, _rows * 8.0f, 0.0f, 0.0f, 1.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Create textures
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 128, 128, 0, GL_ALPHA, GL_UNSIGNED_BYTE, sTexture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    
    // Build VBOs
    size_t size;

    // Create Index Buffer
    GLuint indexHandle;
    size = _columns * _rows * 6 * sizeof(GLuint);
    sIndexBuffer = malloc(size);
    if (!sIndexBuffer) return _CGLerror("ERROR: Cannot allocate index vbo.");
    int len  = _columns * _rows * 6;
    for (int i = 0, v = 0; i < len; i += 6, v += 4)
    {
        sIndexBuffer[i + 0] = v + 0;
        sIndexBuffer[i + 1] = v + 1;
        sIndexBuffer[i + 2] = v + 2;
        sIndexBuffer[i + 3] = v + 2;
        sIndexBuffer[i + 4] = v + 3;
        sIndexBuffer[i + 5] = v + 1;
    }
    glGenBuffers(1, &indexHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, sIndexBuffer, GL_STATIC_DRAW);

    // Create Vertex Buffer
    GLuint vertexHandle;
    size = _columns * _rows * 4 * 3 * sizeof(GLfloat);
    sVertexBuffer = malloc(size);
    memset(sVertexBuffer, 0, size);
    if (!sVertexBuffer) return _CGLerror("ERROR: Cannot allocate vertex vbo.");
    for (int y = 0; y < _rows; y++)
    {
        for (int x = 0; x < _columns; x++)
        {
            static const float offsets[12] = {0,0,0,8,0,0,0,8,0,8,8,0};
            const GLuint addr = (y * _columns + x) * (4 * 3);
            int i = 0;
            while(i < 12)
            {
                sVertexBuffer[addr + i] = (x * 8) + offsets[i]; i++;
                sVertexBuffer[addr + i] = (y * 8) + offsets[i]; i++;
                i++;
            }
        }
    }
    
    glGenBuffers(1, &vertexHandle);
    glBindBuffer(GL_ARRAY_BUFFER, vertexHandle);
    glBufferData(GL_ARRAY_BUFFER, size, sVertexBuffer, GL_STATIC_DRAW);
    
    // Create Texture Coord Buffer
    GLuint textureCoordHandle;
    size = _columns * _rows * 4 * 2 * sizeof(GLshort);
    sTextureCoordBuffer = malloc(size);
    if (!sTextureCoordBuffer) return _CGLerror("ERROR: Cannot allocate texture coordinate vbo.");
    memset(sTextureCoordBuffer, 0, size);
    glGenBuffers(1, &textureCoordHandle);
    glBindBuffer(GL_ARRAY_BUFFER, textureCoordHandle);
    glBufferData(GL_ARRAY_BUFFER, size, sTextureCoordBuffer, GL_DYNAMIC_DRAW);
    
    // Create Foreground Color Buffer
    GLuint fgColorHandle;
    size = _columns * _rows * 4 * 4 * sizeof(GLubyte);
    sFgColorBuffer = malloc(size);
    if (!sFgColorBuffer) return _CGLerror("ERROR: Cannot allocate foreground color vbo.");
    memset(sFgColorBuffer, 255, size);
    glGenBuffers(1, &fgColorHandle);
    glBindBuffer(GL_ARRAY_BUFFER, fgColorHandle);
    glBufferData(GL_ARRAY_BUFFER, size, sFgColorBuffer, GL_DYNAMIC_DRAW);
    
    // Create Background Color Buffer
    GLuint bgColorHandle;
    size = _columns * _rows * 4 * 4 * sizeof(GLubyte);
    sBgColorBuffer = malloc(size);
    if (!sBgColorBuffer) return _CGLerror("ERROR: Cannot allocate background color vbo.");
    memset(sBgColorBuffer, 0, size);
    glGenBuffers(1, &bgColorHandle);
    glBindBuffer(GL_ARRAY_BUFFER, bgColorHandle);
    glBufferData(GL_ARRAY_BUFFER, size, sBgColorBuffer, GL_DYNAMIC_DRAW);
    
    CGLprint("Hello World");
    
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        sTickCallback();
        
        sBlinkTimer--;
        if (sBlinkTimer < 0)
        {
            sBlinkState = !sBlinkState;
            sBlinkTimer = BLINK_SPEED;
            if (!sDirty)
            {
                for (int y = 0; y < _rows; y++)
                {
                    for (int x = 0; x < _columns; x++)
                    {
                        const GLushort d = sChars[y * sCharsWidth + x];
                        const GLubyte blink = (d >> 15) & 1; // TODO: Implement
                        if (blink == 1)
                        {
                            sDirty = GL_TRUE;
                            break;
                        }
                    }
                    if (sDirty) break;
                }
            }
        }
        
        if (sDirty)
        {
            sDirty = GL_FALSE;
            static const float offsets[8] = {0,0,8,0,0,8,8,8};
            for (int y = 0; y < _rows; y++)
            {
                for (int x = 0; x < _columns; x++)
                {
                    const GLushort d = sChars[y * sCharsWidth + x];
                    const GLushort c = d & 255;
                    const int row = c / 16;
                    const int col = c % 16;
                    const GLubyte bg = (d >> 12) & 7;
                    const GLubyte bgr = sPalette[bg * 3 + 0];
                    const GLubyte bgg = sPalette[bg * 3 + 1];
                    const GLubyte bgb = sPalette[bg * 3 + 2];
                    const GLubyte blink = (d >> 15) & 1; // TODO: Implement
                    const GLubyte fg = (d >> 8) & 15;
                    const GLubyte fgr = (blink == 1 && sBlinkState) ? bgr : sPalette[fg * 3 + 0];
                    const GLubyte fgg = (blink == 1 && sBlinkState) ? bgg : sPalette[fg * 3 + 1];
                    const GLubyte fgb = (blink == 1 && sBlinkState) ? bgb : sPalette[fg * 3 + 2];
                    

                    const GLuint addr = (y * _columns + x) * (4 * 2);
                    int i = 0;
                    while(i < 8)
                    {
                        sTextureCoordBuffer[addr + i] = (col * 8) + offsets[i]; i++;
                        sTextureCoordBuffer[addr + i] = (row * 8) + offsets[i]; i++;
                    }
                    
                    const GLuint addr2 = (y * _columns + x) * (4 * 4);
                    i = 0;
                    while (i < 16)
                    {
                        sFgColorBuffer[addr2 + i] = fgr;
                        sBgColorBuffer[addr2 + i++] = bgr;
                        sFgColorBuffer[addr2 + i] = fgg;
                        sBgColorBuffer[addr2 + i++] = bgg;
                        sFgColorBuffer[addr2 + i] = fgb;
                        sBgColorBuffer[addr2 + i++] = bgb;
                        sFgColorBuffer[addr2 + i] = 255;
                        sBgColorBuffer[addr2 + i++] = 255;
                    }
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, textureCoordHandle);
            glBufferData(GL_ARRAY_BUFFER, size, sTextureCoordBuffer, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, fgColorHandle);
            glBufferData(GL_ARRAY_BUFFER, size, sFgColorBuffer, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, bgColorHandle);
            glBufferData(GL_ARRAY_BUFFER, size, sBgColorBuffer, GL_DYNAMIC_DRAW);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexHandle);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, 0);
        
        glBindBuffer(GL_ARRAY_BUFFER, bgColorHandle);
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexHandle);
        glDrawElements(GL_TRIANGLES, _columns * _rows * 6, GL_UNSIGNED_INT, 0);
        
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, texId);
        
        glBindBuffer(GL_ARRAY_BUFFER, textureCoordHandle);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_SHORT, 0, 0);
        
        glBindBuffer(GL_ARRAY_BUFFER, fgColorHandle);
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexHandle);
        glDrawElements(GL_TRIANGLES, _columns * _rows * 6, GL_UNSIGNED_INT, 0);
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);
        
        /* Poll for and process events */
        glfwPollEvents();
    }
    
    glfwTerminate();
    
    free(sChars);
    sChars = NULL;
    
    free(sTexture);
    sTexture = NULL;

    return 0;
}

/**
 *  Sets the default color attribute used.
 *  bits 0-3 = FG Color
 *  bits 4-6 = BG Color
 *  bit 7 = Blink
 */
void CGLsetAttrib(int _attrib)
{
    int pos = sCharsYPos * sCharsWidth + sCharsXPos;
    if (pos >= sCharsArea || pos < 0) return;

    CGLsetAttribXY(_attrib, sCharsXPos, sCharsYPos);

    sCharsXPos++;
    if (sCharsXPos >= sCharsWidth)
    {
        sCharsXPos = 0;
        sCharsYPos++;
    }
}

/**
 * Sets the attribute at the specified location
 */
void CGLsetAttribXY(int _attrib, int _col, int _row)
{
    int pos = _row * sCharsWidth + _col;
    if (pos >= sCharsArea || pos < 0) return;

    sChars[pos] = (sChars[pos] & 255) | (_attrib << 8);
    sDirty = GL_TRUE;
}

/**
 * Moves the cursor to the specified location
 */
void CGLgotoXY(int _col, int _row)
{
    if (_col < 0) return;
    if (_row < 0) return;
    if (_col * _row > sCharsArea) return;
    sCharsXPos = _col;
    sCharsYPos = _row;
}

/**
 * Print a formatted string
 */
void CGLprintf(const char * _format, ...)
{
    char buffer[sCharsArea];
    va_list args;
    va_start (args, _format);
    vsnprintf (buffer, sCharsArea, _format, args);
    CGLprint(buffer);
    va_end (args);
}

void CGLprint(const char * _string)
{
    int len = strlen(_string);
    for (int i = 0; i < len; ++i)
    {
        char c = _string[i];
        switch (c)
        {
            case '\n':
            {
                sCharsXPos = 0;
                sCharsYPos++;
                break;
            }
                
            default:
            {
                CGLputc(c);
                break;
            }
        }
    }
}

/**
 * Prints a single char to the screen
 */
void CGLputc(char _char)
{
    int pos = sCharsYPos * sCharsWidth + sCharsXPos++;
    if (pos >= sCharsArea || pos < 0) return;
    sChars[pos] = _char | sLastAttrib;
    sDirty = GL_TRUE;
    if (sCharsXPos == sCharsWidth)
    {
        sCharsXPos = 0;
        sCharsYPos++;
    }
}

/**
 * Prints a single char to the screen
 */
void CGLputcXY(char _char, int _col, int _row)
{
    int pos = _row * sCharsWidth + _col;
    if(pos >= sCharsArea || pos < 0) return;
    sChars[pos] = _char;
    sDirty = GL_TRUE;
}

/**
 * Closes the window and cleans up
 */
void CGLshutdown()
{
    
}
