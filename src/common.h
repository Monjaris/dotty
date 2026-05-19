#pragma once
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <cstring>
#include <CLI/CLI.hpp>

#define NAMESPACE_START(_name) namespace _name {
#define NAMESPACE_END(_name) }
#define STRCAT(STR1, STR2) STR1 ## STR2
#define COMPTIME_STR constexpr const char* const

using int32 = int32_t;
using uint32 = uint32_t;
using usize = size_t;
using int64 = int64_t;
using strview = std::string_view;
namespace fs = std::filesystem;
using namespace std::string_literals;
