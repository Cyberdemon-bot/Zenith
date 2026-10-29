add_rules("mode.debug", "mode.release")

target("ZenithCore")
    set_kind("static")
    set_languages("cxx20")

    if is_mode("debug") then
        add_defines("ZENITH_DEBUG", {public = true})
        add_defines("ZN_ENABLE_ASSERTS", {public = true})
        set_symbols("debug")
    end

    if is_mode("release") then
        set_optimize("fastest")   
    end

    add_files("src/Zenith/**.cpp")
    add_includedirs("src", {public = true})

target("main")
    set_kind("binary")
    set_languages("cxx20")
    add_deps("ZenithCore")
    set_rundir("$(projectdir)")
    add_files("src/main.cpp")
    add_includedirs("src")

target("test")
    set_kind("binary")
    set_languages("cxx20")
    add_deps("ZenithCore")
    set_rundir("$(projectdir)")
    add_files("tests/test.cpp")
    add_includedirs("src")