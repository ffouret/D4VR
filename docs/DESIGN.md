# Conception initiale de D4VR

État au 24 octobre 2026 : laboratoire validé en casque et observation GPU dans le jeu ; rendu stéréoscopique du jeu encore absent.

## Expérience recherchée

Le joueur observe son personnage depuis une position surélevée dans le monde.
Le sol, les murs et les personnages ont un volume perceptible. La pose de tête
agit sur les vues des deux yeux ; se pencher modifie réellement la perspective.
La vue conserve l'esprit du cadrage isométrique de Diablo, avec des projections
perspectives adaptées aux optiques du casque plutôt qu'une projection
orthographique imposée aux yeux.

L'échelle reste réglable : l'effet de maquette est une possibilité, pas une
obligation. La première expérience à tester conserve un horizon stable et une
caméra de base liée au personnage. Les mouvements physiques de tête s'ajoutent
à cette base. Le suivi du personnage ne doit pas imposer des rotations de tête.

Une origine placée en hauteur permet de regarder naturellement vers le bas.
Incliner tout le repère de tracking pour forcer une vue plongeante risque de
faire pencher le monde lors des mouvements physiques ; ce comportement doit
être évité dans le premier prototype.

## Trois méthodes à distinguer

| Méthode | Données nécessaires | Résultat et limite |
| --- | --- | --- |
| Capture d'écran | Image finale | Surface visible dans le casque, sans géométrie du décor ; ne remplit pas l'objectif |
| Reprojection à partir de la profondeur | Couleur, profondeur, projection et synchronisation | Relief partiel ; les surfaces cachées restent inconnues et les déplacements révèlent des trous |
| Rendu géométrique par œil | Accès aux vues du moteur et à son rendu | Voie recherchée pour une vraie stéréo et une parallaxe de mouvement ; faisabilité dans Diablo IV inconnue |

La reprojection peut servir à une expérience limitée si sa qualité est évaluée
explicitement. Elle ne sera pas annoncée comme équivalente à deux rendus
géométriques. Le premier objectif de recherche est la troisième méthode.

## Séparation des responsabilités proposée

```text
Diablo IV : simulation, caméra de base, rendu de la scène
                       |
        Adaptateur Diablo IV à étudier
        - vues gauche/droite pour un même état du monde
        - culling et effets cohérents pour les deux vues
        - accès aux images, profondeur et synchronisation GPU
                       |
        Partie VR commune proposée
        - pose de tête prédite et projections par œil
        - origine surélevée, échelle, recentrage
        - cycle de vie, images et couches d'interface
                       |
        OpenXR avec SteamVR comme runtime préféré
                       |
        Liaison PC/casque existante, actuellement ALVR
                       |
                   Meta Quest 2
```

Ce schéma ne décrit pas du code existant. L'emplacement du rendu VR dans le
processus du jeu ou dans un processus séparé reste à décider. Le partage des
textures et leur synchronisation doivent être prouvés avant de choisir un
processus compagnon. Une copie CPU des images n'est pas une architecture de
production retenue.

OpenXR est la première API à évaluer pour accéder à SteamVR. L'utilisateur n'a
pas à utiliser un autre environnement VR. OpenVR reste une option de recherche
si les essais sur la configuration réelle justifient ce choix. Les deux API
ne seront pas développées simultanément sans besoin mesuré.

## Windows et Bazzite

Les spécifications publiées par Blizzard indiquent DirectX 12 pour Diablo IV.
Sous Windows, l'intégration graphique devra donc étudier D3D12. Sur Bazzite,
la version Windows tourne dans l'environnement Wine choisi par Lutris ;
vkd3d-proton est une implémentation D3D12 sur Vulkan. Le lancement observé utilise
GE-Proton11-11 et VKD3D-Proton 3.1.0 (`0bb64c56c8f0dcc+`). Le diagnostic GPU
actuel dépend de ce chemin Linux ; il ne fournit pas encore un adaptateur Windows.

Deux voies restent à comparer : un composant Windows utilisable aussi sous
Wine, ou des adaptateurs graphiques Windows/Linux distincts autour d'un cœur
commun. Le fonctionnement d'Overlord ne permet pas de trancher : son chemin
D3D11 ne valide ni D3D12, ni le pont OpenXR de ce runner Lutris.

L'API graphique exposée au jeu, l'API du runtime VR et la liaison vers le Quest
sont trois couches différentes. Acheter Diablo sur Battle.net n'exige pas de
le racheter sur Steam pour viser SteamVR.

## Étapes et preuves attendues

1. **Relever l'installation existante.** Version de Diablo IV, runner Lutris,
   versions de SteamVR, ALVR et du pilote NVIDIA, backend OpenXR/OpenVR utilisé
   par Overlord, emplacement des runtimes. Ces relevés doivent venir du PC de
   jeu ; l'environnement de développement actuel ne les remplace pas.
2. **Établir le point d'intégration de Diablo IV.** Rechercher une interface
   autorisée et les données de caméra disponibles. Documenter la possibilité
   de demander deux rendus, la gestion du culling et le coût GPU. Si cet accès
   est indisponible, réévaluer la faisabilité avant de construire le reste.
3. **Valider la caméra et SteamVR indépendamment.** Une scène synthétique avec
   sol, murs, personnage et occultations vérifie la stéréo, l'échelle et le
   confort. Cette preuve concerne uniquement la partie VR commune.
4. **Prouver une paire stéréo dans Diablo IV.** Les deux vues doivent provenir
   du même état de simulation, avec des positions d'œil et projections correctes.
   Vérifier particules, transparences, ombres, surfaces nouvellement visibles
   et historiques temporels. L’observation de matrices GPU est disponible ; demander deux rendus cohérents
   au même état de simulation reste à réaliser.
5. **Rendre l'ensemble jouable.** Interface lisible, visée compatible avec la
   nouvelle caméra, inventaire, transitions, cinématiques, recentrage et
   reprise après perte du casque. Conserver d'abord les commandes du jeu.
6. **Valider les deux systèmes.** Mesurer séparément Bazzite/Lutris et Windows.
   Une compilation portable ne démontre pas la compatibilité en jeu.

Diablo III viendra après la preuve sur Diablo IV. Seuls les éléments VR
indépendants du jeu sont candidats au partage ; le moteur, les vues et les
commandes nécessitent leur propre étude.

## Critères d'acceptation de la première version jouable

- Les deux yeux reçoivent des vues distinctes, cohérentes dans le temps.
- Le décor reste stable quand on tourne ou penche la tête.
- Un déplacement latéral révèle la géométrie attendue derrière un obstacle.
- Hauteur, échelle et recentrage sont réglables sans redémarrer une partie.
- La caméra et les commandes permettent de viser et combattre correctement.
- Le budget de trame est mesuré au taux choisi dans SteamVR : par exemple
  environ 13,9 ms à 72 Hz, avant marge de rendu, composition et transport.
- Les mesures distinguent temps CPU, GPU et latence de transport ; aucune
  promesse de fréquence n'est déduite du modèle de carte graphique.

## Sources primaires

- [Spécifications PC de Diablo IV publiées par Blizzard](https://news.blizzard.com/en-us/article/23938756/make-sanctuary-yoursplay-your-way-in-diablo-iv)
- [vkd3d-proton : D3D12 sur Vulkan](https://github.com/HansKristian-Work/vkd3d-proton)
- [OpenXR : vues, projections et couches](https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html)
- [SteamVR pour Linux](https://github.com/ValveSoftware/SteamVR-for-Linux)
- [ALVR : hôtes Windows/Linux et casques pris en charge](https://github.com/alvr-org/ALVR)
