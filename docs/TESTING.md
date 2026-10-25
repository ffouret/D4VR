# Vérification du laboratoire et des diagnostics

Essais du 24 octobre 2026.

## Graphes de bibliothèques par œil (25 octobre)

Deux groupes de six essais GPU passent : graphes privés après destruction de
tous les objets sources, et témoin original GPL avec bibliothèques maintenues
en vie jusqu'à la fin du GPU. Les seize vues restent exactes ; les états du
dessin suivant et du calcul sont restaurés. Les deux contrôles négatifs et
Steam Runtime passent dans chaque groupe. Cinq régressions monolithiques passent
aussi. Les tests C++ couvrent leases, quotas, générations, identifiants simulés,
échecs partiels et feedback isolé. Les 100 tests Python passent.

Un essai natif antérieur liant le témoin original après destruction de ses
bibliothèques s'arrête avec le code 139 ; cette condition reste non validée et
sa cause non établie. [Preuves et limites précises](LIBRARY_VARIANTS.md).

## Variantes créées depuis les recettes (25 octobre)

La fixture crée les variantes des deux yeux depuis une copie des paramètres du
pipeline et des octets shader. Les modules sources sont détruits avant cette
création. Les seize vues restent identiques aux références indépendantes, avec
les deux contrôles négatifs toujours détectés, dont Steam Runtime. Les cinq
exécutions du chemin manuel précédent passent également. Le test C++ vérifie
copies, spécialisations, pointeurs ignorés et nettoyage après création partielle
en échec ; il est inclus sous Linux/Windows et avec ASan/UBSan dans la CI Linux.
Les 100 tests Python passent. Voir [PIPELINE_RECIPES.md](PIPELINE_RECIPES.md).

## Recensement statique des shaders (25 octobre)

Les 100 tests Python passent. Cinq contrôles originaux compilés passent
`spirv-val` : texture, input attachment, stores de buffer/image et atomique,
adresse GPU et télémétrie du helper. Le recensement voit leurs accès attendus
sans autoriser leur reprise. La CI Linux exécute les contrôles avec une
grammaire Khronos épinglée et vérifiée par SHA-256 ; Windows exécute les tests
Python. Les 17 modules candidats privés des passes préparatoires passent
séparément le validateur. Voir [SHADER_EFFECTS.md](SHADER_EFFECTS.md) pour les
limites : aucun GPU, nouvel essai du jeu ou rendu VR n'est associé à cet audit.

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
| Rendu du laboratoire dans le Quest 2 sur Bazzite | Confirmé par le propriétaire le 24 octobre 2026 |
| Tests et chargement headless sur Windows | Réussis en CI, sans casque ni GPU de jeu |
| Rendu stéréo de Diablo IV dans le casque | Non implémenté ; diagnostics géométriques décrits ci-dessous |

Le test OpenXR dans le conteneur a échoué au chargement du runtime : SteamVR
ne peut pas y écrire son journal. Le pilote NVIDIA utilisable pour le rendu
n'est pas exposé dans cet environnement ; l'aperçu utilise llvmpipe. Le test VR
s'est terminé avec le code 2. Aucun résultat de performance du GPU ne peut être
déduit de cet aperçu logiciel.

