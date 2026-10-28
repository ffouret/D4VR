# États dynamiques de la reprise graphique

Le moteur reprend maintenant les états dynamiques standard du groupe principal
de VKD3D, avec déclarations de pipeline et contrôle de complétude. **Cette
adaptation est vérifiée sur une scène originale, sans raccordement à Diablo.**

## Audit des sources

Le runtime relevé annonce `vkd3d-proton 3.1.0`, base `0bb64c56c8f0dcc` avec
suffixe de modifications locales. L'audit utilise les fichiers upstream
`command.c` et `state.c` du commit complet
`0bb64c56c8f0dcc11ecc0f6c56e00e4f7bd8d506`. Les modifications locales ne sont
pas reconstituées. Cela ne prouve ni l'identité exacte avec le binaire ni les
commandes effectivement exécutées par une passe de Diablo. Les
[URLs, empreintes et résultats d'audit](proofs/vkd3d-dynamic-state-audit.json)
sont conservés sans recopier le code upstream.

La table principale contient quinze états ; douze sont acceptés. Viewport et
scissor classiques restent aussi disponibles pour les passes utilitaires.

| État de la table | Capture/reprise |
| --- | --- |
| Viewport avec compteur | Tableau copié, jusqu'à 16 viewports |
| Scissor avec compteur | Tableau copié, jusqu'à 16 rectangles |
| Constantes de mélange | Quatre floats copiés |
| Référence stencil | Faces avant/arrière suivies |
| Bornes de profondeur | Deux floats ; intervalle limité à [0, 1] |
| Topologie | Valeur enregistrée |
| Stride des vertex buffers | Buffers, offsets, tailles et strides copiés |
| Redémarrage de primitive | Booléen enregistré |
| Écriture de profondeur | Booléen enregistré |
| Masque d'écriture stencil | Faces suivies séparément |
| Biais de profondeur | Trois floats par la commande standard |
| Activation du biais | Booléen enregistré |
| Fragment shading rate | Refusé |
| Patch control points | Refusé |
| Rasterization samples | Refusé |

La variante conditionnelle `vkCmdSetDepthBias2EXT` reste également refusée.
L'adaptateur doit signaler ces commandes et toute commande non modélisée par
`unsupported()` ; la commande standard ne remplace pas sa variante étendue.

## Contrats

L'option explicite `extendedDynamic` charge les commandes Vulkan standard. Le
périphérique emprunté doit déjà satisfaire leurs prérequis. La fixture utilise
Vulkan 1.3 et active la feature de bounds de profondeur ; le composant ne change
aucune feature du périphérique du jeu.

Le contrat fournit la liste complète `dynamicStates` du pipeline. Le
[mode de déclaration explicite](STATIC_STATE.md) distingue une liste complète
vide d'un contrat inconnu ou du comportement historique. Les états
inconnus ou dupliqués sont refusés. Un dessin doit avoir les valeurs requises,
faces stencil et strides de chaque binding consommé. Lier un pipeline qui rend
un état statique invalide son suivi dynamique. Les modes viewport/scissor
classiques et avec compteur sont suivis distinctement, de façon conservatrice.

Les pipelines historiques du préfixe doivent eux aussi avoir des variantes
auditées, compatibles avec les formats, samples et contraintes des cibles
privées. Une liaison inconnue interdit la reprise même si le dernier pipeline
est reconnu. `vkCmdBindVertexBuffers2` distingue les tailles/strides absents et
présents ; un stride absent n'initialise pas un nouveau stride dynamique. Les
octets GPU restent empruntés et leurs dépendances doivent être auditées.

## Preuve GPU

Le mode `--dynamic-state` lit un vrai attribut de position. Les sommets sont
espacés de **16 octets** ; le pipeline dynamique porte une valeur statique
ignorée de 32 octets. Les références utilisent le shader original, un stride
statique de 16 octets et des descriptor sets traditionnels. Les tableaux CPU
temporaires sont modifiés après enregistrement.

L'écriture de profondeur est activée avant chaque dessin puis désactivée après
le dernier dessin du scope. Le dessin suivant sans rebind doit garder les
couleurs/stencil attendus, en laissant sa profondeur au clear. Sa référence a
une écriture de profondeur statique désactivée. Le triangle strip dynamique de
trois sommets est comparé au triangle list de référence.

Sur RTX 5090, les 16 vues finales correspondent aux références : aucune
différence sur 16 777 216 octets couleur et 1 048 576 valeurs de profondeur et
autant de stencil. Le dessin suivant et le flux original correspondent chacun
sur 10 485 760 octets définis. Le moteur restaure 272 commandes d'état ; le
calcul témoin retrouve ses 32 mots sans rebind. Les trois contrôles positifs
et les deux négatifs passent, dont Steam Runtime avec validation de
synchronisation. Les [mesures et empreintes](proofs/dynamic-replay-v1.json)
identifient les sources et le binaire.

Ces pixels vérifient notamment stride, compteurs et restauration de profondeur.
Le blending et l'activation du biais sont désactivés dans cette scène : leurs
valeurs sont vérifiées par le test sans pilote, sans preuve d'effet visible.
Le test C++ couvre aussi les tableaux temporaires, tailles/strides absents,
états incomplets, invalidation par pipeline statique et liaisons inconnues.
Ses assertions restent actives en Release ; il appartient à la CI Linux/Windows.
Le test GPU ne valide pas une session de Diablo sous Windows.

```sh
glslangValidator -V --target-env vulkan1.1 native/tests/stereo_mrt_vertex.vert -o /travail/mrt-vertex.spv
python3 tools/spirv_probe.py /travail/mrt-vertex.spv --helper /travail/helper.spv --tools /chemin/spirv-tools --output /travail/mrt-vertex.stereo.spv --stereo
```

Utiliser ces shaders dans la [validation de reprise](GRAPHICS_REPLAY.md) et
ajouter `--dynamic-state`, éventuellement avec `--descriptor-buffers` et son
shader de calcul. Il reste à relever les états réels, couvrir les variantes
requises, auditer les ressources et les producteurs des attachements `LOAD`,
puis raccorder une passe et ses traitements ultérieurs à SteamVR avec suivi de
tête. Aucune image de Diablo en VR n'est produite par ces preuves.

Références : [code VKD3D étudié](https://github.com/HansKristian-Work/vkd3d-proton/tree/0bb64c56c8f0dcc11ecc0f6c56e00e4f7bd8d506/libs/vkd3d),
[durée de vie des états dynamiques](https://docs.vulkan.org/guide/latest/dynamic_state.html),
[écriture de profondeur](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetDepthWriteEnable.html).
