#include <iostream>
#include <fstream>
#include <filesystem>
#include <StaticForgeCore/StaticForge.h>
#include "Config.h"
#include "ArgumentNames.h"

namespace Actions {

    bool Help(const Config& config) {
        std::cout << HELP_MESSAGE << std::endl;
        return true;
    }

    bool Info(const Config& config) {
        const auto& infoPath = config.GetInfoPath();
        if (infoPath.empty()) {
            std::cout << "No info path specified." << std::endl;
            return false;
        }

        StaticForge::StaticForgeArchive archive;
        StaticForge::StaticForgeReader reader;
        if (!reader.Load(infoPath, &archive)) {
            std::cout << "Failed to load archive: " << reader.GetError() << std::endl;
            return false;
        }

        std::cout << "Archive: " << infoPath.filename().string() << std::endl;
        std::cout << "Version: " << archive.GetVersion() << std::endl;
        std::cout << "File count: " << archive.GetFileCount() << std::endl;
        std::cout << "Index offset: " << archive.GetIndexOffset() << std::endl;
        std::cout << "Index size: " << archive.GetIndexSize() << std::endl;
        std::cout << "Data offset: " << archive.GetDataOffset() << std::endl;

        if (config.GetVerbosePrint()) {
            std::cout << std::endl <<"Index entries:" << std::endl;

            size_t count = archive.GetFileCount();
            for (size_t i = 0; i < count; ++i) {
                std::cout << "  [" << i << "] hash: " << archive.GetHashName(i)
                    << ", offset: " << archive.GetFileOffset(i)
                    << ", size: " << archive.GetFileSize(i) 
                    << std::endl;
            }
        }
        return true;
    }

    bool Pack(const Config& config) {
        auto name = config.GetArchiveName();
        StaticForge::StaticForgeBuilder builder{ name.empty() ? "main" : name };
        builder
            .SetSourcePath(config.GetSourcePaths())
            .SetOutputPath(config.GetOutputPath())
            .SetCreateOutputDir(config.GetCreateOutputDir())
            .SetDebugMode(config.GetVerbosePrint())
            .SetStoreName(config.GetStoreName());

        if (!builder.Build()) {
            std::cout << "Build failed: " << builder.GetError() << std::endl;
            return false;
        }

        std::cout << "Build was successful" << std::endl;
        return true;
    }

}