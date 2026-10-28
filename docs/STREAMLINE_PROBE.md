# Étape 2 : observer la caméra de Diablo IV

La sonde 0.2.0 prépare l'intégration au jeu après la validation du laboratoire
dans le Quest 2. Elle observe des paramètres transmis à NVIDIA Streamline.
Elle ne produit pas encore de deuxième vue et ne copie pas les pixels GPU.

**Résultat des essais réels : relais refusé par la vérification de signature
de Diablo IV.** L'original est restauré et vérifié. L'installation du relais est
désormais désactivée dans ce profil ; les tests et le staging hors du jeu restent
disponibles.

## Faits observés sur l'installation

- Diablo IV : `3.2.3.73883`, exécutable Windows x64.
- `sl.interposer.dll` et `sl.common.dll` : version `2.12.0.0`.
- La bibliothèque interposer expose 262 symboles nommés.
- L'exécutable contient les noms `slSetConstants`, `slSetTagForFrame` et
  `sl.interposer.dll`. Cela indique un chemin d'intégration potentiel ; un appel
  effectif pendant le jeu doit encore être observé.
- Les journaux historiques contiennent des messages Streamline et indiquent
  DLSS actif. Ils contiennent aussi l'argument `-nostreamline` : son effet sur les
  données réellement émises reste à vérifier. La sonde ne change pas cet argument.
- GPU et pilote relevés sur l'hôte : RTX 5090, NVIDIA `615.78.08`.
- Le test Wine utilise le runner installé `dwproton-11.0-14`, dans un préfixe
  séparé du préfixe Battle.net.

L'API Streamline prévoit des constantes de caméra et des ressources identifiées
par type. Cela fournit une piste pour localiser la caméra du moteur et ses
buffers, sans donner à lui seul le moyen de rendre une deuxième vue.

## Fonctionnement

La sonde est une bibliothèque relais Windows x64. Elle conserve les noms et
ordinaux des 262 exports de la bibliothèque NVIDIA identifiée. Cinq appels sont
observés : initialisation, arrêt, constantes de caméra et les deux API de tags.
Les autres exports sont transmis par le chargeur Windows à la copie originale.

Les paramètres reçus sont transmis tels quels. Le code de retour original est
conservé. Aucun shader, paramètre de caméra ou contenu de texture n'est modifié.
Les objets GPU et les tokens de frame restent opaques.

Les observations contiennent les valeurs de caméra, projections, viewport,
types de buffers, dimensions déclarées, états et durées de validité. Quand
l'appel vient de Diablo IV, la sonde relève son adresse de retour relative au
module afin d'aider la prochaine analyse du code appelant. Elle n'enregistre
ni adresses absolues du processus ni données de compte.

Le journal JSONL est limité à 1024 événements par processus. Les constantes et
les groupes de tags sont échantillonnés une fois tous les 60 appels de leur
catégorie par défaut. Les valeurs numériques invalides deviennent `null` et les
versions de structures inconnues sont signalées. La validité numérique des
matrices ne prouve pas leur utilisation par le renderer.

## Contrôles avant installation

L'outil compare les SHA-256 du jeu et de la DLL originale au profil fixé. Il
vérifie aussi les exports et forwarders de la sonde. Il refuse les versions
différentes, les fichiers de sauvegarde préexistants et une installation pendant
que Diablo IV tourne. Une mise à jour du jeu nécessite un nouvel audit.

L'installation sauvegarde la DLL sous `d4vr_streamline_original.dll`, place la
sonde sous le nom `sl.interposer.dll`, puis enregistre un état de restauration.
Le fichier `d4vr-probe-output.txt` donne un chemin de journal hors du jeu ; cela
fonctionne même si Lutris était déjà ouvert avant l'essai.

Le mécanisme ne modifie aucune vérification de signature. La présence de la
chaîne `WinVerifyTrust` dans l'exécutable et les recommandations de chargement
sécurisé du SDK rendent un refus du relais possible. Un tel refus doit être
documenté et la bibliothèque originale restaurée ; il ne prouve pas que les
constantes de caméra sont inaccessibles par toutes les autres voies.

### Premier lancement sur le PC de référence

Au premier essai du 24 octobre 2026, Battle.net a restauré la DLL NVIDIA avant
de lancer le jeu. Son journal Agent indique `File sl.interposer.dll has invalid
residency, hashing.` à 12:55:19 UTC ; le fichier original a été rétabli à 12:55:22.
Le processus de Diablo IV a chargé cette DLL originale, sans charger la copie
renommée du relais. Aucun événement de la sonde n'a donc été produit pendant
les déplacements confirmés par le propriétaire.

