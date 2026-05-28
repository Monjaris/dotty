#include "cli.hpp"


struct CmdLine::Impl {
    CLI::App cli {"Config Manager CLI tool"};
    int argc; char** argv;
    const char* const default_msg = {
        "Dotty: Config/Dotfiles manager\n"
        "Active: "
    };

    std::map<CLI::App*, std::function<int32()>> sub_commands;

    // cache view
    struct {
        strview delete_profile;
        strview config_;
        strview clean_configs;
        std::string list_properties;
    } v;
};


CmdLine::CmdLine(int argc, char** argv) : impl(new Impl()) {
    impl->argc = argc;
    impl->argv = impl->cli.ensure_utf8(argv);
}

CmdLine::~CmdLine() {delete impl;}



int32 CmdLine::do_init() {
    if (fs::exists(dotty.HOME/dotty.master_src)) {
        std::string in = "y";
        cm::prompt(
            "Master config shouldn't have existed before init!\n"
            "Wanna delete it? (y/N): ", in
        );

        if (std::tolower(in[0]) == 'y') {
            fs::remove(dotty.HOME/dotty.master_src);
            cm::print("Removed '", dotty.HOME/dotty.master_src, "'\n");
        } else {
            cm::terminate("You can't run master config preconfiguired!\n");
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
        std::string("https://github.com/")+gh_auth_name+"/"+repo_name  // SSO probably saves us
    };
    cm::debug("URL constructed: ", repo_url);

    std::string visibility;
    cm::prompt("Repo visibility — enter 'public' or 'private' [private]: ", visibility);
    // cm::print("\n");
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
    fs::path repo_d = cm::parsePathTilde(dotty.storage_d/ini_prof);
    fs::path config = cm::parsePathTilde(dotty.config_d/ini_prof);

    dotty.newProfile(ini_prof, gh_auth_name, repo_name, visibility)
        .printOnBad()
        .terminateOnBad();

    // Write master config
    std::ofstream master(cm::parsePathTilde(dotty.HOME/dotty.master_src), std::ios::app);
    master << "profile.add = \"" << ini_prof << "\"\n";
    master << "profile.active = \"" << ini_prof << "\"\n";
    master << "@" << ini_prof << ".gh-acc = \"" << gh_auth_name << "\"\n";
    master << "@" << ini_prof << ".repo-url = \"" << repo_url << "\"\n";
    master << "\n"; master.close();
    // load recently written config, set things up
    dotty.load(true);

    cm::print("Repo '", repo_name, "' created as ", visibility, " on GitHub.\n");
    cm::print("Setting ", ini_prof, " active profile\n");
    dotty.setActiveProfile(ini_prof).printOnBad();
    return EXIT_SUCCESS;
}



int32 CmdLine::do_list(const strview options/*= "name,repo,url,gh"*/) {
    dotty.load();

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



int32 CmdLine::do_clean(strview option) {
    dotty.load(true);

    // {master, config, storage} toggle bytes
    bool c, s = false; c = s;

    if (option == "all") c=s=true; goto _remove;
    if (option.contains("config")) c = true;
    if (option.contains("storage")) s = true;

    _remove:
    return dotty.cleanConfigs(c, s).printOnBad();
}



int32 CmdLine::do_delete(strview profile_name) {
    dotty.load();

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

    int32 res;
    res = (int32)dotty.setActiveProfile(profile_name.data());
    res = do_clean("all");
    cm::CmdStream del_data;
    del_data
        .add("gh repo delete {}", dotty.getProfileByName(profile_name)->repo_name)
    .run(" && ", false);

    return res;
}



int32 CmdLine::do_update() {
    dotty.load();

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
    cm::CmdStream cmd;
    cmd
        .add("cd {}", (dotty.storage_d/dotty.currentProfile()).string())
        .add("git add .")
        .add("git commit -m \"{}\"", commit_message)
        .add("git push")
    .run(" && ", false);

    return EXIT_SUCCESS;
}



int32 CmdLine::do_install() {
    if (dotty.currentProfile()==dotty.NO_PROFILE || dotty.noProfilesExist()) {
        cm::print("Install operation requires active profile to be set\n");
        return EXIT_FAILURE;
    }
    if (!cm::internet_is_connected()) {
        cm::print("Install operation requires internet connection\n");
        return EXIT_FAILURE;
    }

    cm::CmdStream cmd;
    cmd
        .add("mkdir -p $HOME/.cache/dotty")
        .add("cd $HOME/.cache/dotty")
        .add("git clone {}", dotty.getProfileByName(dotty.currentProfile())->repoUrl())
    
    ;
    cmd.run(" && ", false);

    return EXIT_SUCCESS;
}



int32 CmdLine::do_profile(strview option) {
    $IMPLEMENT(__PRETTY_FUNCTION__);
    return 0;
}



int32 CmdLine::do_config(strview option) {
    // prompt editing suggestion, and edit is accepted
    auto suggest_edit = [](const fs::path cfg_path)->int32 {
        std::string y_N = {};
        cm::prompt("Do you want to edit this file? (y/N): ", y_N);
        if (std::tolower(y_N[0]) != 'y' && $IMPLEMENT("Enter should accept too")) return EXIT_FAILURE;
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

        return cm::CmdStream{}.add("bat ~/.dotty").run(" && ", false);

        return suggest_edit(dotty.HOME/dotty.master_src);
    }

    else
    {
        cm::print("config: Unknown flag '", option, "'\n");
        return EXIT_FAILURE;
    }
}




CLI::App* CmdLine::newSubCmd(
    const char* name, const std::function<int32()>& fn, const char* desc, int32 require_subcommands
) {
    auto* sub_cmd = impl->cli.add_subcommand(name, desc);
    sub_cmd->require_subcommand(require_subcommands);

    impl->sub_commands.emplace(sub_cmd, fn);
    return sub_cmd;
}


#define BIND(_fn_name) [this](){return _fn_name;}
int32 CmdLine::setup()
{
    using SC = CLI::App*;

    SC sc_init = newSubCmd("init", BIND(do_init()), "Initialize dotty config manager in your system");
    //
    SC sc_list = newSubCmd("list", BIND(do_list(impl->v.list_properties)), "List all existing profiles and their properties");
        sc_list->add_option("propeties", impl->v.list_properties, "List profiles and opted properties")
            ->default_val("name,url");
    //
    SC sc_clean = newSubCmd("clean", BIND(do_clean(impl->v.clean_configs)), "Clean all configs for current profile");
        sc_clean->add_option("config", impl->v.clean_configs, "Clean configs")->required();
    //
    SC sc_write = newSubCmd("update", BIND(do_update()), "Write configs to configs storage");
    //
    SC sc_update = newSubCmd("push", BIND(do_push("Updating configs")), "Push config storage to the github repo");
    //
    SC sc_delete = newSubCmd("delete", BIND(do_delete(impl->v.delete_profile)), "Delete a profile");
        sc_delete->add_option("profile", impl->v.delete_profile, "Delete ")->required();
    //
    SC sc_config = newSubCmd("config", BIND(do_config(impl->v.config_)), "Configuration utilities");
        sc_config->add_option("configuration", impl->v.config_, "Configuire master");
    //
    SC sc_profile = newSubCmd("profile", std::function<int32()>{[]{return 0;}}, "Profile related commands", 1);
    //
    SC sc_install = newSubCmd("install", std::function<int32()>{[]{return 0;}}, "Installer for configs", 1);

    CLI11_PARSE(impl->cli, impl->argc, impl->argv);
    return EXIT_SUCCESS;
}
#undef BIND


int32 CmdLine::run()
{

    if (impl->cli.count_all() == 1) {
        cm::print(impl->default_msg, (dotty.load(), dotty.currentProfile()), "\n");
    }

    dotty.load(true);

    for (auto& [app_ptr, fn]  : impl->sub_commands) {
        if (impl->cli.got_subcommand(app_ptr)) {
            fn.operator()();
        }
    }

    return EXIT_SUCCESS;
}

