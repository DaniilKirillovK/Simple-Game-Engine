// GameEngine.cpp: определяет точку входа для приложения.
//

#include "GameEngine.h"
#include "src/Application.h"

int main()
{
    Logger::setLogFile("engine.log");

    LOG_INFO("Starting engine...");

    Application app;
    if (!app.initialize(800, 600, "Simple Game Engine")) 
    {
        return -1;
    }

    app.run();
    LOG_INFO("Engine terminated");
    return 0;
}
