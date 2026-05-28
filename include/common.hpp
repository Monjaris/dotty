#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <string_view>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <cstdint>
#include <cstdio>
#include <cstring>
// 3rd party
#include <CLI/CLI.hpp>
#include <readline/readline.h>

#define NAMESPACE_START(_name) namespace _name {
#define NAMESPACE_END(_name) }
#define COMPTIME_STR constexpr const char* const
// use it only for integers, bool(true) is success
#define FAILED (1)==

#if defined(__linux__) || defined(__APPLE__)
#       define OS_NEWLN '\n'
#elif defined(_WIN32)
#   define OS_NEWLN '\r'
#endif

inline std::vector<std::string> unimplemented;
// bad yes, but i need highlight for better notice(GCC/Clang specific! who cares MSVC tho)
#define $IMPLEMENT(_feature) (unimplemented.push_back(_feature), "")
#define $PRINT_UNIMPLEMENTED_FEATURES() \
    do{if(unimplemented.size())std::cerr<<"\033[33mUNIMPLEMENTED:\n"; \
        for(auto f:unimplemented)std::cerr<<f<<"\n" \
    ;}while(0)

using namespace std::string_literals;
namespace fs = std::filesystem;

using int32 = int32_t;
using uint32 = uint32_t;
using usize = size_t;
using int64 = int64_t;
using strview = std::string_view;
