#include "Utils/Assets/AssetsUtils.h"
#include <filesystem>

void AssetsUtils::scanAssetsDirectory(const std::string& directory, std::string& currentDirectory, std::vector<AssetInfo>& currentDirectoryContent)
{
    currentDirectoryContent.clear();

    try
    {
        if (directory != "assets" && directory != ".")
        {
            AssetInfo parentInfo;
            parentInfo.name = "..";
            parentInfo.path = std::filesystem::path(directory).parent_path().string();
            parentInfo.isDirectory = true;
            parentInfo.extension = "";
            currentDirectoryContent.push_back(parentInfo);
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            AssetInfo info;
            info.name = entry.path().filename().string();
            info.path = entry.path().string();

            if (entry.is_directory() && info.path != "assets\\common")
            {
                info.isDirectory = true;
                info.extension = "";
            }
            else
            {
                info.isDirectory = false;
                info.extension = entry.path().extension().string();

                if (info.extension != ".obj" &&
                    info.extension != ".fbx" &&
                    info.extension != ".png" &&
                    info.extension != ".jpg" &&
                    info.extension != ".jpeg")
                {
                    continue;
                }
            }

            currentDirectoryContent.push_back(info);
        }

        std::sort(currentDirectoryContent.begin(), currentDirectoryContent.end(),
            [](const AssetInfo& a, const AssetInfo& b) {
                if (a.isDirectory != b.isDirectory)
                    return a.isDirectory > b.isDirectory;
                return a.name < b.name;
            });

        currentDirectory = directory;
    }
    catch (const std::exception& e) 
    {
        //
    }
}
