#include "RenderTest.h"

#include <filesystem>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>
#include <string>

#include "Logger.h"
#include "NvrhiRenderAdapter.h"
#include "World.h"
#include "Systems/TransformSystem.h"
#include "Systems/RenderSystem.h"
#include "Utils/Serialization/SceneSerializer.h"
#include "Components/MeshRenderer.h"
#include "Components/Material.h"
#include "Components/Tag.h"
#include "Components/Light.h"
#include "Resources/TextureLoader/Texture.h"
#include "Resources/MeshLoader/MeshLoader.h"
#include "Resources/ShaderLoader/ShaderLoader.h"
#include "Resources/TextureLoader/TextureLoader.h"
#include <Resources/ResourceManager.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <libs/stb_image/stb_image_write.h>
#include <libs/stb_image/stb_image.h> // reader; the implementation lives in TextureLoader.cpp

namespace
{
    constexpr int kFrameWidth = 1280;
    constexpr int kFrameHeight = 720;
    constexpr uint32_t kFrameCount = 3; // two warm-up frames + the captured one

    // Tolerance thresholds ("порог допуска").
    constexpr int kMaxChannelDiff = 2;         // max |a-b| per channel, 0..255
    constexpr double kMaxDifferingFraction = 0.001; // 0.1% of pixels with any channel difference
    constexpr double kMaxMae = 0.2;            // mean absolute error per channel, 0..255

    struct FrameCompare
    {
        int maxChannelDiff = 0;
        size_t differingPixels = 0;
        double mae = 0.0;
        size_t totalPixels = 0;
    };

    FrameCompare compareFrames(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& golden)
    {
        FrameCompare result;
        result.totalPixels = actual.size() / 3;

        double errorSum = 0.0;
        for (size_t i = 0; i + 2 < actual.size(); i += 3)
        {
            int d0 = std::abs(int(actual[i]) - int(golden[i]));
            int d1 = std::abs(int(actual[i + 1]) - int(golden[i + 1]));
            int d2 = std::abs(int(actual[i + 2]) - int(golden[i + 2]));

            result.maxChannelDiff = std::max({ result.maxChannelDiff, d0, d1, d2 });
            errorSum += double(d0 + d1 + d2);
            if (d0 > 0 || d1 > 0 || d2 > 0)
            {
                ++result.differingPixels;
            }
        }

        result.mae = actual.empty() ? 0.0 : errorSum / double(actual.size());
        return result;
    }

    bool withinTolerance(const FrameCompare& c)
    {
        const double differingFraction = c.totalPixels ? double(c.differingPixels) / double(c.totalPixels) : 1.0;
        const bool okMax = c.maxChannelDiff <= kMaxChannelDiff;
        const bool okFraction = differingFraction <= kMaxDifferingFraction;
        const bool okMae = c.mae <= kMaxMae;

        LOG_INFO("Render test metrics: maxChannelDiff=" + std::to_string(c.maxChannelDiff)
            + " (limit " + std::to_string(kMaxChannelDiff) + ")"
            + (okMax ? " ok" : " FAILED")
            + ", differing=" + std::to_string(c.differingPixels) + "/" + std::to_string(c.totalPixels)
            + " " + std::to_string(differingFraction * 100.0) + "%"
            + " (limit " + std::to_string(kMaxDifferingFraction * 100.0) + "%)"
            + (okFraction ? " ok" : " FAILED")
            + ", mae=" + std::to_string(c.mae)
            + " (limit " + std::to_string(kMaxMae) + ")"
            + (okMae ? " ok" : " FAILED"));

        return okMax && okFraction && okMae;
    }

