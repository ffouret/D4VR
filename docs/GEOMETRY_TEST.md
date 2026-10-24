# Test visuel de déplacement géométrique

Ce test expérimental déplace les sommets avant rasterisation, dans une seule
vue de Diablo IV. Il ne constitue pas un mode VR ni une paire stéréo.

## Déplacement appliqué

Le helper original `native/probe_gpu/geometry_shift.vert` vise la projection
mesurée dans la seconde capture : 35° verticaux, rapport 16:9. Il vérifie les
coefficients et la forme perspective avant toute modification. Les autres
projections conservent leurs sommets d'origine.

Dans cette convention droite, `clip.w = -view.z` et
`clip.z = projection[2][2] × view.z + projection[2][3] × view.w`.
Le helper retrouve donc `view.w` à partir de la profondeur, puis applique le
déplacement d'un œil de +0,10 unité de vue :

```text
view.w = (clip.z + projection[2][2] × clip.w) / projection[2][3]
clip.x -= projection[0][0] × 0,10 × view.w
```

Le décalage à l'écran varie avec la distance de chaque sommet. Il n'est pas
obtenu en déplaçant une image finale. L'unité de vue du jeu n'est pas encore
calibrée en mètres pour le casque.

Le préparateur conserve les 2 113 programmes dont les deux matrices sont
reconnues, lie le helper, puis valide chaque SPIR-V. Il lit le `Position`
original avant le retour et écrit le résultat modifié dans cette même sortie.
Les interfaces à sortie autonome ou membre de `gl_PerVertex` sont supportées.

## Preuve sur la RTX 5090

La fixture originale rend un triangle avec la projection mesurée, à deux
distances, dans une image 256 × 256. Le déplacement horizontal du centre des
pixels est comparé à la prédiction géométrique :

| Distance | Déplacement GPU mesuré |
| --- | ---: |
| 2 unités | −11,4232 pixels |
| 4 unités | −5,72029 pixels |

La tolérance tient compte de la rasterisation en pixels. Doubler la distance
divise environ par deux le décalage. Une projection hors cible conserve tous
ses octets rendus. Les tests de l'observateur sans déplacement passent aussi.
Cette preuve concerne une fixture ; elle ne démontre pas la cohérence des
ombres ou effets temporels de Diablo IV.

## Préparer puis lancer

Compiler le helper avec les mêmes outils que dans
[l'observation GPU](CAMERA_OBSERVER.md) :

```sh
glslangValidator -V --target-env vulkan1.1 -Od native/probe_gpu/geometry_shift.vert -o /travail/geometry.spv
```

La commande `camera_bundle.py` décrite dans ce document accepte `--geometry`,
avec ce helper et la fixture native actualisée. Elle exécute le mode
`--geometry` de la fixture, puis valide tous les programmes. Le manifeste
porte le but `geometry_shift_diagnostic` ; aucun bytecode du jeu n'est publié.

```sh
python3 tools/camera_capture.py run --geometry-test --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --bundle /travail/bundle-geometrie --output /travail/test-neuf
```

Le mode explicite doit correspondre au manifeste. Le lanceur vérifie les
empreintes du jeu, de la DLL NVIDIA originale et du runtime VKD3D, puis prépare
un lancement isolé. Le ring de capture de matrices n'est pas activé dans ce mode.
Les fichiers du jeu et les réglages permanents de Lutris restent d'origine.

Entrer en partie et observer le décor, le personnage et les ombres environ une
minute, puis fermer le jeu et Battle.net. Relever les dédoublements, les ombres
décalées ou les éléments disparus. Le test utilise le moniteur ; le casque n'est
pas nécessaire. La fermeture termine le mode ; le lancement Lutris habituel
n'emploie pas les substitutions de ce test.

## Ce que cet essai ne traite pas

Le test ne modifie pas les autres varyings, les vecteurs de mouvement ou les
historiques. Il ne déplace pas nécessairement tous les programmes de géométrie
ou les calculs de culling. Ces limites peuvent apparaître sous forme d'artefacts
et servent à identifier les passes à traiter avant un rendu par œil complet.
La profondeur reste celle du sommet, mais les calculs par pixel dépendant de
la caméra n'ont pas encore été adaptés. Aucune image n'est envoyée à SteamVR.
