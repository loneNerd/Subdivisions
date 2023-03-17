#include "engine.h"

#include <algorithm>
#include <iostream>

#undef APIENTRY
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_impl_opengl3.h>

#include "utils.h"

using namespace CatmullClarkSubdivision;

Engine::Engine()
    : m_window { nullptr },
      m_event { SDL_FIRSTEVENT }
{
    m_isWindowClosed = true;
    m_isInit = false;
}

Engine::~Engine()
{
    if (m_isInit)
        release();
}

void Engine::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER))
        throw std::exception(SDL_GetError());

    m_window = SDL_CreateWindow("",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                INITIAL_WIDTH,
                                INITIAL_HEIGHT,
                                SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);

    if (!m_window)
        throw std::exception(SDL_GetError());

    m_context = SDL_GL_CreateContext(m_window);

    if (m_context == NULL)
        throw std::exception("OPENGL: Can't create SDL context");

    SDL_GL_MakeCurrent(m_window, m_context);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

    if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
        throw std::exception(std::string("OPENGL: ").append("Failed to initialize GLAD").c_str());

    glEnable(GL_DEPTH_TEST);

    addModel("resources\\cube\\cube.obj", "Cube");
    addModel("resources\\torus\\torus.obj", "Torus");

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(m_window, m_context);
    ImGui_ImplOpenGL3_Init(m_glslVersion);

    m_isInit = true;
    m_isWindowClosed = false;
}

void Engine::release()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    for (auto& model : m_models)
        delete model.second;

    SDL_GL_DeleteContext(m_context);

    if (m_window)
        SDL_DestroyWindow(m_window);

    SDL_Quit();

    m_isInit = false;
}

void Engine::update()
{
    glm::vec3 movement = glm::vec3(0.0f);
    glm::vec2 rotation = glm::vec2(0.0f);
    float scale = 1.0f;

    SDL_PollEvent(&m_event);

    switch (m_event.type)
    {
        case SDL_MOUSEMOTION:
        {
            if (SDL_GetMouseState(nullptr, nullptr) & (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK))
            {
                if (m_event.motion.xrel != 0)
                    movement.x = m_event.motion.xrel / MOVEMENT_SPEED;

                if (m_event.motion.yrel != 0)
                    movement.y = m_event.motion.yrel / -MOVEMENT_SPEED;
            }

            if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK)
            {
                if (m_event.motion.xrel != 0)
                    rotation.y = static_cast<float>(m_event.motion.xrel);

                if (m_event.motion.yrel != 0)
                    rotation.x = static_cast<float>(m_event.motion.yrel);
            }

            break;
        }
        case SDL_MOUSEWHEEL:
        {
            if (m_event.wheel.y != 0)
                scale += m_event.wheel.y / 10.0f;

            break;
        }
        case SDL_KEYDOWN:
        {
            if (m_event.key.keysym.sym == SDLK_ESCAPE)
                m_isWindowClosed = true;
            break;
        }
        case SDL_WINDOWEVENT:
        {
            if (m_event.window.event == SDL_WINDOWEVENT_CLOSE)
                m_isWindowClosed = true;
            break;
        }
        case SDL_QUIT:
        {
            m_isWindowClosed = true;
            break;
        }
        default:
        {
            break;
        }
    }

    glClearColor(0.0f, 0.2f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // GUI
    ImGui_ImplSDL2_ProcessEvent(&m_event);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    static size_t idx = 0;
    std::vector<const char*> values;

    std::transform(m_models.begin(), m_models.end(), std::back_inserter(values),
        [](const std::map<std::string, Model*>::value_type& val) { return val.first.c_str(); });

    if (ImGui::Begin("Setup", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::SetWindowSize(ImVec2(300.0f, 160.0f));
        ImGui::Checkbox("Wireframe", &m_wireframe);

        int type = static_cast<int>(m_type);
        ImGui::RadioButton("Original", &type, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Subdivided", &type, 1);
        m_type = static_cast<EModelViewType>(type);

        ImGui::Separator();

        if (ImGui::BeginCombo("Models", values[idx], ImGuiComboFlags_PopupAlignLeft))
        {
            for (int n = 0; n < values.size(); ++n)
            {
                if (ImGui::Selectable(values[n], idx == n))
                    idx = n;

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (idx == n)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        ImGui::Separator();

        ImGui::Text("Vertices: %d", m_models[values[idx]]->getVerticesCount(m_type));
        ImGui::Text("Quads: %d", m_models[values[idx]]->getQuadsCount(m_type));

        ImGui::End();
    }

    Model* model = m_models[values[idx]];
    model->move(movement);
    model->rotateX(model->getAngleX() + rotation.x);
    model->rotateY(model->getAngleY() + rotation.y);
    model->scale(model->getScale() * scale);

    model->draw(m_type);

    if (m_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(m_window);
}

void Engine::addModel(const char* path, const char* name)
{
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
        static_cast<float>(INITIAL_WIDTH) / static_cast<float>(INITIAL_HEIGHT),
        0.1f, 100.0f);

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.25f, -3.0f));

    Model* model = new Model;
    model->loadModel(getFileFullPath(path).c_str(), projection, view);

    m_models[ std::string(name) ] = model;
}

void Engine::setTitle(const char* title)
{
    SDL_SetWindowTitle(m_window, title);
}
