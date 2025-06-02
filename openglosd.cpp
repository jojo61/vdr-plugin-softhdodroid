///
/// @file video.h   @brief Video module header file
///
/// Copyright (c) 2021 by Jojo61.  All Rights Reserved.
///
/// Contributor(s):
///
/// License: AGPLv3
///
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as
/// published by the Free Software Foundation, either version 3 of the
/// License.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU Affero General Public License for more details.
///
/// $Id: 83cd827a8744e8c80c8adba6cb87653b0ad58c45 $
//////////////////////////////////////////////////////////////////////////////

#define __STL_CONFIG_H
#include <algorithm>
#include "openglosd.h"


//#include <GL/glew.h>
#define EGL_EGLEXT_PROTOTYPES
#define MESA_EGL_NO_X11_HEADERS
#include <EGL/egl.h>
#include <libdrm/drm_fourcc.h>
#include <EGL/eglext.h>

#include "ge2d.h"
#include "ge2d_cmd.h"
#include "ion.h"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

// EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT          0x3270

#define EGL_LINUX_DRM_FOURCC_EXT        0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT       0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT   0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT    0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT       0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT   0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT    0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT       0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT   0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT    0x327A
#define EGL_YUV_COLOR_SPACE_HINT_EXT    0x327B

#define USE_DRM


//int fd = -1;
int ge2d_fd = -1;
int ion_fd = -1;

static EGLSurface eglSurface;
static EGLDisplay eglDisplay;
static EGLContext eglContext;

int MyOsdWidth, MyOsdHeight;



extern "C" void ClearDisplay();
extern "C" void WaitVsync();;
extern "C" void ClearCursor(int);
extern "C" void makejpg(uint8_t * data, int width, int height);

/****************************************************************************************
* Helpers
****************************************************************************************/

void ConvertColor(const GLint & colARGB, glm::vec4 & col)
{
    col.a = ((colARGB & 0xFF000000) >> 24) / 255.0;
    col.r = ((colARGB & 0x00FF0000) >> 16) / 255.0;
    col.g = ((colARGB & 0x0000FF00) >> 8) / 255.0;
    col.b = ((colARGB & 0x000000FF)) / 255.0;
}

/****************************************************************************************
* cShader
****************************************************************************************/


const char *glversion = "#version 300 es   ";
//const char *glversion = "#version 300 es ";

const char *rectVertexShader = "%s\n \
\
layout (location = 0) in vec2 position; \
out vec4 rectCol; \
uniform vec4 inColor; \
uniform mat4 projection; \
\
void main() \
{ \
    gl_Position = projection * vec4(position.x, position.y, 0.0, 1.0); \
    rectCol = inColor; \
} \
";

const char *rectFragmentShader = "%s\n \
\
precision mediump float; \
in vec4 rectCol; \
out vec4 color; \
\
void main() \
{ \
    color = rectCol; \
} \
";

const char *textureVertexShader = "%s\n \
\
layout (location = 0) in vec2 position; \
layout (location = 1) in vec2 texCoords; \
\
out vec2 TexCoords; \
out vec4 alphaValue;\
\
uniform mat4 projection; \
uniform vec4 alpha; \
\
void main() \
{ \
    gl_Position = projection * vec4(position.x, position.y, 0.0, 1.0); \
    TexCoords = texCoords; \
    alphaValue = alpha; \
} \
";

const char *textureFragmentShader = "%s\n \
precision mediump float; \
in vec2 TexCoords; \
in vec4 alphaValue; \
out vec4 color; \
\
uniform sampler2D screenTexture; \
\
void main() \
{ \
	color = texture(screenTexture, TexCoords) * alphaValue; \
} \
";

const char *imageVertexShader = "%s\n \
\
layout (location = 0) in vec2 position; \
layout (location = 1) in vec2 texCoords; \
\
out vec2 TexCoords; \
out vec4 alphaValue;\
\
uniform mat4 projection; \
uniform vec4 alpha; \
\
void main() \
{ \
    gl_Position = projection * vec4(position.x, position.y, 0.0, 1.0); \
    TexCoords = texCoords; \
    alphaValue = alpha; \
} \
";

const char *imageFragmentShader = "%s\n \
precision mediump float; \
in vec2 TexCoords; \
in vec4 alphaValue; \
out vec4 color; \
\
uniform sampler2D screenTexture; \
\
void main() \
{ \
    vec4 temp; \
	temp = texture(screenTexture, TexCoords) * alphaValue; \
    color.r = temp.b; \
    color.g = temp.g; \
    color.b = temp.r; \
    color.a = temp.a; \
} \
";

const char *textVertexShader = "%s\n \
\
layout (location = 0) in vec2 position; \
layout (location = 1) in vec2 texCoords; \
\
out vec2 TexCoords; \
out vec4 textColor; \
\
uniform mat4 projection; \
uniform vec4 inColor; \
\
void main() \
{ \
    gl_Position = projection * vec4(position.x, position.y, 0.0, 1.0); \
    TexCoords = texCoords; \
    textColor = inColor; \
} \
";

const char *textFragmentShader = "%s\n \
precision mediump float; \
in vec2 TexCoords; \
in vec4 textColor; \
\
out vec4 color; \
\
uniform sampler2D glyphTexture; \
\
void main() \
{  \
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(glyphTexture, TexCoords).r); \
    color = textColor * sampled; \
} \
";

///
/// GLX check error.
///
#define GlxCheck(void)\
{\
    GLenum err;\
\
    if ((err = glGetError()) != GL_NO_ERROR) {\
        esyslog( "video/glx: error %s:%d %d \n",__FILE__,__LINE__, err);\
    }\
}

static cShader *Shaders[stCount];

void cShader::Use(void)
{
    glUseProgram(id);
}

bool cShader::Load(eShaderType type)
{
    this->type = type;

    const char *vertexCode = NULL;
    const char *fragmentCode = NULL;

    switch (type) {
        case stRect:
            vertexCode = rectVertexShader;
            fragmentCode = rectFragmentShader;
            break;
        case stTexture:
            vertexCode = textureVertexShader;
            fragmentCode = textureFragmentShader;
            break;
        case stText:
            vertexCode = textVertexShader;
            fragmentCode = textFragmentShader;
            break;
        case stImage:
            vertexCode = imageVertexShader;
            fragmentCode = imageFragmentShader;
            break;
        default:
            esyslog("[softhddev]unknown shader type\n");
            break;
    }

    if (vertexCode == NULL || fragmentCode == NULL) {
        esyslog("[softhddev]ERROR reading shader\n");
        return false;
    }

    if (!Compile(vertexCode, fragmentCode)) {
        esyslog("[softhddev]ERROR compiling shader\n");
        return false;
    }
    return true;
}

void cShader::SetFloat(const GLchar * name, GLfloat value)
{
    glUniform1f(glGetUniformLocation(id, name), value);
}

void cShader::SetInteger(const GLchar * name, GLint value)
{
    glUniform1i(glGetUniformLocation(id, name), value);
}

void cShader::SetVector2f(const GLchar * name, GLfloat x, GLfloat y)
{
    glUniform2f(glGetUniformLocation(id, name), x, y);
}

void cShader::SetVector3f(const GLchar * name, GLfloat x, GLfloat y, GLfloat z)
{
    glUniform3f(glGetUniformLocation(id, name), x, y, z);
}

void cShader::SetVector4f(const GLchar * name, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    glUniform4f(glGetUniformLocation(id, name), x, y, z, w);
}

void cShader::SetMatrix4(const GLchar * name, const glm::mat4 & matrix)
{
    glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(matrix));
}

bool cShader::Compile(const char *vertexCode, const char *fragmentCode)
{
    GLuint sVertex, sFragment;
    char *buffer = (char *)malloc(1000);

    // Vertex Shader
    GlxCheck();
    sVertex = glCreateShader(GL_VERTEX_SHADER);
    GlxCheck();
    sprintf(buffer, vertexCode, glversion);
    glShaderSource(sVertex, 1, (const GLchar **)&buffer, NULL);
    glCompileShader(sVertex);
    GlxCheck();

    //esyslog("[softhddev]:SHADER:VERTEX %s\n",buffer);
    if (!CheckCompileErrors(sVertex)) {
        free(buffer);
        return false;
    }
    // Fragment Shader
    sFragment = glCreateShader(GL_FRAGMENT_SHADER);
    sprintf(buffer, fragmentCode, glversion);
    glShaderSource(sFragment, 1, (const GLchar **)&buffer, NULL);
    glCompileShader(sFragment);
    // esyslog("[softhddev]:SHADER:FRAGMENT %s\n",fragmentCode);
    if (!CheckCompileErrors(sFragment)) {
        free(buffer);
        return false;
    }
    // link Program
    id = glCreateProgram();
    glAttachShader(id, sVertex);
    glAttachShader(id, sFragment);
    glLinkProgram(id);
    if (!CheckCompileErrors(id, true))
        return false;
    // Delete the shaders as they're linked into our program now and no longer necessery
    glDeleteShader(sVertex);
    glDeleteShader(sFragment);
    free(buffer);
    return true;
}

bool cShader::CheckCompileErrors(GLuint object, bool program)
{
    GLint success;
    GLchar infoLog[1024];

    if (!program) {
        glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(object, 1024, NULL, infoLog);
            esyslog("\n[softhddev]:SHADER: Compile-time error: Type: %d - \n>%s<\n", type, infoLog);
            return false;
        }
    } else {
        glGetProgramiv(object, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(object, 1024, NULL, infoLog);
            esyslog("[softhddev]:SHADER: Link-time error: Type: %d - \n>%s<\n", type, infoLog);
            return false;
        }
    }
    return true;
}

#define KERNING_UNKNOWN  (-10000)

/****************************************************************************************
* cOglGlyph
****************************************************************************************/
cOglGlyph::cOglGlyph(FT_ULong charCode, FT_BitmapGlyph ftGlyph)
{
    this->charCode = charCode;
    bearingLeft = ftGlyph->left;
    bearingTop = ftGlyph->top;
    width = ftGlyph->bitmap.width;
    height = ftGlyph->bitmap.rows;
    advanceX = ftGlyph->root.advance.x >> 16;   //value in 1/2^16 pixel
    LoadTexture(ftGlyph);
}

cOglGlyph::~cOglGlyph(void)
{

}

