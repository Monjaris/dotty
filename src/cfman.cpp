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
    printf("werfee");
    return Report::Good();
}


Report Cfman::validateRepoName(const std::string& repo) {
    if (repo.empty()) {
        return Report::Bad("Repo name should not be empty");
    }
    cm::print("aedw");
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
std::string Cfman::currentProfile() {
    std::string profile_name = current_profile.name;
    return profile_name;
}


Report Cfman::prerequisite(strview init_prof) {
    $IMPLEMENT(__PRETTY_FUNCTION__);
    return Report::Bad("");
}


// Create a folder and register a new profile
Report Cfman::newProfile(
    const std::string& name, const std::string& github_name,
    const std::string& repo_name, const std::string& repo_visibility
){
    static COMPTIME_STR err = "Can't create profile";
    // quick returns
    if (profileExists(name)) return Report::Bad("{} '{}': Profile already exists.", err, name);
    if (fs::create_directories(config_d/name)) {
        ;
        if (cm::new_file(config_d/name/"config")) {
            cm::debug("Created new config file in: ", (config_d/name/"config").string());
        } else return Report::Bad("{} '{}': Config source couldn't be opened.", err, name);
    } else return Report::Bad("{} '{}': Failed creating config directory tree.", err, name);

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
    // read(in_master), modify and write(out_master) back
    std::ifstream in_master(HOME/master_src, std::ios::in);
    std::ofstream out_master(HOME/master_src, std::ios::trunc);

    if (!in_master || !out_master) return Report::Bad("Could not open master config");

    // load contents of master config to this string and close ifstream
    std::string master_content = {std::istreambuf_iterator(in_master), {}};
    in_master.close();

    std::string adder = "profile.add = \"" + std::string(profile_name) + "\"";
    std::string activator = "profile.active = \"" + std::string(profile_name) + "\"";
    std::string mention = "@" + std::string(profile_name);

    std::istringstream master_old(master_content);
    std::ostringstream master_new;
    std::string line;

    while (std::getline(master_old, line))
    {
        if (line.contains(adder)) continue;
        if (line.contains(activator)) continue;
        if (line.contains(mention)) continue;

        master_new << line << "\n";
    }

    // write back
    out_master << master_new.str();
    return Report::Good();
}


// Set current dotty profile
Report Cfman::setActiveProfile(const std::string& name) {
    if (noProfilesExist()) {
        return Report::Bad("Can't set active profile: No profiles exist yet!");
    }
    if (!profileExists(name)) {
        return Report::Bad("Can't @{} active: Profile doesn't exist!", name);
    }
    if (current_profile.name == name.data()) {
        return Report::Bad("profile @{} is already active", name);
    }
    current_profile.name = name.data();
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
    else if (currentProfile() == NO_PROFILE) {
        return Report::Bad("Can't clean profile configs: No profiles are active");
    }

    if (config) {
        target_path = config_d/currentProfile();
        if (fs::exists(target_path)) {
            cm::print(":: Cleaning profile configs: ", target_path.string());
            cm::remove_dir_contents_recursive(target_path, {config_src});
            // clear config file
            std::ofstream master_cfg(config_d/currentProfile()/config_src, std::ios::out);
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
    fs::path master_path = cm::parsePathTilde(HOME/master_src);
    if (!fs::exists(master_path) && !optimistic) return;

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
    if (FAILED mcparser.unwrap()) cm::print("Master-Config-Parser: Unwrap: Something wrong happened\n");

    profiles = mcparser.profiles;

    // set active profile based on the config
    auto it = mcparser.vars.find("profile.active");
    if (it != mcparser.vars.end()) {
        setActiveProfile(it->second.data()).printOnBad();
    } else if (!optimistic) cm::terminate("dotty.load: setProfile(it->second): Error!");

    cm::debug("Loaded dotty.\n\n");
}


// Copy all source files to destination files, pairs defined by a member
void Cfman::SourceToStorage() {
    for (auto [src, dest] : path_pairs) {
        if (dest.is_absolute()) {
            cm::print(
                __PRETTY_FUNCTION__+5, ": Error: destination should be relative path!",
                "skipping..\n"
            ); continue;
        }

        auto src_path = cm::parsePathTilde(src);

        auto dest_path = cm::parsePathTilde(storage_d/currentProfile())/dest;
        // NOTE: Add an explanation here
        if (dest_path.string().back() == '/') {
            cm::debug("Converting '", dest_path.string(), "' to '",
                dest_path.append(src_path.filename().string()), "'"
            );
            dest_path.append(src_path.filename().string());
        }

        // create missing and copy src->dest
        fs::create_directories(dest_path.parent_path());
        fs::copy_file(
            cm::parsePathTilde(src_path),
            cm::parsePathTilde(dest_path),
            fs::copy_options::update_existing
        );
    }
}


void Cfman::StorageToSources() {
    for (auto [dest, src] : path_pairs) {
        if (dest.is_absolute()) {
            cm::print(
                __PRETTY_FUNCTION__+5, ": Error: destination should be relative path!",
                "skipping..\n"
            ); continue;
        }

        auto dest_path = cm::parsePathTilde(storage_d/currentProfile())/dest;
        auto src_path = cm::parsePathTilde(src);
        cm::debug("salamm!");

        fs::create_directories(dest_path.parent_path());
        fs::copy_file(
            cm::parsePathTilde(dest_path),
            cm::parsePathTilde(src_path),
            fs::copy_options::update_existing
        );
    }
}


Cfman dotty;

