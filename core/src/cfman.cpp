#include "cfman.hpp"


Report Cfman::validateProfileName(const std::string& name) {
    if (!isalpha(name[0])) {
        return Report::Bad("First character should be an alpha");
    }
    else if (std::string::npos !=
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._")
    ){
        return Report::Bad("Profile name contains illegal character");
    }

    return Report::Good();
}


Report Cfman::validateRepoName(const std::string& repo) {
    if (repo.empty()) {
        return Report::Bad("Repo name should not be empty");
    }

    return Report::Good();
}


bool Cfman::noProfilesExist() {
    return profiles.size() == 0;
}


bool Cfman::profileExists(const strview profile_name) {
    for (const Profile& prof : profiles) {
        if (prof.name == profile_name) {
            return true;
        }
    }
    return false;
}


Profile* Cfman::getProfileByName(const strview prof_name) {
    for (uint32 i=0;  i < profiles.size();  ++i) {
        if (profiles[i].name == prof_name) {
            return &profiles[i];
        }
    }
    // not found
    return nullptr;
}


// Get current profile as string
std::string Cfman::activeProf() {
    std::string profile_name = current_profile.name;
    return profile_name;
}


Report Cfman::prerequisite(strview init_prof) {
    Profile* profile = getProfileByName(activeProf());

    if (dotty.activeProf() == NO_PROFILE) {
        return Report::Bad("Active profile is not set!\n");
    }

    // Pre-create required directories
    cm::ensure_directories(config_d/profile->name);
    cm::ensure_directories(data_d/profile->name);
    cm::ensure_directories(data_d/profile->name/dotty.data_cfgref);

    return Report::Good();
}



// Create a folder and register a new profile
Report Cfman::newProfile(
    const std::string& name, const std::string& github_name,
    const std::string& repo_name, const std::string& repo_visibility,
    const char* const initial_commit_message
){
    static COMPTIME_STR err = "Can't create profile";

    // validate profile and repo names quickly
    dotty.validateProfileName(name).printOnBad().terminateOnBad();
    dotty.validateRepoName(repo_name).printOnBad().terminateOnBad();
    if (profileExists(name)) return Report::Bad("{} '{}': Profile already exists.", err, name);
    // create profile directory and files
    if (!fs::create_directories(config_d/name)) return Report::Bad("Coudln't create configuration directories!");
    if (!cm::new_file(config_d/name/config_src))  return Report::Bad("Coudln't create configuration file!");

    cm::debug("Created new config file in: ", (config_d/name/"config").string());

    // constants
    const fs::path repo_d = cm::parsePathTilde(data_d/name);
    const fs::path config = cm::parsePathTilde(config_d/name);

    // create data(also repository) directory and a config-reference
    cm::ensure_directories(repo_d/data_cfgref);
    // create and push github repo
    cm::CmdStream {}
        .add("cd {}", repo_d.string())
        .add("git init")
        .add("touch .gitkeep")
        .add("git add .gitkeep")
        .add("git commit -m {}", initial_commit_message)
        .add("gh repo create {} --{} --source={} --remote=origin --push",
            repo_name, repo_visibility, repo_d.string())
    .run(" && ", false);

    cm::debug("Writing new profile configurations to master config");
    std::ofstream master(HOME/master_src, std::ios::app);
    master << "\n\n";
    master << "profile.add = \"" << name << "\"\n";
    master << "profile.active = \"" << name << "\"\n";
    master << "@" << name << ".gh-acc = \"" << github_name << "\"\n";
    master << "@" << name << ".repo-url = \"" << cm::make_repo_url(github_name, repo_name) << "\"\n";
    master << "\n"; master.close();

    load(false);
    return Report::Good();
}



Report Cfman::deleteProfile(const strview profile_name) {
    if (!profileExists(profile_name)) {
        return Report::Bad("Can't delete '{}', it doesn't exist!", profile_name);
    }
    // else if (activeProf() == profile_name) {
        // return Report::Bad("Can't delete active profile! Switch to another profile to delete '{}'", profile_name);
    // }

    // read(in_master), modify and write(out_master) back
    std::ifstream master_old(HOME/master_src, std::ios::in);
    std::ofstream master_new(HOME/master_src, std::ios::trunc);

    if (!master_old || !master_new) return Report::Bad("Could not open master config");

    // load contents of master config to this string and close ifstream
    master_old.close();

    std::string adder = "profile.add = \"" + std::string(profile_name) + "\"";
    std::string activator = "profile.active = \"" + std::string(profile_name) + "\"";
    std::string mention = "@" + std::string(profile_name);

    std::istringstream master_data_old(std::string{std::istreambuf_iterator(master_old), {}});
    std::ostringstream master_data_new;
    std::string line;

    while (std::getline(master_data_old, line))
    {
        if (line.contains(adder)) continue;
        if (line.contains(activator)) continue;
        if (line.contains(mention)) continue;

        master_data_new << line << "\n";
    }

    // write back
    master_new << master_data_new.str();
    return Report::Good();
}



