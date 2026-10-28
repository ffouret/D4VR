# Deux yeux et cibles multiples hors du jeu

Le composant `native/vulkan/stereo_targets` alloue deux ensembles indépendants
de quatre images couleur et d'une profondeur/stencil, avec un paquet `EyeState`
immuable par œil. Le test utilise les formats du groupe candidat relevé dans
Diablo IV : **44, 64, 44, 50 et 129**, à une résolution de contrôle de 256 × 256.
Il ne copie aucun contenu ni shader du jeu.

Ce composant n'est pas encore raccordé à la couche chargée par Diablo. Le mode
initial de la fixture écrit explicitement les dessins d'une scène originale.
Le mode `--recorded-replay` utilise désormais le
[moteur de capture/reprise contrôlée](GRAPHICS_REPLAY.md) pour produire les yeux.
Ce mécanisme ne reprend pas encore les commandes du jeu.
Aucune image de Diablo n'est soumise à SteamVR.

## Preuve géométrique

Chaque exécution positive couvre quatre scènes : un objet à deux unités, un
objet à quatre unités, une occultation de deux objets, puis une vue avec
rotation/translation de tête simulées et projections asymétriques. Chacune est
enregistrée deux fois pour vérifier la réutilisation des cibles.

Chaque soumission contient les deux yeux d'un **même état de scène figé**, ainsi
que des rendus de référence indépendants. Ces références emploient le vertex
shader original avec des constantes de caméra/vue transformées sur le CPU ;
les deux yeux emploient le helper géométrique et deux adresses de paquet
différentes. Il n'y a aucune modification CPU des matrices entre les deux yeux
d'une soumission.

Seize rendus d'œil sont comparés : 16 777 216 octets couleur, 1 048 576 valeurs
de profondeur et autant d'octets stencil. Les essais RTX 5090 ne trouvent aucune
différence. L’écart entre yeux mesure 7,29357 pixels à deux unités et 3,64194 à
quatre. Dans la scène avec obstacle, 1 369 texels par enregistrement montrent
une surface lointaine dans un œil, masquée par le premier plan dans l’autre. Le contrôle de profondeur accepte au maximum quatre unités de
quantification D24, pour tenir compte de l'arithmétique flottante ; les huit bits
indéfinis des copies de profondeur D24 sont exclus de la comparaison.

Un rendu témoin est lu avant et après le travail stéréo, sans être redessiné
entre les deux lectures. Ses données définies restent inchangées. Les tests
vérifient que les images et paquets des yeux et des références ne partagent
aucun handle. Cela prouve l'isolation de ces ressources de contrôle ; aucun
état graphique du jeu n'est intercepté ou restauré par ce test. Le nouveau mode
de reprise ajoute un témoin de restauration pour le flux synthétique, décrit
séparément dans [GRAPHICS_REPLAY.md](GRAPHICS_REPLAY.md).

## Propriété et synchronisation

Le périphérique, les queues et les command buffers sont empruntés. Le composant
ne les crée ni ne les détruit et n'effectue aucune soumission ni attente. Il
libère uniquement ses vues, images, buffers et allocations. Les fonctions
Vulkan passent par la table fournie par l'appelant pour permettre un futur
usage dans une couche.

L'appelant doit activer buffer device address et dynamic rendering, vérifier
les formats, garantir la propriété de queue et attendre la fin du travail avant
de réutiliser ou détruire les ressources. Il fournit explicitement l'utilisation
antérieure des images : contenu abandonné ou source de transfert. Le composant
enregistre les transitions vers les attachements puis vers les sources de
transfert. Il ne maintient pas d'état de layout global entre enregistrements qui
pourraient être exécutés dans un autre ordre.

La fixture utilise une queue et une fence par soumission. Elle ne démontre pas
la synchronisation entre les multiples queues du jeu. Les paquets sont fixes
pour toute la durée de chaque cas ; l'alimentation de poses prédites à chaque
trame et les ressources par trame en vol restent à concevoir.

Un test C++ sans pilote injecte des échecs aux **48 étapes de création,
allocation, liaison et mapping**. Il vérifie le nettoyage partiel, l'ordre de
libération, le rejet des sorties non définies d'un appel en échec et l'absence
de destruction du périphérique emprunté. Les assertions restent actives dans
une compilation Release de ce test.

## Continuité entre passes et aperçu

Les six scopes candidats des deux relevés réels réutilisent tous leurs quatre
couleurs, profondeur et stencil avec `LOAD`. Ils représentent respectivement
171 et 65 commandes de dessin. Les états graphiques, lectures, barrières et
valeurs de clear ne sont pas encore intégralement enregistrés : ces scopes ne
forment donc pas un flux rejouable.

