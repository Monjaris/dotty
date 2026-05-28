set_plat("linux")
set_arch("x86_64")
set_toolchains("gcc")

add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

-- DEBUG/RELEASE modes defined by cli
if is_mode("release") then
    add_defines("DEBUG_ON=0")
end

if is_mode("debug") then
    set_strip("all")
end


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
    set_pcxxheader("include/common.hpp")
