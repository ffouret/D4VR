# Lancement isolé du diagnostic de paires

`tools/paired_capture.py` prépare et lance le diagnostic de création seule de la
[couche Vulkan](PAIRED_PIPELINES.md). Il conserve les DLL et l'exécutable originaux
et n'active aucun dessin privé. **Aucun pipeline réel de Diablo n'a été confirmé
par les essais de lancement décrits ici. Diablo n'est toujours pas rendu en VR.**

## Préparation

Le lanceur exige les empreintes d'une couche ayant passé les contrôles GPU de
paires, pixels, transmission et Steam Runtime. Le bundle de shaders doit être
celui du helper stéréo ABI 1 validé, pour la même version du jeu et le runtime
VKD3D épinglé. Les 2 113 overrides privés existants passent cette vérification ;
ils restent hors dépôt.

La copie privée du script Lutris doit contenir le préfixe Wine explicite du jeu
et le point de lancement Battle.net attendu. Une absence de préfixe ou un chemin
différent est refusé avant toute création de processus. Seule la copie privée
reçoit le runtime épinglé et les chemins de cache de la session. Les deux paquets
d'œil et les journaux restent sous un répertoire neuf, hors du jeu, de mode 0700.

Le sous-répertoire `layer/` contient uniquement la bibliothèque et son manifeste,
copiés puis revérifiés par empreinte. Le JSON de preuves n'entre donc pas dans
la recherche de manifestes du loader et de pressure-vessel. Le mode d'un œil
fixe, les autres captures et le probe XR sont purgés de l'environnement.

```sh
python3 tools/paired_capture.py prepare --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --shader-bundle /travail/overrides-valides --layer-bundle /travail/couche-paires-validee --output /travail/preparation-neuve
python3 tools/paired_capture.py run --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --shader-bundle /travail/overrides-valides --layer-bundle /travail/couche-paires-validee --seconds 240 --output /travail/session-neuve
```

`prepare` ne démarre aucun processus. `run` accepte de 30 à 900 secondes. L'option
`--launch-game` adresse `--exec="launch Fen"` à `Battle.net.exe`, selon la
[méthode du service Battle.net de Lutris](https://github.com/lutris/lutris/blob/master/lutris/services/battlenet.py).
Cette requête ne garantit pas le démarrage : dans nos essais, le client affiche
sa page Diablo IV et son bouton Jouer, sans créer de processus du jeu. Le lanceur
ne contourne ni le client, ni son authentification.

## Fin de session et attribution

Les processus sont identifiés par nom, marqueur d'environnement propre à la
session et heure de démarrage `/proc`. La fermeture utilise des pidfds Linux et
SIGTERM, après vérification de cette identité. Elle n'arrête aucun Wine server
global. Les enfants apparus pendant la fermeture sont recherchés pendant dix
secondes ; des processus restants produisent un résultat d'erreur. Une simple
réutilisation de PID ou un marqueur étranger ne suffit pas pour envoyer un signal.
Les processus `Diablo IV Launcher.exe` sont également suivis par les gardes de
session. Les empreintes du jeu sont vérifiées de nouveau après la fermeture.

Le rapport distingue les traces du loader et celles du jeu. La présence d'un
fichier Vulkan issu d'un utilitaire de démarrage ne prouve rien sur Diablo. Une
création de paires du jeu exige une trace de device associée à un PID observé
comme `Diablo IV.exe`, puis un compteur positif d'exécutables privés. En cas de
fermeture sans événement de destruction du device, les compteurs échantillonnés
restent partiels. Même un compteur positif ne prouverait aucun rendu stéréo : ce
diagnostic ne lie pas les paires aux commandes.

## Résultats et limites

La [preuve de lancement](proofs/paired-capture-v1.json) sépare les deux essais
exploratoires du contrôle final sur la version corrigée du lanceur. Les essais
exploratoires ont ouvert Battle.net, mais aucun processus Diablo, aucune trace
de device du jeu et aucun rendu n'ont été observés. Un relais manuel sans le
script complet avait utilisé le préfixe UMU par défaut ; les processus marqués
ont été arrêtés. Le lanceur final exige le préfixe explicite et conserve le
script Lutris complet. Aucun préfixe préexistant n'a été supprimé.

Le contrôle final de 30 secondes ouvre Battle.net puis ferme ses processus
marqués, sans reste observé. Les empreintes de l'exécutable, de la DLL NVIDIA et
des exports correspondent avant et après. Les 108 tests Python couvrent aussi
les préfixes invalides, le gel des seuls fichiers du loader, les marqueurs et
PID réutilisés, l'attribution des traces et le maintien des preuves de rendu à
faux. Le module de lancement est Linux ; les autres composants restent séparés
du futur adaptateur D3D12 natif Windows.

Le relevé réel des créations du jeu reste à obtenir. L'audit des états GPL,
ressources, alias, `LOAD` et barrières peut continuer hors jeu. Il précède le
raccordement d'une reprise admissible, des traitements suivants, des poses
prédites et de SteamVR. Les captures privées de la page Battle.net, les journaux
et les shaders ne font pas partie de cette preuve publique.