    // Deterministic checkerboard texture generated in code: the test covers the
    // textured path without depending on any asset file.
    std::vector<uint8_t> makeChecker(int size)
    {
        std::vector<uint8_t> pixels(size_t(size) * size * 4);
        const uint8_t light = 235;
        const uint8_t dark = 70;
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const bool isDark = ((x * 8 / size) + (y * 8 / size)) % 2 == 1;
                const uint8_t v = isDark ? dark : light;
                const size_t o = (size_t(y) * size + x) * 4;
                pixels[o] = v;
                pixels[o + 1] = v;
                pixels[o + 2] = v;
                pixels[o + 3] = 255;
            }
        }
        return pixels;
    }

    // Returns a copy of the checker pixels allocated with malloc (TextureData frees
    // its pixels with stbi_image_free, which is plain free()).
    unsigned char* copyPixels(const std::vector<uint8_t>& source)
    {
        auto* pixels = static_cast<unsigned char*>(std::malloc(source.size()));
        if (pixels)
        {
            std::memcpy(pixels, source.data(), source.size());
        }
        return pixels;
    }

    bool runApi(bool vulkan, const std::string& goldenDir, bool updateReference)
    {
        const std::string api = vulkan ? "vulkan" : "d3d12";
        LOG_INFO("Render test: starting " + api + " (" + std::to_string(kFrameWidth) + "x"
            + std::to_string(kFrameHeight) + ")");

        NvrhiRenderAdapter adapter(vulkan, false);
        if (!adapter.initializeHeadless(kFrameWidth, kFrameHeight))
        {
            LOG_ERROR("Render test FAILED: cannot initialize headless " + api);
            return false;
        }

        World world;
        world.addSystem(std::make_unique<TransformSystem>());
        world.addSystem(std::make_unique<RenderSystem>(&adapter));

        // Resource loaders must be registered before the scene is deserialized,
        // otherwise materials (which need compiled shaders) are skipped. The resource
        // cache is cleared first: shaders cached by a previous API run carry GPU ids
        // of that run's device and are invalid for this one.
        RESOURCE_MANAGER.unloadAllResources();
        MeshLoader::getInstance().registerLoader();
        TextureLoader::getInstance().registerLoader(&adapter);
        ShaderLoader::getInstance().registerLoader(&adapter);

        if (!SceneSerializer::loadScene(adapter, world, "assets\\scenes\\render_test.json"))
        {
            LOG_ERROR("Render test FAILED: cannot load assets/scenes/render_test.json");
            return false;
        }

        // Inject the procedural checkerboard texture onto the entities tagged
        // "Checkered" (their material is declared without a texture in the scene file).
        std::vector<uint8_t> checker = makeChecker(256);
        TextureData checkerData(256, 256, 4, copyPixels(checker));
        if (!checkerData.pixels)
        {
            LOG_ERROR("Render test FAILED: cannot allocate checker pixels");
            return false;
        }
        const uint32_t checkerHandle = static_cast<IRenderAdapter&>(adapter).createTexture(checkerData);
        if (checkerHandle == 0)
        {
            LOG_ERROR("Render test FAILED: cannot upload checker texture");
            return false;
        }
        Texture checkerTexture(checkerHandle, checkerData, "render_test:procedural_checker");

        {
            auto& mrPool = world.getComponentPool<MeshRenderer>();
            auto& tagPool = world.getComponentPool<Tag>();
            auto& renderers = mrPool.components();
            auto& rendererEnts = mrPool.entities();
            size_t injected = 0;
            for (size_t i = 0; i < renderers.size(); ++i)
            {
                if (!renderers[i].material) continue;
                if (Tag* tag = tagPool.getComponent(rendererEnts[i]); tag && tag->name == "Checkered")
                {
                    renderers[i].material->diffuseTexture = &checkerTexture;
                    renderers[i].material->hasTexture = true;
                    ++injected;
                }
            }
            LOG_INFO("Render test: checker texture injected into " + std::to_string(injected) + " entities");
        }

        // Render a few frames; the last one is captured. TransformSystem computes the
        // world matrices and RenderSystem submits the scene inside the frame callback.
        adapter.runFramesHeadless([&]() {
            world.update(1.0f / 60.0f);
        }, kFrameCount);

        std::vector<uint8_t> frame;
        uint32_t frameWidth = 0, frameHeight = 0;
        if (!adapter.readSceneToMemory(frame, frameWidth, frameHeight))
        {
            LOG_ERROR("Render test FAILED: frame readback failed");
            return false;
        }
        if (frameWidth != kFrameWidth || frameHeight != kFrameHeight || frame.size() != size_t(kFrameWidth) * kFrameHeight * 3)
        {
            LOG_ERROR("Render test FAILED: unexpected frame size " + std::to_string(frameWidth) + "x"
                + std::to_string(frameHeight));
            return false;
        }

        const std::filesystem::path goldenPath = std::filesystem::path(goldenDir) / api / "scene.png";

        if (updateReference)
        {
            std::error_code ec;
            std::filesystem::create_directories(goldenPath.parent_path(), ec);
            if (!stbi_write_png(goldenPath.string().c_str(), int(frameWidth), int(frameHeight), 3,
                frame.data(), int(frameWidth) * 3))
            {
                LOG_ERROR("Render test FAILED: cannot write golden " + goldenPath.string());
                return false;
            }
            LOG_INFO("Render test: golden written: " + goldenPath.string());
            LOG_INFO("Render test PASSED (" + api + ", reference updated)");
            return true;
        }

        int goldenWidth = 0, goldenHeight = 0, goldenChannels = 0;
        unsigned char* goldenPixels = stbi_load(goldenPath.string().c_str(), &goldenWidth, &goldenHeight, &goldenChannels, 3);
        if (!goldenPixels)
        {
            LOG_ERROR("Render test FAILED: golden missing or unreadable: " + goldenPath.string()
                + " (generate it with --update-reference)");
            return false;
        }
        if (goldenWidth != int(frameWidth) || goldenHeight != int(frameHeight))
        {
            LOG_ERROR("Render test FAILED: golden size mismatch: " + std::to_string(goldenWidth) + "x"
                + std::to_string(goldenHeight) + " vs frame " + std::to_string(frameWidth) + "x"
                + std::to_string(frameHeight));
            stbi_image_free(goldenPixels);
            return false;
        }

        std::vector<uint8_t> goldenRgb(goldenPixels, goldenPixels + size_t(goldenWidth) * goldenHeight * 3);
        stbi_image_free(goldenPixels);

        const FrameCompare compare = compareFrames(frame, goldenRgb);
        const bool passed = withinTolerance(compare);

        if (passed)
        {
            LOG_INFO("Render test PASSED (" + api + ")");
        }
        else
        {
            LOG_ERROR("Render test FAILED (" + api + "): frame differs from the golden beyond tolerance");
        }
        return passed;
    }
}

int runRenderTest(const std::string& api, const std::string& goldenDir, bool updateReference)
{
    bool passed = true;

    if (api == "all" || api == "d3d12")
    {
        passed &= runApi(false, goldenDir, updateReference);
    }
    if (api == "all" || api == "vulkan")
    {
        passed &= runApi(true, goldenDir, updateReference);
    }

    LOG_INFO(passed ? "Render test suite PASSED" : "Render test suite FAILED");
    return passed ? 0 : 1;
}
