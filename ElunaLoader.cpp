/*
* Copyright (C) 2010 - 2024 Eluna Lua Engine <https://elunaluaengine.github.io/>
* Copyright (C) 2022 - 2022 Hour of Twilight <https://www.houroftwilight.net/>
* This program is free software licensed under GPL version 3
* Please see the included DOCS/LICENSE.md for more information
*/

#include "ElunaCompat.h"
#include "ElunaConfig.h"
#include "ElunaLoader.h"
#include "ElunaUtility.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <charconv>

#if defined USING_BOOST
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#if defined ELUNA_WINDOWS
#include <Windows.h>
#endif

#if defined ELUNA_TRINITY || ELUNA_MANGOS
#include "MapManager.h"
#elif defined ELUNA_CMANGOS
#include "Maps/MapManager.h"
#endif

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#if defined ELUNA_TRINITY
void ElunaUpdateListener::handleFileAction(efsw::WatchID /*watchid*/, std::string const& dir, std::string const& filename, efsw::Action /*action*/, std::string /*oldFilename*/)
{
    auto const path = fs::absolute(filename, dir);
    if (!path.has_extension())
        return;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext != ".lua" && ext != ".ext")
        return;

    sElunaLoader->ReloadElunaForMap(RELOAD_ALL_STATES);
}
#endif

ElunaLoader::ElunaLoader() : m_cacheState(SCRIPT_CACHE_NONE)
{
#if defined ELUNA_TRINITY
    lua_scriptWatcher = -1;
#endif
}

ElunaLoader* ElunaLoader::instance()
{
    static ElunaLoader instance;
    return &instance;
}

ElunaLoader::~ElunaLoader()
{
    if (m_reloadThread.joinable())
        m_reloadThread.join();

#if defined ELUNA_TRINITY
    if (lua_scriptWatcher >= 0)
    {
        lua_fileWatcher.removeWatch(lua_scriptWatcher);
        lua_scriptWatcher = -1;
    }
#endif
}

void ElunaLoader::ReloadScriptCache()
{
    if (m_cacheState != SCRIPT_CACHE_READY)
    {
        ELUNA_LOG_DEBUG("[Eluna]: Script cache not ready, skipping reload");
        return;
    }

    if (m_reloadThread.joinable())
        m_reloadThread.join();

    m_cacheState = SCRIPT_CACHE_REINIT;
    m_reloadThread = std::thread(&ElunaLoader::LoadScripts, this);
    ELUNA_LOG_DEBUG("[Eluna]: Script cache reload thread started");
}

// ---------------------------------------------------------------------------
// Manifest helpers
// ---------------------------------------------------------------------------

bool ElunaLoader::HasRootManifest(const std::string& rootPath) const
{
    fs::path manifestPath = fs::path(rootPath) / "manifest.lua";
    return fs::exists(manifestPath) && fs::is_regular_file(manifestPath);
}