int cOglGlyph::GetKerningCache(FT_ULong prevSym)
{
    for (int i = kerningCache.Size(); --i > 0;) {
        if (kerningCache[i].prevSym == prevSym)
            return kerningCache[i].kerning;
    }
    return KERNING_UNKNOWN;
}

void cOglGlyph::SetKerningCache(FT_ULong prevSym, int kerning)
{
    kerningCache.Append(tKerning(prevSym, kerning));
}

void cOglGlyph::BindTexture(void)
{
    glBindTexture(GL_TEXTURE_2D, texture);
}

void cOglGlyph::LoadTexture(FT_BitmapGlyph ftGlyph)
{
    // Disable byte-alignment restriction

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ftGlyph->bitmap.width, ftGlyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
        ftGlyph->bitmap.buffer);
    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

}




/****************************************************************************************
* cOglFont
****************************************************************************************/
FT_Library cOglFont::ftLib = 0;

cList < cOglFont > *cOglFont::fonts = 0;
bool cOglFont::initiated = false;

cOglFont::cOglFont(const char *fontName, int charHeight):name(fontName)
{
    size = charHeight;
    height = 0;
    bottom = 0;

    int error = FT_New_Face(ftLib, fontName, 0, &face);

    if (error)
        esyslog("[softhddev]ERROR: failed to open %s!", *name);

    FT_Set_Char_Size(face, 0, charHeight * 64, 0, 0);
    height = (face->size->metrics.ascender - face->size->metrics.descender + 63) / 64;
    bottom = abs((face->size->metrics.descender - 63) / 64);
}

cOglFont::~cOglFont(void)
{
    FT_Done_Face(face);
}

cOglFont *cOglFont::Get(const char *name, int charHeight)
{
    if (!fonts)
        Init();

    cOglFont *font;

    for (font = fonts->First(); font; font = fonts->Next(font))
        if (!strcmp(font->Name(), name) && charHeight == font->Size()) {
            return font;
        }
    font = new cOglFont(name, charHeight);
    fonts->Add(font);
    return font;
}

void cOglFont::Init(void)
{

    if (FT_Init_FreeType(&ftLib)) {
        esyslog("[softhddev]failed to initialize FreeType library!");
        return;
    }
    fonts = new cList < cOglFont >;
    initiated = true;
}

void cOglFont::Cleanup(void)
{
    if (!initiated)
        return;
    delete fonts;

    fonts = 0;
	if (ftLib) {
		if (FT_Done_FreeType(ftLib))
			esyslog("failed to deinitialize FreeType library!");
	}
	ftLib = 0;
}

cOglGlyph *cOglFont::Glyph(FT_ULong charCode) const
{
    // Non-breaking space:
    if (charCode == 0xA0)
        charCode = 0x20;

    // Lookup in cache:
    for (cOglGlyph * g = glyphCache.First(); g; g = glyphCache.Next(g)) {
        if (g->CharCode() == charCode) {
            return g;
        }
    }

    FT_UInt glyph_index = FT_Get_Char_Index(face, charCode);

    FT_Int32 loadFlags = FT_LOAD_NO_BITMAP;

    // Load glyph image into the slot (erase previous one):
    int error = FT_Load_Glyph(face, glyph_index, loadFlags);

    if (error) {
        esyslog("[softhddev]FT_Error (0x%02x) : %s\n", FT_Errors[error].code, FT_Errors[error].message);
        return NULL;
    }

    FT_Glyph ftGlyph;
    FT_Stroker stroker;

    error = FT_Stroker_New(ftLib, &stroker);
    if (error) {
        esyslog("[softhddev]FT_Stroker_New FT_Error (0x%02x) : %s\n", FT_Errors[error].code, FT_Errors[error].message);
        return NULL;
    }
    float outlineWidth = 0.25f;

    FT_Stroker_Set(stroker, (int)(outlineWidth * 64), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

    error = FT_Get_Glyph(face->glyph, &ftGlyph);
    if (error) {
        esyslog("[softhddev]FT_Get_Glyph FT_Error (0x%02x) : %s\n", FT_Errors[error].code, FT_Errors[error].message);
        return NULL;
    }

    error = FT_Glyph_StrokeBorder(&ftGlyph, stroker, 0, 1);
    if (error) {
        esyslog("[softhddev]FT_Glyph_StrokeBorder FT_Error (0x%02x) : %s\n", FT_Errors[error].code,
            FT_Errors[error].message);
        return NULL;
    }
    FT_Stroker_Done(stroker);

    error = FT_Glyph_To_Bitmap(&ftGlyph, FT_RENDER_MODE_NORMAL, 0, 1);
    if (error) {
        esyslog("[softhddev]FT_Glyph_To_Bitmap FT_Error (0x%02x) : %s\n", FT_Errors[error].code,
            FT_Errors[error].message);
        return NULL;
    }

    cOglGlyph *Glyph = new cOglGlyph(charCode, (FT_BitmapGlyph) ftGlyph);

    glyphCache.Add(Glyph);
    FT_Done_Glyph(ftGlyph);

    return Glyph;
}

int cOglFont::Kerning(cOglGlyph * glyph, FT_ULong prevSym) const
{
    int kerning = 0;

    if (glyph && prevSym) {
        kerning = glyph->GetKerningCache(prevSym);
        if (kerning == KERNING_UNKNOWN) {
            FT_Vector delta;
            FT_UInt glyph_index = FT_Get_Char_Index(face, glyph->CharCode());
            FT_UInt glyph_index_prev = FT_Get_Char_Index(face, prevSym);

            FT_Get_Kerning(face, glyph_index_prev, glyph_index, FT_KERNING_DEFAULT, &delta);
            kerning = delta.x / 64;
            glyph->SetKerningCache(prevSym, kerning);
        }
    }
    return kerning;
}

/****************************************************************************************
* cOglFb
****************************************************************************************/
cOglFb::cOglFb(GLint width, GLint height, GLint viewPortWidth, GLint viewPortHeight)
{
    initiated = false;
    fb = 0;
    texture = 0;

    this->width = width;
    this->height = height;
    this->viewPortWidth = viewPortWidth;
    this->viewPortHeight = viewPortHeight;
    if (width != viewPortWidth || height != viewPortHeight)
        scrollable = true;
    else
        scrollable = false;

}

cOglFb::~cOglFb(void)
{
    if (texture)
        glDeleteTextures(1, &texture);
    if (fb)
        glDeleteFramebuffers(1, &fb);
}

bool cOglFb::Init(void)
{
    initiated = true;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GlxCheck();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GlxCheck();
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        esyslog("[softhddev]ERROR: %d Framebuffer is not complete!\n", __LINE__);

        return false;
    }

    return true;
}

void cOglFb::Bind(void)
{
    if (!initiated)
        Init();
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
}

void cOglFb::BindRead(void)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
}

void cOglFb::BindWrite(void)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);
}

void cOglFb::Unbind(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool cOglFb::BindTexture(void)
{
    if (!initiated)
        return false;
    glBindTexture(GL_TEXTURE_2D, texture);
    return true;
}

void cOglFb::Blit(GLint destX1, GLint destY1, GLint destX2, GLint destY2)
{
    //glBlitFramebuffer(0, 0, width, height, destX1, destY1, destX2, destY2, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glFlush();
}

/****************************************************************************************
* cOglOutputFb
****************************************************************************************/
cOglOutputFb::cOglOutputFb(GLint width, GLint height):cOglFb(width, height, width, height)
{
    // surface = 0;
    initiated = false;
    fb = 0;
    texture = 0;
}

cOglOutputFb::~cOglOutputFb(void)
{
    // glVDPAUUnregisterSurfaceNV(surface);
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &fb);
}

extern int DmaBufferHandle, VideoWindowWidth, VideoWindowHeight;
bool cOglOutputFb::Init(void)
{
    initiated = true;

	glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    void *frameBufferImage = {0};

    if (DmaBufferHandle >= 0 ) {
        EGLint img_attrs[] = {
                EGL_WIDTH, VideoWindowWidth,
                EGL_HEIGHT, VideoWindowHeight,
                EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,	//DRM_FORMAT_RGBA8888
                EGL_DMA_BUF_PLANE0_FD_EXT,	DmaBufferHandle,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, VideoWindowWidth * 4,
                EGL_NONE
            };

        frameBufferImage = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, img_attrs);
        GlxCheck();
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    GlxCheck();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);


    if (DmaBufferHandle >= 0)
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, frameBufferImage);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GlxCheck();
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        esyslog("[softhddev]ERROR::cOglOutputFb: Framebuffer is not complete!");
        return false;
    }
    return true;
}

void cOglOutputFb::BindWrite(void)
{
    if (!initiated)
        Init();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);
}

void cOglOutputFb::Unbind(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/****************************************************************************************
* cOglVb
****************************************************************************************/
static cOglVb *VertexBuffers[vbCount];

cOglVb::cOglVb(int type)
{
    this->type = (eVertexBufferType) type;
    vao = 0;
    vbo = 0;
    sizeVertex1 = 0;
    sizeVertex2 = 0;
    numVertices = 0;
    drawMode = 0;
}

cOglVb::~cOglVb(void)
{
}

bool cOglVb::Init(void)
{

    if (type == vbTexture) {
        // Texture VBO definition
        sizeVertex1 = 2;
        sizeVertex2 = 2;
        numVertices = 6;
        drawMode = GL_TRIANGLES;
        shader = stTexture;
    } else if (type == vbRect) {
        // Rectangle VBO definition
        sizeVertex1 = 2;
        sizeVertex2 = 0;
        numVertices = 4;
        drawMode = GL_TRIANGLE_FAN;
        shader = stRect;
    } else if (type == vbEllipse) {
        // Ellipse VBO definition
        sizeVertex1 = 2;
        sizeVertex2 = 0;
        numVertices = 182;
        drawMode = GL_TRIANGLE_FAN;
        shader = stRect;
    } else if (type == vbSlope) {
        // Slope VBO definition
        sizeVertex1 = 2;
        sizeVertex2 = 0;
        numVertices = 102;
        drawMode = GL_TRIANGLE_FAN;
        shader = stRect;
    } else if (type == vbText) {
        // Text VBO definition
        sizeVertex1 = 2;
        sizeVertex2 = 2;
        numVertices = 6;
        drawMode = GL_TRIANGLES;
        shader = stText;
    } else if (type == vbImage) {
        // Image VBO definition
        sizeVertex1 = 2;
        sizeVertex2 = 2;
        numVertices = 6;
        drawMode = GL_TRIANGLES;
        shader = stImage;
    }
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * (sizeVertex1 + sizeVertex2) * numVertices, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, sizeVertex1, GL_FLOAT, GL_FALSE, (sizeVertex1 + sizeVertex2) * sizeof(GLfloat),
        (GLvoid *) 0);
    if (sizeVertex2 > 0) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, sizeVertex2, GL_FLOAT, GL_FALSE, (sizeVertex1 + sizeVertex2) * sizeof(GLfloat),
            (GLvoid *) (sizeVertex1 * sizeof(GLfloat)));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void cOglVb::Bind(void)
{
    glBindVertexArray(vao);
}

