add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate")
set_policy("build.ccache", true)
set_defaultmode("debug")
add_rules("c++.unity_build")


add_requires("cli11", {system = true})
add_requires("readline", {system = true})

target("dotty")
    set_languages("c++23")
    add_files("src/*.cpp")
    add_includedirs("src")
    add_packages("cli11")
    add_packages("readline")
    set_pcheader("src/common.h")
