# Transformation géométrique pilotable par œil

Le helper `stereo_transform.vert` reçoit une transformation de vue et une
projection par un petit buffer GPU. Il récupère la position homogène dans
l'espace de vue à partir de la projection courante du jeu, puis applique les
deux matrices avant rasterisation. Il ne lit aucune image du bureau ni texture
de profondeur. Les coefficients de profondeur et le jitter de la projection
source sont utilisés tels qu'ils apparaissent dans les constantes du shader.

Ce composant ne duplique pas encore les passes du jeu et ne soumet aucune
image à SteamVR. Le test intégré au jeu utilise un seul œil fixe par lancement.
Les mouvements de tête ne pilotent pas encore Diablo IV.

## ABI et contrôle Vulkan

Le paquet `EyeState`, partagé par le code C++ et GLSL, mesure 176 octets avec
un alignement de 16. Les matrices sont rangées par lignes, multipliées par des
vecteurs colonnes, dans un espace droit regardant vers −Z. Le paquet comprend
un magic, une version, une séquence, l'indice de l'œil, les deux matrices et le
filtre de projection source. Il peut choisir la projection de l'œil ou conserver
la projection courante du jeu. Une adresse nulle ou un paquet inactif/incompatible
laisse la position de sortie inchangée.

La spécialisation 4 transporte l'adresse du buffer. L'instrumentation refuse
un shader original utilisant déjà cet ID et ajoute le marqueur explicite
`D4VR_STEREO_ABI_1`. La couche spécialise uniquement les vertex shaders portant
ce marqueur. Elle préserve les autres constantes et leurs octets, y compris
lorsqu'une ancienne entrée 4 partage sa plage de données avec une autre entrée.
Les modules inline, bibliothèques graphiques et identifiants de cache sont
pris en compte. Les autres shaders sont transmis sans changement.

Le contrôle de couche est activé uniquement par `D4VR_EYE_STATE_FILE`. Le buffer
est créé sur le périphérique de l'application avec la fonction de buffer device
address déjà activée par celle-ci. Son contenu est initialisé avant tout dessin
et reste **immuable pendant le lancement**. Cela évite une mise à jour globale
pendant que plusieurs queues peuvent lire le paquet. Les ressources allouées
par la couche sont libérées avant la destruction du périphérique ; aucune
ressource empruntée au jeu n'est libérée.

La fixture démontre séparément les mises à jour de paquets avec
`vkCmdUpdateBuffer` et les barrières transfert/vertex. Ce chemin dynamique est
réservé à la fixture ; il n'est pas injecté dans les soumissions du jeu. Le
prochain renderer devra gérer les paquets, cibles et synchronisations par œil.

## Vérifications sur RTX 5090

La fixture synthétique compare onze images à des rendus indépendants du shader
original, dont les constantes de caméra/modèle sont modifiées pour représenter
chaque œil. **Aucun octet ne diffère**. Les cas couvrent une séparation de 0,064
unité de vue, un déplacement et une rotation de tête simulés, une projection
asymétrique, le jitter, des plans de profondeur variables et une coordonnée
homogène différente de 1. Dix-huit échantillons de sommets vérifient aussi les
coordonnées de clip et les identifiants du paquet.

L'écart mesuré entre les deux images est de 7,29788 pixels à deux unités et
3,65839 à quatre unités. Il dépend de la profondeur. Les états nuls, invalides,
désactivés et les projections hors filtre conservent l'image initiale.

Dix-huit exécutions locales couvrent la fixture dynamique, le raccordement par
la couche pour gauche/droite/désactivé, les pipelines issus du code ou du cache,
Steam Linux Runtime 4 et les sondes anciennes non marquées. Le buffer possédé
par la couche est effectivement consommé et les images correspondent exactement
aux références aux deux distances. Les fixtures ferment maintenant leurs
ressources explicitement après avoir attendu leurs propres travaux GPU.

La préparation locale valide aussi 2 113 overrides du jeu par `spirv-val`.
La suite Python compte 81 tests ; les tests C++ vérifient le paquet portable,
la sélection des shaders, les spécialisations chevauchantes et le relais Vulkan.
Ces preuves décrivent les fixtures ; elles ne prouvent pas deux vues d'une même
trame de Diablo IV ou une intégration VR jouable.

## Relevé dans Diablo IV

L'essai sur Diablo IV 3.2.3.73883, GE-Proton11-11 et VKD3D-Proton 3.1.0
utilise l'œil droit, séquence 1, translation −0,032 unité de vue et projection
courante du jeu. Le paquet est créé par la couche sur le périphérique du jeu.
Un relevé intermédiaire contient **257 échantillons complets** : 132 sommets
transformés conformément au calcul, dont 118 dans le volume de clip initial,
et 125 sommets hors cible inchangés. Vingt identifiants de shaders participent
aux transformations mesurées. Aucune incohérence, ligne rejetée ni paire de
mesures incomplète n'est signalée dans ce relevé.

Cela prouve que le helper exécuté dans le jeu consomme les paramètres d'œil
fournis par la couche. Cela ne prouve ni la couverture complète du décor, ni la
correction de l'éclairage et des effets temporels après déplacement. Le
propriétaire est entré en partie et ne remarque rien de spécial ; le décalage
choisi est volontairement faible. Aucune image de Diablo n'est soumise à SteamVR.

## Essai isolé sur le moniteur

Construire la couche avec `D4VR_BUILD_VULKAN_TRACE=ON` et la fixture avec
`D4VR_BUILD_GPU_PROBE_FIXTURE=ON`. Compiler le helper sans inlining et préparer
les overrides avec `camera_bundle.py --stereo`. Le mode de capture exige les
empreintes exactes d'une couche validée avec `eye_control_fixture_passed=true`
et d'un bundle `eye_transform_diagnostic`. Il conserve les DLL originales.

```sh
python3 tools/eye_control.py --eye right --output /travail/oeil-droit.bin
python3 tools/eye_capture.py --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --shader-bundle /travail/overrides-valides --layer-bundle /travail/couche-validee --state /travail/oeil-droit.bin --output /travail/session-neuve
python3 tools/eye_report.py /travail/session-neuve/eye.log --state /travail/session-neuve/eye-state.bin --output /travail/rapport-oeil.json
```

Ce lanceur limite volontairement le paquet à une translation latérale fixe,
avec la projection actuelle du jeu. La séparation est exprimée en unités de vue
du jeu ; aucune équivalence avec des mètres n'est établie. Le casque reste posé.
Le rapport vérifie les identifiants et le déplacement des sommets en tenant
compte de l'arrondi du journal et de la précision des calculs GPU. Il ne marque
jamais la stéréo du jeu ou la soumission au casque comme prouvées.

- [Mise à jour d'un petit buffer Vulkan](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdUpdateBuffer.html)
- [Constantes de spécialisation](https://docs.vulkan.org/refpages/latest/refpages/source/VkSpecializationInfo.html)
