#pragma once
#include "core.h"

struct Profile {
    std::string name;
    std::string github_account;
    std::string repo_url;

    Profile(
        const std::string& name, const std::string& github_account,
        const std::string& repo_url
    );

    fs::path get_dir() const;
    fs::path get_config_path() const;
};
