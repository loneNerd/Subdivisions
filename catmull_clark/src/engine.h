#pragma once
#ifndef CATMULL_CLARK_SUBDIVITION_SDL2_H_
#define CATMULL_CLARK_SUBDIVITION_SDL2_H_

#include <map>

#include <sdl2/SDL.h>

#include "model.h"

namespace CatmullClarkSubdivision
{
    class Engine
    {
    public:
        Engine();
        ~Engine();

        Engine(const Engine& other)            = delete;
        Engine(Engine&& other)                 = delete;
        Engine& operator=(const Engine& other) = delete;
        Engine& operator=(Engine&& other)      = delete;

        void init();
        void release();

        void update();

        SDL_Window* getWindow()     const { return m_window; }
        const SDL_Event& getEvent() const { return m_event; }

        void setTitle(const char* title);

        void addModel(const char* path, const char* name);

        Model* getModel(const char* name) { return m_models[name]; }

        bool isWindowClosed() { return m_isWindowClosed; }

        int getWidth()  const { return INITIAL_WIDTH; }
        int getHeight() const { return INITIAL_HEIGHT; }

    private:
        const char* const m_glslVersion = "#version 460";

        const float MOVEMENT_SPEED = 500.0f;
        const int INITIAL_WIDTH    = 1280;
        const int INITIAL_HEIGHT   = 720;

        SDL_Window* m_window = nullptr;
        SDL_GLContext m_context = NULL;
        SDL_Event m_event { SDL_FIRSTEVENT };

        std::map<std::string, Model*> m_models;

        bool m_wireframe = true;
        EModelViewType m_type = EModelViewType::EOriginal;

        bool m_isWindowClosed = false;
        bool m_isInit         = false;
    };
}

#endif // CATMULL_CLARK_SUBDIVITION_SDL2_H_
