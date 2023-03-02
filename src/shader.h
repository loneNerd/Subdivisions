#pragma once
#ifndef CATMULL_CLARK_SUBDIVITION_SHADER_H_
#define CATMULL_CLARK_SUBDIVITION_SHADER_H_

#include <exception>
#include <string>

//#include <gl/glew.h>
#undef APIENTRY
#include <glad/glad.h>

#include <glm/glm.hpp>

namespace CatmullClarkSubdivision
{
    class Shader
    {
    public:
        Shader() { }
        ~Shader() { }

        void loadShader(const char* vertexPath, const char* fragmentPath);
        void release() { glDeleteProgram(m_id); }

        // activate the shader
        // ------------------------------------------------------------------------
        void use()
        {
            glUseProgram(m_id);
        }

        unsigned getId() const { return m_id; }

        // utility uniform functions
        // ------------------------------------------------------------------------
        void setBool(const char* name, bool value) const
        {
            getId();
            glUniform1i(glGetUniformLocation(m_id, name), (int)value);
        }

        // ------------------------------------------------------------------------
        void setInt(const char* name, int value) const
        {
            getId();
            glUniform1i(glGetUniformLocation(m_id, name), value);
        }

        // ------------------------------------------------------------------------
        void setFloat(const char* name, float value) const
        {
            getId();
            glUniform1f(glGetUniformLocation(m_id, name), value);
        }

        // ------------------------------------------------------------------------
        void setVec2(const char* name, const glm::vec2& value) const
        {
            getId();
            glUniform2fv(glGetUniformLocation(m_id, name), 1, &value[0]);
        }
        void setVec2(const char* name, float x, float y) const
        {
            getId();
            glUniform2f(glGetUniformLocation(m_id, name), x, y);
        }

        // ------------------------------------------------------------------------
        void setVec3(const char* name, const glm::vec3& value) const
        {
            getId();
            glUniform3fv(glGetUniformLocation(m_id, name), 1, &value[0]);
        }
        void setVec3(const char* name, float x, float y, float z) const
        {
            getId();
            glUniform3f(glGetUniformLocation(m_id, name), x, y, z);
        }

        // ------------------------------------------------------------------------
        void setVec4(const char* name, const glm::vec4& value) const
        {
            getId();
            glUniform4fv(glGetUniformLocation(m_id, name), 1, &value[0]);
        }
        void setVec4(const char* name, float x, float y, float z, float w)
        {
            getId();
            glUniform4f(glGetUniformLocation(m_id, name), x, y, z, w);
        }

        // ------------------------------------------------------------------------
        void setMat2(const char* name, const glm::mat2& mat) const
        {
            getId();
            glUniformMatrix2fv(glGetUniformLocation(m_id, name), 1, GL_FALSE, &mat[0][0]);
        }

        // ------------------------------------------------------------------------
        void setMat3(const char* name, const glm::mat3& mat) const
        {
            getId();
            glUniformMatrix3fv(glGetUniformLocation(m_id, name), 1, GL_FALSE, &mat[0][0]);
        }

        // ------------------------------------------------------------------------
        void setMat4(const char* name, const glm::mat4& mat) const
        {
            getId();
            glUniformMatrix4fv(glGetUniformLocation(m_id, name), 1, GL_FALSE, &mat[0][0]);
        }

    private:
        void checkError(int id, const char* message)
        {
            int success;
            char infoLog[512];
            glGetShaderiv(id, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(id, 512, nullptr, infoLog);
                throw std::exception(std::string("OPENGL: ").append(message).c_str());
            }
        }

        void compile(const char* path, GLenum type);

        unsigned m_id = 0;
    };
}

#endif // CATMULL_CLARK_SUBDIVITION_SHADER_H_
