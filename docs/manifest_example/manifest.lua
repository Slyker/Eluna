--[[
    manifest.lua — Eluna Manifest Loader
    Placé à la racine de lua_scripts/, ce fichier contrôle
    l'ordre de chargement de façon déclarative et déterministe.

    Rétrocompatibilité :
      - Si ce fichier est absent, Eluna utilise le scan legacy (tri alphabétique).
      - Un module sans son propre manifest.lua est scanné+trié automatiquement.
]]

return {
    -- Ordre de chargement des modules.
    -- Chaque entrée correspond à un sous-dossier de lua_scripts/.
    -- Les dépendances doivent être déclarées AVANT les modules qui en dépendent.
    modules = {
        "libs",      -- utilitaires partagés, chargés en premier
        "core",      -- logique métier centrale
        "rewards",   -- dépend de libs et core
        "events",    -- dépend de rewards
    },

    -- Fichiers standalone à la racine de lua_scripts/ (optionnel).
    -- Chargés AVANT les modules, dans l'ordre déclaré.
    -- files = {
    --     "global_config",
    -- },
}
