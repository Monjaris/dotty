#pragma once
#include "parser.hpp"

NAMESPACE_START(cm)

struct Clr {
    static constexpr char* _esc = (char*)"\033[";
    const char* value;
    Clr (const char* ansi = "0") {
        value = ::strcat(::strcat(_esc, value), "m");
    }
    operator const char*() { return this->value; }
};

// inline Clr NORM = Clr{};
// inline Clr RED = "31";
// inline Clr GRN = "32";
// inline Clr YLW = "33";
// inline Clr BLU = "34";
// inline Clr MAG = "35";
// inline Clr CYN = "36";

NAMESPACE_END(cm)


class Cfman
{
public:
    static COMPTIME_STR NO_PROFILE = "[no-profile]";
    static constexpr bool COLORS = true;

    std::vector<Profile> profiles;

    const fs::path HOME = cm::userHomePath(true, "$HOME is empty");
    fs::path config_d = HOME/".config/dotty";
    fs::path data_d = HOME/".local/share/dotty";

    const char* master_src = ".dotty";
    const char* config_src = "config";
    const char* data_cfgref = ".dotty.d";
    Profile current_profile = Profile{NO_PROFILE, "", "", false};

    std::vector<SrcDest> files_to_copy = {};
    std::vector<SrcDest> files_to_link = {};
    std::vector<SrcDest> dirs_to_copy = {};
    std::vector<SrcDest> dirs_to_link = {};

    enum class Res : uint8_t {
        OK=0,
        ERR=1,
        PathDoesNotExist=2,
        FileCouldNotBeOpened=3,
        DirectoryCouldNotBeCreated=4,
        ProfileDoesNotExist=5,
        ProfileAlreadyExists=6,
        ProfileAlreadySet=7,
    };

    // struct Msg {
        // const char* clr;
        // std::string val;
        // no-color constructor
        // template <class T>
        // Msg(T v) : clr(""), val(std::format("{}", v)) {}
        // colored constructor
        // template <class T>
        // Msg(const char* color, T v) : clr(color), val(std::format("{}", v)) {}
    // };
    // CF stands for ColorFul, yes
    // #define CF(_color, _val) ::Cfman::Msg{_color, _val}
    // void say(std::initializer_list<Msg> args) {
        // for (auto& m : args) {
            // if constexpr (COLORS) cm::print(m.clr, m.val, cm::NORM);
            // else cm::print(m.val);
        // }
    // }

    Report validateProfileName(const std::string& name);
    Report validateRepoName(const std::string& repo);
    bool noProfilesExist();
    bool profileExists(const strview profile_name);
    Profile* getProfileByName(const strview prof_name);
    std::string activeProf();
    Report prerequisite(strview init_prof);
    Report newProfile(const std::string& name, const std::string& github_name,
        const std::string& repo_name, const std::string& repo_visibility,
        const char* const initial_commit_message
    );
    Report deleteProfile(const strview profile_name);
    Report setActiveProfile(const std::string& name);
    Res listProfiles(bool name, bool repo, bool url, bool gh);
    Report cleanConfigs(bool config, bool storage);
    bool detectPreinitConfig();
    void load(bool optimistic = false);
    std::array<std::vector<SrcDest>, 4> systemToRepo();
    void repoToSystem();
};

extern Cfman dotty;
