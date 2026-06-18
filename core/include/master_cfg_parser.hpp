#pragma once
#include "config_parser.hpp"


// methods with 'r*' are config readers,
// whereas 'w*' ones are config writerss.
struct MasterConfigParser {
    toml::table m_table;
    std::map<std::string, std::string> vars;
    std::vector<Profile> profiles;

    // Property symbol table
    static COMPTIME_STR P_ACTIVE_PROF   = "active-profile";
    static COMPTIME_STR P_PROFILES  = "profile";
        static COMPTIME_STR PP_NAME     = "name";  // str
        static COMPTIME_STR PP_REPO_URL = "url";  // str
        static COMPTIME_STR PP_REPO_PUB = "public";  // bool
        static COMPTIME_STR PP_EXTERNAL = "external";  // bool


    Report rParse(const fs::path& path)
    {
        try {
            m_table = toml::parse_file(path.string());
        } catch (const toml::parse_error& e) {
            return Report::Bad("Master config parse-error: {}", e.description());
        }
        return Report::Good();
    }

    Report rEval()
    {
        Report rep;

        vars[P_ACTIVE_PROF] = m_table[P_ACTIVE_PROF].value_or(Profile::NOT);

        auto* arr_profiles = m_table[P_PROFILES].as_array();
        if (!arr_profiles) return rep.Bad("No profiles configuired!");

        for (auto& node_profile  : *arr_profiles) {
            auto* tbl_profile = node_profile.as_table();
            if (tbl_profile->empty()) {
                rep.addComplain("Profile table is empty! ignoring..");
                continue;
            }

            std::string name = tbl_profile->at(PP_NAME).value_or("");
            std::string url  = tbl_profile->at(PP_REPO_URL).value_or("");
            tern is_public   = tbl_profile->at(PP_REPO_PUB).value_or(tern::neutr);
            bool is_extern   = tbl_profile->at(PP_EXTERNAL).value_or(false);

            // `is_public.boolable()` checks for if `is_public` is not `neutr`
            if (name.empty() || url.empty() || !is_public.boolable()) {
                rep.addComplain("Profile have missing properties, skipping..");
                continue;
            }

            profiles.push_back(Profile{name, url, is_public, is_extern});
        }

        return rep;
    }

    Report rValidateConfig() {
        Report report;
        for (auto& prof_1  : profiles) {
            for (auto& prof_2  : profiles) {
                if (&prof_1 == &prof_2) continue;  // skip same object
                if (prof_1.name == prof_2.name) {  // check if they have same name
                    report.addComplain("Duplicate-profile: {}", prof_1.name);
                }
            }
        }
        return report? report.Bad("\nBad config!") : Report::Good();
    }

    Report wAddProfile(const Profile& prof) {
        toml::table entry;
        entry.insert(PP_NAME, prof.name);
        entry.insert(PP_REPO_URL, prof.repo_url);
        entry.insert(PP_REPO_PUB, prof.is_pub);
        entry.insert(PP_EXTERNAL, prof.is_ext);

        // Inert profiles array if it doesn't exist yet
        if (!m_table[P_PROFILES].as_array()) {
            m_table.insert(P_PROFILES, toml::array{});
        }
        // push new profile entry to profiles array
        m_table[P_PROFILES].as_array()->push_back(entry);

        return Report::Good();
    }

    Report wRemoveProfile(const strview name) {
        auto* arr_profiles = m_table[P_PROFILES].as_array();

        for (uint32 i=0;  i < arr_profiles->size();  ++i) {
            if (arr_profiles->at(i).at_path(PP_NAME) == name) {
                arr_profiles->erase(arr_profiles->begin() + i);
            }
        }

        return Report::Good();
    }

    Report wActivateProfile(const strview name) {
        m_table.insert_or_assign(P_ACTIVE_PROF, name);
        return Report::Good();
    }

    Report wSaveConfig(const fs::path& path) {
        std::ofstream fo(path, std::ios::out);
        if (!fo) return Report::Bad("Couldn't open output file: '{}'", path.string());
        fo << m_table;
        return Report::Good();
    }
};

