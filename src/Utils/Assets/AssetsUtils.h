#pragma once
#include <string>
#include <vector>

struct AssetInfo 
{
    std::string name;
    std::string path;
    std::string extension;
    bool isDirectory;
    std::vector<AssetInfo> children;
};

class AssetsUtils
{
public:
    static void scanAssetsDirectory(const std::string& directory, std::string& currentDirectory, std::vector<AssetInfo>& currentDirectoryContent);
};