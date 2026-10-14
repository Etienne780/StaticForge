project "StaticForgeTests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"

    SetTargetAndObjDirs("%{prj.name}")

    files {
        "src/**.cpp",
        "include/**.h",
        "main.cpp"
    }

    -- the tests also use the internal headers of the core
    includedirs {
        "include",
        "../StaticForgeCore/include",
        "../StaticForgeCore/include/StaticForgeCore",
    }

    links {
        "StaticForgeCore",
    }

    ApplyCommonConfigs()

    -- the test sources contain UTF-8 file names
    filter "action:vs*"
        buildoptions { "/utf-8" }

    filter "system:linux"
        links { "pthread" }

    filter {}