void cOglVb::Unbind(void)
{
    glBindVertexArray(0);
}

void cOglVb::ActivateShader(void)
{
    Shaders[shader]->Use();
}

void cOglVb::EnableBlending(void)
{
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void cOglVb::DisableBlending(void)
{
    glDisable(GL_BLEND);
}

void cOglVb::SetShaderColor(GLint color)
{
    glm::vec4 col;
    ConvertColor(color, col);
    Shaders[shader]->SetVector4f("inColor", col.r, col.g, col.b, col.a);
}

void cOglVb::SetShaderAlpha(GLint alpha)
{
    Shaders[shader]->SetVector4f("alpha", 1.0f, 1.0f, 1.0f, (GLfloat) (alpha) / 255.0f);
}

void cOglVb::SetShaderProjectionMatrix(GLint width, GLint height)
{
    glm::mat4 projection = glm::ortho(0.0f, (GLfloat) width, (GLfloat) height, 0.0f, -1.0f, 1.0f);
    Shaders[shader]->SetMatrix4("projection", projection);
}

void cOglVb::SetVertexData(GLfloat * vertices, int count)
{
    if (count == 0)
        count = numVertices;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * (sizeVertex1 + sizeVertex2) * count, vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void cOglVb::DrawArrays(int count)
{
    if (count == 0)
        count = numVertices;
    glDrawArrays(drawMode, 0, count);
    glFlush();
}

/****************************************************************************************
* cOpenGLCmd
****************************************************************************************/
//------------------ cOglCmdInitOutputFb --------------------
cOglCmdInitOutputFb::cOglCmdInitOutputFb(cOglOutputFb * oFb):cOglCmd(NULL)
{
    this->oFb = oFb;
}

bool cOglCmdInitOutputFb::Execute(void)
{
    bool ok = oFb->Init();

    oFb->Unbind();
    return ok;
}

//------------------ cOglCmdInitFb --------------------
cOglCmdInitFb::cOglCmdInitFb(cOglFb * fb, cCondWait * wait):cOglCmd(fb)
{
    this->wait = wait;
}

bool cOglCmdInitFb::Execute(void)
{
    bool ok = fb->Init();

    fb->Unbind();
    if (wait)
        wait->Signal();
    return ok;
}

//------------------ cOglCmdDeleteFb --------------------
cOglCmdDeleteFb::cOglCmdDeleteFb(cOglFb * fb):cOglCmd(fb)
{
}

bool cOglCmdDeleteFb::Execute(void)
{
    if (fb)
        delete fb;

    return true;
}

//------------------ cOglCmdRenderFbToBufferFb --------------------
cOglCmdRenderFbToBufferFb::cOglCmdRenderFbToBufferFb(cOglFb * fb, cOglFb * buffer, GLint x, GLint y, GLint transparency, GLint drawPortX, GLint drawPortY,bool alphablending):cOglCmd
    (fb)
{
    this->buffer = buffer;
    this->x = (GLfloat) x;
    this->y = (GLfloat) y;
    this->drawPortX = (GLfloat) drawPortX;
    this->drawPortY = (GLfloat) drawPortY;
    this->transparency = (alphablending ? transparency : ALPHA_OPAQUE);
    this->alphablending = alphablending;

}

bool cOglCmdRenderFbToBufferFb::Execute(void)
{
    GLfloat x2 = x + fb->ViewportWidth();   //right
    GLfloat y2 = y + fb->ViewportHeight();  //bottom

    GLfloat texX1 = drawPortX / (GLfloat) fb->Width();
    GLfloat texY1 = drawPortY / (GLfloat) fb->Height();
    GLfloat texX2 = texX1 + 1.0f;
    GLfloat texY2 = texY1 + 1.0f;

    if (fb->Scrollable()) {
        GLfloat pageHeight = (GLfloat) fb->ViewportHeight() / (GLfloat) fb->Height();

        texX1 = abs(drawPortX) / (GLfloat) fb->Width();
        texY1 = 1.0f - pageHeight - abs(drawPortY) / (GLfloat) fb->Height();
        texX2 = texX1 + (GLfloat) fb->ViewportWidth() / (GLfloat) fb->Width();
//        x2 = x + fb->Width();
        texY2 = texY1 + pageHeight;
    }
    GLfloat quadVertices[] = {
        // Pos    // TexCoords
        x, y, texX1, texY2,             //left top
        x, y2, texX1, texY1,            //left bottom
        x2, y2, texX2, texY1,           //right bottom

        x, y, texX1, texY2,             //left top
        x2, y2, texX2, texY1,           //right bottom
        x2, y, texX2, texY2             //right top
    };

    VertexBuffers[vbTexture]->ActivateShader();
    VertexBuffers[vbTexture]->SetShaderAlpha(transparency);
    VertexBuffers[vbTexture]->SetShaderProjectionMatrix(buffer->Width(), buffer->Height());

    buffer->Bind();

    if (!fb->BindTexture())
        return false;
    if (!alphablending)
        VertexBuffers[vbTexture]->DisableBlending();

    VertexBuffers[vbTexture]->Bind();
    VertexBuffers[vbTexture]->SetVertexData(quadVertices);
    VertexBuffers[vbTexture]->DrawArrays();
    VertexBuffers[vbTexture]->Unbind();

    if (!alphablending)
        VertexBuffers[vbTexture]->EnableBlending();
    buffer->Unbind();

    return true;
}

//------------------ cOglCmdCopyBufferToOutputFb --------------------
cOglCmdCopyBufferToOutputFb::cOglCmdCopyBufferToOutputFb(cOglFb * fb, cOglOutputFb * oFb, GLint x, GLint y):cOglCmd(fb)
{
    this->oFb = oFb;
    this->x = x;
    this->y = y;
}

unsigned char posd[3840*2160*4];
extern int OsdShown,OsdIsClosing,myKernel;
extern "C" int amlSetInt(char *, int);

bool cOglCmdCopyBufferToOutputFb::Execute(void)
{
    static int Opening=0;
    char path[] = "/sys/class/graphics/fb0/blank";
    if (OsdIsClosing || Opening)
        return true;
    Opening = 1;
    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    OsdShown = 1;
    if (DmaBufferHandle >= 0) {
        fb->BindRead();
        oFb->BindWrite();

        glClearColor(0,0,0,0) ;
        glClear(GL_COLOR_BUFFER_BIT);

        GLfloat x2 = x + (GLfloat)fb->Width();
        y = oFb->Height() - y;
        GLfloat y2 = y - (GLfloat)fb->Height();

        GLfloat texX1 = 0.0f;
        GLfloat texX2 = 1.0f;
        GLfloat texY1 = 1.0f;
        GLfloat texY2 = 0.0f;

        GLfloat quadVertices[] = {
            // Pos    // TexCoords
        (GLfloat)x ,  (GLfloat)y ,  texX1, texY1,          //left top
        (GLfloat)x ,  (GLfloat)y2,  texX1, texY2,          //left bottom
        (GLfloat)x2,  (GLfloat)y2,  texX2, texY2,          //right bottom
        (GLfloat)x ,  (GLfloat)y ,  texX1, texY1,          //left top
        (GLfloat)x2,  (GLfloat)y2,  texX2, texY2,          //right bottom
        (GLfloat)x2,  (GLfloat)y ,  texX2, texY1           //right top
        };

        VertexBuffers[vbTexture]->ActivateShader();
        VertexBuffers[vbTexture]->SetShaderAlpha(255);
        VertexBuffers[vbTexture]->SetShaderProjectionMatrix(oFb->Width(), oFb->Height());
        VertexBuffers[vbTexture]->DisableBlending();
        //VertexBuffers[vbTexture]->SetShaderBorderColor(bcolor);

        glViewport(0, 0, oFb->Width(), oFb->Height());
        if (!fb->BindTexture()) {
            Opening = 0;
            return false;
        }

        VertexBuffers[vbTexture]->Bind();
        VertexBuffers[vbTexture]->SetVertexData(quadVertices);
        VertexBuffers[vbTexture]->DrawArrays();
        VertexBuffers[vbTexture]->Unbind();
        VertexBuffers[vbTexture]->EnableBlending();
        glFlush();

        oFb->Unbind();
        fb->BindRead();
        if (myKernel == 5) {
            usleep(25000);
	        amlSetInt(path, 0 );
        }
        Opening = 0;
        return true;
    }
    //return true;

#if 1
    fb->BindRead();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, fb->Width(), fb->Height(), GL_RGBA, GL_UNSIGNED_BYTE, posd);
    glFlush();

     // Allocate a buffer
    int stride = ALIGN(fb->Width() * 4, 64);
    size_t size =  stride * fb->Height();

    ion_allocation_data allocation_data = { 0 };
    allocation_data.len = size;
    allocation_data.align = 64;
    allocation_data.heap_id_mask = (1 << ION_HEAP_TYPE_DMA);
    allocation_data.flags = 0;

    int io = ioctl(ion_fd, ION_IOC_ALLOC, &allocation_data);
    if (io != 0)
    {
        printf("ION_IOC_ALLOC failed.");
    }

    IonSurface surface = { 0 };
    surface.length = size;
    surface.stride = stride;
    surface.ion_handle = allocation_data.handle;

    //surface.map_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, ionData.fd, 0);
    surface.rect.X = x;
    surface.rect.Y = y;
    surface.rect.Width = fb->Width();
    surface.rect.Height = fb->Height();
    surface.color.R = 0;
    surface.color.G = 0;
    surface.color.B = 0;
    surface.color.A = 0;
    surface.z_order = 0.0;


    ion_fd_data ionData = { 0 };
    ionData.handle = allocation_data.handle;

    io = ioctl(ion_fd, ION_IOC_SHARE, &ionData);
    if (io != 0)
    {
        printf("ION_IOC_SHARE failed.\n");
    }

    surface.share_fd = ionData.fd;


    // Copy data
    void* map_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, ionData.fd, 0);
    if (map_ptr == MAP_FAILED)
    {
        printf("mmap failed.\n");
    }


    uint8_t* src_ptr = posd;
    uint8_t* dst_ptr = (uint8_t*)map_ptr;
    dst_ptr += stride * (fb->Height() - 1);
    for(int i = 0; i < fb->Height(); ++i)
    {
        memcpy(dst_ptr, src_ptr, fb->Width()*4);

        src_ptr += fb->Width() * 4;
        dst_ptr -= stride;

    };

    munmap(map_ptr, surface.length);

        // Blit
    config_para_ex_ion_s blit_config = { 0 };
    blit_config.alu_const_color = 0xffffffff;

    blit_config.dst_para.mem_type = CANVAS_OSD0;
    blit_config.dst_para.format = GE2D_FORMAT_S32_ABGR;

    blit_config.dst_para.left = 0;
    blit_config.dst_para.top = 0;
    blit_config.dst_para.width = MyOsdWidth; //fb->Width();
    blit_config.dst_para.height = MyOsdHeight; // fb->Height();
    blit_config.dst_para.x_rev = 0;
    blit_config.dst_para.y_rev = 0;
    blit_config.src_para.mem_type = CANVAS_ALLOC;
    blit_config.src_para.format = GE2D_FORMAT_S32_ABGR;

    blit_config.src_para.left = 0;
    blit_config.src_para.top = 0;
    blit_config.src_para.width = surface.rect.Width;
    blit_config.src_para.height = surface.rect.Height;
    blit_config.src_para.x_rev = 0;
    blit_config.src_para.y_rev = 0;

    blit_config.src_planes[0].shared_fd = surface.share_fd;
    blit_config.src_planes[0].w = surface.stride / 4;
    blit_config.src_planes[0].h = surface.rect.Height;

    io = ioctl(ge2d_fd, GE2D_CONFIG_EX_ION, &blit_config);
    if (io < 0)
    {
        printf("GE2D_CONFIG failed. Render\n");
    }


    ge2d_para_s blitRect = { 0 };

    blitRect.src1_rect.x = 0;
    blitRect.src1_rect.y = 0;
    blitRect.src1_rect.w = surface.rect.Width;
    blitRect.src1_rect.h = surface.rect.Height;

    blitRect.dst_rect.x = surface.rect.X ;
    blitRect.dst_rect.y = surface.rect.Y ;
    blitRect.dst_rect.w = surface.rect.Width ;
    blitRect.dst_rect.h = surface.rect.Height ;

    io = ioctl(ge2d_fd, GE2D_STRETCHBLIT, &blitRect);
    if (io < 0)
    {
        printf("GE2D_BLIT_NOALPHA failed.\n");
    }


    // Free resources
    close(surface.share_fd);

    ion_handle_data ionHandleData = { 0 };
    ionHandleData.handle = surface.ion_handle;

    io = ioctl(ion_fd, ION_IOC_FREE, &ionHandleData);
    if (io != 0)
    {
        printf("ION_IOC_FREE failed.\n");
    }

    //WaitVsync();

  //  eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext);
    //ActivateOsd(oFb->texture, x, y, fb->Width(), fb->Height());
    return true;
#endif
}

