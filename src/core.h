#pragma once
#include <vector>
#include <map>
#include "common.h"

NAMESPACE_START(cm)

struct CmdStream {
    std::vector<std::string> commands;

    CmdStream () {
        commands.reserve(128);
    }

    template <class... FmtArgs>
    CmdStream& add(std::format_string<FmtArgs...> command_fmt, FmtArgs&&... fmt_args) {
        commands.push_back(std::format(command_fmt, std::forward<FmtArgs>(fmt_args)...));
        return *this;
    }

    void clear() {
        commands.clear();
    }

    int32 run(const char* separator) {
        std::string line;
        for (uint32 i = 0; i < commands.size(); ++i) {
            line += commands[i];
            if (i != commands.size() - 1) line += separator;
        }
        return std::system(line.data());
    }
};



// prompt user via std::istream
template <bool read_line=true, class T>
inline T& prompt(T& lval) {
    if constexpr (read_line) {
        std::getline(std::cin, lval);
    }
    else { // read_line == false
        std::cin >> lval;
    }
    return lval;
}

// print to stdout via std::ostream
template <bool _flush=true, class... Args>
inline void print(Args&&... args) {
    (std::cout << ... << std::forward<Args>(args));
    if(_flush) std::flush(std::cout);
}

template <class... Args>
void debug(Args... args) {
    const strview pre = "[Debug] ";
    const strview post = "\n";
    cm::print(pre, std::forward<Args>(args)..., post);
}

// terminate the application
template <int _errc=1, class... Args>
[[noreturn]] inline void terminate(Args&&... args) {
    (std::cerr << ... << std::forward<Args>(args)) << std::endl;
    std::exit(_errc);
}



template <class T>
bool is_any_of(const T val, std::initializer_list<T> list) {
    for (auto item : list) {
        if (val == item) {
            return true;
        }
    }
    return false;
}

inline std::string concats(const char* base, const char* append) {
    return std::string(base).append(append);
}

template <class T>
inline bool vec_contains(std::vector<T> vector, T element) {
    return std::find(vector.begin(), vector.end(), element) != vector.end();
}

inline bool prefix_strip(const std::string& str, const std::string& prefix, std::string* out) {
    if (str.substr(0, prefix.size()) != prefix) return false;
    if (out) *out = str.substr(prefix.size());
    return true;
}

// create a new file in the file system
inline bool newFile(const fs::path& path) {
    if (fs::exists(path)) return false;
    std::ofstream file(path);
    return file.good();
}

// load $HOME to static constant once and return it
inline const char* userHomePath(bool terminate_on_fail = false, const char* fail_msg="") {
    static const char* home_path = getenv("HOME");
    if (home_path == nullptr) {
        if (terminate_on_fail) terminate(fail_msg);
        else return nullptr;
    }
    return home_path;
}

// parse file path by converting tilde('~') to $HOME variable
constexpr inline fs::path parsePathTilde(std::string path) {
    if (!(path[0] == '~')) return path;
    path.erase(0, 1);
    const char* user_home = userHomePath(true, "User does not have $HOME set");
    path.insert(0, user_home);
    return path;
}

NAMESPACE_END(cm)
