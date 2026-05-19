#pragma once
#include <functional>
#include "cfman.hpp"

struct CmdLine
{
    struct Impl;
    Impl* impl = nullptr;

    CmdLine (int argc, char** argv);
    ~CmdLine ();

    int32 do_list();
    int32 do_init(const char* ini_profile);
    int32 do_write();
    int32 do_update(const char* commit_message);
    int32 do_install();

    void newSubCmd(const char* name, const std::function<int32()>& fn, const char* desc);

    int32 setup();
    int32 run();
};
