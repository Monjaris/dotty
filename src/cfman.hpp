#pragma once
#include "profile.hpp"



struct Token {
    enum TT {
        STRING='s',
        REDIRECTOR='r',

        IDENT='a',
        EQUAL='=',

        NONE='_',
        UNKNOWN='!'
    } type;
    std::string name;
};

struct Lexer {
    std::string line;
    uint32 pos;
    std::vector<Token> tokens;

    char get() { return line[pos]; }
    bool checks() { return line.size() > pos; }
    void step() { if(checks()) ++pos; }
    void skipws() { while(checks() && get()==' ') step(); }

    std::string lexString() {
        step();
        std::string string;
        uint32 len = 0;
        while (checks() && ++len) {
            if (get() == '"') goto ret;
            if (len > 1024) cm::terminate("String length too much!\n");
            string += get();
            step();
        }
    ret:
        step();
        return string;
    }

    std::string lexRedirector() {
        step();
        bool bad = true;
        if (get() == '>') bad = false;

        if (bad) cm::terminate();
        else return (step(), std::string(1, '>') + '>');
    }

    std::string lexEqual() {
        step();
        return "=";
    }

    void print() {
        for (uint32 i=0;  i < tokens.size();  ++i) {
            cm::print((char)tokens[i].type, " : ", tokens[i].name, "\n");
        }
    }

    void lexMain() {
        tokens.clear();
        pos = 0;
        Token tok = {Token::NONE, "<none>"};

        while(checks())
        {
            skipws();

            if (get() == '"') {
                tok.name = lexString();
                tok.type = Token::STRING;
            }
            else if (get() == '>') {
                tok.name = lexRedirector();
                tok.type = Token::REDIRECTOR;
            }
            else if (get() == '=') {
                tok.name = lexEqual();
                tok.type = Token::EQUAL;
            }
            else
            {
                tok.name = "<error>";
                tok.type = Token::UNKNOWN;
            }

            skipws();
            tokens.emplace_back(tok);
        }
    }

    auto result() {
        return tokens;
    }
};


struct MasterConfigParser {
    std::vector<Token> tokens;
    uint32 idx;
    std::map<std::string, std::string> table;
    // distributions of table
    std::map<std::string, std::string> vars;
    std::vector<Profile> profiles;

// builds property cstring
#define PROP(obj, prop) cm::concats(obj, prop)

    Token get() { return tokens[idx]; }
    bool checks() { return tokens.size() > idx; }
    void advance() { if (checks()) ++idx; }


    void parse()
    {
        idx = 0;

        while(checks())
        {
            if (get().type == Token::IDENT) {
                std::string key = get().name;

                advance();
                if (get().type == Token::EQUAL) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string value = get().name;

                        table.insert({key, value});
                    }
                    else cm::terminate("After a EQUAL token next token.type should be STRING\n");
                }
                else cm::terminate("After an IDENT token next token.type should be EQUAL\n");
            }
            else cm::terminate("First token.type should be IDENT\n");

            advance();
        }
    }

    void eval()
    {
        COMPTIME_STR PROFILE = "profile";
            COMPTIME_STR PROFILE_ADD = ".add";
            COMPTIME_STR PROFILE_ACTIVE = ".active";
        COMPTIME_STR MENTION = "@";
            COMPTIME_STR MENTION_GH_ACC = "gh-acc";
            COMPTIME_STR MENTION_REPO_URL = "repo-url";


        for (const auto& [first, second]  : table)
        {
            std::string key = first;
            Profile profile("[no-name]", "[no-github-acc]", "[no-repo-url]");

            if (cm::prefix_strip(key, PROFILE, &key)) {
                if (cm::prefix_strip(key, PROFILE_ADD, &key)) {
                    profile.name = second;
                    profiles.emplace_back(profile);
                }
                else if (cm::prefix_strip(key, PROFILE_ACTIVE, &key)) {
                    vars[PROP(PROFILE, PROFILE_ACTIVE)] = second;
                }
            }
            else if (cm::prefix_strip(key, MENTION, &key)) {
                // if (!cm::vec_contains(profiles, key)) {
                    // cm::print("Master-Config: Profile does not exist: ", key);
                // }
                const char* mentioned_profile = key.data();
                if (cm::prefix_strip(key, "gh-acc", &key)) {
                    vars[PROP(mentioned_profile, MENTION_GH_ACC)] = second;
                }
                else if (cm::prefix_strip(key, MENTION_REPO_URL, &key)) {
                    vars[PROP(mentioned_profile, "repo-url")] = second;
                }
            }
            else
            {
                cm::print("Master-Config: Eval-Error: Invalid tokens: '", first,"', '",second, "'\n");
            }
        }
    }

    void printReadProfiles() {
        for (uint32 i=0;  i < profiles.size();  ++i) {
            auto prof = profiles[i];
            cm::print("1. ", prof.name, "\n");
        }
    }

    bool unwrap() {
        bool result = true;

        for (uint32 i=0;  i < profiles.size();  ++i) {
            auto& prof = profiles[i];
            for (uint32 j=1;  j < profiles.size();  ++j) {
                if (prof.name == profiles[j].name) {
                    result = false;
                    cm::print("Duplicate profile declaration: ", prof.name ,"\n");
                }
            }
        }

        return result;
    }
};






struct Cfman
{
    std::vector<Profile> profiles;

    const fs::path HOME = cm::userHomePath(true, "$HOME is empty");
    fs::path config_d = HOME/".config/dotty";
    fs::path storage_d = HOME/".local/share/dotty";

    const char* master_config = ".dotty";
    const char* config_source_name = "config";
    std::optional<std::string> current_profile = std::nullopt;

    using SrcDest = struct{ fs::path src, path; };
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
        cmd.run(" && ");

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
        cm::debug("Loading dotty...\n");
        fs::path master_path = cm::parsePathTilde(HOME/master_config);
        if (!fs::exists(master_path)) return;

        cm::debug("Opening master config\n");
        std::ifstream master(cm::parsePathTilde(master_config));
        Lexer lexer;
        MasterConfigParser mcparser;
        while (std::getline(master, lexer.line)) {
            lexer.lexMain();
            for (auto& token :  lexer.result()) {
                mcparser.tokens.push_back(token);
            }
        }
        mcparser.eval();
        mcparser.printReadProfiles();
        if( !mcparser.unwrap() ) cm::print("Master-Config-Parser: Unwrap: Something wrong happened\n");

        profiles = mcparser.profiles;

        // set active profile based on the config
        auto it = mcparser.vars.find("profile.active");
        setProfile((it!=mcparser.vars.end())? (it->second) : (cm::terminate("Error: setProfile()"),""));

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


struct ConfigParser {
    std::vector<Token> tokens;
    uint32 idx;
    std::vector<Cfman::SrcDest> path_pairs;

    Token get() { return tokens[idx]; }
    bool checks() { return tokens.size() > idx; }
    void advance() { if (checks()) ++idx; }

    void parseMain()
    {
        idx = 0;

        while(checks())
        {
            if (get().type == Token::STRING) {
                std::string src = get().name;

                advance();
                if (get().type == Token::REDIRECTOR) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;

                        path_pairs.emplace_back(Cfman::SrcDest{src, dest});
                    }
                    else cm::terminate("After a REDIRECTOR token next token.type should be STRING\n");
                }
                else cm::terminate("After a STRING token next token.type should be REDIRECTOR\n");
            }
            else cm::terminate("First token.type should be STRING\n");

            advance();
        }
    }

    auto result() {
        return path_pairs;
    }
};

