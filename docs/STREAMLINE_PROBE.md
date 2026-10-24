# Étape 2 : observer la caméra de Diablo IV

La sonde 0.2.0 prépare l'intégration au jeu après la validation du laboratoire
dans le Quest 2. Elle observe des paramètres transmis à NVIDIA Streamline.
Elle ne produit pas encore de deuxième vue et ne copie pas les pixels GPU.

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

## Commandes de développement

Depuis la racine du dépôt, avec le chemin réel du jeu :

```sh
python3 tools/probe.py audit --game-dir "/chemin/Diablo IV"
python3 tools/probe.py stage --game-dir "/chemin/Diablo IV" --probe "/chemin/sl.interposer.dll" --output work/vendor-staging
```

`stage` prépare un essai hors du jeu. Le test `d4vr_vendor_loader.exe`, copié
dans ce dossier, résout les exports avec la véritable DLL NVIDIA. Il ne crée
pas de périphérique graphique et ne lance pas Diablo IV.

Pour installer la sonde après les tests, jeu fermé :

```sh
python3 tools/probe.py install --game-dir "/chemin/Diablo IV" --probe "/chemin/sl.interposer.dll" --output "/chemin/absolu/capture.jsonl"
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
- Sonde installée sur le PC de référence, jeu fermé ; empreintes de la DLL
  installée, de la sauvegarde originale et de la configuration vérifiées.
- Données émises par Diablo IV pendant une session réelle : pas encore relevées.
- Stéréo géométrique de Diablo IV : pas encore implémentée.

Les bibliothèques de test synthétiques restent dans un dossier séparé. La DLL
NVIDIA et l'exécutable de Diablo IV ne sont pas distribués dans les livrables.

## Suite selon les observations

Si les constantes sont reçues, examiner le code appelant et vérifier qu'il
correspond à la caméra de la scène. Si des tags profondeur/couleur sont reçus,
établir leur synchronisation et la possibilité de les lire correctement sur GPU.
Ensuite seulement rechercher le point de rendu qui peut être exécuté pour deux
vues cohérentes. Modifier les constantes envoyées à DLSS ne suffirait pas à
modifier la caméra qui a déjà servi à dessiner la géométrie.

## Sources

- [SDK Streamline 2.12, révision exacte utilisée](https://github.com/NVIDIA-RTX/Streamline/tree/e8aaa6eaac968711fb62473d4ae8256dde20919b)
- [Constantes de caméra dans ce SDK](https://github.com/NVIDIA-RTX/Streamline/blob/e8aaa6eaac968711fb62473d4ae8256dde20919b/include/sl_consts.h)
- [API de constantes et de tags](https://github.com/NVIDIA-RTX/Streamline/blob/e8aaa6eaac968711fb62473d4ae8256dde20919b/include/sl_core_api.h)
- [Chargement et intégration manuelle](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideManualHooking.md)