// Parse the root manifest.lua.
// Expected Lua format:
//   return {
//       modules = { "libs", "rewards", "events" },  -- optional
//       files   = { "standalone_script" },           -- optional, root-level files
//   }
bool ElunaLoader::ParseRootManifest(lua_State* L, const std::string& rootPath, LuaManifest& out) const
{
    std::string manifestPath = (fs::path(rootPath) / "manifest.lua").generic_string();

    if (luaL_loadfile(L, manifestPath.c_str()) != 0 || lua_pcall(L, 0, 1, 0) != 0)
    {
        ELUNA_LOG_ERROR("[Eluna]: Failed to parse root manifest.lua: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (!lua_istable(L, -1))
    {
        ELUNA_LOG_ERROR("[Eluna]: manifest.lua must return a table");
        lua_pop(L, 1);
        return false;
    }

    // Read `modules` array
    lua_getfield(L, -1, "modules");
    if (lua_istable(L, -1))
    {
        lua_Integer len = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= len; ++i)
        {
            lua_rawgeti(L, -1, i);
            if (lua_isstring(L, -1))
                out.modules.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1); // pop modules

    // Read `files` array (optional root-level files)
    lua_getfield(L, -1, "files");
    if (lua_istable(L, -1))
    {
        lua_Integer len = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= len; ++i)
        {
            lua_rawgeti(L, -1, i);
            if (lua_isstring(L, -1))
                out.files.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1); // pop files

    lua_pop(L, 1); // pop the returned table
    return true;
}

// Parse a module-level manifest.lua.
// Expected Lua format:
//   return {
//       files = { "db", "core", "sub/helper" },
//   }
bool ElunaLoader::ParseModuleManifest(lua_State* L, const std::string& modulePath, LuaManifestModule& out) const
{
    std::string manifestPath = (fs::path(modulePath) / "manifest.lua").generic_string();

    if (luaL_loadfile(L, manifestPath.c_str()) != 0 || lua_pcall(L, 0, 1, 0) != 0)
    {
        ELUNA_LOG_ERROR("[Eluna]: Failed to parse module manifest.lua at `%s`: %s",
            manifestPath.c_str(), lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (!lua_istable(L, -1))
    {
        ELUNA_LOG_ERROR("[Eluna]: Module manifest.lua at `%s` must return a table", manifestPath.c_str());
        lua_pop(L, 1);
        return false;
    }

    lua_getfield(L, -1, "files");
    if (lua_istable(L, -1))
    {
        lua_Integer len = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= len; ++i)
        {
            lua_rawgeti(L, -1, i);
            if (lua_isstring(L, -1))
                out.files.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1); // pop files
    lua_pop(L, 1); // pop the returned table
    return true;
}

// Load a single file identified by its full key relative to rootPath.
// fileKey examples: "libs/utils", "rewards/sub/helper"
// Tries .ext first (so extensions load before regular scripts), then .lua, then .moon.
void ElunaLoader::LoadManifestFile(lua_State* L, const std::string& rootPath, const std::string& fileKey, int32 mapId)
{
    static const std::vector<std::string> exts = { ".ext", ".lua", ".moon" };

    for (const auto& ext : exts)
    {
        fs::path fullPath = fs::path(rootPath) / (fileKey + ext);
        if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath))
            continue;

        std::string fullPathStr  = fullPath.generic_string();
        std::string filename     = fullPath.stem().generic_string(); // no extension
        size_t      filesize     = fs::file_size(fullPath);

        ProcessScript(L, filename + ext, filesize, fullPathStr, mapId);
        return; // stop at first match
    }

    ELUNA_LOG_ERROR("[Eluna]: Manifest references unknown file `%s` (no .ext/.lua/.moon found)", fileKey.c_str());
}

// Load a module:
//   - If module has its own manifest.lua -> load files in declared order
//   - Otherwise -> fallback: scan the module directory and sort alphabetically
void ElunaLoader::LoadModule(lua_State* L, const std::string& rootPath, const std::string& moduleName)
{
    fs::path modulePath = fs::path(rootPath) / moduleName;

    if (!fs::exists(modulePath) || !fs::is_directory(modulePath))
    {
        ELUNA_LOG_ERROR("[Eluna]: Manifest references unknown module `%s`", moduleName.c_str());
        return;
    }

    // Register require path for this module directory
    std::string modulePathStr = modulePath.generic_string();
    m_requirePath +=
        modulePathStr + "/?.lua;" +
        modulePathStr + "/?.ext;" +
        modulePathStr + "/?.moon;";
    m_requirecPath +=
        modulePathStr + "/?.dll;" +
        modulePathStr + "/?.so;";

    fs::path moduleManifest = modulePath / "manifest.lua";
    if (fs::exists(moduleManifest) && fs::is_regular_file(moduleManifest))
    {
        // Module has its own manifest -> load in declared order
        LuaManifestModule modManifest;
        modManifest.name = moduleName;
        if (!ParseModuleManifest(L, modulePathStr, modManifest))
            return;

        ELUNA_LOG_INFO("[Eluna]: Loading module `%s` (%zu files, manifest)",
            moduleName.c_str(), modManifest.files.size());

        for (const auto& relFile : modManifest.files)
        {
            // Full key = moduleName + "/" + relFile  (e.g. "rewards/sub/helper")
            std::string fileKey = moduleName + "/" + relFile;
            LoadManifestFile(L, rootPath, fileKey);
        }
    }
    else
    {
        // No module manifest -> fallback: scan + sort
        ELUNA_LOG_INFO("[Eluna]: Loading module `%s` (no manifest, fallback scan)", moduleName.c_str());
        ReadFiles(L, modulePathStr);
    }
}

// Top-level manifest loader.
// Loads root-level `files` first (in order), then each `modules` entry (in order).
void ElunaLoader::LoadFromManifest(lua_State* L, const std::string& rootPath, const LuaManifest& manifest)
{
    // Register require path for the root directory itself
    m_requirePath +=
        rootPath + "/?.lua;" +
        rootPath + "/?.ext;" +
        rootPath + "/?.moon;";
    m_requirecPath +=
        rootPath + "/?.dll;" +
        rootPath + "/?.so;";

    // 1. Root-level standalone files (extensions first, then scripts — same convention as legacy)
    for (const auto& fileKey : manifest.files)
        LoadManifestFile(L, rootPath, fileKey);

    // 2. Modules in declared order
    for (const auto& moduleName : manifest.modules)
        LoadModule(L, rootPath, moduleName);
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

void ElunaLoader::LoadScripts()
{
    if (m_cacheState != SCRIPT_CACHE_REINIT && m_cacheState != SCRIPT_CACHE_NONE)
        return;

    m_cacheState = SCRIPT_CACHE_LOADING;

    uint32 oldMSTime = ElunaUtil::GetCurrTime();

    std::string lua_folderpath = sElunaConfig->GetConfig(CONFIG_ELUNA_SCRIPT_PATH);
    const std::string& lua_path_extra  = sElunaConfig->GetConfig(CONFIG_ELUNA_REQUIRE_PATH_EXTRA);
    const std::string& lua_cpath_extra = sElunaConfig->GetConfig(CONFIG_ELUNA_REQUIRE_CPATH_EXTRA);

#if !defined ELUNA_WINDOWS
    if (lua_folderpath[0] == '~')
        if (const char* home = getenv("HOME"))
            lua_folderpath.replace(0, 1, home);
#endif

    ELUNA_LOG_INFO("[Eluna]: Searching for scripts in `%s`", lua_folderpath.c_str());

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    m_requirePath.clear();
    m_requirecPath.clear();
    m_usedManifest = false;

    if (HasRootManifest(lua_folderpath))
    {
        // --- Manifest mode ---
        LuaManifest manifest;
        if (ParseRootManifest(L, lua_folderpath, manifest))
        {
            ELUNA_LOG_INFO("[Eluna]: manifest.lua found — loading %zu module(s) in declared order",
                manifest.modules.size());
            LoadFromManifest(L, lua_folderpath, manifest);
            // In manifest mode we skip CombineLists() — scripts are already in insertion order.
            // Extensions were handled inline by LoadManifestFile() which tries .ext first.
            m_scriptCache.clear();
            m_scriptCache.reserve(m_extensions.size() + m_scripts.size());
            std::move(m_extensions.begin(), m_extensions.end(), std::back_inserter(m_scriptCache));
            std::move(m_scripts.begin(), m_scripts.end(), std::back_inserter(m_scriptCache));
            m_extensions.clear();
            m_scripts.clear();
            m_usedManifest = true;
        }
        else
        {
            ELUNA_LOG_ERROR("[Eluna]: manifest.lua parse error — falling back to directory scan");
            ReadFiles(L, lua_folderpath);
            CombineLists();
        }
    }
    else
    {
        // --- Legacy mode (unchanged behaviour) ---
        ELUNA_LOG_INFO("[Eluna]: No manifest.lua found — using legacy directory scan");
        ReadFiles(L, lua_folderpath);
        CombineLists();
    }

    lua_close(L);

    if (!lua_path_extra.empty())
        m_requirePath += lua_path_extra;
    if (!lua_cpath_extra.empty())
        m_requirecPath += lua_cpath_extra;

    if (!m_requirePath.empty())
        m_requirePath.erase(m_requirePath.end() - 1);
    if (!m_requirecPath.empty())
        m_requirecPath.erase(m_requirecPath.end() - 1);

    ELUNA_LOG_INFO("[Eluna]: Loaded and precompiled %u scripts in %u ms (mode: %s)",
        uint32(m_scriptCache.size()),
        ElunaUtil::GetTimeDiff(oldMSTime),
        m_usedManifest ? "manifest" : "legacy scan");

    m_cacheState = SCRIPT_CACHE_READY;
}

// ---------------------------------------------------------------------------
// Unchanged legacy methods
// ---------------------------------------------------------------------------

int ElunaLoader::LoadBytecodeChunk(lua_State* /*L*/, uint8* bytes, size_t len, BytecodeBuffer* buffer)
{
    buffer->insert(buffer->end(), bytes, bytes + len);
    return 0;
}

void ElunaLoader::ReadFiles(lua_State* L, std::string path)
{
    std::string lua_folderpath = sElunaConfig->GetConfig(CONFIG_ELUNA_SCRIPT_PATH);

    ELUNA_LOG_DEBUG("[Eluna]: ReadFiles from path `%s`", path.c_str());

    fs::path someDir(path);
    fs::directory_iterator end_iter;

    if (fs::exists(someDir) && fs::is_directory(someDir) && !fs::is_empty(someDir))
    {
        m_requirePath +=
            path + "/?.lua;" +
            path + "/?.ext;" +
            path + "/?.moon;";

        m_requirecPath +=
            path + "/?.dll;" +
            path + "/?.so;";

        for (fs::directory_iterator dir_iter(someDir); dir_iter != end_iter; ++dir_iter)
        {
            std::string fullpath = dir_iter->path().generic_string();
#if defined ELUNA_WINDOWS
            DWORD dwAttrib = GetFileAttributes(fullpath.c_str());
            if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_HIDDEN))
                continue;
#else
            std::string name = dir_iter->path().filename().generic_string().c_str();
            if (name[0] == '.')
                continue;
#endif

            if (fs::is_directory(dir_iter->status()))
            {
                ReadFiles(L, fullpath);
                continue;
            }

            if (fs::is_regular_file(dir_iter->status()))
            {
                int32 mapId = -1;

                std::string subfolder = dir_iter->path().generic_string();
                subfolder = subfolder.erase(0, lua_folderpath.size() + 1);

                auto [ptr, ec] = std::from_chars(subfolder.data(), subfolder.data() + subfolder.size(), mapId);

                if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range || mapId < -1)
                    mapId = -1;

                std::string filename = dir_iter->path().filename().generic_string();
                size_t filesize = fs::file_size(dir_iter->path());
                ProcessScript(L, filename, filesize, fullpath, mapId);
            }
        }
    }
}

