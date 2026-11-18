#include "GameEngine.h"
#include "src/Application.h"
#include <filesystem>

int main(int argc, char** argv) try
{
    Logger::setLogFile("engine.log");

    LOG_INFO("Starting engine...");

    const std::string backend = argc == 1 ? "--d3d12" : argv[1];
    if (argc > 3 || (argc == 3 && std::string(argv[2]) != "--verify") || (backend != "--d3d12" && backend != "--vulkan"))
    {
        LOG_ERROR("Usage: GameEngine [--d3d12 | --vulkan] [--verify]");
        return 2;
    }
    Application app;
    if (!app.initialize(1920, 1080, backend == "--vulkan", argc == 3))
    {
        return -1;
    }

    app.run();
    LOG_INFO("Engine terminated");
    return 0;
}
catch (const std::exception& error)
{
    LOG_ERROR(error.what());
    return 1;
}
