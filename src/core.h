#pragma once
#include "common.h"

#define PRINT_ENABLED (true)
#define DEBUG_ENABLED (true)

NAMESPACE_START(cm)

class CmdStream;  // (defined below after functions)


// print to stdout via std::ostream
template <bool _flush=true, class... Args>
inline void print(Args&&... args) {
    if (!PRINT_ENABLED) return;
    (std::cout << ... << std::forward<Args>(args));
    if(_flush) std::flush(std::cout);
}


template <class... Args>
void debug(Args... args) {
    if (!DEBUG_ENABLED) return;
    const strview pre = "[Debug] ";
    const strview post = "\n";
    cm::print(pre, std::forward<Args>(args)..., post);
}


// uses std::istream
template <bool no_ansi_esc_seq=true, class T>
inline T& prompt(const char* prompt, T& lval) {
    // Read input relevantly
    if constexpr (std::is_arithmetic_v<T>)
    {
        if constexpr (no_ansi_esc_seq) {
            print(prompt);
            $IMPLEMENT("cm::prompt: no-ansi-esc-seq for arithmetics");
        }
        else {
            print(prompt);
            std::cin >> lval;
        }
    }
    else
    {
        if constexpr (no_ansi_esc_seq) {
            char* raw = readline(prompt);
            if (raw) {
                lval = raw;
                free(raw);
            }
        }
        else {
            print(prompt);
            std::getline(std::cin, lval);
        }
    }

    return lval;
}
// overload
template <bool no_ansi_esc_seq=true, class T>
inline T& prompt(T& lval) {
    return prompt("", lval);
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

[[nodiscard]] inline std::string strip_nl(std::string input) {
    std::string str = std::move(input);
    str.erase(std::remove(str.begin(), str.end(), OS_NEWLN), str.end());
    return std::move(str);
}

// get pair of strings, first being left of the first '.' and second being the rest
inline std::pair<std::string, std::string> obj_prop(std::string str, char sep='.') {
    std::string rest;
    for (uint32 i=0;  i < str.size();  ++i) {
        if (str[i] == sep) {
            rest = str.substr(i);
            str = str.substr(0, i);
            return {std::move(str), std::move(rest)};
        }
    }
    return {std::move(str), ""};
}

template <class T>
inline bool vec_contains(std::vector<T> vector, T element) {
    return std::find(vector.begin(), vector.end(), element) != vector.end();
}

// takes @str writes to @out only if @prefix is @str's prefix, else returns false
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




class cm::CmdStream {
    std::vector<std::string> commands;
    std::string output_buf;

public:
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

    int32 run(const char* separator, bool capture_output) {
        output_buf.clear();

        std::string line;
        for (uint32 i = 0; i < commands.size(); ++i) {
            line += commands[i];
            if (i != commands.size() - 1) line += separator;
        }

        if (capture_output) {
            std::string out;
            std::unique_ptr<FILE, decltype(&pclose)> pipe = {  // coolest RAII trick fr
                popen(line.c_str(), "r"), pclose
            };
            if (pipe == nullptr) return -1;

            char buf[256];
            while (fgets(buf, sizeof(buf), pipe.get()) != nullptr)
            {
                output_buf.append(buf);
            }
            output_buf = cm::strip_nl(output_buf);

            return pclose(pipe.release());
        }
        return std::system(line.data());
    }

    // Output loads to internal buffer after calling .run().
    // this function, moves internal buffer when returns
    [[nodiscard]] std::string output() {
        return std::move(output_buf);
    }
};

