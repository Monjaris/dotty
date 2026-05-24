#include "profile.hpp"
#include "cfman.hpp"

Profile::Profile(
    const std::string& name, const std::string& github_name,
    const std::string& repo_name
) : name(name), github_name(github_name), repo_name(repo_name)
{
    ;
}

std::string Profile::repoUrl() const {
    std::string url = "https://github.com/" + github_name + "/" + repo_name;
    return url;
}

fs::path Profile::getConfigPath() const {
    return  dotty.config_d/name/dotty.config_source_name;
}

