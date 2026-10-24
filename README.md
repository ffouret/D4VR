# D4VR

Projet expérimental pour jouer à **Diablo IV en VR stéréoscopique, depuis une
position surélevée au-dessus du personnage et du décor**.

L'objectif est de percevoir le monde du jeu en volume et de pouvoir observer le
décor avec les mouvements de la tête. La hauteur d'observation et l'échelle du
monde doivent être réglables. Une capture du bureau affichée dans le casque ne
répond pas à cet objectif.

## État du projet

**État expérimental : transformation géométrique d’un œil fixe pilotée par la
couche Vulkan et mesurée dans Diablo IV. La chaîne native SteamVR est validée
séparément en casque. Diablo n'est pas encore rendu en VR.**
Le dépôt contient le laboratoire Godot/OpenXR, les outils d'analyse et de
diagnostic de la géométrie du jeu, un cœur de projection par œil et une chaîne
native OpenXR/Vulkan. Produire deux vues cohérentes de Diablo IV et les soumettre
au casque reste à implémenter.

Le chargement de la scène, son aperçu bureau et les tests de caméra ont été
vérifiés dans l'environnement de développement et en CI sur Linux et Windows.
Le propriétaire a confirmé le rendu du laboratoire dans le Quest 2 sur sa
Bazzite le 24 octobre 2026. L'exécution VR sous Windows et les essais détaillés
des commandes et des reprises de session restent à vérifier. Voir les
[preuves et limites des essais](docs/TESTING.md).

## Démarrer le laboratoire

Prérequis : Python 3.10 ou plus récent, Linux x64 ou Windows x64. La préparation
télécharge Godot 4.7.2 depuis sa publication officielle, vérifie son SHA-512 et
le conserve dans `work/`. Aucune installation système n'est nécessaire.

Depuis le dossier du dépôt, sous Bazzite :

```sh
./launch.sh setup
./launch.sh
```

Sous Windows :

```bat
launch.cmd setup
launch.cmd
```

Démarrer SteamVR et connecter le casque avant le deuxième lancement. Le lanceur
cherche le manifest OpenXR de SteamVR et le sélectionne pour son processus ; il
ne change pas le runtime enregistré dans le système. ALVR peut rester la liaison
du casque. Le prototype s'exécute nativement, séparément de Lutris et de Diablo.

Pour inspecter la scène sur le moniteur : `./launch.sh run --desktop`, ou
`launch.cmd run --desktop` sur Windows. Ce mode sert au développement et ne
représente pas le rendu attendu dans le casque. Un échec OpenXR arrête le mode
VR avec une erreur ; il ne le remplace pas par un écran virtuel.

Un Godot déjà présent peut être fourni par `GODOT_BIN` ou
`python3 tools/d4vr.py --godot /chemin/vers/godot run`.

## Étape 2 : sonde du renderer de Diablo IV

Une sonde expérimentale Windows x64 observe les constantes de caméra et les
tags de ressources de NVIDIA Streamline 2.12 fourni avec le jeu. Son relais
d'appels a été testé sous Wine, avec un programme contrôlé ; le chargement des
262 exports a aussi été vérifié avec la véritable DLL NVIDIA hors du jeu.

