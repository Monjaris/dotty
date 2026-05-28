#include "profile.hpp"

Profile::Profile(
    const std::string& name, const std::string& github_name,
    const std::string& repo_name, bool is_external
): name(name), github_name(github_name), repo_name(repo_name), is_external(is_external)
{
    ;
}

std::string Profile::repoUrl() const {
    return cm::make_repo_url(github_name, repo_name);
}

// fs::path Profile::getConfigPath() const {
    // return  dotty.config_d/name/dotty.config_source;
// }