La [CI Linux et Windows](https://github.com/ffouret/D4VR/actions/runs/40090387527)
a réussi les tests Python, la préparation vérifiée du moteur, les tests de caméra
et le chargement complet de la scène au commit `afcfc0538f31066ac68243789bc0ddb7886485e6`.
Elle ne vérifie pas le casque, les pilotes de jeu et la liaison ALVR.

## Retour du test dans le Quest 2

Le 24 octobre 2026, après livraison du laboratoire, le propriétaire confirme :
« résultat parfait dans le quest ».

Ce retour valide l'affichage du prototype sur sa configuration de référence
Bazzite, RTX 5090, SteamVR et Quest 2, avec la liaison ALVR précédemment déclarée.
Il s'agit d'une observation de l'utilisateur. Le numéro de build effectivement
lancé, les temps de trame, les contrôles pris individuellement et les scénarios
de perte/reprise de tracking n'ont pas été relevés séparément. Ce retour ne
valide pas l'intégration à Diablo IV, qui n'est pas encore implémentée.

L'échec antérieur dans le conteneur est conservé ci-dessus comme limite de cet
environnement de développement ; il ne décrit pas le résultat dans la session
Bazzite native.

## Procédure de reproduction et essais détaillés

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

## Observer GPU ajouté le 24 octobre

La suite locale comprend maintenant 48 tests Python. Les nouvelles fixtures
vérifient le parcours de deux transformations et l'instrumentation des shaders,
avec des sources synthétiques originales. Les cas invalides n'annoncent aucune
caméra et les journaux incomplets ou dupliqués sont rejetés.

Le programme Vulkan optionnel s'exécute sur l'hôte Bazzite et sa RTX 5090,
séparément du conteneur : les 32 valeurs observées sont exactes, les 16 384 octets
de l'image restent identiques et les bornes d'échantillonnage sont vérifiées.
Ce test nécessite le GPU ; il n'est pas lancé dans la CI générale sans GPU.

La première session réelle fournit 1 024 matrices complètes de projection.
Une seconde sonde, validée sur 2 113 shaders, prélève les deux transformations
ensemble avec un intervalle étendu. [Procédure et portée](CAMERA_OBSERVER.md).
Le rendu géométrique gauche/droite de Diablo IV n'est pas encore implémenté.

Le cœur de projection C++ vérifie aussi les frusta asymétriques, la disparité à
2, 10 et 30 unités, un déplacement de tête, une rotation de 45° et le rejet des
matrices invalides. Le test est inclus dans CTest pour Linux et Windows. Il
calcule des sommets et ne soumet aucune image du jeu.

## Relevé des attachements et shaders

La suite locale compte maintenant 73 tests Python. Les tests C++ sans pilote
vérifient les snapshots des ressources/pipelines, GPL, modules inline ou mis
en cache, buffers secondaires réutilisés, soumissions en cours et limites.
Quatorze exécutions GPU comparent les pixels avec et sans couche ; elles couvrent
le rendu dynamique, Steam Runtime et les créations de pipelines par identifiant.

Quatre relevés ponctuels dans Diablo récupèrent les attachements et pipelines,
avec une image normale confirmée par le propriétaire. Les deux derniers
reconnaissent les shaders du cache et permettent de relier des candidats de la
preuve géométrique à un groupe de sorties. Ces données ne prouvent aucun rendu
stéréo du jeu. [Résultats et limites détaillés](FRAME_TRACE.md).

## Transformation contrôlée par œil

La suite locale compte 81 tests Python. Le nouveau paquet C++ `EyeState` est
portable et son test fait partie de CTest Linux/Windows. La couche Vulkan de
contrôle cible actuellement Linux ; le backend natif du jeu sous Windows reste
à développer.

Dix-huit essais GPU réussissent sur la RTX 5090 : onze images de référence par
fixture stéréo correspondent octet pour octet ; le raccordement du buffer de
couche est vérifié pour gauche/droite/désactivé, dans Steam Runtime et avec les
pipelines du cache. Les shaders anciens non marqués conservent leur rendu.
Les 2 113 overrides préparés pour le jeu passent la validation SPIR-V.

Le relevé en partie confirme numériquement le déplacement commandé par le
paquet, pour un œil fixe pendant tout le lancement. Cette preuve ne démontre
aucune paire stéréo de la même trame ni aucune image de Diablo dans le casque.
[Résultats et procédure](EYE_CONTROL.md).

## Cibles stéréo indépendantes et sorties multiples

La fixture Vulkan ajoute deux ensembles de quatre sorties couleur et une
profondeur/stencil aux formats du groupe candidat du jeu. Seize rendus d'œil
sont comparés à des références indépendantes dans chaque exécution positive :
aucune différence couleur, profondeur ou stencil sur RTX 5090. Le rendu témoin
reste intact. La validation de synchronisation est activée et vérifiée par un
contrôle négatif ; un test sans GPU couvre 48 échecs de création partielle.

Les sorties sont celles d'une scène originale. Les passes de Diablo ne sont
pas encore reprises ni rendues deux fois. [Procédure et limites](STEREO_TARGETS.md).

## Reprise du flux graphique de contrôle

Le moteur de capture/reprise produit automatiquement les deux yeux à partir
du flux original de la fixture. Avec passes séparées, chaque exécution positive
reprend 12 scopes et émet 24 copies de dessins. Les 16 vues finales correspondent
aux références indépendantes, sans différence couleur, profondeur ou stencil.
Un dessin suivant sans rebind et le flux original transmis correspondent chacun
à leur référence sur 10 485 760 octets définis.

Les trois exécutions positives et le contrôle négatif de synchronisation passent,
dont Steam Runtime avec validation Vulkan. Le nouveau test C++ sans pilote couvre
les données temporaires, arguments indexés, restauration et chemins de refus ;
il est ajouté aux builds CTest Linux et Windows. Les 81 tests Python passent.
Ces résultats ne rendent pas Diablo en VR. [Contrats, reproduction et travail
restant](GRAPHICS_REPLAY.md).

Le mode descriptor buffers reprend les mêmes scènes et compare les yeux à
des références utilisant des sets traditionnels. Un dispatch de calcul sans
rebind après la reprise retrouve 32 mots exacts sur huit exécutions ; un offset
valide mais volontairement incorrect produit huit erreurs de données détectées.
Les trois contrôles positifs et les deux négatifs passent sur RTX 5090, dont
Steam Runtime avec validation Vulkan. Les tests sans pilote couvrent aussi
les tableaux réutilisés, offsets 64 bits, changement de type de liaison,
invalidation après rebind et restauration indépendante du calcul. Cela reste
une preuve synthétique UBO, sans reprise des heaps réels du jeu.

Le mode d'états dynamiques déclare douze états de la table principale VKD3D de
référence. Les positions proviennent d'un vertex buffer à stride dynamique de
16 octets. Les vues et le flux original correspondent aux références ; un dessin
suivant sans rebind conserve l'écriture de profondeur désactivée. Les trois
contrôles positifs et les deux négatifs passent, dont Steam Runtime avec
validation Vulkan. Le test C++ ajoute complétude et invalidation des états.
Ce relevé de sources ne remplace pas une capture des états effectivement utilisés
par Diablo. [Procédure et limites](DYNAMIC_STATE.md).

La couche de diagnostic conserve aussi les déclarations dynamiques et clear
values. Quatre contrôles positifs et deux négatifs passent avec le validateur
GPU, dont l'absence de capture sans opt-in ; quatorze essais de régression de
la couche préservent les pixels. Les tests Python sont au nombre de 90. Le test
C++ de métadonnées est inclus dans la CI Windows sans pilote. L'analyse des
deux anciens relevés du jeu identifie deux racines de clear par capture, mais
laisse deux et un `LOAD` non résolus. [Preuves et limites](PASS_DEPENDENCIES.md).


Le diagnostic de paires est maintenant raccordé aux créations de la couche
Vulkan, derrière deux paquets explicites. Deux groupes de huit essais GPU
couvrent les pipelines monolithiques et GPL, les métadonnées, Steam Runtime,
l'activation absente/invalide et les contrôles négatifs. Trois essais ajoutent
les modules puis les identifiants issus d'un cache préparé ; quatorze
régressions de pixels et dix-huit du mode ancien à un œil restent séparées.
Les paires de la couche ne sont jamais utilisées pour dessiner. Les 101 tests
Python passent. La transmission des créations originales partielles et les
échecs privés sont testés sans pilote. [Contrat et preuves](PAIRED_PIPELINES.md).
