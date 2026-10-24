# D4VR

Projet expérimental pour jouer à **Diablo IV en VR stéréoscopique, depuis une
position surélevée au-dessus du personnage et du décor**.

L'objectif est de percevoir le monde du jeu en volume et de pouvoir observer le
décor avec les mouvements de la tête. La hauteur d'observation et l'échelle du
monde doivent être réglables. Une capture du bureau affichée dans le casque ne
répond pas à cet objectif.

## État du projet

**Version 0.1.0 : laboratoire 3D autonome, sans intégration à Diablo IV.**
Le dépôt contient un prototype Godot/OpenXR avec une scène géométrique, un
personnage déplaçable, un point d'observation surélevé, le suivi de tête et des
réglages d'échelle. L'accès à la caméra et au rendu stéréoscopique de Diablo IV
reste à démontrer.

Le chargement de la scène, son aperçu bureau et les tests de caméra ont été
vérifiés dans l'environnement de développement. L'affichage réel dans le
Quest 2 et l'exécution sur Windows restent à tester. Voir les
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
| Liaison existante | ALVR ; indépendante de l'API utilisée par D4VR |
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
