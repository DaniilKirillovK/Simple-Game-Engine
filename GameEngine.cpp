#include "GameEngine.h"
#include "src/Application.h"
#include "src/RenderTest.h"
#include <filesystem>

int main(int argc, char** argv) try
{
    Logger::setLogFile("engine.log");

    LOG_INFO("Starting engine...");

    // Automated offscreen render test: fixed scene + fixed camera, no window,
    // compared with golden images using tolerance thresholds.
    //   GameEngine --render-test [d3d12|vulkan|all] [--update-reference] [--golden <dir>]
    if (argc >= 2 && std::string(argv[1]) == "--render-test")
    {
        std::string api = "all";
        std::string goldenDir = "tests/render/golden";
        bool updateReference = false;

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--update-reference")
            {
                updateReference = true;
            }
            else if (arg == "--golden" && i + 1 < argc)
            {
                goldenDir = argv[++i];
            }
            else if (arg == "--api" && i + 1 < argc)
            {
                api = argv[++i];
            }
            else if (arg == "d3d12" || arg == "vulkan" || arg == "all")
            {
                api = arg;
            }
            else
            {
                LOG_ERROR("Usage: GameEngine --render-test [d3d12|vulkan|all] [--update-reference] [--golden <dir>]");
                return 2;
            }
        }

        if (api != "d3d12" && api != "vulkan" && api != "all")
        {
            LOG_ERROR("Usage: GameEngine --render-test [d3d12|vulkan|all] [--update-reference] [--golden <dir>]");
            return 2;
        }

        return runRenderTest(api, goldenDir, updateReference);
    }

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
