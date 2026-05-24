#pragma once
#include "parser.hpp"


struct Cfman
{
    static COMPTIME_STR NO_PROFILE = "[no-profile]";
    std::vector<Profile> profiles;

    const fs::path HOME = cm::userHomePath(true, "$HOME is empty");
    fs::path config_d = HOME/".config/dotty";
    fs::path storage_d = HOME/".local/share/dotty";

    const char* master_config = ".dotty";
    const char* config_source_name = "config";
    std::optional<Profile> current_profile = std::nullopt;

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


    Report validateProfileName(const std::string& name) {
        if (!isalpha(name[0])) {
            return Report::Bad("First character should be an alpha");
        }
        else if (cm::str_has_any_of(name, {'~','{','}','/','@','#','*','='})) {
            return Report::Bad("Contains illegal character");
        }
        return Report::Good();
    }

    Report validateRepoName(const std::string& repo) {
        if (repo.empty()) {
            return Report::Bad("Repo name should not be empty");
        }
        return Report::Good();
    }

    bool noProfilesExist() {
        return profiles.size() == 0;
    }

    bool profileExists(const strview profile_name) {
        for (const Profile& prof : profiles) {
            if (prof.name == profile_name) {
                return true;
            }
        }
        return false;
    }

    Profile* getProfileByName(const strview prof_name) {
        for (uint32 i=0;  i < profiles.size();  ++i) {
            if (profiles[i].name == prof_name) {
                return &profiles[i];
            }
        }
        // not found
        return nullptr;
    }


    // Get current profile as string
    std::string currentProfile() {
        if (!current_profile.has_value()) {
            // nice trick over here, extract function name
            cm::debug(__PRETTY_FUNCTION__+12, ": No profile is active");
            return NO_PROFILE;
        }
        std::string current = current_profile.value().name;
        return current;
    }


    // Create a folder and register a new profile
    Res newProfile(
        const std::string& name, const std::string& repo_name,
        const std::string& repo_visibility
    ){
        // quick returns
        if (profileExists(name)) return Res::ProfileAlreadyExists;
        if (fs::create_directories(config_d/name)) {
            ;
            if (cm::newFile(config_d/name/"config")) {
                cm::debug("Created new config file in: ", (config_d/name/"config").string());
            } else return Res::FileCouldNotBeOpened;
        } else return Res::DirectoryCouldNotBeCreated;

        // constants
        const fs::path repo_d = cm::parsePathTilde(storage_d/name);
        const fs::path config = cm::parsePathTilde(config_d/name);

        cm::CmdStream cmd;
        cmd
            .add("mkdir -p {}", repo_d.string())
            .add("cd {}", repo_d.string())
            .add("git init")
            .add("touch .gitkeep")
            .add("git add .gitkeep")
            .add("git commit -m 'Dotty profile repository: Initial commit'")
            .add("gh repo create {} --{} --source={} --remote=origin --push",
                repo_name, repo_visibility, repo_d.string());
        cmd.run(" && ", false);

        return Res::OK;
    }


    // Set current dotty profile
    Report setActiveProfile(const std::string& name) {
        if (noProfilesExist()) {
            return Report::Bad("Can't set active profile: No profiles exist yet!");
        }
        if (!profileExists(name)) {
            return Report::Bad("Can't @{} active: Profile doesn't exist!", name);
        }
        if (current_profile.value().name == name.data()) {
            return Report::Bad("profile @{} is already active", name);
        }
        current_profile.value().name = name.data();
        return Report::Good();
    }


    Res listProfiles(const std::string&& fields="name,repo,url") {
        bool fname, frepo, furl = false;
        if (fields.contains("name")) fname = true;
        if (fields.contains("url")) frepo = true;
        if (fields.contains("repo")) furl = true;

        for (uint32 i=0;  i < profiles.size();  ++i) {
            auto prof = profiles[i];
            std::string msg;
            if(fname) msg += prof.name;
            cm::print(i, ": ", prof.name, "\n");
        }
        return Res::OK;
    }