Le composant expose maintenant séparément la préparation des images, la
description du rendu, son démarrage et la transition de lecture. La description
possède ses tableaux d'attachements et ne peut être copiée ou déplacée ; ses
pointeurs restent valides pendant sa durée de vie. Les clear values sont
conservées et les load ops de profondeur et stencil sont indépendants.

La fixture `--split-passes` dessine le premier objet dans une passe avec clear,
puis le second dans une passe avec `LOAD`, dans chaque œil. Les références
indépendantes dessinent les deux objets dans une seule passe. Le résultat final
correspond exactement pour les quatre couleurs, la profondeur et le stencil,
y compris avec projections asymétriques. Vingt-quatre passes d'œil produisent
les seize vues finales comparées. Les scopes de contrôle sont encore enregistrés
explicitement dans ce mode initial. L'option supplémentaire `--recorded-replay`
confie maintenant l'émission des deux yeux au moteur de reprise.

`--preview-directory /travail/apercus-neufs` exporte les vues gauche/droite de
la scène avec obstacle en PPM RGB, après la vérification des références. Les
pixels proviennent de l'attachement couleur 0 lu sur le GPU. Ce sont deux
triangles originaux, pas des images de Diablo. Le contrôle négatif refuse cet
export. L'option peut être combinée à `--split-passes`.

Le test sans pilote peut aussi être construit avec
`D4VR_BUILD_STEREO_TARGET_TESTS=ON` et
`D4VR_VULKAN_INCLUDE_DIR=/chemin/Vulkan-Headers/include`. Il ne lie aucune
bibliothèque du pilote. La CI Windows récupère les headers Khronos v1.3.275 et
compile/exécute ce composant commun avec MSVC. Cela ne valide pas un backend
D3D12 ni une session de Diablo sous Windows.

## Validation et reproduction

`tools/stereo_mrt_validate.py --split-passes` vérifie la continuité entre passes.
Sans ce flag, il vérifie le chemin initial à une seule passe. Le script lance le test natif, la validation Vulkan avec
synchronisation, un contrôle négatif, puis Steam Linux Runtime si fourni. Le
contrôle négatif écrit volontairement dans une plage de buffer sans la
dépendance requise : il doit déclencher `SYNC-HAZARD-WRITE-AFTER-WRITE` et ne
peut pas être déclaré comme un rendu positif validé. Les autres exécutions
doivent charger la couche et ne signaler aucune erreur Vulkan ni de
synchronisation. Les journaux restent dans un dossier privé.

Construire avec `D4VR_BUILD_GPU_PROBE_FIXTURE=ON`, puis compiler les shaders :

```sh
glslangValidator -V --target-env vulkan1.1 -Od native/probe_gpu/stereo_transform.vert -o /travail/helper.spv
glslangValidator -V --target-env vulkan1.1 native/tests/stereo_mrt.vert -o /travail/mrt.vert.spv
glslangValidator -V --target-env vulkan1.1 native/tests/stereo_mrt.frag -o /travail/mrt.frag.spv
python3 tools/spirv_probe.py /travail/mrt.vert.spv --helper /travail/helper.spv --tools /chemin/spirv-tools --output /travail/mrt.stereo.spv --stereo
python3 tools/stereo_mrt_validate.py --fixture /build/d4vr_stereo_targets_fixture --original /travail/mrt.vert.spv --instrumented /travail/mrt.stereo.spv --fragment /travail/mrt.frag.spv --validation-layer /chemin/manifest-khronos --runtime /chemin/SteamLinuxRuntime_4/run --output /travail/releve-neuf
```

Le dossier de validation doit contenir `VkLayer_khronos_validation.json` avec
un chemin de bibliothèque résolu sur l'hôte. Aucune installation ni modification
globale de Vulkan n'est nécessaire pour cette procédure. La CI compile le
composant et les shaders et exécute le test de propriété sans GPU ; les rendus
GPU sont vérifiés séparément sur la machine de référence. Trois exécutions
positives et le contrôle négatif passent, dont Steam Linux Runtime 4 avec
validation de synchronisation.

- [Rendu dynamique et attachements](https://docs.vulkan.org/refpages/latest/refpages/source/VkRenderingInfo.html)
- [Copies d'images et profondeur D24](https://docs.vulkan.org/spec/latest/chapters/copies.html)
- [Configuration de la validation de synchronisation](https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/main/docs/updating_from_VK_EXT_validation_features.md)
