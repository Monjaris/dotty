#pragma once
#pragma GCC diagnostic ignored "-Wignored-attributes"
#include "common.hpp"

#ifndef DEBUG_ON
#   define DEBUG_ON (1)
#endif

#ifndef PRINT_ON
#   define PRINT_ON (1)
#endif


NAMESPACE_START(cm)

struct Report;
class CmdStream;


// print to stdout via std::ostream
template <bool _flush=true, class... Args>
inline void print(Args&&... args) {
#if !PRINT_ON
        return;
#endif
    (std::cout << ... << std::forward<Args>(args));
    if(_flush) std::flush(std::cout);
}


template <class... Args>
void debug(Args... args) {
#if !DEBUG_ON
    return;
#endif
    const strview pre = "\033[2m[Debug]\033[0m ";
    const strview post = "\n";
    cm::print(pre, std::forward<Args>(args)..., post);
}


// uses std::cin or readline()
template <bool no_ansi_esc_seq=true, class T>
inline T& prompt(const char* prompt, T& lval) {
    // Read input relevantly
    if constexpr (std::is_same_v<char, T> || std::is_convertible_v<char, T>) {
        if (no_ansi_esc_seq) {
            print(prompt);
            char* raw = readline(prompt);
            if (raw) {
                lval = raw[0];
                free(raw);
            }
            cm::print("\n");
        } else {
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
            cm::print("\n");
        }
        else {
            cm::print(prompt);
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

// for arithmetic values
template <arithmetic T, class String>
requires std::same_as<String, const char*>
inline void prompt_number(const String prompt, T& number, bool no_ansi_esc_seq = true) {
    if (!no_ansi_esc_seq) {
        cm::print(prompt);
        std::cin >> number;
    }
    else {
        char* read_number = readline(prompt);
        if (!read_number || read_number[0]=='\0') return;

        try {
            number = std::stold(read_number);
        } catch (const std::exception& e) {
            cm::print("\n");
            return;
        }

        free(read_number);
    }
}


// utility for YES/NO questions
inline bool ask_confirm(const strview message, bool default_yes = true) {
    const char* post = default_yes?(" (Y/n): "):(" (y/N): ");
    std::string ask_msg = std::string(message).append(post);

    char* inp = readline(ask_msg.c_str());
    if (!inp) return(default_yes);

    // pass ownership and free
    std::string polish = (inp[::strcspn(inp, "\n")] = '\0', inp);
    free(inp);

    if (polish.empty()) return default_yes;
    if (::tolower(polish[0]) == 'y') return true;
    return(false);
}


// terminate the application
template <int _errc=1, class... Args>
[[noreturn]] inline void terminate(Args&&... args) {
    (std::cerr << ... << std::forward<Args>(args)) << std::endl;
    ::exit(_errc);
}



inline bool is_even(int32 x) {
    return (x & 1) == 0;
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
    return str;
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

template <class Container, class Underlying>
inline bool contains(const Container& cont, const Underlying& element) {
    return std::find(cont.begin(), cont.end(), element) != cont.end();
}


inline bool str_has_any_of(const std::string& str, std::initializer_list<char> chars) {
    for (char c : chars) {
        if (str.contains(c)) return true;
    }
    return false;
}

// takes @str writes to @out only if @prefix is @str's prefix, else returns false
inline bool prefix_strip(const std::string& str, const std::string& prefix, std::string* out) {
    if (str.substr(0, prefix.size()) != prefix) return false;
    if (out) *out = str.substr(prefix.size());
    return true;
}

// create a new file, return false if unsuccessful
inline bool new_file(const fs::path& path) {
    if (fs::exists(path)) return false;
    std::ofstream file(path);
    return file.good();
}

// creates directory if doesnt exist, else no-op
inline void ensure_directories(const fs::path& dir_path) {
    fs::create_directories(dir_path);
}

// copy while directory without worrying about flags to pass
inline void copy_directory(const fs::path& src_d, const fs::path& dest_d, bool cp_if_src_is_newer=false) {
    fs::copy(src_d, dest_d,
        fs::copy_options::recursive | (cp_if_src_is_newer?
        fs::copy_options::update_existing : fs::copy_options::overwrite_existing
    ));
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


// remove all files/subfolders inside a directory but not itself
// return a pair of integers: .first(how many item is removed), .second(how many item it had)
inline bool remove_directory_contents(
    fs::path directory, std::initializer_list<const char*> exclude={}
) {
    std::error_code remove_result;

    for (auto& item : fs::directory_iterator(directory)) {
        if (exclude.size() && !cm::contains(exclude, item.path().filename().c_str())) {
            fs::remove_all(item, remove_result);
        }
    }
    cm::debug(remove_result.message());
    return remove_result.value();
}

// TODO: Add explanation to this function
inline std::pair<int32, int32> remove_dir_contents_recursive(
    fs::path directory, std::initializer_list<const char*> exclude={}
) {
    uint32 total_c = 0;
    uint32 removed_c = 0;

    for (auto& item : fs::directory_iterator(directory)) {
        total_c += fs::exists(item) ? std::distance(
            fs::recursive_directory_iterator(item),
            fs::recursive_directory_iterator{}
        ) + 1 : 1;
        if (cm::contains(exclude, item.path().filename().c_str()))
            continue;
        std::error_code ec;
        removed_c += fs::remove_all(item, ec);
    }

    return {removed_c, total_c};
}


// get system editor with nice fallbacks
inline const char* get_sys_editor() {
    static const char* env_visual = ::getenv("VISUAL");
    static const char* env_editor = ::getenv("EDITOR");
    static const char* text_editor = nullptr;

    if (text_editor == nullptr) {
        if (env_visual)  return (text_editor = env_visual);
        if (env_editor)  return (text_editor = env_editor);
        else if (!::system("which nano >/dev/null 2>&1")) return (text_editor = "nano");
        else if (!::system("which vi >/dev/null 2>&1"))   return (text_editor = "vi");
        else return nullptr;
    }
    else {
        return text_editor;
    }
}


[[nodiscard]] inline
std::string make_repo_url(const strview github_name, const strview repo_name) {
    const std::string url = std::format("https://github.com/{}/{}", github_name, repo_name);
    return url;
}


[[nodiscard]] inline
std::string repo_from_url(const strview repo_url) {
    static constexpr const char* BAD_URL = "[BAD-URL]";
    static constexpr strview prefix = "https://github.com/";

    // starts with "https://github.com/"
    if (repo_url.size()<=prefix.size() || repo_url.substr(0, prefix.size())!=prefix) return BAD_URL;
    strview path = repo_url.substr(prefix.size());

    size_t first_slash = path.find('/');
    if (first_slash == strview::npos || first_slash == 0) return BAD_URL;

    strview repo_part = path.substr(first_slash + 1);
    if (repo_part.empty()) return BAD_URL;

    // remove if it has trailing slash
    size_t next_slash = repo_part.find('/');
    if (next_slash != strview::npos) repo_part = repo_part.substr(0, next_slash);

    if (repo_part.empty()) return BAD_URL;
    else return std::string(repo_part);
}


inline std::optional<std::string> active_github_account() {
    std::string gh_acc = {};

    Uptr<FILE, decltype(&pclose)> pipe = {
        popen("gh api user --jq '.login'", "r"),
        pclose
    };
    if (pipe == nullptr) return std::nullopt;

    char buf[256];
    while (::fgets(buf, sizeof(buf), pipe.get())) {
        gh_acc += buf;
    }

    if (gh_acc.empty()) return std::nullopt;
    return strip_nl(gh_acc);
}


// tries Cloudfare(fallbacks to Quad9) DNS at always open port 53
inline bool internet_is_connected(uint32 timeout_seconds = 2) {
    char cmd[64];
    // -z for scan only, -w for timeout seconds
    snprintf(cmd, sizeof(cmd),
        "nc -zw%u 1.1.1.1 53 2>/dev/null||nc -z-w%u 9.9.9.9 53 2>/dev/null",
        timeout_seconds, timeout_seconds
    );
    return !::system(cmd);
}

// void check_internet_async(std::function<void(bool)> callback) {
    // std::thread([callback]() {
        // int result = ::system("nc -zw1 1.1.1.1 53 > /dev/null 2>&1");
        // callback(result == 0);
    // }).detach();
// }


NAMESPACE_END(cm)




// contains error message and an error code
struct [[nodiscard]] cm::Report {
    bool err = false;
    std::string msg = {};

    [[nodiscard]]
    static Report Good() {
        return Report {false, ""};
    }

    template <class... FmtArgs> [[nodiscard]]
    static Report Bad(std::format_string<FmtArgs...> err_msg, FmtArgs&&... err_msg_args) {
        return Report {true, std::format(err_msg, std::forward<FmtArgs>(err_msg_args)...)};
    }

    constexpr inline operator bool () const {
        return err;
    }

    // print `msg` and return true if `errc` is bad
    Report& printOnBad(bool new_line=true) {
        if (bool(*this)) {
            cm::print(msg, ((new_line)? "\n":""));
        }
        return *this;
    }

    template <class... FmtArgs>
    void addComplain(std::format_string<FmtArgs...> complain_msg, FmtArgs&&... complain_args) {
        std::string complain = "\n" + std::format(
            complain_msg, std::forward<FmtArgs>(complain_args)...
        );
        msg.append(complain);
    }

    void terminateOnBad() {
        if (bool(*this)) {
            cm::terminate("Reported bad behaviour: this->", __func__, "");
        }
    }
};

using cm::Report;




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

    // provide @arg seperator to put given string between each command(;, &&, ||)
    int32 run(const char* separator, bool capture_output) {
        output_buf.clear();

        std::string line;
        for (uint32 i = 0; i < commands.size(); ++i) {
            line += commands[i];
            if (i != commands.size() - 1) line += separator;
        }

        if (capture_output) {
            std::string out;
            std::unique_ptr<FILE, decltype(&pclose)> pipe = {
                popen(line.c_str(), "r"), pclose
            };
            if (pipe == nullptr) return -1;

            char buf[256];
            while (::fgets(buf, sizeof(buf), pipe.get()) != nullptr)
            {
                output_buf.append(buf);
            }
            output_buf = cm::strip_nl(output_buf);

            return pclose(pipe.release());
        }
        return ::system(line.data());
    }

    // Output loads to internal buffer after calling .run().
    // this function, moves internal buffer when returns
    [[nodiscard]] std::string output() {
        return std::move(output_buf);
    }
};
