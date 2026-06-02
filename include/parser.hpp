#pragma once
#include "profile.hpp"

struct Token {
    enum TT {
        STRING='s',
        REDIRECTOR='r',

        IDENT='a',
        EQUAL='=',
        MENTION='@',

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
    void step(uint n=1) { while(checks() && n--) { ++pos; } }
    void skipws() { while(checks() && get()==' ') step(); }

    std::string lexString() {
        step();
        std::string string;
        while (checks()) {
            if (get() == '"') goto ret;
            if (string.size() > 1024) cm::terminate("String length is too much!\n");
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

    std::string lexMention() {
        std::string mention;
        step();
        if (!isalpha(get())) { // cant be path -> cant be a profile
            cm::terminate("lexMention(): first character is not alpha!");
        }
        while (isalnum(get()) || get() == '.' || get() == '-') {
            mention += get();
            step();
        }
        return mention;
    }

    std::string lexIdent() {
        std::string ident;
        while (isalpha(get()) || get() == '.' || get() == '-') {
            ident += get();
            step();
        }
        return ident;
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
            else if (get() == '@') {
                tok.name = lexMention();
                tok.type = Token::MENTION;
            }
            else if (isalpha(get())) {
                tok.name = lexIdent();
                tok.type = Token::IDENT;
            }
            else if (get() == '=') {
                tok.name = lexEqual();
                tok.type = Token::EQUAL;
            }
            else
            {
                cm::debug("Lexer::lexMain(): Encountered unknown character: '", get(), "'");
                tokens.emplace_back(tok.type=Token::UNKNOWN, tok.name="<error>");
                step(tok.name.size());
                continue;
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


    Report parse()
    {
        Report rep;
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
                        ;
                        table.insert({key, value});
                    }
                    else rep.addComplain("After a EQUAL token next token.type should be STRING\n");
                }
                else rep.addComplain("After an IDENT token next token.type should be EQUAL\n");
            }

            else if (get().type == Token::MENTION) {
                std::string mention = get().name;
                ;
                advance();
                if (get().type == Token::EQUAL) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string value = get().name;
                        ;
                        table.insert({mention, value});
                    }
                }
            }

            else rep.addComplain("First token.type should be IDENT\n");
            advance();
        }

        return rep;
    }

    void eval()
    {
        COMPTIME_STR PROFILE = "profile";
            COMPTIME_STR PROFILE_ADD = ".add";
            COMPTIME_STR PROFILE_ACTIVE = ".active";
        COMPTIME_STR MENTION = "@";
            COMPTIME_STR MENTION_GH_ACC = ".gh-acc";
            COMPTIME_STR MENTION_REPO_URL = ".repo-url";


        for (const auto& [first, second]  : table)
        {
            std::string left = first;
            Profile profile("[no-name]", "[no-github-acc]", "[no-repo-url]", false);

            if (cm::prefix_strip(left, PROFILE, &left)) {
                if (cm::prefix_strip(left, PROFILE_ADD, &left)) {
                    profile.name = second;
                    profiles.emplace_back(profile);
                }
                else if (cm::prefix_strip(left, PROFILE_ACTIVE, &left)) {
                    vars[PROP(PROFILE, PROFILE_ACTIVE)] = second;
                }
            }
            else if (!cm::obj_prop(left).first.empty()) {
                const char* mentioned  = cm::obj_prop(left).first.c_str();
                const std::string prop  = cm::obj_prop(left).second;
                ;
                if (prop == MENTION_GH_ACC) {
                    vars[PROP(mentioned, MENTION_GH_ACC)] = second;
                }
                else if (prop == MENTION_REPO_URL) {
                    vars[PROP(mentioned, MENTION_REPO_URL)] = second;
                }
            }
            else
            {
                cm::print("Master-Config: Eval-Error: Invalid tokens: '", first,"', '",second, "'\n");
            }
        }

        for (auto& prof  : profiles) {
            auto it_gh_acc = vars.find(PROP(prof.name.c_str(), MENTION_GH_ACC).c_str());
            auto it_repo_url = vars.find(PROP(prof.name.c_str(), MENTION_REPO_URL));

            if (it_gh_acc != vars.end()) {
                prof.github_name = it_gh_acc->second;
                cm::debug(prof.github_name);
            } else cm::print("Warning: '",prof.name,'.',MENTION_GH_ACC, "' could not get fetched!\n");

            if (it_repo_url != vars.end()) {
                prof.repo_name = cm::repo_from_url(it_repo_url->second);
                cm::debug(cm::make_repo_url(prof.github_name, prof.repo_name));
            } else cm::print("Warning: '",prof.name,'.',MENTION_REPO_URL, "' could not get fetched!\n");

            ;
        }
    }

    Report unwrap() {
        for (uint32 i=0;  i < profiles.size();  ++i) {
            auto& prof = profiles[i];
            for (uint32 j=1;  j < profiles.size();  ++j) {
                if (prof.name == profiles[j].name) {
                    return Report::Bad("Duplicate profile declarations: ");
                }
            }
        }

        return Report::Good();
    }
};




struct ConfigParser {
    std::vector<Token> tokens;
    uint32 idx;
    std::vector<SrcDest> path_pairs;

    Token get() { return tokens[idx]; }
    bool checks() { return tokens.size() > idx; }
    void advance() { if (checks()) ++idx; }


    Report parseMain()
    {
        Report res;
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

                        path_pairs.emplace_back(SrcDest{src, dest});
                    }
                    else res.addComplain("After a REDIRECTOR token next token.type should be STRING\n");
                }
                else res.addComplain("After a STRING token next token.type should be REDIRECTOR\n");
            }
            else res.addComplain("First token.type should be STRING\n");

            advance();
        }

        return res;
    }

    std::vector<SrcDest> result() const {
        return path_pairs;
    }
};


