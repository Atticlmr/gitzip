#include "gitignore.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../third_party/lzma-sdk-26.00/C/CpuArch.h"
#include "../third_party/lzma-sdk-26.00/CPP/Common/MyException.h"
#include "../third_party/lzma-sdk-26.00/CPP/Common/StdOutStream.h"
#include "../third_party/lzma-sdk-26.00/CPP/Windows/ErrorMsg.h"
#include "../third_party/lzma-sdk-26.00/CPP/Windows/NtCheck.h"
#include "../third_party/lzma-sdk-26.00/CPP/7zip/UI/Common/ExitCode.h"
#include "../third_party/lzma-sdk-26.00/CPP/7zip/UI/Common/EnumDirItems.h"
#include "../third_party/lzma-sdk-26.00/CPP/7zip/UI/Console/ConsoleClose.h"

namespace fs = std::filesystem;

extern int Main2(int numArgs, char* args[]);

using namespace NWindows;

CStdOutStream* g_StdStream = nullptr;
CStdOutStream* g_ErrStream = nullptr;

namespace {

struct Options {
    fs::path sourceDir = ".";
    fs::path outputArchive;
    fs::path ignoreFile;
    std::string archiveType = "7z";
    bool listOnly = false;
    bool verbose = false;
    bool solid = true;
    std::optional<unsigned> compressionLevel;
    std::optional<unsigned> threadCount;
    std::optional<std::string> password;
    std::vector<std::string> extraSdkArgs;
};

struct TempFile {
    explicit TempFile(fs::path path) : path(std::move(path)) {}
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    TempFile(TempFile&& other) noexcept : path(std::move(other.path)) {
        other.path.clear();
    }

    TempFile& operator=(TempFile&& other) noexcept {
        if (this != &other) {
            path = std::move(other.path);
            other.path.clear();
        }
        return *this;
    }

    ~TempFile() {
        if (!path.empty()) {
            std::error_code ec;
            fs::remove(path, ec);
        }
    }

    fs::path path;
};

std::string normalizeRelativePath(const fs::path& path) {
    return path.lexically_normal().generic_string();
}

class IgnoreResolver {
public:
    IgnoreResolver(fs::path sourceDir, fs::path rootIgnoreFile)
        : sourceDir_(std::move(sourceDir)),
          rootIgnoreFile_(std::move(rootIgnoreFile)),
          rootMatcher_(GitIgnoreMatcher::fromFile(rootIgnoreFile_)) {}

    bool isIgnored(
        const fs::path& absolutePath,
        const fs::path& relativePath,
        bool isDirectory) {
        bool ignored = false;

        if (const auto rootDecision = rootMatcher_.matchDecision(relativePath, isDirectory)) {
            ignored = *rootDecision;
        }

        const fs::path parentRelativeDir = relativePath.parent_path();
        fs::path current;
        for (const auto& part : parentRelativeDir) {
            const std::string piece = part.generic_string();
            if (piece.empty() || piece == ".") {
                continue;
            }

            current /= part;

            const fs::path ignoreFile = sourceDir_ / current / ".gitignore";
            if (!fs::exists(ignoreFile) ||
                ignoreFile.lexically_normal() == rootIgnoreFile_.lexically_normal()) {
                continue;
            }

            GitIgnoreMatcher& matcher = loadMatcher(current);
            const fs::path relativeToCurrent = fs::relative(absolutePath, sourceDir_ / current);
            if (const auto decision = matcher.matchDecision(relativeToCurrent, isDirectory)) {
                ignored = *decision;
            }
        }

        return ignored;
    }

private:
    GitIgnoreMatcher& loadMatcher(const fs::path& relativeDir) {
        const std::string key = normalizeRelativePath(relativeDir);
        auto it = matcherCache_.find(key);
        if (it == matcherCache_.end()) {
            const fs::path ignoreFile = sourceDir_ / relativeDir / ".gitignore";
            it = matcherCache_.emplace(
                key,
                GitIgnoreMatcher::fromFile(ignoreFile)).first;
        }
        return it->second;
    }

