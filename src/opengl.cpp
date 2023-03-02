#include "opengl.h"

#include <iostream>

#include <gl/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include "sdl2.h"

using namespace CatmullClarkSubdivision;

OpenGL::OpenGL()
    : m_window { nullptr },
      m_context { NULL }
{
    m_isInit = false;
}

OpenGL::~OpenGL()
{
    std::cout << "OpenGL Destructor" << std::endl;

    if (m_isInit)
        release();
}

void OpenGL::init(SDL2Window* window)
{
    std::cout << "Initializing OpenGL" << std::endl;

    m_window = window;

    m_context = SDL_GL_CreateContext(m_window->getWindow());

    if (m_context == NULL)
        throw std::exception("OPENGL: Can't create SDL context");

    SDL_GL_MakeCurrent(m_window->getWindow(), m_context);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();

    if (glewStatus != 0)
        throw std::exception(std::string("OPENGL: ").append(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))).c_str());

    m_isInit = true;
}

void OpenGL::release()
{
    std::cout << "Releasing OpenGL" << std::endl;

    SDL_GL_DeleteContext(m_context);

    m_isInit = false;
}

void OpenGL::newFrame(std::list<Model*>& models)
{
    glClearColor(0.0f, 0.2f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (Model* mod : models)
    {
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
            static_cast<float>(m_window->getWidth()) / static_cast<float>(m_window->getHeight()),
            0.1f, 100.0f);

        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f)); // it's a bit too big for our scene, so scale it down

        mod->setModel(model);
        mod->setView(view);
        mod->setProjection(projection);

        mod->draw();
    }

    SDL_GL_SwapWindow(m_window->getWindow());
}