// Set current dotty profile
Report Cfman::setActiveProfile(const std::string& name) {
    if (noProfilesExist()) {
        return Report::Bad("Can't set active profile: No profiles exist yet!");
    }
    if (!profileExists(name)) {
        return Report::Bad("Can't switch to @{}: Profile doesn't exist!", name);
    }
    if (current_profile.name == name.c_str()) {
        // return Report::Bad("profile @{} is already active", name);
        return Report::Good();
    }

    // open master-config, read to string var, close master-config
    std::ifstream master_old(dotty.HOME/dotty.master_src, std::ios::in);
    if (!master_old) return Report::Bad("Couldn't open(write) master config, setting active profile aborted!");
    std::istringstream master_content(std::string{std::istreambuf_iterator(master_old), {}});
    master_old.close();

    // open master-config, write new config
    std::ofstream master_new(dotty.HOME/dotty.master_src, std::ios::out);
    if (!master_new) return Report::Bad("Couldn't open(write) master config, setting active profile aborted!");
    std::string line;

    while (std::getline(master_content, line)) {
        if (Lexer::RemoveComment(line).contains("profile.active")) {
            continue;
        }
        master_new << line << "\n";
    }

    master_new << "\n";
    master_new << "profile.active = \"" << name << "\"\n";
    master_new << "\n";

    Profile* found = getProfileByName(name);
    current_profile = *found;
    return Report::Good();
}


Cfman::Res Cfman::listProfiles(bool name, bool repo, bool url, bool gh) {
    for (uint32 i=0;  i < profiles.size();  ++i) {
        auto prof = profiles[i];

        std::string msg;
        // TODO: make them switch-case after testing
        if(name) msg += " | " + prof.name;
        if(repo) msg += " | " + prof.repo_name;
        if(url)  msg += " | " + prof.repoUrl();
        if(gh)   msg += " | " + prof.github_name;

        cm::print(i+1, ": ", msg ," |\n");
    }
    return Res::OK;
}