    fs::path sourceDir_;
    fs::path rootIgnoreFile_;
    GitIgnoreMatcher rootMatcher_;
    std::unordered_map<std::string, GitIgnoreMatcher> matcherCache_;
};

std::string defaultArchiveBaseName(const fs::path& sourceDir) {
    fs::path candidate = sourceDir.filename();
    if (candidate.empty()) {
        candidate = sourceDir.parent_path().filename();
    }

    if (candidate.empty()) {
        return "archive";
    }

    return candidate.string();
}

std::string defaultArchiveFileName(const fs::path& sourceDir, const std::string& archiveType) {
    const std::string baseName = defaultArchiveBaseName(sourceDir);
    if (archiveType == "xz") {
        return baseName + ".tar.xz";
    }
    return baseName + "." + archiveType;
}

bool containsGitMetadataPath(const fs::path& relativePath) {
    for (const auto& part : relativePath) {
        if (part.generic_string() == ".git") {
            return true;
        }
    }
    return false;
}

void printUsage() {
    std::cout
        << "Usage: gitzip [options]\n"
        << "  --source <dir>         Source directory to archive\n"
        << "  --output <file>        Output archive path\n"
        << "  --ignore-file <file>   Ignore rules file, default: <source>/.gitignore\n"
        << "  --format <7z|xz>       Archive format, default: 7z\n"
        << "  --level <1-9>          Compression level passed to embedded 7-Zip core\n"
        << "  --threads <N>          Compression thread count\n"
        << "  --password <text>      Enable archive encryption for 7z\n"
        << "  --no-solid             Disable solid mode\n"
        << "  --list-only            Print file list without archiving\n"
        << "  --verbose              Print ignored files\n"
        << "  -- <7zip switches...>  Pass extra switches to embedded 7-Zip core\n"
        << "  --help                 Show this help\n";
}

Options parseArguments(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        auto requireValue = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + flag);
            }
            return argv[++i];
        };

        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                options.extraSdkArgs.emplace_back(argv[j]);
            }
            break;
        }

        if (arg == "--source") {
            options.sourceDir = requireValue(arg);
        } else if (arg == "--output") {
            options.outputArchive = requireValue(arg);
        } else if (arg == "--ignore-file") {
            options.ignoreFile = requireValue(arg);
        } else if (arg == "--format") {
            options.archiveType = requireValue(arg);
        } else if (arg == "--level") {
            options.compressionLevel = static_cast<unsigned>(std::stoul(requireValue(arg)));
        } else if (arg == "--threads") {
            options.threadCount = static_cast<unsigned>(std::stoul(requireValue(arg)));
        } else if (arg == "--password") {
            options.password = requireValue(arg);
        } else if (arg == "--no-solid") {
            options.solid = false;
        } else if (arg == "--list-only") {
            options.listOnly = true;
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    options.sourceDir = fs::weakly_canonical(options.sourceDir);

    if (options.ignoreFile.empty()) {
        options.ignoreFile = options.sourceDir / ".gitignore";
    } else if (options.ignoreFile.is_relative()) {
        options.ignoreFile = fs::absolute(options.ignoreFile).lexically_normal();
    }

    if (options.outputArchive.empty()) {
        options.outputArchive = fs::current_path() /
            defaultArchiveFileName(options.sourceDir, options.archiveType);
    } else if (options.outputArchive.is_relative()) {
        options.outputArchive = fs::absolute(options.outputArchive).lexically_normal();
    }

    if (options.archiveType != "7z" && options.archiveType != "xz") {
        throw std::runtime_error("Unsupported archive format: " + options.archiveType);
    }

    if (options.compressionLevel && *options.compressionLevel > 9) {
        throw std::runtime_error("Compression level must be between 1 and 9");
    }

    if (options.threadCount && *options.threadCount == 0) {
        throw std::runtime_error("Thread count must be greater than 0");
    }

    if (options.password && options.archiveType == "xz") {
        throw std::runtime_error("Password encryption is only supported for 7z archives");
    }

    return options;
}

TempFile writeListFile(const std::vector<fs::path>& files) {
    fs::path path = fs::temp_directory_path() /
        fs::path("gitzip-list-" + std::to_string(std::rand()) + ".txt");

    std::ofstream output(path);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open temporary list file");
    }

    for (const auto& file : files) {
        output << file.generic_string() << '\n';
    }

    return TempFile(path);
}

TempFile createTempFilePath(const std::string& suffix) {
    fs::path path = fs::temp_directory_path() /
        fs::path("gitzip-temp-" + std::to_string(std::rand()) + suffix);
    return TempFile(path);
}

