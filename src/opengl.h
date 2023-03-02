#pragma once
#ifndef CATMULL_CLARK_SUBDIVITION_OPENGL_H_
#define CATMULL_CLARK_SUBDIVITION_OPENGL_H_

#include <exception>
#include <string>
#include <list>

#include <sdl2/SDL.h>

#include "model.h"

namespace CatmullClarkSubdivision
{
    class SDL2Window;
    class Model;

    class OpenGL
    {
    public:
        OpenGL();
        ~OpenGL();

        OpenGL(const OpenGL& other)            = delete;
        OpenGL(OpenGL&& other)                 = delete;
        OpenGL& operator=(const OpenGL& other) = delete;
        OpenGL& operator=(OpenGL&& other)      = delete;

        void init(SDL2Window* window);
        void release();
        void newFrame(std::list<Model*>& models);

    private:
        SDL2Window* m_window = nullptr;

        SDL_GLContext m_context = NULL;

        bool m_isInit = false;
    };
}

#endif // CATMULL_CLARK_SUBDIVITION_OPENGL_H_