//------------------ cOglCmdFill --------------------
cOglCmdFill::cOglCmdFill(cOglFb * fb, GLint color):cOglCmd(fb)
{
    this->color = color;
}

bool cOglCmdFill::Execute(void)
{
    glm::vec4 col;
    ConvertColor(color, col);
    fb->Bind();
    glClearColor(col.r, col.g, col.b, col.a);
    glClear(GL_COLOR_BUFFER_BIT);
    fb->Unbind();
    return true;
}

//------------------ cOglCmdDrawRectangle --------------------
cOglCmdDrawRectangle::cOglCmdDrawRectangle(cOglFb * fb, GLint x, GLint y, GLint width, GLint height, GLint color):cOglCmd
    (fb)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
    this->color = color;
}

bool cOglCmdDrawRectangle::Execute(void)
{
    if (width <= 0 || height <= 0)
        return false;
    GLfloat x1 = x;
    GLfloat y1 = y;
    GLfloat x2 = x + width;
    GLfloat y2 = y + height;

    GLfloat vertices[] = {
        x1, y1,                         //left top
        x2, y1,                         //right top
        x2, y2,                         //right bottom
        x1, y2                          //left bottom
    };

    VertexBuffers[vbRect]->ActivateShader();
    VertexBuffers[vbRect]->SetShaderColor(color);
    VertexBuffers[vbRect]->SetShaderProjectionMatrix(fb->Width(), fb->Height());

    fb->Bind();
    VertexBuffers[vbRect]->DisableBlending();
    VertexBuffers[vbRect]->Bind();
    VertexBuffers[vbRect]->SetVertexData(vertices);
    VertexBuffers[vbRect]->DrawArrays();
    VertexBuffers[vbRect]->Unbind();
    VertexBuffers[vbRect]->EnableBlending();
    fb->Unbind();

    return true;
}

//------------------ cOglCmdDrawEllipse --------------------
///quadrants:
///< 0       draws the entire ellipse
///< 1..4    draws only the first, second, third or fourth quadrant, respectively
///< 5..8    draws the right, top, left or bottom half, respectively
///< -1..-4  draws the inverted part of the given quadrant
cOglCmdDrawEllipse::cOglCmdDrawEllipse(cOglFb * fb, GLint x, GLint y, GLint width, GLint height, GLint color, GLint quadrants):cOglCmd
    (fb)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
    this->color = color;
    this->quadrants = quadrants;
}

bool cOglCmdDrawEllipse::Execute(void)
{
    if (width <= 0 || height <= 0)
        return false;
    int numVertices = 0;
    GLfloat *vertices = NULL;

    switch (quadrants) {
        case 0:
            vertices = CreateVerticesFull(numVertices);
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case -1:
        case -2:
        case -3:
        case -4:
            vertices = CreateVerticesQuadrant(numVertices);
            break;
        case 5:
        case 6:
        case 7:
        case 8:
            vertices = CreateVerticesHalf(numVertices);
            break;
        default:
            break;
    }

    VertexBuffers[vbEllipse]->ActivateShader();
    VertexBuffers[vbEllipse]->SetShaderColor(color);
    VertexBuffers[vbEllipse]->SetShaderProjectionMatrix(fb->Width(), fb->Height());

    // not antialiased
    fb->Bind();
    VertexBuffers[vbEllipse]->DisableBlending();
    VertexBuffers[vbEllipse]->Bind();
    VertexBuffers[vbEllipse]->SetVertexData(vertices, numVertices);
    VertexBuffers[vbEllipse]->DrawArrays(numVertices);
    VertexBuffers[vbEllipse]->Unbind();
    VertexBuffers[vbEllipse]->EnableBlending();
    fb->Unbind();

    delete[]vertices;
    return true;
}

GLfloat *cOglCmdDrawEllipse::CreateVerticesFull(int &numVertices)
{
    int size = 364;

    numVertices = size / 2;
    GLfloat radiusX = (GLfloat) width / 2;
    GLfloat radiusY = (GLfloat) height / 2;
    GLfloat *vertices = new GLfloat[size];

    vertices[0] = x + radiusX;
    vertices[1] = y + radiusY;
    for (int i = 0; i <= 180; i++) {
        vertices[2 * i + 2] = x + radiusX + (GLfloat) cos(2 * i * M_PI / 180.0f) * radiusX;
        vertices[2 * i + 3] = y + radiusY - (GLfloat) sin(2 * i * M_PI / 180.0f) * radiusY;
    }
    return vertices;
}

GLfloat *cOglCmdDrawEllipse::CreateVerticesQuadrant(int &numVertices)
{
    int size = 94;

    numVertices = size / 2;
    GLfloat radiusX = (GLfloat) width;
    GLfloat radiusY = (GLfloat) height;
    GLint transX = 0;
    GLint transY = 0;
    GLint startAngle = 0;
    GLfloat *vertices = new GLfloat[size];

    switch (quadrants) {
        case 1:
            vertices[0] = x;
            vertices[1] = y + height;
            transY = radiusY;
            break;
        case 2:
            vertices[0] = x + width;
            vertices[1] = y + height;
            transX = radiusX;
            transY = radiusY;
            startAngle = 90;
            break;
        case 3:
            vertices[0] = x + width;
            vertices[1] = y;
            transX = radiusX;
            startAngle = 180;
            break;
        case 4:
            vertices[0] = x;
            vertices[1] = y;
            startAngle = 270;
            break;
        case -1:
            vertices[0] = x + width;
            vertices[1] = y;
            transY = radiusY;
            break;
        case -2:
            vertices[0] = x;
            vertices[1] = y;
            transX = radiusX;
            transY = radiusY;
            startAngle = 90;
            break;
        case -3:
            vertices[0] = x;
            vertices[1] = y + height;
            transX = radiusX;
            startAngle = 180;
            break;
        case -4:
            vertices[0] = x + width;
            vertices[1] = y + height;
            startAngle = 270;
            break;
        default:
            break;
    }
    for (int i = 0; i <= 45; i++) {
        vertices[2 * i + 2] = x + transX + (GLfloat) cos((2 * i + startAngle) * M_PI / 180.0f) * radiusX;
        vertices[2 * i + 3] = y + transY - (GLfloat) sin((2 * i + startAngle) * M_PI / 180.0f) * radiusY;
    }
    return vertices;
}

