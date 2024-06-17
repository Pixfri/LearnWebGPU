set_xmakever("2.8.1")

set_project("LearnWebGPU")
set_version("1.0.0")

set_allowedplats("windows")
set_allowedarchs("windows|x64")

add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")
set_languages("cxx20")
set_optimize("fastest")

add_requires("glfw", "wgpu-native", "glfw3webgpu")

local outputdir = "$(mode)-$(os)-$(arch)"

target("LearnWebGPU")
    set_kind("binary")

    set_targetdir("build/" .. outputdir .. "/LearnWebGPU/bin")
    set_objectdir("build/" .. outputdir .. "/LearnWebGPU/obj")

    -- C++ source files
    add_files("LearnWebGPU/Source/**.cpp")

    -- C++ header files
    add_includedirs("LearnWebGPU/Include", {public = true})
    add_headerfiles("LearnWebGPU/Include/**.hpp")

    add_packages("glfw", "wgpu-native", "glfw3webgpu")