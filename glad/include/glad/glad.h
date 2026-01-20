#ifndef __glad_h_
#define __glad_h_

#ifdef __gl_h_
#error OpenGL header already included
#endif
#define __gl_h_

#include <KHR/khrplatform.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*GLADloadproc)(const char *name);
int gladLoadGLLoader(GLADloadproc load);

/* OpenGL Types */
typedef void GLvoid;
typedef unsigned int GLenum;
typedef khronos_float_t GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef khronos_uint8_t GLubyte;
typedef khronos_ssize_t GLsizeiptr;
typedef khronos_intptr_t GLintptr;
typedef char GLchar;

/* OpenGL Constants */
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TRIANGLES 0x0004
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_INT 0x1405
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_LINEAR 0x2601
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_UNSIGNED_BYTE 0x1401
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TEXTURE0 0x84C0

/* OpenGL Functions */
typedef void(KHRONOS_APIENTRY *PFNGLCLEARPROC)(GLbitfield mask);
typedef void(KHRONOS_APIENTRY *PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green,
                                                    GLfloat blue,
                                                    GLfloat alpha);
typedef void(KHRONOS_APIENTRY *PFNGLVIEWPORTPROC)(GLint x, GLint y,
                                                  GLsizei width,
                                                  GLsizei height);
