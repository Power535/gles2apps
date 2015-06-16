#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

int create_program(const char *v, const char *f) {
    char pszInfoLog[1024];
    int nShaderStatus, nInfoLogLength;

    //
    int vertshaderhandle = glCreateShader(GL_VERTEX_SHADER);
    int fragshaderhandle = glCreateShader(GL_FRAGMENT_SHADER);
    int programhandle = glCreateProgram();

    //
    char *pszProgramString0 = (char *)v;
    int nProgramLength0 = strlen(v);
    glShaderSource(vertshaderhandle, 1, (const char **)&pszProgramString0,
                   &nProgramLength0);
    glCompileShader(vertshaderhandle);
    glGetShaderiv(vertshaderhandle, GL_COMPILE_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE) {
        printf("Error: Failed to compile GLSL shader\n");
        glGetShaderInfoLog(vertshaderhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }
    glAttachShader(programhandle, vertshaderhandle);

    //
    char *pszProgramString1 = (char *)f;
    int nProgramLength1 = strlen(f);
    glShaderSource(fragshaderhandle, 1, (const char **)&pszProgramString1,
                   &nProgramLength1);
    glCompileShader(fragshaderhandle);
    glGetShaderiv(fragshaderhandle, GL_COMPILE_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE) {
        printf("Error: Failed to compile GLSL shader\n");
        glGetShaderInfoLog(fragshaderhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }
    glAttachShader(programhandle, fragshaderhandle);

    glLinkProgram(programhandle);
    glGetProgramiv(programhandle, GL_LINK_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE) {
        printf("Error: Failed to link GLSL program\n");
        glGetProgramInfoLog(programhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }

    glValidateProgram(programhandle);
    glGetProgramiv(programhandle, GL_VALIDATE_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE) {
        printf("Error: Failed to validate GLSL program\n");
        glGetProgramInfoLog(programhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }

    return programhandle;
}