std::vector<fs::path> collectFiles(
    const Options& options,
    IgnoreResolver& ignoreResolver) {
    std::vector<fs::path> files;
    const std::string archivePath = normalizeRelativePath(options.outputArchive);
    std::error_code ec;

    fs::recursive_directory_iterator it(options.sourceDir, ec);
    if (ec) {
        throw std::runtime_error("Failed to scan source directory: " + ec.message());
    }

    for (const auto& entry : it) {
        const fs::path absolutePath = entry.path().lexically_normal();
        const fs::path relativePath = fs::relative(absolutePath, options.sourceDir, ec);
        if (ec) {
            throw std::runtime_error("Failed to resolve relative path: " + ec.message());
        }

        const std::string relative = normalizeRelativePath(relativePath);
        if (relative.empty()) {
            continue;
        }

        if (containsGitMetadataPath(relativePath)) {
            if (entry.is_directory()) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (normalizeRelativePath(absolutePath) == archivePath) {
            continue;
        }

        if (entry.is_directory()) {
            if (ignoreResolver.isIgnored(absolutePath, relativePath, true)) {
                if (options.verbose) {
                    std::cout << "ignored: " << relative << "/\n";
                }
                it.disable_recursion_pending();
            }
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        const bool ignored = ignoreResolver.isIgnored(absolutePath, relativePath, false);
        if (ignored) {
            if (options.verbose) {
                std::cout << "ignored: " << relative << '\n';
            }
            continue;
        }

        files.push_back(relativePath.lexically_normal());
    }

    std::sort(files.begin(), files.end());
    return files;
}

bool checkIsa() {
#ifdef MY_CPU_X86_OR_AMD64
    #if defined(__AVX2__)
    if (!CPU_IsSupported_AVX2()) {
        return false;
    }
    #elif defined(__AVX__)
    if (!CPU_IsSupported_AVX()) {
        return false;
    }
    #elif (defined(__SSE2__) && !defined(MY_CPU_AMD64)) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
    if (!CPU_IsSupported_SSE2()) {
        return false;
    }
    #elif (defined(__SSE__) && !defined(MY_CPU_AMD64)) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 1))
    if (!CPU_IsSupported_SSE() || !CPU_IsSupported_CMOV()) {
        return false;
    }
    #endif
#endif
    return true;
}

int runSdkMain(std::vector<std::string> args) {
    NConsoleClose::CCtrlHandlerSetter ctrlHandlerSetter;

    std::vector<char*> argv;
    argv.reserve(args.size());
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }

    try {
        return Main2(static_cast<int>(argv.size()), argv.data());
    } catch (const CNewException&) {
        std::cerr << "ERROR: Can't allocate required memory!\n";
        return NExitCode::kMemoryError;
    } catch (const CMessagePathException& ex) {
        std::cerr << "Command Line Error: " << ex << '\n';
        return NExitCode::kUserError;
    } catch (const CSystemException& systemError) {
        if (systemError.ErrorCode == E_OUTOFMEMORY) {
            std::cerr << "ERROR: Can't allocate required memory!\n";
            return NExitCode::kMemoryError;
        }
        if (systemError.ErrorCode == E_ABORT) {
            std::cerr << "Break signaled\n";
            return NExitCode::kUserBreak;
        }
        std::cerr << "System ERROR: "
                  << NError::MyFormatMessage(systemError.ErrorCode).Ptr()
                  << '\n';
        return NExitCode::kFatalError;
    } catch (NExitCode::EEnum exitCode) {
        std::cerr << "Internal Error #" << static_cast<int>(exitCode) << '\n';
        return exitCode;
    } catch (const UString& ex) {
        std::cerr << "ERROR: " << ex << '\n';
        return NExitCode::kFatalError;
    } catch (const AString& ex) {
        std::cerr << "ERROR: " << ex.Ptr() << '\n';
        return NExitCode::kFatalError;
    } catch (const char* ex) {
        std::cerr << "ERROR: " << ex << '\n';
        return NExitCode::kFatalError;
    } catch (const wchar_t* ex) {
        std::wcerr << L"ERROR: " << ex << L'\n';
        return NExitCode::kFatalError;
    }
}

void writeOctal(char* dest, std::size_t size, std::uint64_t value) {
    std::snprintf(dest, size, "%0*lo", static_cast<int>(size - 1), static_cast<unsigned long>(value));
}

void fillHeaderField(char* dest, std::size_t size, const std::string& value) {
    std::memset(dest, 0, size);
    const std::size_t copySize = std::min(value.size(), size);
    std::memcpy(dest, value.data(), copySize);
}

void splitTarPath(const std::string& path, std::string& name, std::string& prefix) {
    if (path.size() <= 100) {
        name = path;
        prefix.clear();
        return;
    }

    const std::size_t splitPos = path.rfind('/', 255);
    if (splitPos == std::string::npos || splitPos > 155 || (path.size() - splitPos - 1) > 100) {
        throw std::runtime_error("Path too long for tar header: " + path);
    }

    prefix = path.substr(0, splitPos);
    name = path.substr(splitPos + 1);
}

void writeTarHeader(std::ofstream& output, const fs::path& fullPath, const fs::path& relativePath) {
    char header[512];
    std::memset(header, 0, sizeof(header));

    const std::string tarPath = relativePath.generic_string();
    std::string name;
    std::string prefix;
    splitTarPath(tarPath, name, prefix);

    fillHeaderField(header, 100, name);
    writeOctal(header + 100, 8, 0644);
    writeOctal(header + 108, 8, 0);
    writeOctal(header + 116, 8, 0);
    writeOctal(header + 124, 12, static_cast<std::uint64_t>(fs::file_size(fullPath)));
    writeOctal(header + 136, 12, static_cast<std::uint64_t>(std::time(nullptr)));
    std::memset(header + 148, ' ', 8);
    header[156] = '0';
    fillHeaderField(header + 257, 6, "ustar");
    fillHeaderField(header + 263, 2, "00");
    fillHeaderField(header + 345, 155, prefix);

    unsigned int checksum = 0;
    for (unsigned char byte : header) {
        checksum += byte;
    }

    std::snprintf(header + 148, 8, "%06o", checksum);
    header[154] = '\0';
    header[155] = ' ';
    output.write(header, sizeof(header));
}

TempFile writeTarFile(const Options& options, const std::vector<fs::path>& files) {
    TempFile tarFile = createTempFilePath(".tar");
    std::ofstream output(tarFile.path, std::ios::binary);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to create temporary tar file");
    }

    std::vector<char> buffer(64 * 1024);
    for (const auto& relativePath : files) {
        const fs::path fullPath = options.sourceDir / relativePath;
        writeTarHeader(output, fullPath, relativePath);

        std::ifstream input(fullPath, std::ios::binary);
        if (!input.is_open()) {
            throw std::runtime_error("Failed to read file for tar staging: " + fullPath.string());
        }

        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = input.gcount();
            if (count > 0) {
                output.write(buffer.data(), count);
            }
        }

        const std::uint64_t fileSize = fs::file_size(fullPath);
        const std::uint64_t padding = (512 - (fileSize % 512)) % 512;
        if (padding != 0) {
            static const char zeros[512] = {};
            output.write(zeros, static_cast<std::streamsize>(padding));
        }
    }

    static const char zeroBlock[512] = {};
    output.write(zeroBlock, sizeof(zeroBlock));
    output.write(zeroBlock, sizeof(zeroBlock));
    return tarFile;
}

