#include "cli.hpp"


struct CmdLine::Impl {
    CLI::App cli {"Config Manager CLI tool"};
    int argc; char** argv;
    const char* const default_msg = {
        "Dotty: Config/Dotfiles manager\n"
    };

    std::map<CLI::App*, std::function<int32()>> sub_commands;

    // cache view
    struct {
        strview delete_profile_name;
    } v;
};


CmdLine::CmdLine(int argc, char** argv) : impl(new Impl()) {
    impl->argc = argc;
    impl->argv = impl->cli.ensure_utf8(argv);
}

CmdLine::~CmdLine() {delete impl;}



int32 CmdLine::do_list() {
    if (dotty.noProfilesExist()) {
        cm::print("No profiles exist yet!\n");
        return EXIT_FAILURE;
    }

    dotty.listProfiles();
    return EXIT_SUCCESS;
}


int32 CmdLine::do_clean(strview option) {
    // {master, config, storage} toggle bytes
    bool m, c, s = false; m = c = s;

    if (option.contains("master")) m = true;
    if (option.contains("config")) c = true;
    if (option.contains("storage")) s = true;

    return dotty.cleanConfigs(m, c, s).printOnBad();
}


int32 CmdLine::do_init() {
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
    dotty.validateRepoName(repo_name).printOnBad();

    const std::string repo_url = {
        std::string("https://github.com/")+gh_auth_name+"/"+repo_name  // SSO probably saves us
    };
    cm::debug("URL constructed: ", repo_url);

    std::string visibility;
    cm::prompt("Repo visibility — enter 'public' or 'private' [private]: ", visibility);
    cm::print("\n");
    if (visibility.empty()) visibility = "private";
    if (!cm::is_any_of(visibility, {"private"s, "public"s})) {
        cm::terminate("Invalid visibility '{}'. Must be 'public' or 'private'.\n", visibility);
    }

    std::string ini_prof;
    cm::prompt("Enter profile name(same as profile's directory name) [main]: ", ini_prof);
    cm::print("\n");
    if (ini_prof.empty()) ini_prof = "main";

    dotty.validateProfileName(ini_prof).printOnBad();

    // config storage
    fs::path repo_d = cm::parsePathTilde(dotty.storage_d/ini_prof);
    fs::path config = cm::parsePathTilde(dotty.config_d/ini_prof);

    Cfman::Res res;
    res = dotty.newProfile(ini_prof, repo_name, visibility);
    cm::print("dotty.newProfile(): ", (int)res, "\n");

    // Write master config
    std::ofstream master(cm::parsePathTilde(dotty.HOME/dotty.master_config), std::ios::app);
    master << "profile.add = \"" << ini_prof << "\"\n";
    master << "profile.active = \"" << ini_prof << "\"\n";
    master << "@" << ini_prof << ".gh-acc = \"" << gh_auth_name << "\"\n";
    master << "@" << ini_prof << ".repo-url = \"" << repo_url << "\"\n";
    master << "\n"; master.close();
    // load recently written config, set things up
    dotty.load();

    cm::print("Repo '", repo_name, "' created as ", visibility, " on GitHub.\n");
    return EXIT_SUCCESS;
}


int32 CmdLine::do_delete(strview profile_name) {
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


int32 CmdLine::do_write() {
    if (dotty.noProfilesExist()) {
        cm::print("To write a profile you should first have to create a profile\n");
        return EXIT_FAILURE;
    }
    if (dotty.currentProfile() == dotty.NO_PROFILE) {
        cm::print("To write a profile you should first have to have a active profile set\n");
        return EXIT_FAILURE;
    }

    Lexer lexer;
    ConfigParser parser;
    std::ifstream conf(
        cm::parsePathTilde(dotty.config_d/dotty.currentProfile()/dotty.config_source_name),
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
    dotty.src_to_dest();
    cm::print("\n\nLexed tokens:\n");
    lexer.print();

    return EXIT_SUCCESS;
}


int32 CmdLine::do_update(const char* commit_message) {
    if (dotty.currentProfile() == dotty.NO_PROFILE) {
        cm::print("To update a profile, first you have to set active profile");
        return EXIT_FAILURE;
    }
    cm::CmdStream cmd;
    cmd
        .add("cd {}", (dotty.storage_d/dotty.currentProfile()).string())
        .add("git add .")
        .add("git commit {}", commit_message)
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


CLI::App* CmdLine::newSubCmd(const char* name, const std::function<int32()>& fn, const char* desc)
{
    auto* sub_cmd = impl->cli.add_subcommand(name, desc);
    impl->sub_commands.emplace(sub_cmd, fn);
    return sub_cmd;
}

#define BIND(_fn_name) [this](){return _fn_name;}
int32 CmdLine::setup()
{
    using SC = CLI::App*;

    SC sc_init =
    newSubCmd("init", BIND(do_init()), "Initialize dotty config manager in your system");
    SC sc_list =
    newSubCmd("list", BIND(do_list()), "List all existing profiles");
    SC sc_clean =
    newSubCmd("clean", BIND(do_clean("master,config,storage")), "Clean all configs for current profile");
    SC sc_write =
    newSubCmd("write", BIND(do_write()), "Write configs to configs storage");
    SC sc_update =
    newSubCmd("update", BIND(do_update("Updating configs")), "Push config storage to the github repo");

    SC sc_delete = newSubCmd("delete", BIND(do_delete(impl->v.delete_profile_name)), "");
    sc_delete->add_option("profile", impl->v.delete_profile_name)->required();

    CLI11_PARSE(impl->cli, impl->argc, impl->argv);
    return EXIT_SUCCESS;
}
#undef BIND


int32 CmdLine::run()
{

    if (impl->cli.count_all() == 1) {
        cm::print(impl->default_msg);
    }

    dotty.load();

    for (auto& [app_ptr, fn]  : impl->sub_commands) {
        if (impl->cli.got_subcommand(app_ptr)) {
            fn.operator()();
        }
    }

    return EXIT_SUCCESS;
}

