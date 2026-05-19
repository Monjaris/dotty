#include "profile.hpp"
#include "cfman.hpp"

Profile::Profile(
    const std::string& name, const std::string& github_account,
    const std::string& repo_url
) : name(name), github_account(github_account), repo_url(repo_url)
{
    dotty.profiles.push_back(*this);
}

fs::path Profile::get_dir() const {
    return dotty.config_d / name;
}

fs::path Profile::get_config_path() const {
    return get_dir() / dotty.config_source_name;
}
