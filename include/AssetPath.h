#ifndef ASSET_PATH_H
#define ASSET_PATH_H

#include <filesystem>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class AssetPath {
public:
    static void initialize() {
        if (initialized) return;

        resourceRoot = findResourceRoot();
        initialized = true;
    }

    static const std::filesystem::path& root() {
        initialize();
        return resourceRoot;
    }

    static std::filesystem::path resolve(const std::filesystem::path& path) {
        if (path.empty() || path.is_absolute()) return path;
        initialize();
        return resourceRoot / path;
    }

    static void makeRootCurrentDirectory() {
        initialize();
        try {
            std::filesystem::current_path(resourceRoot);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to set working directory to asset root: "
                      << resourceRoot.string() << " (" << e.what() << ")" << std::endl;
        }
    }

private:
    static inline bool initialized = false;
    static inline std::filesystem::path resourceRoot;

    static bool isResourceRoot(const std::filesystem::path& path) {
        return std::filesystem::exists(path / "assets") &&
               std::filesystem::exists(path / "shaders");
    }

    static std::filesystem::path searchParents(std::filesystem::path path) {
        if (path.empty()) return {};

        if (path.has_filename() && !std::filesystem::is_directory(path)) {
            path = path.parent_path();
        }

        while (!path.empty()) {
            if (isResourceRoot(path)) return path;
            std::filesystem::path parent = path.parent_path();
            if (parent == path) break;
            path = parent;
        }

        return {};
    }

    static std::filesystem::path executablePath() {
#ifdef _WIN32
        char buffer[MAX_PATH] = {};
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH) return std::filesystem::path(buffer);
#endif
        return {};
    }

    static std::filesystem::path findResourceRoot() {
        std::filesystem::path found = searchParents(executablePath());
        if (!found.empty()) return found;

        try {
            found = searchParents(std::filesystem::current_path());
            if (!found.empty()) return found;
            return std::filesystem::current_path();
        } catch (const std::filesystem::filesystem_error&) {
            return ".";
        }
    }
};

#endif
