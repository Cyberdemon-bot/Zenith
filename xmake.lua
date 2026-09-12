add_rules("mode.debug", "mode.release")

target("Zenith")
    set_kind("binary")
    set_languages("cxx20")
    add_files("src/**.cpp")
    add_includedirs("src", {public = true})