typedef void(KHRONOS_APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void(KHRONOS_APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target,
                                                    GLuint buffer);
typedef void(KHRONOS_APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target,
                                                    GLsizeiptr size,
                                                    const void *data,
                                                    GLenum usage);
typedef void(KHRONOS_APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n,
                                                         GLuint *arrays);
typedef void(KHRONOS_APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void(KHRONOS_APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void(KHRONOS_APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void *pointer);
typedef GLuint(KHRONOS_APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void(KHRONOS_APIENTRY *PFNGLSHADERSOURCEPROC)(
    GLuint shader, GLsizei count, const GLchar *const *string,
    const GLint *length);
typedef void(KHRONOS_APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void(KHRONOS_APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader,
                                                     GLenum pname,
                                                     GLint *params);
typedef void(KHRONOS_APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader,
                                                          GLsizei bufSize,
                                                          GLsizei *length,
                                                          GLchar *infoLog);
typedef GLuint(KHRONOS_APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void(KHRONOS_APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program,
                                                      GLuint shader);
typedef void(KHRONOS_APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void(KHRONOS_APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program,
                                                      GLenum pname,
                                                      GLint *params);
typedef void(KHRONOS_APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLchar *infoLog);
typedef void(KHRONOS_APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void(KHRONOS_APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
typedef void(KHRONOS_APIENTRY *PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first,
                                                    GLsizei count);
typedef void(KHRONOS_APIENTRY *PFNGLDRAWELEMENTSPROC)(GLenum mode,
                                                      GLsizei count,
                                                      GLenum type,
                                                      const void *indices);
typedef void(KHRONOS_APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n,
                                                       const GLuint *buffers);
typedef void(KHRONOS_APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(
    GLsizei n, const GLuint *arrays);
typedef void(KHRONOS_APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef GLint(KHRONOS_APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(
    GLuint program, const GLchar *name);
typedef void(KHRONOS_APIENTRY *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0,
                                                   GLfloat v1);
typedef void(KHRONOS_APIENTRY *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(KHRONOS_APIENTRY *PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0,
                                                   GLfloat v1, GLfloat v2);
typedef void(KHRONOS_APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void(KHRONOS_APIENTRY *PFNGLGENTEXTURESPROC)(GLsizei n,
                                                     GLuint *textures);
typedef void(KHRONOS_APIENTRY *PFNGLBINDTEXTUREPROC)(GLenum target,
                                                     GLuint texture);
typedef void(KHRONOS_APIENTRY *PFNGLTEXIMAGE2DPROC)(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void *pixels);
typedef void(KHRONOS_APIENTRY *PFNGLTEXPARAMETERIPROC)(GLenum target,
                                                       GLenum pname,
                                                       GLint param);
typedef void(KHRONOS_APIENTRY *PFNGLDELETETEXTURESPROC)(GLsizei n,
                                                        const GLuint *textures);
typedef void(KHRONOS_APIENTRY *PFNGLENABLEPROC)(GLenum cap);
typedef void(KHRONOS_APIENTRY *PFNGLBLENDFUNCPROC)(GLenum sfactor,
                                                   GLenum dfactor);
typedef void(KHRONOS_APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void(KHRONOS_APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint location,
                                                          GLsizei count,
                                                          GLboolean transpose,
                                                          const GLfloat *value);

extern PFNGLCLEARPROC glad_glClear;
#define glClear glad_glClear
extern PFNGLCLEARCOLORPROC glad_glClearColor;
#define glClearColor glad_glClearColor
extern PFNGLVIEWPORTPROC glad_glViewport;
#define glViewport glad_glViewport
extern PFNGLGENBUFFERSPROC glad_glGenBuffers;
#define glGenBuffers glad_glGenBuffers
extern PFNGLBINDBUFFERPROC glad_glBindBuffer;
#define glBindBuffer glad_glBindBuffer
extern PFNGLBUFFERDATAPROC glad_glBufferData;
#define glBufferData glad_glBufferData
extern PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
#define glGenVertexArrays glad_glGenVertexArrays
extern PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
#define glBindVertexArray glad_glBindVertexArray
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
extern PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
#define glVertexAttribPointer glad_glVertexAttribPointer
extern PFNGLCREATESHADERPROC glad_glCreateShader;
#define glCreateShader glad_glCreateShader
extern PFNGLSHADERSOURCEPROC glad_glShaderSource;
#define glShaderSource glad_glShaderSource
extern PFNGLCOMPILESHADERPROC glad_glCompileShader;
#define glCompileShader glad_glCompileShader
extern PFNGLGETSHADERIVPROC glad_glGetShaderiv;
#define glGetShaderiv glad_glGetShaderiv
extern PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
#define glGetShaderInfoLog glad_glGetShaderInfoLog
extern PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
#define glCreateProgram glad_glCreateProgram
extern PFNGLATTACHSHADERPROC glad_glAttachShader;
#define glAttachShader glad_glAttachShader
extern PFNGLLINKPROGRAMPROC glad_glLinkProgram;
#define glLinkProgram glad_glLinkProgram
extern PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
#define glGetProgramiv glad_glGetProgramiv
extern PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
#define glGetProgramInfoLog glad_glGetProgramInfoLog
extern PFNGLUSEPROGRAMPROC glad_glUseProgram;
#define glUseProgram glad_glUseProgram
extern PFNGLDELETESHADERPROC glad_glDeleteShader;
#define glDeleteShader glad_glDeleteShader
extern PFNGLDRAWARRAYSPROC glad_glDrawArrays;
#define glDrawArrays glad_glDrawArrays
extern PFNGLDRAWELEMENTSPROC glad_glDrawElements;
#define glDrawElements glad_glDrawElements
extern PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
#define glDeleteBuffers glad_glDeleteBuffers
extern PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
#define glDeleteVertexArrays glad_glDeleteVertexArrays
extern PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
#define glDeleteProgram glad_glDeleteProgram
extern PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
#define glGetUniformLocation glad_glGetUniformLocation
extern PFNGLUNIFORM2FPROC glad_glUniform2f;
#define glUniform2f glad_glUniform2f
extern PFNGLUNIFORM1FPROC glad_glUniform1f;
#define glUniform1f glad_glUniform1f
extern PFNGLUNIFORM3FPROC glad_glUniform3f;
#define glUniform3f glad_glUniform3f
extern PFNGLUNIFORM1IPROC glad_glUniform1i;
#define glUniform1i glad_glUniform1i
extern PFNGLGENTEXTURESPROC glad_glGenTextures;
#define glGenTextures glad_glGenTextures
extern PFNGLBINDTEXTUREPROC glad_glBindTexture;
#define glBindTexture glad_glBindTexture
extern PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
#define glTexImage2D glad_glTexImage2D
extern PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
#define glTexParameteri glad_glTexParameteri
extern PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
#define glDeleteTextures glad_glDeleteTextures
extern PFNGLENABLEPROC glad_glEnable;
#define glEnable glad_glEnable
extern PFNGLBLENDFUNCPROC glad_glBlendFunc;
#define glBlendFunc glad_glBlendFunc
extern PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
#define glActiveTexture glad_glActiveTexture
extern PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
#define glUniformMatrix4fv glad_glUniformMatrix4fv

#ifdef __cplusplus
}
#endif

#endif