Au second essai, le relais a été installé après l'ouverture de Battle.net et
est resté en place au lancement. Le journal du jeu a indiqué `Unable to verify
Streamline library signature` à 13:04:39 UTC. Le processus n'a chargé aucun
module Streamline et aucun journal de la sonde n'a été créé.

Après fermeture du jeu, la DLL NVIDIA originale a été restaurée. Les empreintes
de la DLL et du jeu correspondent au profil ; les fichiers de sauvegarde et de
configuration de la sonde ont été retirés. Aucune vérification n'a été modifiée.

## Commandes de développement

Depuis la racine du dépôt, avec le chemin réel du jeu :

```sh
python3 tools/probe.py audit --game-dir "/chemin/Diablo IV"
python3 tools/probe.py stage --game-dir "/chemin/Diablo IV" --probe "/chemin/sl.interposer.dll" --output work/vendor-staging
```

`stage` prépare un essai hors du jeu. Le test `d4vr_vendor_loader.exe`, copié
dans ce dossier, résout les exports avec la véritable DLL NVIDIA. Il ne crée
pas de périphérique graphique et ne lance pas Diablo IV.

La commande d'installation refuse désormais ce profil incompatible. La commande
de restauration reste disponible pour une ancienne installation, jeu fermé :

```sh
python3 tools/probe.py report "/chemin/absolu/capture.jsonl"
python3 tools/probe.py uninstall --game-dir "/chemin/Diablo IV"
```

Sous Wine, le journal utilise le lecteur `Z:` standard. `D4VR_PROBE_LOG` peut
fournir un chemin Windows absolu pour un lancement particulier ; il a priorité
sur le fichier de configuration. `D4VR_PROBE_STRIDE=1` est réservé aux tests.

La restauration vérifie la sauvegarde et le fichier actuellement installé.
Elle refuse d'écraser une nouvelle DLL déposée entre-temps par une mise à jour.

## Preuves disponibles

- Tests d'observation compilés avec GCC et exécutés sur l'hôte : réussis.
- Tests d'observation Windows x64 exécutés sous DW-Proton : réussis.
- Programme contrôlé : paramètres inchangés, codes de retour et export relayé
  vérifiés sous Wine ; configuration par fichier et plafond de journal vérifiés.
- Résolution des 262 exports avec la vraie DLL NVIDIA, hors du jeu : réussie.
- Tests Python : format PE, installation/restauration, échec de copie, versions
  différentes et préservation d'une mise à jour : réussis.
- [Contrôles GitHub Actions Linux et Windows](https://github.com/ffouret/D4VR/actions/runs/40095580572) :
  réussis, dont compilation MSVC, tests du relais et vérification des 262 noms,
  ordinaux et forwarders sur la DLL Windows produite.
- Deux essais réels : restauration par Battle.net, puis refus de signature par
  le jeu ; aucune constante capturée.
- DLL originale restaurée, empreinte vérifiée et fichiers de la sonde retirés.
- Stéréo géométrique de Diablo IV : pas encore implémentée.

Les bibliothèques de test synthétiques restent dans un dossier séparé. La DLL
NVIDIA et l'exécutable de Diablo IV ne sont pas distribués dans les livrables.

## Suite de l'intégration

La piste suivante est le chemin DirectX 12. Sous Bazzite, VKD3D-Proton dispose
d'une fonction de relevé des shaders, activée pour un lancement par une variable
d'environnement. Elle peut aider à identifier les programmes de géométrie et
les buffers qui portent les transformations. Ce relevé ne fournit pas les valeurs
de caméra en direct et ne crée pas deux vues. Le point qui permet de dessiner
la géométrie pour chaque œil doit encore être trouvé ; modifier les constantes
envoyées à DLSS ne suffirait pas à changer une image déjà dessinée.

## Sources

- [SDK Streamline 2.12, révision exacte utilisée](https://github.com/NVIDIA-RTX/Streamline/tree/e8aaa6eaac968711fb62473d4ae8256dde20919b)
- [Constantes de caméra dans ce SDK](https://github.com/NVIDIA-RTX/Streamline/blob/e8aaa6eaac968711fb62473d4ae8256dde20919b/include/sl_consts.h)
- [API de constantes et de tags](https://github.com/NVIDIA-RTX/Streamline/blob/e8aaa6eaac968711fb62473d4ae8256dde20919b/include/sl_core_api.h)
- [Chargement et intégration manuelle](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideManualHooking.md)
- [Relevé de shaders VKD3D-Proton](https://github.com/HansKristian-Work/vkd3d-proton#advanced-shader-debugging)