// 1. deletes config storage
// 2. removes master config
// 3. removes active profile's config
Report Cfman::cleanConfigs(bool config, bool storage) {
    Report report;
    fs::path target_path;

    // if (master) {
    //     target_path = HOME/master_src;
    //     if (fs::exists(target_path)) {
    //         cm::print(":: Resetting master config\n");
    //         std::ofstream master_cfg(HOME/master_src, std::ios::out);  // clears file
    //     } else {
    //         report.addComplain("Path does not exist: {}", target_path.string());
    //     }
    // }


    if (noProfilesExist()) {
        return Report::Bad("Can't clean profile configs: No profiles exist");
    }
    else if (activeProf() == NO_PROFILE) {
        return Report::Bad("Can't clean profile configs: No profiles are active");
    }

    if (config) {
        target_path = config_d/activeProf();
        if (fs::exists(target_path)) {
            cm::print(":: Cleaning profile configs: ", target_path.string());
            cm::remove_dir_contents_recursive(target_path, {config_src});
            // clear config file
            std::ofstream master_cfg(config_d/activeProf()/config_src, std::ios::out);
        } else {
            report.addComplain("Path does not exist: {}", target_path.string());
        }
    }

    if (storage) {
        target_path = data_d/activeProf();
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

    return report;
}


bool Cfman::detectPreinitConfig() {
    std::ifstream master(HOME/master_src, std::ios::in);
    if (!master) return false;  // doesn't even exist
    else if (fs::is_empty(HOME/master_src)) return false;
    return true;
}


// Load dotty configuration and debug
void Cfman::load(bool optimistic) {
    cm::debug("dotty.load()..\n");
    std::string prof = activeProf();
    fs::path master_path = HOME/master_src;

    // Create needed directories&&files if not exist
    if (!fs::exists(master_path)) cm::new_file(master_path);
    cm::ensure_directories(config_d);
    cm::ensure_directories(data_d);
    for (auto& profile : profiles) {
        cm::ensure_directories(config_d/profile.name);
        if (!fs::exists(config_d/prof/config_src)) cm::new_file(config_d/prof/config_src);
        cm::ensure_directories(data_d/profile.name/data_cfgref);
    }

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
    mcparser.parse().printOnBad();
    mcparser.eval();
    mcparser.unwrap().printOnBad();
    if (FAILED mcparser.unwrap()) cm::print("Master-Config-Parser: Unwrap: Something wrong happened\n");

    profiles = mcparser.profiles;

    // set active profile based on the config
    auto it = mcparser.vars.find("profile.active");
    if (it != mcparser.vars.end()) {
        setActiveProfile(it->second.data()).printOnBad();
    } else if (!optimistic) cm::terminate("dotty.load: setProfile(it->second): Error!");

    std::string profile = activeProf();

    cm::debug("Loaded dotty.\n\n");
}  // .load()


// Copy all source files to destination files, pairs defined by a member
std::array<std::vector<SrcDest>, 4> Cfman::systemToRepo() {
    static std::vector<SrcDest> succeed_cp_f;
    static std::vector<SrcDest> succeed_cp_d;
    static std::vector<SrcDest> succeed_ln_f;
    static std::vector<SrcDest> succeed_ln_d;

    COMPTIME_STR ERR = "Skipping target: ";
    auto should_skip = [ERR](const fs::path& src, const fs::path& dest, bool accept_dirs) ->bool {
        bool signal_skip = false;
        const std::string& dest_str = dest.string();
        // destination should be relative to the repo
        if (dest.is_absolute()) {
            cm::print(
                ERR, "destination should be relative path!\n"
            ); signal_skip = true;
        }
        // Neither source nor destination can be written as a directory
        else if (!accept_dirs && (
            fs::directory_entry(src).is_directory() || fs::directory_entry(dest).is_directory()
        )){
            cm::print(
                ERR, "neither source nor destination can be written as a directory!\n"
            ); signal_skip = true;
        }
        // Dont allow trailing '/'
        else if (dest_str.ends_with("/")) {
            cm::print(
                ERR, "path has trailing '/'\n"
            ); signal_skip = true;
        }
        else if (dest_str.starts_with("./") || dest_str.starts_with("../")) {
            cm::print(
                ERR, "path starts with illegal character set\n"
            ); signal_skip = true;
        }
        // return value will signal if called should 'continue' loop
        return signal_skip;
    }; // lambda should_skip;


    fs::path repo_d = data_d/activeProf();

    // COPY-FILES
    for (auto [src, dest] : files_to_copy) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            fs::copy_file(src, dest, fs::copy_options::update_existing);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_cp_f.emplace_back(src, dest);
    }
    // LINK-FILES
    for (auto [src, dest] : files_to_link) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            fs::create_symlink(src, dest);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_ln_f.emplace_back(src, dest);
    }
    // COPY-DIRECTORIES
    for (auto [src, dest] : dirs_to_copy) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            cm::copy_directory(src, dest, true);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_cp_d.emplace_back(src, dest);
    }
    // LINK-DIRECTORIES
    for (auto [src, dest] : dirs_to_link) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            fs::create_directory_symlink(
                src, dest
            );
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_ln_d.emplace_back(src, dest);
    }

    // return paths which succeed to be copied/linked
    return std::array<std::vector<SrcDest>, 4>{
        succeed_cp_f, succeed_ln_f, succeed_cp_d, succeed_ln_d
    };
}


// Copy/link files/directories from repo(config storage) to their system targets
void Cfman::repoToSystem() {
    COMPTIME_STR ERR = "Skipping target: ";
    // COPY-FILES
    for (auto [src, dest] : files_to_copy) {
        cm::ensure_directories(dest.parent_path());
        try {
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
    // LINK-FILES
    for (auto [src, dest] : files_to_link) {
        cm::ensure_directories(dest.parent_path());
        try {
            // check if dest's dereference exists or if dest is symlink
            if (fs::exists(dest) || fs::is_symlink(dest)) fs::remove(dest);
            fs::create_symlink(src, dest);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
    // COPY-DIRECTORIES
    for (auto [src, dest] : dirs_to_copy) {
        cm::ensure_directories(dest.parent_path());
        try {
            cm::copy_directory(src, dest, true);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
    // LINK-DIRECTORIES
    for (auto [src, dest] : dirs_to_link) {
        cm::ensure_directories(dest.parent_path());
        try {
            if (fs::exists(dest) || fs::is_symlink(dest)) fs::remove(dest);
            fs::create_directory_symlink(src, dest);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
}

Cfman dotty;