bool ElunaLoader::CompileScript(lua_State* L, LuaScript& script)
{
    int err = 0;
    if (script.fileext == ".moon")
    {
        std::string str = "return require('moonscript').loadfile([[" + script.filepath + "]])";
        err = luaL_dostring(L, str.c_str());
    } else
        err = luaL_loadfile(L, script.filepath.c_str());

    if (err != 0)
    {
        ELUNA_LOG_ERROR("[Eluna]: CompileScript failed to load the Lua script `%s`.", script.filename.c_str());
        Eluna::Report(L);
        return false;
    }
    ELUNA_LOG_DEBUG("[Eluna]: CompileScript loaded Lua script `%s`", script.filename.c_str());

    err = lua_dump(L, (lua_Writer)LoadBytecodeChunk, &script.bytecode);
    if (err || script.bytecode.empty())
    {
        ELUNA_LOG_ERROR("[Eluna]: CompileScript failed to dump the Lua script `%s` to bytecode.", script.filename.c_str());
        Eluna::Report(L);
        return false;
    }
    ELUNA_LOG_DEBUG("[Eluna]: CompileScript dumped Lua script `%s` to bytecode.", script.filename.c_str());

    lua_pop(L, 1);
    return true;
}

void ElunaLoader::ProcessScript(lua_State* L, std::string filename, const size_t& filesize, const std::string& fullpath, int32 mapId)
{
    ELUNA_LOG_DEBUG("[Eluna]: ProcessScript checking file `%s`", fullpath.c_str());

    std::size_t extDot = filename.find_last_of('.');
    if (extDot == std::string::npos)
        return;
    std::string ext = filename.substr(extDot);
    filename = filename.substr(0, extDot);

    if (ext != ".lua" && ext != ".ext" && ext != ".moon")
        return;
    bool extension = ext == ".ext";

    LuaScript script;
    script.fileext = ext;
    script.filename = filename;
    script.filepath = fullpath;
    script.modulepath = fullpath.substr(0, fullpath.length() - filename.length() - ext.length());
    script.bytecode.reserve(filesize);
    script.mapId = mapId;

    if (!CompileScript(L, script))
        return;

    if (extension)
        m_extensions.push_back(script);
    else
        m_scripts.push_back(script);

    ELUNA_LOG_DEBUG("[Eluna]: ProcessScript processed `%s` successfully", fullpath.c_str());
}

