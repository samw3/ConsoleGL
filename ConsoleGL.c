// ConsoleGL
//
// Licensed public domain

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLFW/glfw3.h>
#include "vincent.h"

static void (*sTickCallback)(void);
static GLboolean sDirty = GL_TRUE;
static GLushort *sChars;
static GLubyte *sTexture;

static GLuint *sIndexBuffer;
static GLfloat *sVertexBuffer;
static GLshort *sTextureCoordBuffer;

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
    sTickCallback = _callback;
    GLFWwindow* window;
    
    /* Initialize the library */
    if (!glfwInit()) return _CGLerror("ERROR: glfwInit failed.");
    
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(_columns * 8, _rows * 8, _windowTitle, NULL, NULL);
    if (!window) return _CGLerror("ERROR: glfwCreateWindow failed.");
    
    // Allocate screen buffer
    sChars = malloc(_columns * _rows * sizeof(unsigned short));
    if (!sChars) return _CGLerror("ERROR: Cannot allocate screen buffer.");
    
    // Allocate texture
    sTexture = malloc(128 * 128);
    if (!sTexture) return _CGLerror("ERROR: Cannot allocate texture.");
    memset(sTexture, 0, 128 * 128);
    // Copy font to texture
    {
        for (int x = 0; x < 16; x++)
        {
            for (int y = 0; y < 8; y++)
            {
                for (int yy = 0; yy < 8; yy++)
                {
                    GLubyte row = vincent_data[y * 16 + x][yy];
                    for (int xx = 0; xx < 8; xx++)
                    {
                        GLubyte alpha = ((row >> (7 - xx)) & 1) * 255;
                        sTexture[((y * 8 + yy) * 128) + (x * 8 + xx)] = alpha;
                    }
                }
            }
        }
    }
    
    // Write some test data to screen buffer
    {
        int len = _rows * _columns;
        for (int i = 0; i < len; i++)
        {
            sChars[i] = i & 127;
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
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    
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
    for (int y = 0; y < _rows; y++)
    {
        for (int x = 0; x < _columns; x++)
        {
            static const float offsets[8] = {0,0,8,0,0,8,8,8};
            int i = 0;
            const GLuint addr = (y * _columns + x) * (4 * 2);
            while(i < 8)
            {
                sTextureCoordBuffer[addr + i] = (x * 8) + offsets[i]; i++;
                sTextureCoordBuffer[addr + i] = (y * 8) + offsets[i]; i++;
            }
        }
    }
    glGenBuffers(1, &textureCoordHandle);
    glBindBuffer(GL_ARRAY_BUFFER, textureCoordHandle);
    glBufferData(GL_ARRAY_BUFFER, size, sTextureCoordBuffer, GL_DYNAMIC_DRAW);
    
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        sTickCallback();
        
        if (sDirty)
        {
            sDirty = GL_FALSE;
            
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, texId);
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexHandle);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, 0);
        
        glBindBuffer(GL_ARRAY_BUFFER, textureCoordHandle);
//        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_SHORT, 0, 0);
        
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
    
}

/**
 * Sets the attribute at the specified location
 */
void CGLsetAttribXY(int _attrib, int _col, int _row)
{
    
}

/**
 * Moves the cursor to the specified location
 */
void CGLgotoXY(int _col, int _row)
{
    
}

/**
 * Print a formatted string
 */
void CGLprintf(const char * _format, ...)
{
    
}

/**
 * Prints a single char to the screen
 */
void CGLputc(char _char)
{
    
}

/**
 * Closes the window and cleans up
 */
void CGLshutdown()
{
    
}
