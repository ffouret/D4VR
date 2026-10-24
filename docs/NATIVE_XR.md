# Chaîne native OpenXR et Vulkan

Le laboratoire Godot a validé la présentation surélevée. L'outil
`d4vr_xr_readiness` vérifie maintenant une chaîne native OpenXR/Vulkan pouvant
servir de base au futur adaptateur graphique. Il reste indépendant du processus
Diablo IV et ne reçoit aucune image ou géométrie du jeu.

## Résultats du 24 octobre 2026

Le premier appel a échoué avec `XR_ERROR_RUNTIME_FAILURE` alors que SteamVR
était arrêté. Après lancement de SteamVR et connexion du Quest 2 par Steam Link,
le contrôle a réussi :

| Élément | Relevé |
| --- | --- |
| Runtime | SteamVR/OpenXR 2.18.4 |
| Casque exposé | SteamVR/OpenXR : oculus |
| GPU choisi par le runtime | NVIDIA GeForce RTX 5090 |
| Extension | XR_KHR_vulkan_enable2 |
| Version Vulkan choisie dans la plage annoncée | 1.2.0 |
| Images recommandées | Deux vues de 2 244 × 2 352 pixels, un échantillon |

La version Vulkan choisie appartient à ce diagnostic. Elle ne démontre pas la
compatibilité des instances et périphériques Vulkan déjà créés par VKD3D.
La liaison déclarée pour cet essai est Steam Link ; ALVR reste la liaison
initialement décrite pour le PC. Le composant demande SteamVR via son manifest
OpenXR, sans modifier le runtime enregistré dans le système.

Un essai natif d'environ 25 secondes a ensuite créé une session, un périphérique
Vulkan et deux swapchains. Une petite scène originale comprend un cube bleu
et un plateau quadrillé. Le rendu utilise les poses et champs de vision de
chaque œil pour le même instant prédit :

- 2 241 trames stéréo acceptées par `xrEndFrame` ;
- position et orientation suivies pour ces 2 241 trames ;
- écart entre les vues mesuré à environ 68,0 mm ;
- déplacement maximal du centre de la tête d'environ 21,7 cm ;
- aucune erreur retournée ;
- affichage confirmé par le propriétaire : « oui parfait ».

Ces nombres sont des observations du diagnostic, sans mesure détaillée des
temps GPU ou de la latence. Ils ne constituent pas un benchmark de Diablo IV.
Les images de cette scène sont originales ; aucune image du jeu n'est soumise.

## Code et conventions

`native/xr/readiness.cpp` énumère les extensions, sélectionne le système stéréo,
interroge les exigences graphiques et demande au runtime une instance Vulkan
et son périphérique physique. Sans argument, il ne crée pas de session.

Avec `--frames`, `frame_test.cpp` demande un périphérique Vulkan au runtime,
crée une session liée à sa queue graphique, puis rend la scène originale. Les
événements OpenXR pilotent le démarrage et l'arrêt. Chaque trame utilise
`xrWaitFrame`, `xrLocateViews` et deux images acquises/attendues/libérées avant
la soumission d'une couche de projection. Un test sans trame suivie ou échoué
retourne un code d'échec.

Les matrices emploient le cœur C++ commun. Une conversion de la pose produit
la vue inverse ; les tangentes des champs de vision produisent les projections
asymétriques. Un viewport Vulkan de hauteur négative adapte l'axe vertical.
Les deux yeux partagent le même ancrage de scène, stable dans le repère local.
Les images et ressources sont détruites à la fin ; SteamVR reste ouvert.

Cet outil crée son propre périphérique Vulkan. Partager celui du jeu, produire
deux rendus géométriques cohérents de Diablo IV et traiter culling, interfaces
et historiques restent à réaliser. Le contrôle natif n'est pas un adaptateur
D3D12 Windows. La partie native OpenXR est actuellement testée sur Bazzite ;
les tests du cœur de projection et des analyseurs passent aussi sous Windows.

## Construire et exécuter

Il faut les en-têtes OpenXR et Vulkan, leurs loaders et glslangValidator. Sur la
machine d'essai, les en-têtes OpenXR 1.0.20 proviennent du paquet Ubuntu
`libopenxr-dev`, extrait dans le dossier de travail sans installation système.
Le programme utilise le loader OpenXR 1.1.63 de l'hôte. Ces dépendances et leurs
binaires ne sont pas redistribués dans le dépôt.

```sh
cmake -S native -B work/native-xr -DD4VR_BUILD_XR_READINESS=ON
cmake --build work/native-xr --target d4vr_xr_readiness
XR_RUNTIME_JSON=/chemin/SteamVR/steamxr_linux64.json work/native-xr/d4vr_xr_readiness
XR_RUNTIME_JSON=/chemin/SteamVR/steamxr_linux64.json work/native-xr/d4vr_xr_readiness --frames work/native-xr/scene.vert.spv work/native-xr/scene.frag.spv
```

Si les SDK sont extraits localement, fournir les chemins
`D4VR_OPENXR_INCLUDE_DIR`, `D4VR_OPENXR_LOADER`, `Vulkan_INCLUDE_DIR` et
`Vulkan_LIBRARY` à CMake. Quand glslang est trouvé, les shaders de la scène sont
compilés automatiquement. Le programme accepte leurs chemins explicitement.
La CI Linux compile ce code et valide les shaders sans ouvrir de session XR ;
l'essai en casque est effectué séparément sur le PC de référence.

Le mode `--requirements` interroge les extensions nécessaires à la liaison
Vulkan historique, avec une instance OpenXR séparée. Il ne crée aucun objet
Vulkan ni session. Son relevé et la comparaison avec le renderer réel sont
décrits dans [le diagnostic Vulkan](VULKAN_TRACE.md#exigences-de-liaison-steamvr).

## Sources primaires des interfaces

- [Création d'une instance Vulkan par OpenXR](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrCreateVulkanInstanceKHR.html)
- [Choix du périphérique physique](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrGetVulkanGraphicsDevice2KHR.html)
- [Localisation des vues](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrLocateViews.html)
- [Soumission d'une trame](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrEndFrame.html)