GLfloat *cOglCmdDrawEllipse::CreateVerticesHalf(int &numVertices)
{
    int size = 184;

    numVertices = size / 2;
    GLfloat radiusX = 0.0f;
    GLfloat radiusY = 0.0f;
    GLint transX = 0;
    GLint transY = 0;
    GLint startAngle = 0;
    GLfloat *vertices = new GLfloat[size];

    switch (quadrants) {
        case 5:
            radiusX = (GLfloat) width;
            radiusY = (GLfloat) height / 2;
            vertices[0] = x;
            vertices[1] = y + radiusY;
            startAngle = 270;
            transY = radiusY;
            break;
        case 6:
            radiusX = (GLfloat) width / 2;
            radiusY = (GLfloat) height;
            vertices[0] = x + radiusX;
            vertices[1] = y + radiusY;
            startAngle = 0;
            transX = radiusX;
            transY = radiusY;
            break;
        case 7:
            radiusX = (GLfloat) width;
            radiusY = (GLfloat) height / 2;
            vertices[0] = x + radiusX;
            vertices[1] = y + radiusY;
            startAngle = 90;
            transX = radiusX;
            transY = radiusY;
            break;
        case 8:
            radiusX = (GLfloat) width / 2;
            radiusY = (GLfloat) height;
            vertices[0] = x + radiusX;
            vertices[1] = y;
            startAngle = 180;
            transX = radiusX;
            break;
        default:
            break;
    }
    for (int i = 0; i <= 90; i++) {
        vertices[2 * i + 2] = x + transX + (GLfloat) cos((2 * i + startAngle) * M_PI / 180.0f) * radiusX;
        vertices[2 * i + 3] = y + transY - (GLfloat) sin((2 * i + startAngle) * M_PI / 180.0f) * radiusY;
    }
    return vertices;
}

//------------------ cOglCmdDrawSlope --------------------
///type:
///< 0: horizontal, rising,  lower
///< 1: horizontal, rising,  upper
///< 2: horizontal, falling, lower
///< 3: horizontal, falling, upper
///< 4: vertical,   rising,  lower
///< 5: vertical,   rising,  upper
///< 6: vertical,   falling, lower
///< 7: vertical,   falling, upper
cOglCmdDrawSlope::cOglCmdDrawSlope(cOglFb * fb, GLint x, GLint y, GLint width, GLint height, GLint color, GLint type):cOglCmd
    (fb)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
    this->color = color;
    this->type = type;
}

bool cOglCmdDrawSlope::Execute(void)
{
    if (width <= 0 || height <= 0)
        return false;
    bool falling = type & 0x02;
    bool vertical = type & 0x04;

    int steps = 100;

    if (width < 100)
        steps = 25;
    int numVertices = steps + 2;
    GLfloat *vertices = new GLfloat[numVertices * 2];

    switch (type) {
        case 0:
        case 4:
            vertices[0] = (GLfloat) (x + width);
            vertices[1] = (GLfloat) (y + height);
            break;
        case 1:
        case 5:
            vertices[0] = (GLfloat) x;
            vertices[1] = (GLfloat) y;
            break;
        case 2:
        case 6:
            vertices[0] = (GLfloat) x;
            vertices[1] = (GLfloat) (y + height);
            break;
        case 3:
        case 7:
            vertices[0] = (GLfloat) (x + width);
            vertices[1] = (GLfloat) y;
            break;
        default:
            vertices[0] = (GLfloat) (x);
            vertices[1] = (GLfloat) (y);
            break;
    }

    for (int i = 0; i <= steps; i++) {
        GLfloat c = cos(i * M_PI / steps);

        if (falling)
            c = -c;
        if (vertical) {
            vertices[2 * i + 2] = (GLfloat) x + (GLfloat) width / 2.0f + (GLfloat) width *c / 2.0f;
            vertices[2 * i + 3] = (GLfloat) y + (GLfloat) i *((GLfloat) height) / steps;
        } else {
            vertices[2 * i + 2] = (GLfloat) x + (GLfloat) i *((GLfloat) width) / steps;
            vertices[2 * i + 3] = (GLfloat) y + (GLfloat) height / 2.0f + (GLfloat) height *c / 2.0f;
        }
    }

    VertexBuffers[vbSlope]->ActivateShader();
    VertexBuffers[vbSlope]->SetShaderColor(color);
    VertexBuffers[vbSlope]->SetShaderProjectionMatrix(fb->Width(), fb->Height());

    // not antialiased
    fb->Bind();
    VertexBuffers[vbSlope]->DisableBlending();
    VertexBuffers[vbSlope]->Bind();
    VertexBuffers[vbSlope]->SetVertexData(vertices, numVertices);
    VertexBuffers[vbSlope]->DrawArrays(numVertices);
    VertexBuffers[vbSlope]->Unbind();
    VertexBuffers[vbSlope]->EnableBlending();
    fb->Unbind();

    delete[]vertices;
    return true;
}

//------------------ cOglCmdDrawText --------------------
cOglCmdDrawText::cOglCmdDrawText(cOglFb * fb, GLint x, GLint y, unsigned int *symbols, GLint limitX, const char *name,
    int fontSize, tColor colorText):cOglCmd(fb), fontName(name)
{
    this->x = x;
    this->y = y;
    this->limitX = limitX;
    this->colorText = colorText;
    this->fontSize = fontSize;
    this->symbols = symbols;
    this->fontName = name;
}

cOglCmdDrawText::~cOglCmdDrawText(void)
{
    free(symbols);
}

bool cOglCmdDrawText::Execute(void)
{
    cOglFont *f = cOglFont::Get(*fontName, fontSize);

    if (!f || !symbols[0])
        return false;

    VertexBuffers[vbText]->ActivateShader();
    VertexBuffers[vbText]->SetShaderColor(colorText);
    VertexBuffers[vbText]->SetShaderProjectionMatrix(fb->Width(), fb->Height());

    fb->Bind();
    VertexBuffers[vbText]->Bind();

    int xGlyph = x;
    int fontHeight = f->Height();
    int bottom = f->Bottom();
    FT_ULong sym = 0;
    FT_ULong prevSym = 0;
    int kerning = 0;

    for (int i = 0; symbols[i]; i++) {
        sym = symbols[i];
        cOglGlyph *g = f->Glyph(sym);

        if (!g) {
            esyslog("[softhddev]ERROR: could not load glyph %lx", sym);
        }

        if (limitX && xGlyph + g->AdvanceX() > limitX) {
            break;
        }

        kerning = f->Kerning(g, prevSym);
        prevSym = sym;

        GLfloat x1 = xGlyph + kerning + g->BearingLeft();   //left
        GLfloat y1 = y + (fontHeight - bottom - g->BearingTop());   //top
        GLfloat x2 = x1 + g->Width();   //right
        GLfloat y2 = y1 + g->Height();  //bottom

        GLfloat vertices[] = {
            x1, y2, 0.0, 1.0,           // left bottom
            x1, y1, 0.0, 0.0,           // left top
            x2, y1, 1.0, 0.0,           // right top

            x1, y2, 0.0, 1.0,           // left bottom
            x2, y1, 1.0, 0.0,           // right top
            x2, y2, 1.0, 1.0            // right bottom
        };

        g->BindTexture();
        VertexBuffers[vbText]->SetVertexData(vertices);
        VertexBuffers[vbText]->DrawArrays();

        xGlyph += kerning + g->AdvanceX();

        if (xGlyph > fb->Width() - 1)
            break;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    VertexBuffers[vbText]->Unbind();
    fb->Unbind();
    return true;
}

//------------------ cOglCmdDrawImage --------------------
cOglCmdDrawImage::cOglCmdDrawImage(cOglFb * fb, tColor * argb, GLint width, GLint height, GLint x, GLint y,
    bool overlay, double scaleX, double scaleY):cOglCmd(fb)
{
    this->argb = argb;
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
    this->overlay = overlay;
    this->scaleX = scaleX;
    this->scaleY = scaleY;

}

cOglCmdDrawImage::~cOglCmdDrawImage(void)
{
    free(argb);
}

bool cOglCmdDrawImage::Execute(void)
{
    if (width <= 0 || height <= 0)
        return false;
    GLuint texture;


//  pthread_mutex_lock(&OSDMutex);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, argb);
    GlxCheck();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFlush();

    GlxCheck();
//  pthread_mutex_unlock(&OSDMutex);
    if (x + width * scaleX > VideoWindowWidth) {
	 printf("Scaling over the Width edge %f\n",x + width * scaleX);
         scaleX = 1.0;
    }
    if (y + height * scaleY > VideoWindowHeight) {
	 printf("Scaling over the Height edge %f\n",x + height * scaleY);
         scaleY = 1.0;
    }
	
    GLfloat x1 = x;                     //left
    GLfloat y1 = y;                     //top
    GLfloat x2 = x + width * scaleX;    //right
    GLfloat y2 = y + height * scaleY;   //bottom

    GLfloat quadVertices[] = {
        x1, y2, 0.0, 1.0,               // left bottom
        x1, y1, 0.0, 0.0,               // left top
        x2, y1, 1.0, 0.0,               // right top

        x1, y2, 0.0, 1.0,               // left bottom
        x2, y1, 1.0, 0.0,               // right top
        x2, y2, 1.0, 1.0                // right bottom
    };

    VertexBuffers[vbImage]->ActivateShader();
    VertexBuffers[vbImage]->SetShaderAlpha(255);
    VertexBuffers[vbImage]->SetShaderProjectionMatrix(fb->Width(), fb->Height());

    fb->Bind();
    glBindTexture(GL_TEXTURE_2D, texture);
    if (overlay)
        VertexBuffers[vbImage]->DisableBlending();
    VertexBuffers[vbImage]->Bind();
    VertexBuffers[vbImage]->SetVertexData(quadVertices);
    VertexBuffers[vbImage]->DrawArrays();
    VertexBuffers[vbImage]->Unbind();
    if (overlay)
        VertexBuffers[vbImage]->EnableBlending();
    fb->Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);

    return true;
}

//------------------ cOglCmdDrawTexture --------------------
cOglCmdDrawTexture::cOglCmdDrawTexture(cOglFb * fb, sOglImage * imageRef, GLint x, GLint y, double scaleX, double scaleY):cOglCmd(fb)
{
    this->imageRef = imageRef;
    this->x = x;
    this->y = y;
    this->scaleX = scaleX;
    this->scaleY = scaleY;
}

