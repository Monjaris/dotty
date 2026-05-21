#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <cstring>
// #include <unistd.h>  // to help termios.h only
// #include <termios.h>
#include <readline/readline.h>
#include <CLI/CLI.hpp>

#define NAMESPACE_START(_name) namespace _name {
#define NAMESPACE_END(_name) }
#define COMPTIME_STR constexpr const char* const
#define FAILED (1)==

#if defined(__linux__) || defined(__APPLE__)
#       define OS_NEWLN '\n'
#elif defined(_WIN32)
#   define OS_NEWLN '\r'
#endif

std::vector<std::string> unimplemented;
// bad yes, but i need highlight for better notice(GCC/Clang specific! who cares MSVC tho)
#define $IMPLEMENT(_feature) (unimplemented.push_back(_feature), "")

using namespace std::string_literals;
namespace fs = std::filesystem;

using int32 = int32_t;
using uint32 = uint32_t;
using usize = size_t;
using int64 = int64_t;
using strview = std::string_view;

// contains T  and an error code
template <class T>
struct Resulted {
    T result = {};
    int32 errc = 0;
};
