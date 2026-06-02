#include "cli.hpp"



struct CmdLine::Impl {
    CLI::App cli {"Config Manager CLI tool"};

    int argc; char** argv;

    const char* const default_msg = {
        "Dotty: Config/Dotfiles manager\n"
        "Active: "
    };


    std::map<CLI::App*, std::function<int32()>> sub_commands;

    // cache values
    struct {
        strview config_;
        strview clean_configs;
        std::string list_properties;
        std::string new_name;
            std::string new_repo_name;
            std::string new_repo_visb;
        strview delete_profile;
    } v;
};

#   define APP (impl->cli)



CmdLine::CmdLine(int argc, char** argv) : impl(new Impl()) {
    impl->argc = argc;
    impl->argv = impl->cli.ensure_utf8(argv);
}

CmdLine::~CmdLine() {delete impl;}



int32 CmdLine::do_init() {
    if (fs::exists(dotty.HOME/dotty.master_src)) {
        if (!cm::ask_confirm("Master config shouldn't have existed before init!\nWanna delete it?")) {
            cm::terminate("You can't run master config preconfiguired!\n");
        }
        else {
            fs::remove(dotty.HOME/dotty.master_src);
            cm::print("Removed '", dotty.HOME/dotty.master_src, "'\n");
        }
    }

    // Check if internet is connected
    if (!cm::internet_is_connected()) {
        cm::terminate(
            "Your device is not connected to the internet!\n",
            "Github repo initialization requires a wifi connection."
        );
    }

    // Check github authentication status
    cm::print("Checking GitHub CLI authentication...\n");
    int auth_status = std::system("gh auth status --hostname github.com > /dev/null 2>&1");
    if (FAILED auth_status) {
        cm::terminate("gh is not authenticated. Please run 'gh auth login' first.\n");
    }
    cm::print("GitHub CLI authenticated.\n\n");

    // Get github authenticated username
    cm::CmdStream gh_handle;
    gh_handle.add("gh api user --jq '.login'");
    if (FAILED gh_handle.run(" && ", true)) cm::terminate("Fetching github username failed!");
    const std::string& gh_auth_name = gh_handle.output();


    // Prompts for making the repo
    std::string repo_name;
    cm::prompt("Enter a name for your dotty config repo: ", repo_name);
    dotty.validateRepoName(repo_name).printOnBad().terminateOnBad();

    const std::string repo_url = {
        std::string("https://github.com/")+gh_auth_name+"/"+repo_name  // SSO may saves us
    };
    cm::debug("URL constructed: ", repo_url);

    std::string visibility;
    cm::prompt("Repo visibility — enter 'public' or 'private' [private]: ", visibility);

    if (visibility.empty()) visibility = "private";
    if (!cm::is_any_of(visibility, {"private"s, "public"s})) {
        cm::terminate("Invalid visibility '{}'. Must be 'public' or 'private'.", visibility);
    }

    std::string ini_prof;
    cm::prompt("Enter profile name(same as profile's directory name) [main]: ", ini_prof);
    cm::print("\n");
    if (ini_prof.empty()) ini_prof = "main";

    dotty.validateProfileName(ini_prof).printOnBad().terminateOnBad();

    // config storage
    fs::path repo_d = cm::parsePathTilde(dotty.data_d/ini_prof);
    fs::path config = cm::parsePathTilde(dotty.config_d/ini_prof);

    dotty.newProfile(ini_prof, gh_auth_name, repo_name, visibility)
        .printOnBad()
        .terminateOnBad();
    // dotty.load(true);  // newProfile loads anyways

    cm::print("Repo '", repo_name, "' created as ", visibility, " on GitHub.\n");
    cm::print("Setting ", ini_prof, " active profile\n");
    dotty.setActiveProfile(ini_prof).printOnBad();
    return EXIT_SUCCESS;
}



// int32 CmdLine::do_clean(strview option) {
//     // {master, config, storage} toggle bytes
//     bool c, s = false; c = s;

//     if (option == "all") {c=s=true; goto _remove;}
//     if (option.contains("config")) c = true;
//     if (option.contains("storage")) s = true;

//     _remove:
//     return dotty.cleanConfigs(c, s).printOnBad();
// }


int32 CmdLine::do_update() {
    if (dotty.noProfilesExist()) {
        cm::print("To update a profile you should first have to create a profile\n");
        return EXIT_FAILURE;
    }
    if (dotty.currentProfile() == dotty.NO_PROFILE) {
        cm::print("To update a profile you should first have to have a active profile set\n");
        return EXIT_FAILURE;
    }

    Lexer lexer;
    ConfigParser parser;
    std::ifstream conf(
        cm::parsePathTilde(dotty.config_d/dotty.currentProfile()/dotty.config_src),
        std::ios::in
    );
    if (!conf.is_open()) cm::terminate("File could not be opened!\n");

    while (std::getline(conf, lexer.line)) {
        cm::print("Lexing config...\n");
        lexer.lexMain();
        cm::print("Parsing tokens...\n");
        parser.tokens = lexer.result();
        parser.parseMain();
        cm::print("Adding new values to the base...\n");
        dotty.path_pairs = parser.result();
    }
    cm::print("\n\nCopying configs to their destinations...\n");
    dotty.SourceToStorage();
    cm::print("\n\nLexed tokens:\n");
    lexer.print();

    return EXIT_SUCCESS;
}



