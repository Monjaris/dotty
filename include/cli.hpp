#pragma once
#include "cfman.hpp"

struct CmdLine
{
    struct Impl;
    Impl* impl = nullptr;

    CmdLine (int argc, char** argv);
    ~CmdLine ();

    // arg-fmt: "master" or "master,storage"
    // int32 do_clean(const strview options);
    int32 do_init();
    int32 do_update();
    int32 do_push(const char* commit_message);
    int32 do_pull();
    int32 do_config(strview options);
    int32 do_profile_(strview options);
        int32 do_p_list(const strview options);
        int32 do_p_new(const std::string& name, const std::string& repo_name, const std::string& visibility);
        int32 do_p_delete(strview options);

    CLI::App* newSubCmd(
        CLI::App* parent, const char* name, const std::function<int32()>& fn, const char* desc,
        int32 require_subcommands=0
    );

    int32 setup();
    int32 run();
};
