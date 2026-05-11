// GameEngine.cpp: определяет точку входа для приложения.
//

#include "GameEngine.h"
#include "src/Application.h"
#include <filesystem>

int main()
{
    Logger::setLogFile("engine.log");

    LOG_INFO("Starting engine...");

    Application app;
    if (!app.initialize(1920, 1080, "Simple Game Engine")) 
    {
        return -1;
    }

    app.run();
    LOG_INFO("Engine terminated");
    return 0;
}