int32 CmdLine::do_push(const char* commit_message) {
    if (dotty.currentProfile() == dotty.NO_PROFILE) {
        cm::print("To push a profile, first you have to set active profile");
        return EXIT_FAILURE;
    }

    cm::ensure_directory(dotty.config_d / dotty.currentProfile() / dotty.storage_cfgref);
    cm::ensure_directory(dotty.data_d / dotty.currentProfile() / dotty.storage_cfgref);
    // Copy all config source and includes to local repo(config storage) before push
    fs::copy(
        dotty.config_d / dotty.currentProfile() / dotty.storage_cfgref,
        dotty.data_d / dotty.currentProfile() / dotty.storage_cfgref,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing
    );

    cm::CmdStream cmd;
    cmd
        .add("cd {}", (dotty.data_d/dotty.currentProfile()).string())
        .add("git add .")
        .add("git commit -m \"{}\"", commit_message)
        .add("git push")
    .run(" && ", false);

    return EXIT_SUCCESS;
}



int32 CmdLine::do_pull() {
    if (dotty.currentProfile()==dotty.NO_PROFILE || dotty.noProfilesExist()) {
        cm::print("Install operation requires active profile to be set\n");
        return EXIT_FAILURE;
    }
    if (!cm::internet_is_connected()) {
        cm::print("Install operation requires internet connection\n");
        return EXIT_FAILURE;
    }

    if (!cm::ask_confirm("You are about to overwrite your current profile, are you are sure?")) {
        cm::terminate("Pulling aborted.");
    }

    Profile* active_prof = dotty.getProfileByName(dotty.currentProfile());
    std::string active_prof_cfg_d = (dotty.config_d/active_prof->name).string();

    cm::CmdStream {}
        .add("mkdir -p $HOME/.cache/dotty")
        .add("cd $HOME/.cache/dotty")
        .add("git clone {}", active_prof->repoUrl())
        .add("rm -rf {}", active_prof_cfg_d)
        .add("cp -rav ./{} {}", active_prof->repo_name, active_prof_cfg_d)
    .run(" && ", false);

    // Copy all storage config references back to dotty config directory
    fs::copy(
        dotty.data_d / dotty.currentProfile() / dotty.storage_cfgref,
        dotty.config_d / dotty.currentProfile() / dotty.storage_cfgref,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing
    );
    do_update();

    return EXIT_SUCCESS;
}

int32 CmdLine::do_config(strview option) {
    // prompt editing suggestion, and edit is accepted
    auto suggest_edit = [](const fs::path cfg_path)->int32 {
        if (!cm::ask_confirm("Do you want to edit this file?")) {
            return EXIT_FAILURE;
        }
        else {
            return cm::CmdStream{}
                .add("$EDITOR {}", cfg_path.c_str())
                .run(" && ", false);
        }
    };

    dotty.load();

    if (option == "") {
        fs::path config_source = dotty.config_d/dotty.currentProfile()/dotty.config_src;
        if (!fs::exists(config_source)) {
            cm::print("Can't find config source: ", config_source, " doesn't exist!\n");
            return EXIT_FAILURE;
        }

        cm::CmdStream{}
            .add("bat {}", (dotty.config_d/dotty.currentProfile()/dotty.config_src).string())
            .run(" && ", false);

        return suggest_edit(dotty.config_d/dotty.currentProfile()/dotty.config_src);
    }

    else if (option == "master") {
        if (!fs::exists(dotty.HOME/dotty.master_src)) {
            cm::print("Can't find master configuration: File doesn't exist!\n");
            return EXIT_FAILURE;
        }

        cm::CmdStream{}.add("bat ~/.dotty").run(" && ", false);
        return suggest_edit(dotty.HOME/dotty.master_src);
    }

    else
    {
        cm::print("config: Unknown flag '", option, "'\n");
        return EXIT_FAILURE;
    }
}



// This is a subcommand with subcommands(naming convention: do_<subc>_)
int32 CmdLine::do_profile_(strview option) {
    return 0;
}


int32 CmdLine::do_p_list(const strview options/*= "name,repo,url,gh"*/) {
    if (dotty.noProfilesExist()) {
        cm::print("No profiles exist yet!\n");
        return EXIT_FAILURE;
    }

    bool name, repo, url, gh = false; name = repo = url = gh;
    if (options == "all") {name=repo=url=gh=true; goto _print;}
    if (options.contains("name")) name = true;
    if (options.contains("repo")) repo = true;
    if (options.contains("url"))  url = true;
    if (options.contains("gh"))   gh = true;

_print:
    cm::print("    [ PROFILES ]\n");
    dotty.listProfiles(name, repo, url, gh);
    return EXIT_SUCCESS;
}


