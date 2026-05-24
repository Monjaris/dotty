#pragma once
#include "core.h"

struct SrcDest { fs::path src, path; };

struct Profile {
    std::string name;
    std::string github_name;
    std::string repo_name;

    Profile(
        const std::string& name, const std::string& github_name,
        const std::string& repo_url
    );

    std::string repoUrl() const;
    fs::path getConfigPath() const;

};