    // 1. deletes config storage
    // 2. removes master config
    // 3. removes active profile's config
    Report cleanConfigs(bool config, bool storage, bool master) {
        Report report;
        fs::path target_path;

        if (!noProfilesExist()) {
            return Report::Bad("Can't clean profile: No profiles exist");
        }
        else if (currentProfile() == NO_PROFILE) {
            return Report::Bad("Can't clean profile: No profiles are active");
        }

        if (config) {
            target_path = config_d/currentProfile();
            if (fs::exists(target_path)) {
                cm::print(":: Cleaning profile configs: ", target_path.string());
                cm::remove_dir_contents_recursive(target_path, {config_source_name});
                // clear config file
                std::ofstream master_cfg(config_d/currentProfile()/config_source_name, std::ios::out);
            } else {
                report.addComplain("Path does not exist: {}", target_path.string());
            }
        }

        if (storage) {
            target_path = storage_d/currentProfile();
            if (fs::exists(target_path)) {
                cm::print(":: Removing config storage contents: ", target_path.string());
                std::pair ratio = cm::remove_dir_contents_recursive(target_path);
                if (!(ratio.first == ratio.second)) {  // not all content is removed
                    report.addComplain("Removed ", ratio.first, "items out of ", ratio.second, "\n");
                }
                else cm::debug("All ", ratio.first, " items removed");
            } else {
                cm::debug("Path does not exist: ", target_path.string());
            }
        }

        if (master) {
            target_path = HOME/master_config;
            if (fs::exists(target_path)) {
                cm::print(":: Resetting master config");
                std::ofstream master_cfg(HOME/master_config, std::ios::out);  // clears file
            } else {
                report.addComplain("Path does not exist: {}", target_path.string());
            }
        }

        return report;
    }


    // Load dotty configuration and debug
    void load() {
        cm::debug("dotty.load()..\n");
        fs::path master_path = cm::parsePathTilde(HOME/master_config);
        if (!fs::exists(master_path)) return;

        cm::debug("dotty.load() -> Opening master config\n");
        std::ifstream master(master_path);
        Lexer lexer;
        MasterConfigParser mcparser;
        cm::debug("Reading master-config lines..");
        while (std::getline(master, lexer.line)) {
            static uint32 lines = 0; ++lines;
            lexer.lexMain();
            for (auto& token :  lexer.result()) {
                mcparser.tokens.push_back(token);
            }
        }
        mcparser.parse();
        mcparser.eval();
        mcparser.unwrap();
        if(DEBUG_ON) mcparser.printReadProfiles();
        if (FAILED mcparser.unwrap()) cm::print("Master-Config-Parser: Unwrap: Something wrong happened\n");

        profiles = mcparser.profiles;

        // set active profile based on the config
        auto it = mcparser.vars.find("profile.active");
        if (it != mcparser.vars.end()) {
            setActiveProfile(it->second.data()).printOnBad();
        } else cm::terminate("dotty.load: setProfile(it->second): Error!");

        cm::debug("Loaded dotty.\n\n");
    }


    // Copy all source files to destination files, pairs defined by a member
    void src_to_dest() {
        for (auto [src, dest] : path_pairs) {
            if (dest.is_absolute()) {
                cm::print(
                    "Config-Manager: Write: Error: destination should be relative path!",
                    "skipping..\n"
                ); continue;
            }

            auto dest_path = cm::parsePathTilde(storage_d/currentProfile())/dest;
            auto src_path = cm::parsePathTilde(src);

            // create missing and copy src->dest
            fs::create_directories(dest_path.parent_path());
            fs::copy_file(
                cm::parsePathTilde(src_path),
                cm::parsePathTilde(dest_path),
                fs::copy_options::update_existing
            );
        }
    }

    void dest_to_src() {
        for (auto [dest, src] : path_pairs) {
            // TODO: implement this function
        }
    }
};

extern Cfman dotty;