int32 CmdLine::do_p_new(
    const std::string& name, const std::string& repo_name, const std::string& visibility
){
    std::optional<std::string> gh_acc = cm::active_github_account();
    if (!gh_acc.has_value()) cm::terminate("Github login not found");

    dotty.newProfile(name, cm::active_github_account().value(), repo_name, visibility);

    return EXIT_SUCCESS;
}


int32 CmdLine::do_p_delete(strview profile_name) {
    if (dotty.noProfilesExist()) {
        cm::print("Can't delete a profile: no profiles exist yet!\n");
        return EXIT_FAILURE;
    }
    if (dotty.getProfileByName(profile_name) == nullptr) {
        cm::print(
            "Could not delete '", profile_name,
            "': profile does not exist!\n"
        );
        return EXIT_FAILURE;
    }

    // delete github repo
    int32 del_gh_repo =  cm::CmdStream{}
        .add("gh repo delete {}", dotty.getProfileByName(profile_name)->repo_name)
    .run(" && ", false);

    if (FAILED del_gh_repo) {
        cm::print("Couldn't delete github repo!");
        std::string in = "y";
        cm::prompt("Do you want to proceed with removing directories of the profile?", in);
        if (not (in[0] == 'y')) cm::terminate("Could not remove '", profile_name, "'!");
    }

    if( fs::remove_all(dotty.config_d/profile_name)  ==  0 ) cm::print("[ERROR]: No profile config removed!");
    if( fs::remove_all(dotty.data_d/profile_name)  ==  0 ) cm::print("[ERROR]: No profile storage data removed!");

    return EXIT_SUCCESS;
}



CLI::App* CmdLine::newSubCmd(
    CLI::App* parent, const char* name, const std::function<int32()>& fn, const char* desc,
    int32 require_subcommands
) {
    auto* sub_cmd = parent->add_subcommand(name, desc);
    sub_cmd->require_subcommand(require_subcommands);

    impl->sub_commands.emplace(sub_cmd, fn);
    return sub_cmd;
}


#define BIND(_fn_name) [this](){return _fn_name;}
int32 CmdLine::setup()
{
    using SubCmd = CLI::App;

    SubCmd* sc_init = newSubCmd(&APP, "init",
        BIND(do_init()), "Initialize dotty config manager in your system");
    //
    SubCmd* sc_update = newSubCmd(&APP, "update",
        BIND(do_update()), "Write configs to configs storage");
        sc_update->alias("u");
    //
    SubCmd* sc_push = newSubCmd(&APP, "push",
        BIND(do_push("Updating configs")), "Push config storage to the github repo");
    //
    SubCmd* sc_pull = newSubCmd(&APP, "pull",
        BIND(do_pull()), "Pull your config from the repo", 1);
    //
    SubCmd* sc_config = newSubCmd(&APP, "config",
        BIND(do_config(impl->v.config_)), "Configuration utilities");
        sc_config->alias("c");
        sc_config->add_option("configuration", impl->v.config_, "Configuire master");
    //
    SubCmd* sc_profile_ = newSubCmd(&APP, "profile",
        BIND(do_profile_("")), "Profile related commands", 1)->require_subcommand()
        ->alias("p");
    //
        SubCmd* ssc_list = newSubCmd(sc_profile_, "list",
            BIND(do_p_list(impl->v.list_properties)), "List all existing profiles");
            ssc_list->alias("l");
            ssc_list->add_option("properties", impl->v.list_properties, "List profiles and opted properties")->default_val("name,url");
    //
        SubCmd* ssc_new = newSubCmd(sc_profile_, "new",
            BIND(do_p_new(impl->v.new_name, impl->v.new_repo_name, impl->v.new_repo_visb)), "Create a new profile");
            ssc_new->alias("n");
            ssc_new->add_option("-r");
    //
        SubCmd* ssc_delete = newSubCmd(sc_profile_, "delete",
            BIND(do_p_delete(impl->v.delete_profile)), "Delete a profile");
            ssc_delete->alias("d");
            ssc_delete->add_option("profile", impl->v.delete_profile, "Delete ")->required();


    //
    // SC sc_clean = newSubCmd("clean", BIND(do_clean(impl->v.clean_configs)), "Clean all configs for current profile", 1);
    //     sc_clean->add_option("config", impl->v.clean_configs, "Clean configs");
    //

    CLI11_PARSE(impl->cli, impl->argc, impl->argv);
    return EXIT_SUCCESS;
}
#undef BIND


int32 CmdLine::run()
{

    if (APP.count_all() == 1) {
        cm::print(impl->default_msg, (dotty.load(), dotty.currentProfile()), "\n");
    }

    dotty.load(true);

    for (auto& [sub_cmd, fn]  : impl->sub_commands) {
        if (sub_cmd->parsed()) {
            fn.operator()();
        }
    }

    return EXIT_SUCCESS;
}

