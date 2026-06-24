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

inline void initialize() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
}

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
inline void debug(Args... args) {
#if !DEBUG_ON
    return;
#endif
    const strview pre = "\033[2m[Debug]\033[0m ";
    const strview post = "\n";
    cm::print(pre, std::forward<Args>(args)..., post);
}

template <class... Args>
inline void perror(std::format_string<Args...>, Args... args) {
    $IMPLEMENT("impl this function and make sure its the main function for printing errors!");
}


// uses std::cin or dotl::read_string()
template <bool std_line_reader=true, class T>
inline T& prompt(const char* prompt, T& lval) {
    // Read input relevantly
    if constexpr (std::is_same_v<char, T> || std::is_convertible_v<char, T>) {
        if (!std_line_reader) {
            lval = dotl::prompt(prompt).read_string();
            cm::print("\n");
        } else {
            cm::print(prompt);
            std::cin >> lval;
        }
    }
    else
    {
        if constexpr (!std_line_reader) {
            dotl::prompt(prompt);
            std::string in = dotl::read_string().string();
            cm::print("\n");
        }
        else {
            cm::print(prompt);
            std::getline(std::cin, lval);
        }
    }

    return lval;
}

// for arithmetic values
template <class Number, class StringType>
requires std::is_arithmetic_v<Number> && std::same_as<StringType, const char*>
inline void prompt_number(const StringType prompt, Number& number, bool std_line_reader = true) {
    if (std_line_reader || true) {
        cm::print(prompt);
        std::cin >> number;
    }
    else {
        dotl::prompt(prompt);
        dotl::ParseRes res = dotl::read_string().parse<Number>();
        if (!res.error()) {
            number = res.value();
        } else {
            return;
        }
    }
}


