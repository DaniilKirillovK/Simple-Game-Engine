#pragma once
#include <string>

// Automated offscreen render test (LR 3 addon, "задел к лекции 14"):
// renders a fixed scene and a fixed camera headlessly (no window), compares the
// frame with a golden PNG per graphics API using tolerance thresholds, and returns
// a non-zero exit code on mismatch.
//
//   GameEngine.exe --render-test [d3d12|vulkan|all] [--update-reference] [--golden <dir>]
//
// Golden files: <golden dir>/<api>/scene.png
int runRenderTest(const std::string& api, const std::string& goldenDir, bool updateReference);
