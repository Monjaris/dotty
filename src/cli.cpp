#include "cli.hpp"


struct CmdLine::Impl {
    CLI::App cli {"Config Manager CLI tool"};
    int argc; char** argv;
    const char* const default_msg = {
        "Dotty: Config/Dotfiles manager\n"
    };

    std::map<CLI::App*, std::function<int32()>> sub_commands;
};


CmdLine::CmdLine(int argc, char** argv) : impl(new Impl()) {
    impl->argc = argc;
    impl->argv = impl->cli.ensure_utf8(argv);
}

CmdLine::~CmdLine() {delete impl;}



int32 CmdLine::do_list() {
    if (dotty.profiles.size() == 0) {
        cm::print("No profiles exist yet!\n");
        return EXIT_SUCCESS;
    }

    dotty.listProfiles();
    return EXIT_SUCCESS;
}


int32 CmdLine::do_init(const char* const ini_prof) {
    // Check github authentication status
    cm::print("Checking GitHub CLI authentication...\n");
    int auth_status = std::system("gh auth status --hostname github.com > /dev/null 2>&1");
    if (auth_status != 0) {
        cm::terminate("gh is not authenticated. Please run 'gh auth login' first.\n");
    }
    cm::print("GitHub CLI authenticated.\n\n");

    // Prompts for making the repo
    std::string repo_name;
    cm::print("Enter a name for your dotty config repo: ");
    cm::prompt(repo_name);

    if (repo_name.empty()) cm::terminate("Repo name cannot be empty.\n");

    std::string visibility;
    cm::print("Repo visibility — enter 'public' or 'private' [private]: ");
    cm::prompt(visibility);
    cm::print("\n");
    if (visibility.empty()) visibility = "private";
    if (!cm::is_any_of(visibility, {"private"s, "public"s})) {
        cm::terminate("Invalid visibility '{}'. Must be 'public' or 'private'.\n", visibility);
    }

    // config storage
    fs::path repo_d = cm::parsePathTilde(dotty.storage_d/ini_prof);
    fs::path config = cm::parsePathTilde(dotty.config_d/ini_prof);

    Cfman::Res res;
    res = dotty.newProfile(ini_prof, repo_name, visibility);
    cm::print("dotty.newProfile(): ", (int)res, "\n");

    // Write master config
    std::ofstream master(cm::parsePathTilde(dotty.HOME/dotty.master_config), std::ios::app);
    master << "profile.add = \"" << ini_prof << "\"\n";
    master << "profile.active = " << ini_prof << "\n";
    master << "\n"; master.close();

    dotty.load();

    cm::print("Repo '", repo_name, "' created as ", visibility, " on GitHub.\n");
    return EXIT_SUCCESS;
}


int32 CmdLine::do_write() {
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
    dotty.write();
    cm::print("\n\nLexed tokens:\n");
    lexer.print();

    return EXIT_SUCCESS;
}

int32 CmdLine::do_update(const char* commit_message) {
    cm::CmdStream cmd;
    cmd
        .add("cd {}", (dotty.storage_d/dotty.currentProfile() ).string())
        .add("git add .")
        .add("git commit {}", commit_message)
        .add("git push")
    ;
    cmd.run(" && ");

    return EXIT_SUCCESS;
}

int32 CmdLine::do_install() {
    cm::CmdStream cmd;
    cmd
        .add("")
    ;
    cmd.run("; ");

    return EXIT_SUCCESS;
}


void CmdLine::newSubCmd(const char* name, const std::function<int32()>& fn, const char* desc)
{
    impl->sub_commands.emplace(
        impl->cli.add_subcommand(name, desc),
        fn
    );
}

#define BIND(_fn_name) [this](){return _fn_name;}
int32 CmdLine::setup()
{
    newSubCmd("init", BIND(do_init("main")), "Initialize dotty config manager in your system");
    newSubCmd("list", BIND(do_list()), "List all existing profiles");
    newSubCmd("write", BIND(do_write()), "Update configs to configs storage");

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

