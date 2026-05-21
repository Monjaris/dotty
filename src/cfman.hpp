#pragma once
#include "parser.hpp"


struct Cfman
{
    std::vector<Profile> profiles;

    const fs::path HOME = cm::userHomePath(true, "$HOME is empty");
    fs::path config_d = HOME/".config/dotty";
    fs::path storage_d = HOME/".local/share/dotty";

    const char* master_config = ".dotty";
    const char* config_source_name = "config";
    std::optional<std::string> current_profile = std::nullopt;

    std::vector<SrcDest> path_pairs = {};

    enum class Res : uint8_t {
        OK=0,
        ERR=1,
        FileCouldNotBeOpened=2,
        DirectoryCouldNotBeCreated=3,
        ProfileDoesNotExist=4,
        ProfileAlreadyExists=5,
        ProfileAlreadySet=6,
    };


    // Get current profile as string
    std::string currentProfile() {
        static COMPTIME_STR NO_PROFILE = "[no-profile]";
        if (!current_profile.has_value()) {
            return "[no-profile]";
        }
        std::string current = current_profile.value();
        return current;
    }


    // Create a folder and register a new profile
    Res newProfile(
        const std::string& name, const std::string& repo_name,
        const std::string& repo_visibility
    ){
        // quick returns
        if (fs::exists(config_d/name)) return Res::ProfileAlreadyExists;
        if (fs::create_directories(config_d/name)) {
            ;
            if (cm::newFile(config_d/name/"config")) {
                ;
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
    Res setProfile(const std::string& name) {
        if (!fs::exists(config_d/name)) return Res::ProfileDoesNotExist;
        if (current_profile == name.data()) return Res::ProfileAlreadySet;
        current_profile = name.data();
        return Res::OK;
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
        mcparser.printReadProfiles();
        if( !mcparser.unwrap() ) cm::print("Master-Config-Parser: Unwrap: Something wrong happened\n");

        profiles = mcparser.profiles;

        // set active profile based on the config
        auto it = mcparser.vars.find("profile.active");
        auto res = Cfman::Res{};
        if (it != mcparser.vars.end()) {
            res = setProfile(it->second);
            cm::debug("setProfile() -> ", (int)res);
        } else cm::terminate("dotty.load: setProfile(it->second): Error!");

        cm::print("Loaded dotty.\n\n");
    }


    // Copy all source files to destination files, pairs defined by a member
    void write() {
        for (auto [src, dest] : path_pairs) {
            if (dest.is_absolute()) {
                cm::print(
                    "Config-Manager: Write: Error: destination should be relative path!\n"
                ); continue;
            }
            fs::copy_file(
                cm::parsePathTilde(src),
                cm::parsePathTilde(storage_d/currentProfile())/dest,
                fs::copy_options::update_existing
            );
        }
    }
};

extern Cfman dotty;

