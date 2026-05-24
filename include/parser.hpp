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
    void step() { if(checks()) { ++pos; } }
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
                step();
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
                        ;
                        table.insert({key, value});
                    }
                    else cm::terminate("After a EQUAL token next token.type should be STRING\n");
                }
                else cm::terminate("After an IDENT token next token.type should be EQUAL\n");
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
            std::string left = first;
            Profile profile("[no-name]", "[no-github-acc]", "[no-repo-url]");

            if (cm::prefix_strip(left, PROFILE, &left)) {
                if (cm::prefix_strip(left, PROFILE_ADD, &left)) {
                    profile.name = second;
                    profiles.emplace_back(profile);
                }
                else if (cm::prefix_strip(left, PROFILE_ACTIVE, &left)) {
                    vars[PROP(PROFILE, PROFILE_ACTIVE)] = second;
                }
            }
            // else if (cm::prefix_strip(left, MENTION, &left)) {
            //     // if (!cm::vec_contains(profiles, key)) {
            //         // cm::print("Master-Config: Profile does not exist: ", key);
            //     // }
            //     const char* mentioned_profile = left.data();
            //     if (cm::prefix_strip(left, "gh-acc", &left)) {
            //         vars[PROP(mentioned_profile, MENTION_GH_ACC)] = second;
            //     }
            //     else if (cm::prefix_strip(left, MENTION_REPO_URL, &left)) {
            //         vars[PROP(mentioned_profile, "repo-url")] = second;
            //     }
            // }
            else if (!cm::obj_prop(left).first.empty()) {
                const char* mentioned  = cm::obj_prop(left).first.c_str();
                const std::string prop  = cm::obj_prop(left).second;
                ;
                if (prop == cm::concats(".", MENTION_GH_ACC)) {
                    vars[PROP(mentioned, MENTION_GH_ACC)] = prop;
                }
                else if (prop == cm::concats(".", MENTION_REPO_URL)) {
                    vars[PROP(mentioned, MENTION_REPO_URL)] = prop;
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
            cm::print(i, ". ", prof.name, "\n");
        }
        cm::print("\n");
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

                        path_pairs.emplace_back(SrcDest{src, dest});
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