// utility for YES/NO questions
inline bool ask_confirm(const strview message, bool default_yes = true) {
    const char* post = default_yes?(" (Y/n): "):(" (y/N): ");
    std::string ask_msg = std::string(message).append(post);

    std::string inps = dotl::prompt(message.data()).read_string().string();
    char* inp = inps.data();
    if (!inp) return(default_yes);
    // erase trailing nl
    inps.erase(inps.begin()+inps.size());

    if (inps.empty()) return default_yes;
    if (::tolower(inps[0]) == 'y') return true;
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

// simply, adds slash between two strings and returns
inline fs::path cat_path(const fs::path& parent, const fs::path& child) {
    return parent / child;
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

namespace os
{
    // all environment variables of process
    inline char** get_environ() {
        return ::environ;  // global unistd variable
    }

    // spawn process(posix compliant), returns -1 on failure
    inline pid_t spawn_child(const char* prog, char** args) {
        constexpr usize MAX_ARGS = 64;

        char* argv[MAX_ARGS];
        argv[0] = const_cast<char*>(prog);

        usize ri = 1;  // read index of args, [0] is prog
        for (usize wi = 0; wi < MAX_ARGS-1-1; ++wi) {
            if (args[wi] == nullptr) break;
            argv[ri++] = args[wi];
        }
        argv[ri] = nullptr;

        pid_t pid;
        uint32 status = posix_spawnp(
            &pid, prog, nullptr, nullptr,
            argv,
            get_environ()
        );
        return (status==0)? pid : -1;
    }

    // wait for process to finish by passing the id of that process
    inline bool wait_proc(pid_t pid) {
        int32 status;
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        return (WIFEXITED(status) && WEXITSTATUS(status)) == 0;
    }


    // load $HOME to static constant once and return it
    inline const char* userHomePath() {
        static const char* home_path_cache = nullptr;
        if (home_path_cache != nullptr) return home_path_cache;

        const char* home_env = nullptr;
        #if defined(DOTTY_GENERIC_UNIX)
            home_env = ::getenv("HOME");
        #elif defined(_WIN32)
            home_env = ::getenv("USERPROFILE");
        #endif
    
        if (home_env == nullptr) {
            throw std::runtime_error("HOME environment variable is not set!");
        } else {
            return (home_path_cache = home_env);
        }
    }


    // get configuration directory based on the OS
    inline fs::path get_config_d() {
        #define NO_PATH "////////////////////////"
        static fs::path os_config_dir = NO_PATH;
        // operate once and keep in static storage
        if (os_config_dir == NO_PATH) {
            #if defined(DOTTY_FOSS_UNIX)
                const char* env = ::getenv("XDG_CONFIG_HOME");
                if (env) return os_config_dir = env;
                else return os_config_dir = cat_path(userHomePath(), ".config");
            #elif defined(__APPLE__)
                return os_config_dir = cat_path(userHomePath(), "Library/Preferences");
            #elif defined(_WIN32)
                return os_config_dir = cat_path(userHomePath(), "AppData/Roaming");
            #else
                return os_config_dir = cat_path(userHomePath(), ".config");
            #endif
        }
        else return os_config_dir;
    }


    // get system editor with nice fallbacks
    inline const char* get_txt_editor() {
        static const char* env_visual = ::getenv("VISUAL");
        static const char* env_editor = ::getenv("EDITOR");
        static const char* text_editor = nullptr;

        if (text_editor == nullptr) {
            if (env_editor)  return (text_editor = env_editor);
            if (env_visual)  return (text_editor = env_visual);
            else if (!::system("which nano >" NULLDEV)) return (text_editor = "nano");
            else if (!::system("which vi >" NULLDEV))   return (text_editor = "vi");
            else return nullptr;
        }
        else {
            return text_editor;
        }
    }
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


// parse file path by converting tilde('~') to $HOME variable
constexpr inline fs::path parsePathTilde(std::string path) {
    if (!(path[0] == '~')) return path;
    path.erase(0, 1);
    const char* const user_home = ::cm::os::userHomePath();
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


COMPTIME_STR PPRINTER = "bat";
// pretty-print file, calls 'bat' (cat alternative)
inline bool pprint_file(const char* const fpath) {
    char* argv[] = {const_cast<char*>(fpath)};
    pid_t pid = os::spawn_child(PPRINTER, argv);
    if (pid == -1) return false;
    return os::wait_proc(pid);
}
// overload for const path reference
inline bool pprint_file(const fs::path& fpath) {
    return pprint_file(fpath.c_str());
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
    if (repo_url.size()<=prefix.size() || !repo_url.starts_with(prefix)) return BAD_URL;
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

[[nodiscard]] inline
std::string gh_host_from_url(const strview repo_url) {
    static constexpr const char* BAD_URL = "[BAD-URL]";
    static constexpr strview prefix = "https://github.com/";

    if (repo_url.size() <= prefix.size() || !repo_url.starts_with(prefix)) {
        return BAD_URL;
    }
    strview path = repo_url.substr(prefix.size());

    size_t first_slash = path.find('/');
    if (first_slash == strview::npos || first_slash == 0) {
        return BAD_URL;
    }

    return std::string(path.substr(0, first_slash));
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
        "nc -zw%u 1.1.1.1 53 2>/dev/null||nc -zw%u 9.9.9.9 53 2>/dev/null",
        timeout_seconds, timeout_seconds
    );
    return ::system(cmd) == 0;
}

// void check_internet_async(std::function<void(bool)> callback) {
    // std::thread([callback]() {
        // int result = ::system("nc -zw1 1.1.1.1 53 > /dev/null 2>&1");
        // callback(result == 0);
    // }).detach();
// }


NAMESPACE_END(cm)


struct tern {
    enum value_t { yes, no, neutr} value;

    tern (value_t ternary_value): value(ternary_value) {}

    bool boolable() const { return (value==yes) || (value==no); }

    operator bool() const {
        switch (value) {
            case yes: return true;
            case no: return false;
            case neutr: {
                throw std::logic_error("Can't convert neutral value to bool!");
            }
        }
        std::unreachable();
    }
};

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

    constexpr explicit inline
        operator bool () const { return err; }
    bool success() { return !(bool)*this; }
    bool error()   { return (bool)*this; }


    void printComplains() {
        if (!msg.empty()) {
            cm::print(msg, "\n");
        }
    }

    // print `msg` and return true if `errc` is bad
    Report& printOnBad() {
        if (this->error()) this->printComplains();
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
        if (this->error()) {
            cm::terminate("Invalid action, terminating!");
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

