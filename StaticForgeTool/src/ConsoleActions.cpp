#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <vector>
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

    void PrintError(const std::string& context, const std::string& error) {
        std::vector<std::string> lines;
        std::istringstream stream(error);
        for (std::string line; std::getline(stream, line);) {
            if (!line.empty())
                lines.push_back(line);
        }

        std::cout << "[Error] " << context;
        if (lines.empty()) {
            std::cout << std::endl;
            return;
        }

        if (lines.size() == 1) {
            std::cout << (context.empty() ? "" : ": ") << lines[0] << std::endl;
            return;
        }

        std::cout << ":" << std::endl;
        for (const auto& line : lines)
            std::cout << "  - " << line << std::endl;
    }

    bool Help(const Config&) {
        std::cout << HELP_MESSAGE << std::endl;
        return true;
    }

    bool Info(const Config& config) {
        const auto& infoPaths = config.GetInfoPaths();
        if (infoPaths.empty()) {
            PrintError("", "No info path specified");
            return false;
        }

        for (const auto& p : infoPaths) {
            StaticForge::StaticForgeArchive archive;
            StaticForge::StaticForgeReader reader;
            if (!reader.Load(p, &archive)) {
                PrintError("Failed to load archive '" + p.generic_u8string() + "'", reader.GetError());
                return false;
            }

            if (infoPaths.size() > 1) {
                std::cout << "Archive: " << p.generic_u8string() << std::endl;
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
        if (config.GetCompressMinSaving())
            builder.SetCompressionMinSaving(*config.GetCompressMinSaving());

        if (!builder.Build()) {
            PrintError("Build failed", builder.GetError());
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
                PrintError("", "Output path '" + outputPath.generic_u8string() + "' does not exist (use --mkdir to create it)");
                return false;
            }

            if (!fs::create_directories(outputPath, ec) || ec) {
                PrintError("", "Failed to create output directory '" + outputPath.generic_u8string() + "'" + (ec ? ": " + ec.message() : std::string()));
                return false;
            }
        }

        for (const auto& archivePath : config.GetExtractPaths()) {
            StaticForge::StaticForgeArchive archive;
            StaticForge::StaticForgeReader reader;
            if (!reader.Load(archivePath, &archive)) {
                PrintError("Failed to load archive '" + archivePath.generic_u8string() + "'", reader.GetError());
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
                    PrintError("", "Invalid file name '" + relative.generic_u8string() + "' in archive '"
                        + archivePath.generic_u8string() + "' (must be a relative path inside the archive)");
                    return false;
                }

                fs::path target = (archiveDir / relative).make_preferred();
                if (fs::exists(target, ec)) {
                    PrintError("", "File '" + target.generic_u8string() + "' already exists, nothing was extracted");
                    return false;
                }

                targets.push_back(std::move(target));
            }

            std::cout << "Extracting '" << archivePath.generic_u8string() << "' (" << count << (count == 1 ? " file" : " files")
                << (storesNames ? "" : ", no names stored, files are named by hash") << ")" << std::endl;

            std::vector<std::byte> data;
            for (size_t i = 0; i < count; i++) {
                const fs::path& target = targets[i];

                fs::create_directories(target.parent_path(), ec);
                if (ec) {
                    PrintError("", "Failed to create directory '" + target.parent_path().generic_u8string() + "': " + ec.message());
                    return false;
                }

                if (!archive.LoadAssetAt(i, &data)) {
                    PrintError("Failed to load file '" + target.generic_u8string() + "'", archive.TakeError());
                    return false;
                }

                std::ofstream file(target, std::ios::binary | std::ios::trunc);
                file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
                if (!file) {
                    PrintError("", "Failed to write file '" + target.generic_u8string() + "'");
                    return false;
                }

                if (verbose)
                    std::cout << "  " << target.generic_u8string() << " (" << data.size() << " bytes)" << std::endl;
            }

            std::cout << "Extracted to '" << archiveDir.generic_u8string() << "'" << std::endl;
        }

        return true;
    }

    bool Verify(const Config& config) {
        bool allValid = true;

        // checks all archives, also if one of them is damaged
        for (const auto& archivePath : config.GetVerifyPaths()) {
            StaticForge::StaticForgeArchive archive;
            StaticForge::StaticForgeReader reader;
            if (!reader.Load(archivePath, &archive)) {
                PrintError(archivePath.generic_u8string(), reader.GetError());
                allValid = false;
                continue;
            }

            if (!archive.VerifyAll()) {
                PrintError(archivePath.generic_u8string(), archive.TakeError());
                allValid = false;
                continue;
            }

            std::cout << "[OK]    " << archivePath.generic_u8string() << " (" << archive.GetFileCount()
                << (archive.GetFileCount() == 1 ? " file" : " files") << ")" << std::endl;
        }

        return allValid;
    }

}