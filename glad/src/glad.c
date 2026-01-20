#include <glad/glad.h>
#include <stddef.h>

PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
PFNGLUNIFORM2FPROC glad_glUniform2f = NULL;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
PFNGLUNIFORM3FPROC glad_glUniform3f = NULL;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
PFNGLENABLEPROC glad_glEnable = NULL;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;

int gladLoadGLLoader(GLADloadproc load) {
  glad_glClear = (PFNGLCLEARPROC)load("glClear");
  glad_glClearColor = (PFNGLCLEARCOLORPROC)load("glClearColor");
  glad_glViewport = (PFNGLVIEWPORTPROC)load("glViewport");
  glad_glGenBuffers = (PFNGLGENBUFFERSPROC)load("glGenBuffers");
  glad_glBindBuffer = (PFNGLBINDBUFFERPROC)load("glBindBuffer");
  glad_glBufferData = (PFNGLBUFFERDATAPROC)load("glBufferData");
  glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)load("glGenVertexArrays");
  glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)load("glBindVertexArray");
  glad_glEnableVertexAttribArray =
      (PFNGLENABLEVERTEXATTRIBARRAYPROC)load("glEnableVertexAttribArray");
  glad_glVertexAttribPointer =
      (PFNGLVERTEXATTRIBPOINTERPROC)load("glVertexAttribPointer");
  glad_glCreateShader = (PFNGLCREATESHADERPROC)load("glCreateShader");
  glad_glShaderSource = (PFNGLSHADERSOURCEPROC)load("glShaderSource");
  glad_glCompileShader = (PFNGLCOMPILESHADERPROC)load("glCompileShader");
  glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)load("glGetShaderiv");
  glad_glGetShaderInfoLog =
      (PFNGLGETSHADERINFOLOGPROC)load("glGetShaderInfoLog");
  glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)load("glCreateProgram");
  glad_glAttachShader = (PFNGLATTACHSHADERPROC)load("glAttachShader");
  glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)load("glLinkProgram");
  glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)load("glGetProgramiv");
  glad_glGetProgramInfoLog =
      (PFNGLGETPROGRAMINFOLOGPROC)load("glGetProgramInfoLog");
  glad_glUseProgram = (PFNGLUSEPROGRAMPROC)load("glUseProgram");
  glad_glDeleteShader = (PFNGLDELETESHADERPROC)load("glDeleteShader");
  glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)load("glDrawArrays");
  glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)load("glDrawElements");
  glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)load("glDeleteBuffers");
  glad_glDeleteVertexArrays =
      (PFNGLDELETEVERTEXARRAYSPROC)load("glDeleteVertexArrays");
  glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)load("glDeleteProgram");
  glad_glGetUniformLocation =
      (PFNGLGETUNIFORMLOCATIONPROC)load("glGetUniformLocation");
  glad_glUniform2f = (PFNGLUNIFORM2FPROC)load("glUniform2f");
  glad_glUniform1f = (PFNGLUNIFORM1FPROC)load("glUniform1f");
  glad_glUniform3f = (PFNGLUNIFORM3FPROC)load("glUniform3f");
  glad_glUniform1i = (PFNGLUNIFORM1IPROC)load("glUniform1i");
  glad_glGenTextures = (PFNGLGENTEXTURESPROC)load("glGenTextures");
  glad_glBindTexture = (PFNGLBINDTEXTUREPROC)load("glBindTexture");
  glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC)load("glTexImage2D");
  glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC)load("glTexParameteri");
  glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC)load("glDeleteTextures");
  glad_glEnable = (PFNGLENABLEPROC)load("glEnable");
  glad_glBlendFunc = (PFNGLBLENDFUNCPROC)load("glBlendFunc");
  glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC)load("glActiveTexture");
  glad_glUniformMatrix4fv =
      (PFNGLUNIFORMMATRIX4FVPROC)load("glUniformMatrix4fv");
  return 1;
}
