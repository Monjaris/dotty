#pragma once
#include "parser.hpp"

struct Cfman
{
    static COMPTIME_STR NO_PROFILE = "[no-profile]";

    std::vector<Profile> profiles;

    const fs::path HOME = cm::userHomePath(true, "$HOME is empty");
    fs::path config_d = HOME/".config/dotty";
    fs::path storage_d = HOME/".local/share/dotty";

    const char* master_src = ".dotty";
    const char* config_src = "config";
    Profile current_profile = Profile{NO_PROFILE, "", "", false};

    std::vector<SrcDest> path_pairs = {};

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

    Report validateProfileName(const std::string& name);
    Report validateRepoName(const std::string& repo);
    bool noProfilesExist();
    bool profileExists(const strview profile_name);
    Profile* getProfileByName(const strview prof_name);
    std::string currentProfile();
    Report prerequisite(strview init_prof);
    Report newProfile(const std::string& name, const std::string& github_name,
        const std::string& repo_name, const std::string& repo_visibility
    );
    Report deleteProfile(const strview profile_name);
    Report setActiveProfile(const std::string& name);
    Res listProfiles(bool name, bool repo, bool url, bool gh);
    Report cleanConfigs(bool config, bool storage);
    bool detectPreinitConfig();
    void load(bool optimistic = false);
    void SourceToStorage();
    void StorageToSources();
};

extern Cfman dotty;
