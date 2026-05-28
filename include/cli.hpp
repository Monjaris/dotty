#pragma once
#include "cfman.hpp"

struct CmdLine
{
    struct Impl;
    Impl* impl = nullptr;

    CmdLine (int argc, char** argv);
    ~CmdLine ();

    // arg-fmt: "name,repo" or "name,repo,url"
    int32 do_list(const strview options);
    // arg-fmt: "master" or "master,storage"
    int32 do_clean(const strview options);
    int32 do_init();
    int32 do_update();
    int32 do_push(const char* commit_message);
    int32 do_install();
    int32 do_delete(strview options);
    int32 do_profile(strview options);
    int32 do_config(strview options);

    CLI::App* newSubCmd(
        const char* name, const std::function<int32()>& fn, const char* desc,
        int32 require_subcommands=0
    );

    int32 setup();
    int32 run();
};
