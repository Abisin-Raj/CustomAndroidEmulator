#include "core/utils.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace core {

std::filesystem::path getExecutableDir() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

} // namespace core
