set_plat("linux")
set_arch("x86_64")
set_toolchains("gcc")

add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

add_rules("plugin.compile_commands.autoupdate")
set_policy("build.ccache", true)
-- add_rules("c++.unity_build")


add_requires("cli11", {system = true})
add_requires("readline", {system = true})

target("dotty")
    add_cxflags("-Wno-ignored-attributes")
    set_languages("c++23")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_packages("cli11")
    add_packages("readline")
    set_pcheader("src/common.h")
