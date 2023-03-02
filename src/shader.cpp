#include "shader.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include "utils.h"

using namespace CatmullClarkSubdivision;

void Shader::loadShader(const char* vertexPath, const char* fragmentPath)
{
    m_id = glCreateProgram();

    compile(vertexPath, GL_VERTEX_SHADER);
    compile(fragmentPath, GL_FRAGMENT_SHADER);

    glLinkProgram(m_id);

    /*
    const char* tmp;
    std::string code;
    std::stringstream stream;
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    unsigned id;

    file.open(vertexPath);
    stream.str(std::string());
    stream << file.rdbuf();
    file.close();
    code = stream.str();

    // Compile vertex shader
    id = glCreateShader(GL_VERTEX_SHADER);
    tmp = code.c_str();
    glShaderSource(id, 1, &tmp, nullptr);
    glCompileShader(id);
    checkError(id, "Can't compile vertex shader");

    glAttachShader(m_id, id);
    glDeleteShader(id);

    checkError(m_id, "Can't execute shader program");
    */
}

void Shader::compile(const char* path, GLenum type)
{
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.open(path);

    std::stringstream stream;
    stream.str(std::string());
    stream << file.rdbuf();
    file.close();
    std::string code = stream.str();

    // Compile fragment shader
    unsigned id = glCreateShader(type);
    const char* tmp = code.c_str();
    glShaderSource(id, 1, &tmp, nullptr);
    glCompileShader(id);
    checkError(id, "Can't compile fragment shader");

    glAttachShader(m_id, id);
    glDeleteShader(id);

    checkError(m_id, "Can't execute shader program");
}
