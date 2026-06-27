--[[
    libs/manifest.lua — Manifest du module "libs"
    Déclare l'ordre de chargement des fichiers internes au module.

    Les chemins sont relatifs AU DOSSIER DU MODULE (libs/).
    Sous-dossiers supportés : "helpers/utils" -> libs/helpers/utils.lua

    Si ce fichier est absent, Eluna scanne et trie libs/ alphabétiquement.
]]

return {
    files = {
        "config",         -- libs/config.lua  (chargé en premier, pas de dépendances)
        "utils",          -- libs/utils.lua   (peut dépendre de config)
        "helpers/format", -- libs/helpers/format.lua (sous-dossier OK)
    },
}