int runArchive(const Options& options, const std::vector<fs::path>& files) {
    if (files.empty()) {
        throw std::runtime_error("No files selected for archiving");
    }

    const TempFile listFile = writeListFile(files);
    std::optional<TempFile> tarFile;
    std::vector<std::string> sdkArgs{
        "gitzip",
        "a",
        "-y",
        options.archiveType == "xz" ? "-txz" : "-t7z"
    };

    if (options.compressionLevel) {
        sdkArgs.push_back("-mx=" + std::to_string(*options.compressionLevel));
    }
    if (options.threadCount) {
        sdkArgs.push_back("-mmt=" + std::to_string(*options.threadCount));
    }
    if (!options.solid && options.archiveType == "7z") {
        sdkArgs.push_back("-ms=off");
    }
    if (options.password) {
        sdkArgs.push_back("-p" + *options.password);
        sdkArgs.push_back("-mhe=on");
    }
    for (const std::string& extra : options.extraSdkArgs) {
        sdkArgs.push_back(extra);
    }

    sdkArgs.push_back(options.outputArchive.string());
    if (options.archiveType == "xz") {
        tarFile = writeTarFile(options, files);
        sdkArgs.push_back(tarFile->path.string());
    } else {
        sdkArgs.push_back("@" + listFile.path.string());
    }

    const fs::path previousCwd = fs::current_path();
    fs::current_path(options.sourceDir);
    const int exitCode = runSdkMain(std::move(sdkArgs));
    fs::current_path(previousCwd);

    return exitCode;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        g_ErrStream = &g_StdErr;
        g_StdStream = &g_StdOut;

        if (!checkIsa()) {
            throw std::runtime_error("processor doesn't support required ISA extension");
        }

        const Options options = parseArguments(argc, argv);

        if (!fs::exists(options.sourceDir) || !fs::is_directory(options.sourceDir)) {
            throw std::runtime_error("Source directory does not exist: " + options.sourceDir.string());
        }

        IgnoreResolver ignoreResolver(options.sourceDir, options.ignoreFile);
        const auto files = collectFiles(options, ignoreResolver);

        if (options.listOnly) {
            for (const auto& file : files) {
                std::cout << file.generic_string() << '\n';
            }
            std::cout << "Total files: " << files.size() << '\n';
            return 0;
        }

        const int exitCode = runArchive(options, files);
        if (exitCode != 0) {
            throw std::runtime_error(
                "embedded 7-Zip core failed with exit code " + std::to_string(exitCode));
        }

        std::cout << "Archive created: " << options.outputArchive << '\n';
        std::cout << "Files archived: " << files.size() << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
