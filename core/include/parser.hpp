#pragma once
#include "profile.hpp"

struct Token {
    enum TT {
        // values
        STRING='s',
        // operators
        COPIER='c',
        LINKER='l',
        DIR_COPIER='C',
        DIR_LINKER='L',
        // punctuators
        DIRECTIVE='#',
        IDENT='a',
        EQUAL='=',
        MENTION='@',
        // sentinel
        NONE='_',
        UNKNOWN='!'
    } type;
    std::string name;
};

struct Lexer {
    std::string line;
    uint32 pos;
    std::vector<Token> tokens;
    static constexpr char CMNT = '#';

    struct [[nodiscard]] LexRes {
        static constexpr strview BAD = "<lex-error>";
        std::string str = "<no-lex>";
        bool bad = false;
        explicit LexRes (bool succeed) { str=BAD; bad=!succeed; }
        explicit LexRes (const std::string& res) { str=res; bad = false; }
    };

    [[nodiscard]] char get() { return line[pos]; }
    bool checks() { return line.size() > pos; }
    void step(uint n=1) { while(checks() && n--) { ++pos; } }
    void skipws() { while(checks() && get()==' ') step(); }

    std::string lexString() {
        step();
        std::string string;
        while (checks()) {
            if (get() == '"') goto _ret;
            if (string.size() > 1024) cm::terminate("String length is too much!\n");
            string += get();
            step();
        }
    _ret:
        step();
        return string;
    }


    LexRes lexCopier() {
        std::string copier = ">";
        step();
        if (get() == '>') {
            copier += get();
            step();
        } else return LexRes(false);
        return LexRes(copier);
    }

    LexRes lexLinker() {
        std::string linker = "-";
        step();
        if (get() == '>') {
            linker += get();
            step();
        } else return LexRes(false);
        return LexRes(linker);
    }

    LexRes lexDirCopier() {
        std::string dir_copier = ">";
        step();
        if (get() == '>') {
            dir_copier += get();
            step();
        } else return LexRes(false);
        if (get() == '*') {
            dir_copier += get();
            step();
        }
        else return LexRes(false);
        return LexRes(dir_copier);
    }

    LexRes lexDirLinker() {
        std::string dir_copier = "-";
        step();
        if (get() == '>') {
            dir_copier += get();
            step();
        } else return LexRes(false);
        if (get() == '*') {
            dir_copier += get();
            step();
        }
        else return LexRes(false);
        return LexRes(dir_copier);
    }


    std::string lexDirective() {
        std::string directive;
        step();
        if (!isalpha(get())) {
            cm::terminate("lexDirective(): first character is not alpha!");
        }
        while (isalpha(get()) || get()=='-') {
            directive += get();
            step();
        }
        return directive;
    }

    std::string lexMention() {
        std::string mention;
        step();
        if (!::isalpha(get())) { // cant be a profile if starts with non-alpha
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
        while (::isalpha(get()) || get() == '.' || get() == '-') {
            ident += get();
            step();
        }
        return ident;
    }

    std::string lexEqual() {
        step();
        return "=";
    }

    [[nodiscard]]
    static std::string RemoveComment(std::string line) {
        std::vector<int32> quotes;
        quotes.reserve(128);

        for (uint32 i=0;  i < line.size();  ++i) {
            if (line[i] == '\'') {
                quotes.push_back(i);
            }

            if (cm::is_even(quotes.size())) {
                if (line[i] == CMNT) {
                    return line.substr(0, i);
                }
            }
        }

        return line;
    }

    void print() {
        for (uint32 i=0;  i < tokens.size();  ++i) {
            cm::print((char)tokens[i].type, " : '", tokens[i].name, "'\n");
        }
    }

    void lexMain() {
        tokens.clear();
        pos = 0;
        Token tok = {Token::NONE, "<none>"};

        while(checks())
        {
            line = RemoveComment(line);
            skipws();

            if (get() == '#') {
                tok.name = lexDirective();
                tok.type = Token::DIRECTIVE;
            }
            else if (get() == '"') {
                tok.name = lexString();
                tok.type = Token::STRING;
            }
            // Lex COPIER && DIR_COPIER
            else if (get() == '>') {
                tok.name = lexCopier().str;
                if (tok.name == LexRes::BAD) {
                    tok.name = lexDirCopier().str;
                    if (tok.name == LexRes::BAD) {
                        continue;
                    } else {
                        tok.type = Token::DIR_COPIER;
                    }
                } else {
                    tok.type = Token::COPIER;
                }
            }
            // Lex LINKER && DIR_LINKER
            else if (get() == '-') {
                tok.name = lexLinker().str;
                if (tok.name == LexRes::BAD) {
                    tok.name = lexDirLinker().str;
                    if (tok.name == LexRes::BAD) {
                        continue;
                    } else {
                        tok.type = Token::DIR_LINKER;
                    }
                } else {
                    tok.type = Token::LINKER;
                }
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
        // COMPTIME_STR MENTION = "@";
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
                std::string mentioned  = cm::obj_prop(left).first;
                const std::string prop  = cm::obj_prop(left).second;
                ;
                if (prop == MENTION_GH_ACC) {
                    vars[PROP(mentioned.c_str(), MENTION_GH_ACC)] = second;
                }
                else if (prop == MENTION_REPO_URL) {
                    vars[PROP(mentioned.c_str(), MENTION_REPO_URL)] = second;
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
    std::vector<SrcDest> copy_files;
    std::vector<SrcDest> copy_dirs;
    std::vector<SrcDest> link_files;
    std::vector<SrcDest> link_dirs;

    Token get() { return tokens[idx]; }
    bool checks() { return tokens.size() > idx; }
    void advance() { if (checks()) ++idx; }

    // Interdiamate parse function
    void parsePaths(std::string* src, std::string* dest) {
        // SRC parsing
        *src = cm::parsePathTilde(*src);
        // DEST parsing
        if (dest->ends_with("/..")) { dest->replace(dest->size()-3, 3, fs::path(*src).filename()); }
        else if (*dest == "..") { *dest = fs::path(*src).filename(); }
    }

    Report parseMain()
    {
        copy_files.reserve(32);
        copy_dirs.reserve(32);
        link_files.reserve(32);
        link_dirs.reserve(32);

        Report res;
        while(checks())
        {
            // STRING -> OP -> STRING
            // DIRECTIVE -> IDENT

            if (get().type == Token::STRING) {
                std::string src = get().name;
                advance();

                if (get().type == Token::COPIER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        copy_files.emplace_back(SrcDest{src, dest});
                    }
                    else res.addComplain("Expected STRING after COPIER operator\n");
                }
                else if (get().type == Token::LINKER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        link_files.emplace_back(SrcDest{src, dest});
                    }
                    else res.addComplain("Expected STRING after LINKER operator\n");
                }
                else if (get().type == Token::DIR_COPIER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        copy_dirs.emplace_back(SrcDest{src, dest});
                    }
                    else res.addComplain("Expected STRING after DIR-COPIER operator\n");
                }
                else if (get().type == Token::DIR_LINKER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        link_dirs.emplace_back(SrcDest{src, dest});
                    }
                    else res.addComplain("Expected STRING after DIR-LINKER operator\n");
                }
                else res.addComplain("Expected operator after STRING\n");
            }  // if STRING

            else
            {
                res.addComplain(
                    "Unexpected token: '{}' with the type of <{}>",
                    get().name, (char)get().type
                );
            }

            advance();
        }

        return res;
    }
};


