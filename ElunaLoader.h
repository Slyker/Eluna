/*
* Copyright (C) 2010 - 2024 Eluna Lua Engine <https://elunaluaengine.github.io/>
* Copyright (C) 2022 - 2022 Hour of Twilight <https://www.houroftwilight.net/>
* This program is free software licensed under GPL version 3
* Please see the included DOCS/LICENSE.md for more information
*/

#ifndef _ELUNALOADER_H
#define _ELUNALOADER_H

#include "LuaEngine.h"

#if defined ELUNA_TRINITY
#include <efsw/efsw.hpp>
#endif

extern "C"
{
#include "lua.h"
};

enum ElunaReloadActions
{
    RELOAD_CACHE_ONLY   = -3,
    RELOAD_ALL_STATES   = -2,
    RELOAD_GLOBAL_STATE = -1
};

enum ElunaScriptCacheState
{
    SCRIPT_CACHE_NONE = 0,
    SCRIPT_CACHE_REINIT = 1,
    SCRIPT_CACHE_LOADING = 2,
    SCRIPT_CACHE_READY = 3
};

struct LuaScript;

// Represents a parsed manifest.lua entry (module name -> ordered list of relative file paths)
struct LuaManifestModule
{
    std::string name;               // e.g. "rewards"
    std::vector<std::string> files; // relative paths within the module, e.g. {"db", "core"}
};

// Top-level manifest descriptor parsed from lua_scripts/manifest.lua
struct LuaManifest
{
    // Ordered list of modules (from the `modules` key)
    std::vector<std::string> modules;
    // Ordered list of bare files at root level (from the `files` key, optional)
    std::vector<std::string> files;
};

class ElunaLoader
{
private:
    ElunaLoader();
    ~ElunaLoader();

public:
    ElunaLoader(ElunaLoader const&) = delete;
    ElunaLoader(ElunaLoader&&) = delete;

    ElunaLoader& operator= (ElunaLoader const&) = delete;
    ElunaLoader& operator= (ElunaLoader&&) = delete;
    static ElunaLoader* instance();

    void LoadScripts();
    void ReloadElunaForMap(int mapId);

    uint8 GetCacheState() const { return m_cacheState; }
    const std::vector<LuaScript>& GetLuaScripts() const { return m_scriptCache; }
    const std::string& GetRequirePath() const { return m_requirePath; }
    const std::string& GetRequireCPath() const { return m_requirecPath; }

#if defined ELUNA_TRINITY
    // efsw file watcher
    void InitializeFileWatcher();
    efsw::FileWatcher lua_fileWatcher;
    efsw::WatchID lua_scriptWatcher;
#endif

private:
    void ReloadScriptCache();

    // --- Manifest-based loading (new) ---
    // Returns true if a manifest.lua exists at the root of the script folder.
    bool HasRootManifest(const std::string& rootPath) const;
    // Parse the root manifest.lua. Returns false on parse error.
    bool ParseRootManifest(lua_State* L, const std::string& rootPath, LuaManifest& out) const;
    // Parse a module-level manifest.lua. Returns false on parse error.
    bool ParseModuleManifest(lua_State* L, const std::string& modulePath, LuaManifestModule& out) const;
    // Load scripts declared in the root manifest (and each module's manifest).
    void LoadFromManifest(lua_State* L, const std::string& rootPath, const LuaManifest& manifest);
    // Load a single module: reads its manifest.lua if present, else falls back to sorted scan.
    void LoadModule(lua_State* L, const std::string& rootPath, const std::string& moduleName);
    // Load a single file by its full key (e.g. "rewards/core"), resolving extensions automatically.
    void LoadManifestFile(lua_State* L, const std::string& rootPath, const std::string& fileKey, int32 mapId = -1);

    // --- Legacy scan-based loading (unchanged, used as fallback) ---
    void ReadFiles(lua_State* L, std::string path);
    void CombineLists();
    void ProcessScript(lua_State* L, std::string filename, const size_t& filesize, const std::string& fullpath, int32 mapId);
    bool CompileScript(lua_State* L, LuaScript& script);
    static int LoadBytecodeChunk(lua_State* L, uint8* bytes, size_t len, BytecodeBuffer* buffer);

    // Whether the last load was manifest-driven (for logging)
    bool m_usedManifest = false;

    std::atomic<uint8> m_cacheState;
    std::vector<LuaScript> m_scriptCache;
    std::string m_requirePath;
    std::string m_requirecPath;
    std::list<LuaScript> m_scripts;
    std::list<LuaScript> m_extensions;
    std::thread m_reloadThread;
};

#if defined ELUNA_TRINITY
/// File watcher responsible for watching lua scripts
class ElunaUpdateListener : public efsw::FileWatchListener
{
public:
    ElunaUpdateListener() { }
    virtual ~ElunaUpdateListener() { }

    void handleFileAction(efsw::WatchID /*watchid*/, std::string const& dir,
        std::string const& filename, efsw::Action /*action*/, std::string oldFilename = "") final override;
};

static ElunaUpdateListener elunaUpdateListener;
#endif

#define sElunaLoader ElunaLoader::instance()

#endif