static bool ScriptPathComparator(const LuaScript& first, const LuaScript& second)
{
    return first.filepath < second.filepath;
}

void ElunaLoader::CombineLists()
{
    m_extensions.sort(ScriptPathComparator);
    m_scripts.sort(ScriptPathComparator);

    m_scriptCache.clear();
    m_scriptCache.reserve(m_extensions.size() + m_scripts.size());

    std::move(m_extensions.begin(), m_extensions.end(), std::back_inserter(m_scriptCache));
    std::move(m_scripts.begin(), m_scripts.end(), std::back_inserter(m_scriptCache));

    m_extensions.clear();
    m_scripts.clear();
}

#if defined ELUNA_TRINITY
void ElunaLoader::InitializeFileWatcher()
{
    std::string lua_folderpath = sElunaConfig->GetConfig(CONFIG_ELUNA_SCRIPT_PATH);

    lua_scriptWatcher = lua_fileWatcher.addWatch(lua_folderpath, &elunaUpdateListener, true);
    if (lua_scriptWatcher >= 0)
        ELUNA_LOG_INFO("[Eluna]: Script reloader is listening on `%s`.", lua_folderpath.c_str());
    else
        ELUNA_LOG_INFO("[Eluna]: Failed to initialize the script reloader on `%s`.", lua_folderpath.c_str());

    lua_fileWatcher.watch();
}
#endif

void ElunaLoader::ReloadElunaForMap(int mapId)
{
    ReloadScriptCache();

    if (mapId != RELOAD_CACHE_ONLY)
    {
        if (mapId == RELOAD_GLOBAL_STATE || mapId == RELOAD_ALL_STATES)
#if defined ELUNA_TRINITY || defined ELUNA_AZEROTHCORE
            if (Eluna* e = sWorld->GetEluna())
#else
            if (Eluna* e = sWorld.GetEluna())
#endif
                e->ReloadEluna();

#if defined ELUNA_TRINITY || defined ELUNA_AZEROTHCORE
        sMapMgr->DoForAllMaps([&](Map* map)
#else
        sMapMgr.DoForAllMaps([&](Map* map)
#endif
            {
                if (mapId == RELOAD_ALL_STATES || mapId == static_cast<int>(map->GetId()))
                    if (Eluna* e = map->GetEluna())
                        e->ReloadEluna();
            }
        );
    }
}
