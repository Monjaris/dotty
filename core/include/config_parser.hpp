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
    void step(uint32 n=1) { while(checks() && n--) { ++pos; } }
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
        if (dest->ends_with("/..")) { dest->replace(dest->size()-2, 2, fs::path(*src).filename()); }
        else if (*dest == "..") { *dest = fs::path(*src).filename(); }
    }

    Report parseMain()
    {
        idx = 0;
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


