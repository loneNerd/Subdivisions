#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2main.lib")
#pragma comment(lib, "assimp.lib")

#include <exception>
#include <iostream>

#include "engine.h"

using namespace CatmullClarkSubdivision;

int main(int argc, char* argv[])
{
    try
    {
        Engine engine;
        engine.init();
        engine.setTitle("Catmull-Clark Subdivision");

        for (; !engine.isWindowClosed(); engine.update());

        engine.release();
    }
    catch (std::exception ex)
    {
        std::cerr << "ERROR OCCURED: " << ex.what() << std::endl;
        return -1;
    }

    return 0;
}