Les deux essais réels ont établi un blocage : Battle.net restaure la bibliothèque
originale et, quand le relais reste en place, Diablo IV refuse sa signature.
L'original a été restauré et vérifié. L'installation de ce relais est désormais
désactivée pour ce profil ; ses tests hors du jeu restent disponibles. Le rendu
VR de Diablo IV reste à implémenter et requiert un autre point d'intégration.
Voir [l'étude, les tests et la procédure](docs/STREAMLINE_PROBE.md).

## Étape 3 : relevé DirectX 12

Le lancement de diagnostic VKD3D a récupéré 10 249 fichiers DXIL, 10 377 SPIR-V
et cinq fichiers de signature racine, avec les DLL NVIDIA d'origine. L'analyseur
portable lit les types de shaders, signatures d'entrée/sortie et ressources
déclarées. Les 20 626 fichiers de shaders ont été analysés sans erreur ; cela
ne prouve pas quels programmes dessinent le décor ni quelles données portent
la caméra. Voir [la capture et ses limites](docs/SHADER_CAPTURE.md).

## Étape 4 : observation de matrices GPU

L'analyse des 5 784 vertex shaders retrouve 2 679 multiplications finales par
une matrice commune. Une sonde exécutée dans le jeu a récupéré 1 024 matrices
complètes : leurs valeurs correspondent à des projections, sans prouver encore
la caméra de scène. Une seconde sonde prélève ensemble la projection et la
transformation qui la précède dans 2 113 programmes identifiés. Sa fixture sur
la RTX 5090 récupère les 32 valeurs exactes sans changer l'image de contrôle.

Les shaders instrumentés utilisent le ring de diagnostic VKD3D ; les DLL du jeu
et de NVIDIA restent originales. Cela ne fournit pas encore de stéréo ni de
rendu SteamVR de Diablo IV. La seconde session a récupéré 245 paires complètes.
Un cœur C++ portable calcule maintenant les transformations géométriques par œil ;
ses tests vérifient la disparité selon la profondeur, sans intégration au renderer.
Voir [les résultats, outils et limites](docs/CAMERA_OBSERVER.md).

## Étape 5 : premier déplacement géométrique

Un mode de diagnostic applique un petit déplacement latéral aux sommets de la
projection 35° identifiée. La fixture RTX 5090 mesure 11,4 pixels à deux unités
de distance, contre 5,7 à quatre unités. Le déplacement dépend de la profondeur.
Un relevé dans le jeu confirme maintenant 12 déplacements conformes au calcul,
et 23 sommets hors cible inchangés. Le test ne produit pas encore deux vues du jeu.
Voir [le test géométrique et ses limites](docs/GEOMETRY_TEST.md).

## Étape 6 : chaîne native SteamVR

Un composant natif OpenXR/Vulkan a soumis 2 241 trames stéréo d'une scène
originale avec suivi de tête. Le propriétaire confirme l'affichage dans le
Quest 2 connecté par Steam Link. SteamVR sélectionne la RTX 5090 et recommande
2 244 × 2 352 pixels par œil. Ce composant reste indépendant de Diablo IV.
Voir [le relevé natif et sa portée](docs/NATIVE_XR.md).

## Étape 7 : observation du renderer Vulkan

Une couche Vulkan Linux observe les commandes soumises, les passes de rendu,
les versions et extensions utilisées. Elle prend aussi en compte les commandes
générées sur le GPU et les buffers secondaires réutilisés. Ses tests de relais
et son chargement dans Steam Runtime passent ; la fixture RTX 5090 conserve ses
pixels et sa parallaxe. La couche n'ajoute pas de rendu par œil.
Le relevé en partie retrouve 13 800 présentations, sans buffer inconnu ; le
propriétaire confirme une image normale. Les extensions requises par la liaison
Vulkan de SteamVR ont été comparées avant le contrôle de liaison.
Le contrôle suivant a créé une session temporaire et obtenu trois images
OpenXR sur le périphérique Vulkan 1.3 de Diablo, avec identité du GPU vérifiée
par UUID. Cette session n'est pas démarrée et ne soumet aucune image.
Voir [le lancement isolé et les limites du relevé](docs/VULKAN_TRACE.md).

## Étape 8 : sorties des passes et shaders utilisés

Le diagnostic peut relever à la demande les attachements couleur/profondeur,
les dimensions et les empreintes des shaders de chaque pipeline utilisé.
Les comparaisons de pixels sur RTX 5090 passent en rendu classique et dynamique,
y compris dans Steam Runtime. Ce relevé prépare le choix des passes à adapter ;
il ne produit aucune stéréo du jeu. Deux relevés réels avec les identifiants du
cache retrouvent des candidats pour 328/339 et 262/267 clés de shader. Six des
huit shaders du déplacement géométrique précédent trouvent des candidats dans
un même groupe de quatre sorties couleur et d'une profondeur. Le propriétaire
confirme une image normale ; le rôle visible exact de ce groupe reste à vérifier.
Voir [le déclenchement, les preuves et les limites](docs/FRAME_TRACE.md).

## Étape 9 : transformation géométrique pilotable par œil

Un paquet GPU contient maintenant la transformation de vue et la projection de
chaque œil. Onze images synthétiques correspondent exactement à des rendus
indépendants, avec déplacement/rotation de tête simulés, frusta asymétriques et
jitter. Dix-huit exécutions valident ce helper et son raccordement à une couche
Vulkan possédant son propre buffer, y compris avec les shaders du cache.

Un essai dans Diablo confirme que les sommets consomment ce paquet et suivent
le déplacement demandé. Cet essai utilise un seul œil fixe pendant tout le
lancement. Les deux vues d'une même trame, les traitements par œil et leur envoi
à SteamVR restent à implémenter. Voir [les preuves et limites](docs/EYE_CONTROL.md).

## Étape 10 : deux yeux avec sorties couleur et profondeur indépendantes

Un composant Vulkan possède maintenant quatre cibles couleur et une profondeur
par œil. Une scène originale est rendue pour les deux yeux dans la même
soumission, avec des références indépendantes. Les quatre sorties, la profondeur
et le stencil correspondent aux références ; les cibles du rendu témoin sont
préservées. Le test passe aussi dans Steam Runtime avec validation Vulkan de la
synchronisation. La reprise des passes de Diablo reste à implémenter.
Voir [les résultats, la propriété des ressources et les limites](docs/STEREO_TARGETS.md).

## Commandes du prototype

| Action | Quest Touch | Clavier |
| --- | --- | --- |
| Déplacer le personnage | Stick gauche | Flèches ou positions physiques WASD/ZQSD |
| Recentrer | A droit | R |
| Changer l'échelle | B droit | Tab |
| Activer/désactiver le suivi du personnage | Y gauche | F |
| Tourner autour du personnage | Stick droit horizontal, pas de 30° | Positions physiques Q / E, soit A / E sur AZERTY |
| Régler la hauteur | Stick droit vertical | Page haut / bas |
| Quitter | Fermer l'application dans SteamVR | Échap |

Regarder naturellement vers le bas pour observer la scène. Le recentrage
conserve l'inclinaison réelle de la tête ; il ne force pas une rotation du monde.
Les échelles proposées sont 1:1, 1:4, 1:8 et 1:12. Les associations Touch sont
enregistrées dans le projet, mais leur acceptation en casque reste à vérifier.

## Cible de départ

| Élément | Cible |
| --- | --- |
| Jeu | Diablo IV acheté sur Battle.net |
| PC principal | Bazzite, Battle.net lancé avec Lutris |
| GPU | NVIDIA RTX 5090, 32 Go, déclaré par le propriétaire |
| Casque | Meta Quest 2 |
| Environnement VR préféré | SteamVR |
| Liaison existante | ALVR initialement ; essai natif actuel avec Steam Link, via SteamVR |
| Compatibilité visée | Windows également, dès la conception |
| Commandes initiales | Commandes habituelles du jeu ; choix précis à confirmer |
| Extension envisagée | Diablo III, avec une étude et un adaptateur distincts |

Overlord fonctionne déjà sur ce PC, selon le propriétaire. C'est une référence
utile pour la chaîne VR existante, pas une validation de Diablo IV.

## Documents

- [Conception et critères de réussite](docs/DESIGN.md)
- [Étude d'Overlord et limites du transfert](docs/OVERLORD.md)
- [Configuration de référence déclarée](config/reference-system.toml)
- [Tests, installation détectée et limites actuelles](docs/TESTING.md)
- [Observation des transformations GPU](docs/CAMERA_OBSERVER.md)
- [Sonde de caméra et de ressources Streamline](docs/STREAMLINE_PROBE.md)
- [Dépendances et mentions tierces](THIRD_PARTY_NOTICES.md)

## Vérifier le développement

```sh
python3 -m unittest discover -s tests -v
python3 tools/d4vr.py test
python3 tools/d4vr.py smoke
python3 tools/d4vr.py doctor
```

`doctor` relève les manifests et l'installation Diablo IV depuis les fichiers
Lutris, sans lancer le jeu. `--game-dir` permet de préciser son emplacement.
Le rapport contient des chemins locaux ; il est destiné au diagnostic local.

Le prototype Godot sert à valider la présentation et le confort. Le futur
adaptateur de Diablo IV pourra nécessiter une autre technologie et ne sera pas
déduit du fonctionnement de cette scène indépendante.

## Contrainte liée à Diablo IV

Blizzard avertit que les logiciels modifiant Diablo IV sans autorisation peuvent
entraîner des sanctions, jusqu'à une suspension permanente du compte.
L'usage VR n'est pas présenté comme une exception dans cet avertissement.
[Avis officiel Blizzard](https://us.forums.blizzard.com/en/d4/t/a-notice-regarding-unauthorized-game-modifying-software-in-diablo-iv/102121).

Le projet ne bénéficie d'aucune autorisation connue de Blizzard. L'étude doit
établir le mode d'intégration disponible avant tout essai dans le jeu ; aucune
méthode d'évitement de l'anti-triche n'est prévue.

Le code original de D4VR est sous licence MIT, voir [LICENSE](LICENSE).
Aucun code d'Overlord ni contenu de Diablo n'est inclus.
