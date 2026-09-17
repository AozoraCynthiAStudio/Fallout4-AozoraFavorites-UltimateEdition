set_xmakever("3.0.0")

set_project("AozoraFavoritesNativeSkyuiLike")
set_version("0.1.0")
set_arch("x64")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.releasedbg")

local commonlib_path = os.getenv("COMMONLIBF4_PATH")
if commonlib_path == nil or commonlib_path == "" then
    commonlib_path = "Y:/Games/MODCreation/Workspace/_upstream/commonlibf4-dear-latest"
end

includes(commonlib_path)

target("AozoraFavoritesSWF", function()
    add_rules("commonlibf4.plugin", {
        name = "AozoraFavoritesSWF",
        author = "Aozora",
        plugin_template = "commonlibf4-plugin.cpp.in"
    })

    add_files("src/**.cpp")
    add_includedirs("include")
end)
