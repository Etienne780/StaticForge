#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <StaticForgeCore/StaticForge.h>
#include "Config.h"
#include "ArgumentNames.h"

namespace Actions {

    static std::string HashToHex(uint64_t hash) {
        std::ostringstream stream;
        stream << std::hex << std::setw(16) << std::setfill('0') << hash;
        return stream.str();
    }

    // names come from the archive, so they must not point outside of the output folder
    static bool IsSafeRelativePath(const std::filesystem::path& path) {
        if (path.empty() || path.has_root_name() || path.has_root_directory())
            return false;

        for (const auto& part : path) {
            if (part == "..")
                return false;
        }

        return true;
    }

    bool Help(const Config& config) {
        std::cout << HELP_MESSAGE << std::endl;
        return true;
    }

    bool Info(const Config& config) {
        const auto& infoPaths = config.GetInfoPaths();
        if (infoPaths.empty()) {
            std::cout << "No info path specified." << std::endl;
            return false;
        }

        for (const auto& p : infoPaths) {
            StaticForge::StaticForgeArchive archive;
            StaticForge::StaticForgeReader reader;
            if (!reader.Load(p, &archive)) {
                std::cout << "Failed to load archive: " << reader.GetError() << std::endl;
                return false;
            }

            if (infoPaths.size() > 1) {
                std::cout << "Archive: " << p.u8string() << std::endl;
            }
            else {
                std::cout << "Archive: " << p.filename().u8string() << std::endl;
            }

            std::cout << "Version: " << archive.GetVersion() << std::endl;
            std::cout << "File count: " << archive.GetFileCount() << std::endl;
            std::cout << "Index offset: " << archive.GetIndexOffset() << std::endl;
            std::cout << "Index size: " << archive.GetIndexSize() << std::endl;
            std::cout << "Data offset: " << archive.GetDataOffset() << std::endl;

            if (config.GetVerbosePrint()) {
                std::cout << std::endl << "Index entries:" << std::endl;

                bool storeNames = archive.StoresNames();

                size_t count = archive.GetFileCount();
                for (size_t i = 0; i < count; ++i) {
                    std::cout << "  [" << i << "] ";

                    if (storeNames)
                        std::cout << "name: " << archive.GetName(i) << ", ";

                    std::cout << "hash: " << HashToHex(archive.GetHashName(i))
                        << ", offset: " << archive.GetFileOffset(i)
                        << ", size: " << archive.GetFileSize(i)
                        << std::endl;
                }
            }

            std::cout << std::endl;
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
            .SetStoreNames(config.GetStoreNames())
            .SetCompress(config.GetCompress());

        // only override the meta file values if they were set
        if (config.GetCompressBlockSize())
            builder.SetCompressionBlockSize(*config.GetCompressBlockSize());
        if (config.GetCompressTableSize())
            builder.SetCompressionTableSizeLog2(*config.GetCompressTableSize());
        if (config.GetCompressMinMatch())
            builder.SetCompressionMinMatchLength(*config.GetCompressMinMatch());

        if (!builder.Build()) {
            std::cout << "Build failed: " << builder.GetError() << std::endl;
            return false;
        }

        std::cout << "Build was successful" << std::endl;
        return true;
    }

    bool Extract(const Config& config) {
        namespace fs = std::filesystem;

        const fs::path& outputPath = config.GetOutputPath();
        const bool verbose = config.GetVerbosePrint();

        std::error_code ec;
        if (!fs::exists(outputPath, ec)) {
            if (!config.GetCreateOutputDir()) {
                std::cout << "Output path '" << outputPath.u8string() << "' does not exist (use --mkdir true to create it)" << std::endl;
                return false;
            }

            if (!fs::create_directories(outputPath, ec) || ec) {
                std::cout << "Failed to create output directory '" << outputPath.u8string() << "'" << std::endl;
                return false;
            }
        }

        for (const auto& archivePath : config.GetExtractPaths()) {
            StaticForge::StaticForgeArchive archive;
            StaticForge::StaticForgeReader reader;
            if (!reader.Load(archivePath, &archive)) {
                std::cout << "Failed to load archive '" << archivePath.u8string() << "': " << reader.GetError() << std::endl;
                return false;
            }

            // each archive gets its own folder
            const fs::path archiveDir = outputPath / archivePath.stem();
            const bool storesNames = archive.StoresNames();
            const size_t count = archive.GetFileCount();

            // resolve and check all targets first, so nothing gets written if one is invalid
            std::vector<fs::path> targets;
            targets.reserve(count);
            for (size_t i = 0; i < count; i++) {
                fs::path relative = storesNames
                    ? fs::u8path(archive.GetName(i))
                    : fs::path(HashToHex(archive.GetHashName(i)) + ".bin");

                if (!IsSafeRelativePath(relative)) {
                    std::cout << "Invalid file name '" << relative.u8string() << "' in archive '"
                        << archivePath.u8string() << "' (must be a relative path inside the archive)" << std::endl;
                    return false;
                }

                fs::path target = (archiveDir / relative).make_preferred();
                if (fs::exists(target, ec)) {
                    std::cout << "File '" << target.u8string() << "' already exists, nothing was extracted" << std::endl;
                    return false;
                }

                targets.push_back(std::move(target));
            }

            std::cout << "Extracting '" << archivePath.u8string() << "' (" << count << " files"
                << (storesNames ? "" : ", no names stored, files are named by hash") << ")" << std::endl;

            std::vector<std::byte> data;
            for (size_t i = 0; i < count; i++) {
                const fs::path& target = targets[i];

                fs::create_directories(target.parent_path(), ec);
                if (ec) {
                    std::cout << "Failed to create directory '" << target.parent_path().u8string() << "': " << ec.message() << std::endl;
                    return false;
                }

                if (!archive.LoadAssetAt(i, &data)) {
                    std::cout << "Failed to load file '" << target.u8string() << "': " << archive.TakeError() << std::endl;
                    return false;
                }

                std::ofstream file(target, std::ios::binary | std::ios::trunc);
                file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
                if (!file) {
                    std::cout << "Failed to write file '" << target.u8string() << "'" << std::endl;
                    return false;
                }

                if (verbose)
                    std::cout << "  " << target.u8string() << " (" << data.size() << " bytes)" << std::endl;
            }

            std::cout << "Extracted to '" << archiveDir.u8string() << "'" << std::endl;
        }

        return true;
    }

}