bool cOglCmdDrawTexture::Execute(void)
{
    if (imageRef->width <= 0 || imageRef->height <= 0)
        return false;
    GLfloat x1 = x;                              //top
    GLfloat y1 = y;                              //left
    GLfloat x2 = x + imageRef->width * scaleX;   //right
    GLfloat y2 = y + imageRef->height * scaleY;  //bottom

    GLfloat quadVertices[] = {
        // Pos    // TexCoords
        x1, y1, 0.0f, 0.0f,             //left bottom
        x1, y2, 0.0f, 1.0f,             //left top
        x2, y2, 1.0f, 1.0f,             //right top

        x1, y1, 0.0f, 0.0f,             //left bottom
        x2, y2, 1.0f, 1.0f,             //right top
        x2, y1, 1.0f, 0.0f              //right bottom
    };

    VertexBuffers[vbTexture]->ActivateShader();
    VertexBuffers[vbTexture]->SetShaderAlpha(255);
    VertexBuffers[vbTexture]->SetShaderProjectionMatrix(fb->Width(), fb->Height());

    fb->Bind();
    glBindTexture(GL_TEXTURE_2D, imageRef->texture);
    VertexBuffers[vbTexture]->Bind();
    VertexBuffers[vbTexture]->SetVertexData(quadVertices);
    VertexBuffers[vbTexture]->DrawArrays();
    VertexBuffers[vbTexture]->Unbind();
    fb->Unbind();

    return true;
}

//------------------ cOglCmdStoreImage --------------------
cOglCmdStoreImage::cOglCmdStoreImage(sOglImage * imageRef, tColor * argb):cOglCmd(NULL)
{
    this->imageRef = imageRef;
    data = argb;
}

cOglCmdStoreImage::~cOglCmdStoreImage(void)
{
    free(data);
}

bool cOglCmdStoreImage::Execute(void)
{

    glGenTextures(1, &imageRef->texture);
    glBindTexture(GL_TEXTURE_2D, imageRef->texture);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageRef->width, imageRef->height, 0, GL_BGRA,
    //    GL_UNSIGNED_INT_8_8_8_8_REV, data);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageRef->width, imageRef->height, 0, GL_RGBA,
+        GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFlush();

    return true;
}

//------------------ cOglCmdDropImage --------------------
cOglCmdDropImage::cOglCmdDropImage(sOglImage * imageRef, cCondWait * wait):cOglCmd(NULL)
{
    this->imageRef = imageRef;
    this->wait = wait;
}

bool cOglCmdDropImage::Execute(void)
{
    if (imageRef->texture != GL_NONE)
        glDeleteTextures(1, &imageRef->texture);
    wait->Signal();
    return true;
}

/******************************************************************************
* cOglThread
******************************************************************************/
cOglThread::cOglThread(cCondWait * startWait, int maxCacheSize):cThread("oglThread")
{
    stalled = false;
    memCached = 0;

    this->maxCacheSize = 0;
    this->startWait = startWait;
    wait = new cCondWait();
    maxTextureSize = 0;
    for (int i = 0; i < OGL_MAX_OSDIMAGES; i++) {
        imageCache[i].used = false;
        imageCache[i].texture = GL_NONE;
        imageCache[i].width = 0;
        imageCache[i].height = 0;
    }

    Start();

}

cOglThread::~cOglThread()
{
    delete wait;

    wait = NULL;
    ClearCursor(0);
    //close(fd);
    //close(ge2d_fd);
    close(ion_fd);
}

void cOglThread::Stop(void)
{
    for (int i = 0; i < OGL_MAX_OSDIMAGES; i++) {
        if (imageCache[i].used) {
            DropImageData(i);
        }
    }
    Cancel(2);
    stalled = false;
}

void cOglThread::DoCmd(cOglCmd * cmd)
{
    while (stalled)
        cCondWait::SleepMs(10);

    bool doSignal = false;

    Lock();
    if (commands.size() == 0)
        doSignal = true;
    commands.push(cmd);
    Unlock();

    if (commands.size() > OGL_CMDQUEUE_SIZE) {
        stalled = true;
    }

    if (doSignal || stalled)
        wait->Signal();
}

int cOglThread::StoreImage(const cImage & image)
{

    if (!maxCacheSize) {
        return 0;
    }

    if (image.Width() > maxTextureSize || image.Height() > maxTextureSize) {
        esyslog("[softhddev] cannot store image of %dpx x %dpx " "(maximum size is %dpx x %dpx) - falling back to "
            "cOsdProvider::StoreImageData()", image.Width(), image.Height(), maxTextureSize, maxTextureSize);
        return 0;
    }

    long int imgSize = image.Width() * image.Height();
    int newMemUsed = imgSize * sizeof(tColor) + memCached;

    if (newMemUsed > maxCacheSize) {
        float cachedMB = memCached / 1024.0f / 1024.0f;
        float maxMB = maxCacheSize / 1024.0f / 1024.0f;

        esyslog("[softhddev]Maximum size for GPU cache reached. Used: %.2fMB Max: %.2fMB", cachedMB, maxMB);
        return 0;
    }

    int slot = GetFreeSlot();

    if (!slot)
        return 0;

    tColor *argb = MALLOC(tColor, imgSize);

    if (!argb) {
        esyslog("[softhddev]memory allocation of %ld kb for OSD image failed", (imgSize * sizeof(tColor)) / 1024);
        ClearSlot(slot);
        slot = 0;
        return 0;
    }

    memcpy(argb, image.Data(), sizeof(tColor) * imgSize);

    sOglImage *imageRef = GetImageRef(slot);

    imageRef->width = image.Width();
    imageRef->height = image.Height();
    DoCmd(new cOglCmdStoreImage(imageRef, argb));

    cTimeMs timer(5000);

    while (imageRef->used && imageRef->texture == 0 && !timer.TimedOut())
        cCondWait::SleepMs(2);

    if (imageRef->texture == GL_NONE) {
        esyslog("[softhddev]failed to store OSD image texture! (%s)",
            timer.TimedOut()? "timed out" : "allocation failed");
        DropImageData(slot);
        slot = 0;
    }

    memCached += imgSize * sizeof(tColor);
    return slot;
}

int cOglThread::GetFreeSlot(void)
{
    Lock();
    int slot = 0;

    for (int i = 0; i < OGL_MAX_OSDIMAGES && !slot; i++) {
        if (!imageCache[i].used) {
            imageCache[i].used = true;
            slot = -i - 1;
        }
    }
    Unlock();
    return slot;
}

void cOglThread::ClearSlot(int slot)
{
    int i = -slot - 1;

    if (i >= 0 && i < OGL_MAX_OSDIMAGES) {
        Lock();
        imageCache[i].used = false;
        imageCache[i].texture = GL_NONE;
        imageCache[i].width = 0;
        imageCache[i].height = 0;
        Unlock();
    }
}

sOglImage *cOglThread::GetImageRef(int slot)
{
    int i = -slot - 1;

    if (0 <= i && i < OGL_MAX_OSDIMAGES)
        return &imageCache[i];
    return 0;
}

void cOglThread::DropImageData(int imageHandle)
{
    sOglImage *imageRef = GetImageRef(imageHandle);

    if (!imageRef)
        return;
    int imgSize = imageRef->width * imageRef->height * sizeof(tColor);

    memCached -= imgSize;
    cCondWait dropWait;

    DoCmd(new cOglCmdDropImage(imageRef, &dropWait));
    dropWait.Wait();
    ClearSlot(imageHandle);
}

void cOglThread::Action(void)
{
    if (!InitOpenGL()) {
        esyslog("[softhddev]Could not initiate OpenGL Context");
        Cleanup();
        startWait->Signal();
        return;
    }
    dsyslog("[softhddev]OpenGL Context initialized");

    if (!InitShaders()) {
        esyslog("[softhddev]Could not initiate Shaders");
        Cleanup();
        startWait->Signal();
        return;
    }
    dsyslog("[softhddev]Shaders initialized");

    if (!InitVertexBuffers()) {
        esyslog("[softhddev]: Vertex Buffers NOT initialized");
        Cleanup();
        startWait->Signal();
        return;
    }
    dsyslog("[softhddev]Vertex buffers initialized");

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    dsyslog("[softhddev]Maximum Pixmap size: %dx%dpx", maxTextureSize, maxTextureSize);

    //now Thread is ready to do his job
    startWait->Signal();
    stalled = false;

    while (Running()) {

        if (commands.empty()) {
            wait->Wait(20);
            continue;
        }

        Lock();
        cOglCmd *cmd = commands.front();

        commands.pop();
        Unlock();
        // uint64_t start = cTimeMs::Now();
        cmd->Execute();
        // esyslog("[softhddev]\"%s\", %dms, %d commands left, time %" PRIu64 "", cmd->Description(), (int)(cTimeMs::Now() - start), commands.size(), cTimeMs::Now());
        delete cmd;

        if (stalled && commands.size() < OGL_CMDQUEUE_SIZE / 2)
            stalled = false;
    }

    dsyslog("[softhddev]Cleaning up OpenGL stuff");
    Cleanup();
    dsyslog("[softhddev]OpenGL Worker Thread Ended");
}


