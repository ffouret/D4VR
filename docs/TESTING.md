# Vérification du laboratoire 0.1.0

Essais du 24 octobre 2026.

## Installation détectée en lecture seule

- L'hôte identifié par son fichier système est Bazzite ; les outils de Codex
  s'exécutent dans un conteneur Ubuntu 24.04.
- Une entrée Battle.net est présente dans la configuration Lutris.
- Le préfixe référencé contient `Diablo IV.exe` et `.build.info`.
- La version active lue dans ce dernier fichier est **3.2.3.73883**.
- Le manifest OpenXR utilisateur désigne SteamVR ; son fichier cible est visible.

Ces observations confirment l'emplacement et les métadonnées. Elles ne
démontrent ni le démarrage de Diablo IV ni sa compatibilité VR. Les chemins
personnels du diagnostic restent dans les fichiers de travail locaux.

## Vérifications effectuées

| Vérification | Résultat |
| --- | --- |
| Godot Linux 4.7.2 officiel, SHA-512 de l'archive | Vérifié |
| Six tests Python de découverte et de sélection du runtime | Réussis |
| Recentrage : position et conservation de l'axe vertical | Réussi |
| Déplacement physique et translation du suivi du personnage | Réussis |
| Changement d'échelle sans déplacement de la tête dans le monde | Réussi mathématiquement |
| Valeurs d'échelle bornées et rejet des valeurs non finies | Réussis |
| Action map sauvegardée : six actions et sept bindings Touch | Vérifiée |
| Chargement complet de la scène en mode headless | Réussi |
| Rendu de l'aperçu et capture PNG | Réussis, OpenGL logiciel llvmpipe |
| Affichage stéréoscopique dans le Quest 2 | Non vérifié |
| Démarrage sur Windows | Non vérifié localement ; workflow de tests prévu |
| Rendu ou interaction avec Diablo IV | Non implémentés |

Le test OpenXR dans le conteneur a échoué au chargement du runtime : SteamVR
ne peut pas y écrire son journal. Le pilote NVIDIA utilisable pour le rendu
n'est pas exposé dans cet environnement ; l'aperçu utilise llvmpipe. Le test VR
s'est terminé avec le code 2. Aucun résultat de performance du GPU ne peut être
déduit de cet aperçu logiciel.

La CI vérifie les tests sans casque sur Linux et Windows. Sa présence dans le
dépôt ne signifie pas que les premiers jobs ont déjà réussi. Même une CI verte
ne vérifierait pas le casque, les pilotes et la liaison ALVR.

## Essai à faire depuis la session Bazzite

1. Démarrer SteamVR avec le Quest 2 connecté par la liaison habituelle.
2. Depuis le dossier D4VR, exécuter `./launch.sh setup`, puis `./launch.sh`.
3. Regarder vers le bas : la salle doit apparaître en volume dans le casque.
4. Se pencher doucement à gauche et à droite devant les piliers : vérifier
   parallaxe et surfaces progressivement dévoilées.
5. Appuyer sur A en regardant vers le bas : le monde doit rester vertical.
6. Changer l'échelle avec B, déplacer le personnage, tester les rotations par
   pas et retirer/remettre le casque. Vérifier que le personnage s'arrête quand
   la session perd le focus et que le suivi reste cohérent après la reprise.
7. Relever le temps de trame dans SteamVR et les éventuels artefacts visuels.

Ce test lance uniquement une scène originale. Il ne nécessite pas de lancer
Battle.net ou Diablo IV.

## Reproduire un aperçu

```sh
python3 tools/d4vr.py run --desktop --capture /chemin/absolu/apercu.png
```

Le moteur capture son propre viewport puis quitte. L'image du moniteur
illustre la géométrie et le cadrage, pas la vision binoculaire dans le casque.

## Références de l'implémentation du laboratoire

- [Configuration OpenXR dans Godot](https://docs.godotengine.org/en/stable/tutorials/xr/setting_up_xr.html)
- [Origine et échelle de tracking](https://docs.godotengine.org/en/stable/classes/class_xrorigin3d.html)
- [Actions OpenXR](https://docs.godotengine.org/en/stable/tutorials/xr/xr_action_map.html)
- [Publication officielle Godot 4.7.2](https://github.com/godotengine/godot/releases/tag/4.7.2-stable)