bool cOglThread::InitOpenGL(void)
{

    EGLNativeDisplayType nativeDisplay=EGL_DEFAULT_DISPLAY;

	ion_fd = open("/dev/ion", O_RDWR);
	if (ion_fd < 0)
	{
		printf("open /dev/ion failed.");
	}

    eglDisplay = eglGetDisplay(nativeDisplay);

        // Initialize EGL
	EGLint major;
	EGLint minor;
	EGLBoolean success = eglInitialize( eglDisplay, &major, &minor);
	if (success != EGL_TRUE)
	{
        printf("failed to initialize EGL display\n");
		GlxCheck();
	}

#if 0
	printf("EGL: major=%d, minor=%d\n", major, minor);
	printf("EGL: Vendor=%s\n", eglQueryString(eglDisplay, EGL_VENDOR));
	printf("EGL: Version=%s\n", eglQueryString(eglDisplay, EGL_VERSION));
	printf("EGL: ClientAPIs=%s\n", eglQueryString(eglDisplay, EGL_CLIENT_APIS));
	printf("EGL: Extensions=%s\n", eglQueryString(eglDisplay, EGL_EXTENSIONS));
	printf("EGL: ClientExtensions=%s\n", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
	printf("\n");
#endif

    const EGLint configAttributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_ALPHA_SIZE, 0,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 0,
        EGL_BUFFER_SIZE,  32,
        EGL_SAMPLES, 4,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

	int num_configs;
	success = eglChooseConfig(eglDisplay, configAttributes, NULL, 0, &num_configs);
	if (success != EGL_TRUE)
	{
		GlxCheck();
	}

    EGLConfig* configs = new EGLConfig[num_configs];
	success = eglChooseConfig(eglDisplay, configAttributes, configs, num_configs, &num_configs);
	if (success != EGL_TRUE)
	{
        printf("No EGL Config found\n");
		GlxCheck();
	}

	EGLConfig match = 0;

	for (int i = 0; i < num_configs; ++i)
	{
		EGLint configRedSize;
		EGLint configGreenSize;
		EGLint configBlueSize;
		EGLint configAlphaSize;
		EGLint configDepthSize;
		EGLint configStencilSize;

		eglGetConfigAttrib(eglDisplay, configs[i], EGL_RED_SIZE, &configRedSize);
		eglGetConfigAttrib(eglDisplay, configs[i], EGL_GREEN_SIZE, &configGreenSize);
		eglGetConfigAttrib(eglDisplay, configs[i], EGL_BLUE_SIZE, &configBlueSize);
		eglGetConfigAttrib(eglDisplay, configs[i], EGL_ALPHA_SIZE, &configAlphaSize);
		eglGetConfigAttrib(eglDisplay, configs[i], EGL_DEPTH_SIZE, &configDepthSize);
		eglGetConfigAttrib(eglDisplay, configs[i], EGL_STENCIL_SIZE, &configStencilSize);

		if (configRedSize == 8 &&
			configBlueSize == 8 &&
			configGreenSize == 8 &&
			configAlphaSize == 8 )
		{
			match = configs[i];
			break;
		}
	}

    delete[] configs;

    if (match == 0) {
        printf("No config found\n");
    }

    const EGLint contextAttributes[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };


    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
    {
        printf( "failed to bind EGL API\n");
    }
    GlxCheck();

    eglContext = eglCreateContext(eglDisplay, match, EGL_NO_CONTEXT, contextAttributes);
    GlxCheck();
    if (eglContext == EGL_NO_CONTEXT) {
        printf("no Context created\n");
    }

    //eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    ClearCursor(1);

    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext);

#if 0
    // Clear 3 OSD Buffers
    glClearColor(0,0,0,0) ;
    glClear(GL_COLOR_BUFFER_BIT);
    WaitVsync();
    eglSwapBuffers(eglDisplay, eglSurface);
    glClear(GL_COLOR_BUFFER_BIT);
    WaitVsync();
    eglSwapBuffers(eglDisplay, eglSurface);
    glClear(GL_COLOR_BUFFER_BIT);
    WaitVsync();
    eglSwapBuffers(eglDisplay, eglSurface);
#endif

    VertexBuffers[vbText]->EnableBlending();

    glDisable(GL_DEPTH_TEST);
    GlxCheck();
    OsdClose();
    return true;

}

bool cOglThread::InitShaders(void)
{
    for (int i = 0; i < stCount; i++) {
        cShader *shader = new cShader();

        if (!shader->Load((eShaderType) i))
            return false;
        Shaders[i] = shader;
    }
    return true;
}

void cOglThread::DeleteShaders(void)
{
    for (int i = 0; i < stCount; i++)
        delete Shaders[i];
}

bool cOglThread::InitVertexBuffers(void)
{
    for (int i = 0; i < vbCount; i++) {
        cOglVb *vb = new cOglVb(i);

        if (!vb->Init())
            return false;
        VertexBuffers[i] = vb;
    }
    return true;
}

void cOglThread::DeleteVertexBuffers(void)
{
    for (int i = 0; i < vbCount; i++) {
        delete VertexBuffers[i];
    }
}

void cOglThread::Cleanup(void)
{
    esyslog("[softhddev]OglThread cleanup\n");

    OsdClose();

    DeleteVertexBuffers();
    delete cOglOsd::oFb;

    cOglOsd::oFb = NULL;
    DeleteShaders();
    // glVDPAUFiniNV();
    cOglFont::Cleanup();

}

/****************************************************************************************
* cOglPixmap
****************************************************************************************/

cOglPixmap::cOglPixmap(std::shared_ptr < cOglThread > oglThread, int Layer, const cRect & ViewPort,
    const cRect & DrawPort):cPixmap(Layer, ViewPort, DrawPort)
{
    this->oglThread = oglThread;
    int width = DrawPort.IsEmpty()? ViewPort.Width() : DrawPort.Width();
    int height = DrawPort.IsEmpty()? ViewPort.Height() : DrawPort.Height();

    fb = new cOglFb(width, height, ViewPort.Width(), ViewPort.Height());
    dirty = true;
}

cOglPixmap::~cOglPixmap(void)
{
    if (!oglThread->Active())
        return;
    oglThread->DoCmd(new cOglCmdDeleteFb(fb));
}

void cOglPixmap::SetAlpha(int Alpha)
{
    Alpha = constrain(Alpha, ALPHA_TRANSPARENT, ALPHA_OPAQUE);
    if (Alpha != cPixmap::Alpha()) {
        cPixmap::SetAlpha(Alpha);
        SetDirty();
    }
}

void cOglPixmap::SetTile(bool Tile)
{
    cPixmap::SetTile(Tile);
    SetDirty();
}

void cOglPixmap::SetViewPort(const cRect & Rect)
{
    cPixmap::SetViewPort(Rect);
    SetDirty();
}

void cOglPixmap::SetDrawPortPoint(const cPoint & Point, bool Dirty)
{
    cPixmap::SetDrawPortPoint(Point, Dirty);
    if (Dirty)
        SetDirty();
}

void cOglPixmap::Clear(void)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    oglThread->DoCmd(new cOglCmdFill(fb, clrTransparent));
    SetDirty();
    MarkDrawPortDirty(DrawPort());
}

void cOglPixmap::Fill(tColor Color)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    oglThread->DoCmd(new cOglCmdFill(fb, Color));
    SetDirty();
    MarkDrawPortDirty(DrawPort());
}

void cOglPixmap::DrawImage(const cPoint & Point, const cImage & Image)
{
    DrawScaledImage(Point, Image, 1.0, 1.0);
}

void cOglPixmap::DrawScaledImage(const cPoint & Point, const cImage & Image, double FactorX, double FactorY, bool AntiAlias)
{
    if (!oglThread->Active())
        return;
    tColor *argb = MALLOC(tColor, Image.Width() * Image.Height());

    if (!argb)
        return;
    memcpy(argb, Image.Data(), sizeof(tColor) * Image.Width() * Image.Height());

    oglThread->DoCmd(new cOglCmdDrawImage(fb, argb, Image.Width(), Image.Height(), Point.X(), Point.Y(), true, FactorX, FactorY));
    SetDirty();
    MarkDrawPortDirty(cRect(Point, cSize(Image.Width() * FactorX, Image.Height() * FactorY)).Intersected(DrawPort().Size()));
}

void cOglPixmap::DrawImage(const cPoint & Point, int ImageHandle)
{
    DrawScaledImage(Point, ImageHandle, 1.0, 1.0);
}

void cOglPixmap::DrawScaledImage(const cPoint & Point, int ImageHandle, double FactorX, double FactorY, bool AntiAlias)
{
    if (!oglThread->Active())
        return;

    if (ImageHandle < 0 && oglThread->GetImageRef(ImageHandle)) {
        sOglImage *img = oglThread->GetImageRef(ImageHandle);

        oglThread->DoCmd(new cOglCmdDrawTexture(fb, img, Point.X(), Point.Y(), FactorX, FactorY));
        SetDirty();
        MarkDrawPortDirty(cRect(Point, cSize(img->width * FactorX, img->height * FactorY)).Intersected(DrawPort().Size()));
    }
    /*
       Fallback to VDR implementation, needs to separate cSoftOsdProvider from softhddevice.cpp
       else {
       if (cSoftOsdProvider::GetImageData(ImageHandle))
       DrawImage(Point, *cSoftOsdProvider::GetImageData(ImageHandle));
       }
     */
}

void cOglPixmap::DrawPixel(const cPoint & Point, tColor Color)
{
    cRect r(Point.X(), Point.Y(), 1, 1);

    oglThread->DoCmd(new cOglCmdDrawRectangle(fb, r.X(), r.Y(), r.Width(), r.Height(), Color));

    SetDirty();
    MarkDrawPortDirty(r);
}

void cOglPixmap::DrawBitmap(const cPoint & Point, const cBitmap & Bitmap, tColor ColorFg, tColor ColorBg, bool Overlay)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    bool specialColors = ColorFg || ColorBg;
    tColor *argb = MALLOC(tColor, Bitmap.Width() * Bitmap.Height());

    if (!argb)
        return;

    tColor *p = argb;

    for (int py = 0; py < Bitmap.Height(); py++)
        for (int px = 0; px < Bitmap.Width(); px++) {
            tIndex index = *Bitmap.Data(px, py);

            *p++ = (!index
                && Overlay) ? clrTransparent : (specialColors ? (index == 0 ? ColorBg : index ==
                    1 ? ColorFg : Bitmap.Color(index)) : Bitmap.Color(index));
        }

    oglThread->DoCmd(new cOglCmdDrawImage(fb, argb, Bitmap.Width(), Bitmap.Height(), Point.X(), Point.Y(), true, 1.0, 1.0));
    SetDirty();
    MarkDrawPortDirty(cRect(Point, cSize(Bitmap.Width(), Bitmap.Height())).Intersected(DrawPort().Size()));
}

void cOglPixmap::DrawText(const cPoint & Point, const char *s, tColor ColorFg, tColor ColorBg, const cFont * Font,
    int Width, int Height, int Alignment)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    int len = s ? Utf8StrLen(s) : 0;
    unsigned int *symbols = MALLOC(unsigned int, len + 1);

    if (!symbols)
        return;

    if (len)
        Utf8ToArray(s, symbols, len + 1);
    else
        symbols[0] = 0;
    int x = Point.X();
    int y = Point.Y();
    int w = Font->Width(s);
    int h = Font->Height();
    int limitX = 0;
    int cw = Width ? Width : w;
    int ch = Height ? Height : h;
    cRect r(x, y, cw, ch);

    if (ColorBg != clrTransparent)
        oglThread->DoCmd(new cOglCmdDrawRectangle(fb, r.X(), r.Y(), r.Width(), r.Height(), ColorBg));

    if (Width || Height) {
        limitX = x + cw;
        if (Width) {
            if ((Alignment & taLeft) != 0) {
                if ((Alignment & taBorder) != 0)
                    x += std::max(h / TEXT_ALIGN_BORDER, 1);
            } else if ((Alignment & taRight) != 0) {
                if (w < Width)
                    x += Width - w;
                if ((Alignment & taBorder) != 0)
                    x -= std::max(h / TEXT_ALIGN_BORDER, 1);
            } else {                    // taCentered
                if (w < Width)
                    x += (Width - w) / 2;
            }
        }

        if (Height) {
            if ((Alignment & taTop) != 0) ;
            else if ((Alignment & taBottom) != 0) {
                if (h < Height)
                    y += Height - h;
            } else {                    // taCentered
                if (h < Height)
                    y += (Height - h) / 2;
            }
        }
    }
    oglThread->DoCmd(new cOglCmdDrawText(fb, x, y, symbols, limitX, Font->FontName(), Font->Size(), ColorFg));

    SetDirty();
    MarkDrawPortDirty(r);
}

void cOglPixmap::DrawRectangle(const cRect & Rect, tColor Color)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    oglThread->DoCmd(new cOglCmdDrawRectangle(fb, Rect.X(), Rect.Y(), Rect.Width(), Rect.Height(), Color));
    SetDirty();
    MarkDrawPortDirty(Rect);
}

void cOglPixmap::DrawEllipse(const cRect & Rect, tColor Color, int Quadrants)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    oglThread->DoCmd(new cOglCmdDrawEllipse(fb, Rect.X(), Rect.Y(), Rect.Width(), Rect.Height(), Color, Quadrants));
    SetDirty();
    MarkDrawPortDirty(Rect);
}

void cOglPixmap::DrawSlope(const cRect & Rect, tColor Color, int Type)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    oglThread->DoCmd(new cOglCmdDrawSlope(fb, Rect.X(), Rect.Y(), Rect.Width(), Rect.Height(), Color, Type));
    SetDirty();
    MarkDrawPortDirty(Rect);
}

void cOglPixmap::Render(const cPixmap * Pixmap, const cRect & Source, const cPoint & Dest)
{
    esyslog("[softhddev] Render %d %d %d not implemented in OpenGl OSD", Pixmap->ViewPort().X(), Source.X(), Dest.X());
}

void cOglPixmap::Copy(const cPixmap * Pixmap, const cRect & Source, const cPoint & Dest)
{
    esyslog("[softhddev] Copy %d %d %d not implemented in OpenGl OSD", Pixmap->ViewPort().X(), Source.X(), Dest.X());
}

void cOglPixmap::Scroll(const cPoint & Dest, const cRect & Source)
{
    esyslog("[softhddev] Scroll %d %d not implemented in OpenGl OSD", Source.X(), Dest.X());
}

void cOglPixmap::Pan(const cPoint & Dest, const cRect & Source)
{
    esyslog("[softhddev] Pan %d %d not implemented in OpenGl OSD", Source.X(), Dest.X());
}

/******************************************************************************
* cOglOsd
******************************************************************************/
cOglOutputFb *cOglOsd::oFb = NULL;
cSize cOglOsd::maxPixmapSize_ODROID(8192,8192);

cOglOsd::cOglOsd(int Left, int Top, uint Level, std::shared_ptr < cOglThread > oglThread):cOsd(Left, Top, Level)
{
    this->oglThread = oglThread;
    bFb = NULL;
    isSubtitleOsd = false;
 //   int osdWidth = 0;
 //   int osdHeight = 0;

//    pthread_mutex_lock(&OSDMutex);
    VideoGetOsdSize((int*)&MyOsdWidth, (int *)&MyOsdHeight);

    dsyslog("[softhddev]cOglOsd osdLeft %d osdTop %d screenWidth %d screenHeight %d", Left, Top, MyOsdWidth, MyOsdHeight);

#if 0
    if (posd)
        free(posd);
    posd = (unsigned char *)calloc(osdWidth * osdHeight * 4, 1);
#endif

    cSize maxPixmapSize_ODROID(oglThread->MaxTextureSize(), oglThread->MaxTextureSize());

    // create output framebuffer

    if (!oFb) {
        oFb = new cOglOutputFb(MyOsdWidth, MyOsdHeight);
        oglThread->DoCmd(new cOglCmdInitOutputFb(oFb));
    }
//    pthread_mutex_unlock(&OSDMutex);
}

cOglOsd::~cOglOsd()
{
    OsdClose();
    SetActive(false);

    oglThread->DoCmd(new cOglCmdDeleteFb(bFb));
}

eOsdError cOglOsd::SetAreas(const tArea * Areas, int NumAreas)
{
    cRect r;

    if (NumAreas > 1)
        isSubtitleOsd = true;
    for (int i = 0; i < NumAreas; i++)
        r.Combine(cRect(Areas[i].x1, Areas[i].y1, Areas[i].Width(), Areas[i].Height()));

    tArea area = { r.Left(), r.Top(), r.Right(), r.Bottom(), 32 };

    // now we know the actuaL osd size, create double buffer frame buffer
    if (bFb) {
        oglThread->DoCmd(new cOglCmdDeleteFb(bFb));
        DestroyPixmap(oglPixmaps[0]);
    }
    bFb = new cOglFb(r.Width(), r.Height(), r.Width(), r.Height());
    cCondWait initiated;

    oglThread->DoCmd(new cOglCmdInitFb(bFb, &initiated));
    initiated.Wait();

    return cOsd::SetAreas(&area, 1);
}

const cSize &cOglOsd::MaxPixmapSize(void) const
{
  return maxPixmapSize_ODROID;
}

cPixmap *cOglOsd::CreatePixmap(int Layer, const cRect & ViewPort, const cRect & DrawPort)
{
    if (!oglThread->Active())
        return NULL;
    LOCK_PIXMAPS;
    int width = DrawPort.IsEmpty()? ViewPort.Width() : DrawPort.Width();
    int height = DrawPort.IsEmpty()? ViewPort.Height() : DrawPort.Height();

    if (width > oglThread->MaxTextureSize() || height > oglThread->MaxTextureSize()) {
        esyslog("[softhddev] cannot allocate pixmap of %dpx x %dpx, clipped to %dpx x %dpx!", width, height,
            std::min(width, oglThread->MaxTextureSize()), std::min(height, oglThread->MaxTextureSize()));
        width = std::min(width, oglThread->MaxTextureSize());
        height = std::min(height, oglThread->MaxTextureSize());
    }

    cOglPixmap *p = new cOglPixmap(oglThread, Layer, ViewPort, DrawPort);

    if (cOsd::AddPixmap(p)) {
        // find free slot
        for (int i = 0; i < oglPixmaps.Size(); i++)
            if (!oglPixmaps[i])
                return oglPixmaps[i] = p;
        // append at end
        oglPixmaps.Append(p);
        return p;
    }
    delete p;

    return NULL;
}

void cOglOsd::DestroyPixmap(cPixmap * Pixmap)
{
    if (!oglThread->Active())
        return;
    if (!Pixmap)
        return;

    LOCK_PIXMAPS;
    int start = 1;

    if (isSubtitleOsd)
        start = 0;
    for (int i = start; i < oglPixmaps.Size(); i++) {
        if (oglPixmaps[i] == Pixmap) {
            if (Pixmap->Layer() >= 0)
                oglPixmaps[0]->SetDirty();
            oglPixmaps[i] = NULL;
            cOsd::DestroyPixmap(Pixmap);
            return;
        }
    }
}

void cOglOsd::Flush(void)
{
    if (!oglThread->Active())
        return;
    LOCK_PIXMAPS;
    // check if any pixmap is dirty
    bool dirty = false;

    for (int i = 0; i < oglPixmaps.Size() && !dirty; i++)
        if (oglPixmaps[i] && oglPixmaps[i]->Layer() >= 0 && oglPixmaps[i]->IsDirty())
            dirty = true;
    if (!dirty)
        return;
    // clear buffer
    // uint64_t start = cTimeMs::Now();
    // dsyslog("[softhddev]Start Flush at %" PRIu64 "", cTimeMs::Now());

    oglThread->DoCmd(new cOglCmdFill(bFb, clrTransparent));

    // render pixmap textures blended to buffer
    for (int layer = 0; layer < MAXPIXMAPLAYERS; layer++) {
        for (int i = 0; i < oglPixmaps.Size(); i++) {
            if (oglPixmaps[i]) {
                if (oglPixmaps[i]->Layer() == layer) {
                    //printf("%d %d Layer %d\n",oglPixmaps[i]->Width(),oglPixmaps[i]->Height(),i);
                    bool alphablending = layer == 0 ? false : true; // Decide wether to render (with alpha) or copy a pixmap
                    oglThread->DoCmd(new cOglCmdRenderFbToBufferFb( oglPixmaps[i]->Fb(), 
                                                                    bFb,
                                                                    oglPixmaps[i]->ViewPort().X(), 
                                                                    (!isSubtitleOsd) ? oglPixmaps[i]->ViewPort().Y() : 0,
                                                                    oglPixmaps[i]->Alpha(), 
                                                                    oglPixmaps[i]->DrawPort().X(), 
                                                                    oglPixmaps[i]->DrawPort().Y(),
                                                                    alphablending
                                                                    ));
                    oglPixmaps[i]->SetDirty(false);
                }
            }
        }
    }
    oglThread->DoCmd(new cOglCmdCopyBufferToOutputFb(bFb, oFb, Left(), Top()));

    // dsyslog("[softhddev]End Flush at %" PRIu64 ", duration %d", cTimeMs::Now(), (int)(cTimeMs::Now()-start));
}

void cOglOsd::DrawScaledBitmap(int x, int y, const cBitmap & Bitmap, double FactorX, double FactorY, bool AntiAlias)
{
    (void)FactorX;
    (void)FactorY;
    (void)AntiAlias;
    int yNew = y - oglPixmaps[0]->ViewPort().Y();

    oglPixmaps[0]->DrawBitmap(cPoint(x, yNew), Bitmap);
}
