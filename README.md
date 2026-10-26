# D4VR

Projet expérimental pour jouer à **Diablo IV en VR stéréoscopique, depuis une
position surélevée au-dessus du personnage et du décor**.

L'objectif est de percevoir le monde du jeu en volume et de pouvoir observer le
décor avec les mouvements de la tête. La hauteur d'observation et l'échelle du
monde doivent être réglables. Une capture du bureau affichée dans le casque ne
répond pas à cet objectif.

## État du projet

Le [relevé par dessin](docs/SAMPLED_WRITER_STATE.md) conserve maintenant les
push constants et les déclarations viewport/scissor dans les préfixes des images
livrées. Quatre contrôles en fenêtre préservent les pixels originaux et comparent
seize états aux vrais appels de leur application, sans erreur Khronos.
Un [nouvel essai Diablo](docs/proofs/diablo-sampled-writer-state-v1.json) sauve
six paires successives et les 28 octets push de onze dessins du writer ciblé.
Un flag de pipeline non modélisé maintient leur compatibilité de layout et leurs
viewport/scissor inconnus : le rectangle ne peut pas encore leur être attribué.
Le jeu est sorti normalement, les originaux restent identiques ; les découpes
persistent et aucune image de Diablo n’a atteint le Quest.

Le [contrôle ciblé de la passe couleur](docs/SAMPLED_WRITER_CONTROL.md) utilise
ses deux modules originaux inchangés sur des textures possédées. Dix-huit cas
GPU en natif et dans le runtime Steam comparent exactement 265 420 800 octets,
sans erreur de synchronisation Khronos. Ils distinguent l’image choisie, le
décalage UV, le sampler et la couverture viewport/scissor. Les valeurs réelles
de sélection et de couverture dans Diablo restent à établir ; les découpes ne sont pas réparées.

Les [préfixes attachés aux captures](docs/CAPTURED_WRITERS.md) suivent maintenant
les opérations du jeu sur ses véritables images sources. Un nouvel essai relie
onze captures à une déclaration couleur et au même pipeline, avec les identités
et les traces de soumission conservées ; une première capture garde un préfixe
vide. L’entrée échantillonnée et l’état effectif de dessin restent à établir.
Les découpes persistent et aucune image de Diablo n’a atteint le Quest.

Le [lien entre une capture couleur et son transfert d’origine](docs/COLOR_BLIT_PROVENANCE.md)
est maintenant conservé jusqu’à sa livraison. Un nouvel essai réel identifie trois
images sources précises ; le filtrage évite la saturation observée auparavant.
Les contrôles en fenêtre et les tests CPU passent. Les passes suivies ne relient
pas encore ces sources à la profondeur, et certaines traces secondaires manquent.
Les découpes persistent : aucune image de Diablo n’a atteint le Quest.

Un [observateur des utilisations d’images](docs/SOURCE_CONSUMERS.md) ajoute
des requêtes de descripteurs, les attachements couleur déclarés et les extrémités
des transferts dans la même session que la profondeur. Quatre contrôles en
fenêtre préservent 67 108 864 octets de références, sans erreur Khronos.
 Ces observations préparent l’analyse des découpes : une requête de descripteur
ne prouve ni sa sélection ni une lecture du shader, et la liaison aux passes antérieures
reste à établir.

L’analyse des quatre shaders sans variante a isolé deux calculs de profondeur
appliqués après la projection. Le [nouveau contrôle géométrique](docs/POST_PROJECTION_DEPTH.md)
préserve ces calculs et corrige leurs unités de clip : 144 références GPU passent
en natif et dans le runtime Steam, sans erreur Khronos. Deux candidats privés
sont désormais admis dans un diagnostic monoscopique distinct. Un
[nouvel essai réel](docs/proofs/diablo-post-depth-v1.json) observe leur sélection
sur 144 dessins retenus ; les originaux restent identiques et le jeu est fermé.
Dans une région ciblée, la profondeur suit la projection tandis que la couleur
reste différente. Les grandes découpes persistent au champ élargi : la couverture
n’est pas réparée et aucune image de Diablo n’a atteint le Quest.

Le [suivi de la source de profondeur](docs/SOURCE_HISTORY.md) relie maintenant les
lectures aux utilisations exactes de leur image, dans la même session. Un
[nouvel essai Diablo](docs/proofs/diablo-source-history-v1.json) garde trois ponts
vérifiés vers 21 scopes d’attachement et 38 pipelines d’application par historique.
Une déclaration de clear et 67 dessins sans variante privée sont observés ; leur
contribution réelle à la profondeur et la cause des trous restent à établir.
Les historiques sont partiels et ne prouvent pas l’initialisation des pixels.
Quatre contrôles en fenêtre préservent 67 108 864 octets de références, sans erreur
Khronos. Le jeu est sorti normalement, les originaux restent identiques et aucune
image de Diablo n’a atteint le Quest.

Le [diagnostic combiné profondeur/couleur sous une vue maintenue](docs/HELD_DEPTH_COLOR.md)
est maintenant implémenté. Quatre contrôles réels en fenêtre, natifs et dans le
runtime Steam, vérifient des triangles écrivant leur profondeur, les images couleur
et les paquets caméra exacts. Les 67 108 864 octets de références originales restent
identiques avec le diagnostic activé ; les demandes prématurées ou après arrêt sont
refusées. Les régressions et 16 contrôles de preuves altérées passent. Cela prépare
une comparaison des zones manquantes dans Diablo ; ce laboratoire ne prouve pas
encore leur cause et aucune nouvelle image du jeu n’est envoyée au casque.

Un [nouvel essai réel dans Diablo](docs/proofs/diablo-held-depth-color-v1.json)
obtient trois profondeurs D24 et six paires couleur successives : référence, champ
élargi et retour. Les vues sont maintenues plus de cinq secondes côté hôte, avec
paquets et identités d’origine conservés. Le relevé dure 18 secondes ; sortie
normale et originaux inchangés. La comparaison des régions manquantes reste
partielle, sans preuve d’un même instant ni d’une réparation de la couverture.
La comparaison retrouve 48 zones suivant l’optique en couleur et profondeur.
Certaines découpes ont une profondeur saturée ; une autre zone sombre garde une
profondeur variable qui diffère pourtant de la référence projetée. La cause ne
peut donc pas encore être attribuée au seul culling ou au seul traitement couleur.


Le [producteur borné de profondeur au point COPY](docs/DEPTH_COPY_READBACK.md)
est maintenant implémenté dans la couche Vulkan. Il conserve ses buffers et fences,
invalide les anciennes lectures avant une nouvelle soumission et préserve les
scopes originaux. Douze contrôles GPU en natif et dans le runtime Steam vérifient
des profondeurs géométriques, la couleur et le stencil originaux, les commandes
réutilisées, les dépendances futures et les fences applicatives, sans erreur de
synchronisation détectée. Ces scènes de test restent distinctes du jeu réel.

Un [nouvel essai dans Diablo réel](docs/proofs/diablo-depth-copy-v1.json)
exporte désormais **une profondeur D24 de 1707 × 960**, soit 6 554 880 octets et
223 074 valeurs distinctes. L’aperçu permet de reconnaître le personnage, le
terrain et la falaise. La caméra et les shaders restent originaux ; le jeu est
sorti normalement et les fichiers vérifiés sont identiques. Ce relevé brut ne
prouve pas l’initialisation de chaque pixel ni une profondeur complète de scène.
Les trous au champ élargi persistent et aucune image de Diablo n’a atteint le Quest.

Un [audit sans lecture GPU, essayé dans Diablo](docs/DEPTH_AUDIT.md), établit
que la [sonde de profondeur préparée](docs/DEPTH_READBACK.md) ne peut pas encore
y être activée : les scopes de synchronisation, commandes et descriptions de
ressources sortent de ses conditions admises. Sur 400 573 soumissions observées,
377 890 sortent de cette politique ; les 35 configurations de rendu avec profondeur
sont également exclues. Les appels du jeu sont transmis sans modification ni refus.
Le jeu est sorti normalement et les originaux sont préservés. Cette observation
avait identifié la lecture originale utilisée pour préparer le producteur COPY. Le nouveau producteur COPY est distinct
de cette ancienne sonde ; ses résultats et limites sont décrits ci-dessus.

Le [maintien explicite d’une vue diagnostique](docs/FIXED_VIEW_HOLD.md) corrige
une erreur du protocole précédent : cinq secondes entre les demandes ne
maintenaient pas la projection modifiée. Le nouveau pilote conserve la vue
monoscopique et ses reçus observent maintenant 871 à 874 présentations
consécutives sous le même paquet, sur environ six secondes côté hôte. Le nouvel
essai réel garde 25 zones de terrain suivant l’optique puis revenues, avec des
trous toujours visibles au champ élargi. Ces intervalles ne mesurent pas le temps
GPU et ne certifient pas l’historique de toutes les passes.

Le [nouveau diagnostic des fragments](docs/FRAGMENT_PROJECTION.md) suit les
consommateurs réellement associés aux pipelines observés. Quatre fragments
recalculent des coordonnées avec la projection source. La construction explicite
et la couche Vulkan peuvent maintenant leur transmettre le même paquet caméra
qu'aux sommets, avec barrières GPU adaptées. Les 48 références GPU par environnement
et les huit essais en fenêtre passent, avec contrôles négatifs.

Un nouvel essai **dans Diablo réel**, à vue fixe et sans casque, applique cette
correction à trois vertices et quatre fragments ciblés ; trois chargements de
fragments sont observés. Six paires successives couvrent référence, champ élargi
et retour, avec cinq secondes entre les demandes. Cette attente ne maintenait pas
la projection modifiée ; la qualification précédente de stabilisation est corrigée. Les 21 zones de terrain admises
suivent l'échelle optique et retrouvent leurs coordonnées, mais **les grandes zones
manquantes persistent au champ élargi**. Cette correction partielle ne suffit donc
pas à rétablir la couverture. Aucun rendu continu de Diablo dans le Quest n'est
encore établi.

Le [diagnostic de projections partenaires](docs/PARTNER_PROJECTION.md) et le
[relevé GPU sans modification de caméra](docs/CAMERA_BLOCK.md) conservent les étapes
précédentes : matrices voisines, routes originales et contrôles indépendants.
Les rôles temporels, les autres passes et le culling restent à distinguer.

Une [variante de projection en amont des calculs du vertex](docs/PROJECTION_INPUT.md)
conserve maintenant la cohérence des sorties qui dépendent de cette projection,
vérifiée par un contrôle GPU que la modification finale de Position échoue.
Son nouvel essai dans Diablo garde 22 zones de terrain décalées puis revenues,
mais les trous au champ élargi persistent. Cela oriente la suite vers les autres
passes et le culling ; la cause exacte et la couverture immersive restent ouvertes.

La [calibration de projection et de profondeur](docs/VIEW_CALIBRATION.md) a
maintenant été essayée dans **Diablo IV réel**, sans casque : six paires GPU
successives conservent les paquets caméra de leur production. Les 26 zones de
terrain acceptées se décalent en sens opposés pour les yeux, reviennent à leur
place et réagissent au déplacement vertical. La profondeur propre aux passes
sélectionnées est conservée à l'échelle choisie. En revanche, un champ de vision
fortement élargi laisse des trous visibles : la couverture immersive reste à
corriger. Aucune image de Diablo n'a encore été envoyée au Quest.

Le [raccordement coopératif de la couche Vulkan à OpenXR](docs/COOPERATIVE_LAYER_XR.md)
transmet maintenant les images du producteur GPU au présentateur, avec poses
issues de `xrLocateViews`, sans répéter les commandes de l'application. Les
16 contrôles en fenêtre Vulkan utilisent une scène possédée et un **runtime XR
simulé** : 7 864 320 octets d'images arrivées dans XR correspondent exactement
aux vues originales retenues. Les arrêts et les accès de queue refusés sont
vérifiés, avec 11 contrôles de non-régression. Ce nouveau chemin n'a pas été
lancé avec XR dans Diablo ou SteamVR physique. Les nouveaux contrôles à
profondeur conservée passent aussi avec ce runtime simulé ; les deux vues
restent successives et partielles, avec une échelle et une ancre provisoires.

Les preuves ci-dessous conservent les étapes antérieures et leurs limites au
moment de chaque essai.

Le [cinquième essai réel de vues successives](docs/SEQUENTIAL_CAPTURE.md#cinquième-essai-réel--plafond-de-pipelines-élargi-et-vérifié)
étend le décalage géométrique observé dans **Diablo IV, en 2560 × 1440** : les
24 zones de terrain acceptées se décalent entre les yeux et reviennent à leur
place au même œil. La lumière nocturne réduit le nombre de
zones mesurables ; ce relevé ne prouve pas la couverture complète du monde.
Le plafond élargi à 8 192 permet cette fois de conserver jusqu'à 6 445 graphes
privés, avec zéro refus de budget aux quatre captures. Ces images
successives ne sont pas encore envoyées à SteamVR et ne constituent pas un jeu
VR continu avec suivi de tête.

Le lecteur DXIL confondait les identifiants de ressources des buffers de caméra
avec ceux des textures ou samplers. Il suit maintenant uniquement la liste des
constant buffers dans la table de ressources. Le bundle privé passe de 2 679
à 5 378 projections reconnues, sans perdre les précédentes ; les modules passent
SPIR-V et les références GPU sans télémétrie. Les 170 tests Python incluent les
collisions de classes et les tables ambiguës. Les preuves historiques, dont le
troisième essai partiel à 21 zones décalées sur 36, et les pixels bruts sont conservés.

La [reprise figée vers SteamVR](docs/FROZEN_REPLAY_XR.md) affiche maintenant les
images issues de l'interception à 1024 × 1024 par œil : 890 paires suivies en dix
secondes, image plus nette, relief et mouvements confirmés dans le Quest. Cette
scène reste synthétique et le contrôle Vulkan signale encore des erreurs dans
des appels internes à SteamVR. Ce premier affichage ne constitue pas un rendu
VR de Diablo ni une preuve sans erreur de validation.

Le [présentateur OpenXR accepte aussi un producteur d'images GPU](docs/PROJECTION_IMAGES.md).
Il vérifie la correspondance des deux yeux avec la scène, la séquence et l'heure
de pose demandées avant acquisition des swapchains. Les pixels copiés par le GPU
sont comparés exactement aux références du programme, avec refus des paires périmées
et conservation de l'ancien chemin de reprise. Ces essais utilisent un runtime
XR simulé ; le producteur d'images cohérentes de Diablo reste à raccorder.
La session peut maintenant avancer par étapes en rendant la main entre les
attentes GPU, avec une seule paire en cours. Douze contrôles supplémentaires
vérifient ce cycle, les timeouts et les arrêts. Le cadencement XR et le producteur
réel du jeu demandent encore leur intégration.

Le [producteur de captures GPU réutilisables](docs/GPU_PRESENT_CAPTURE.md) existe
maintenant dans la couche de présentation. Son essai réel dans Diablo a livré
114 images avec leur œil et leur présentation d'origine, sans lecture CPU dans
ce chemin. Le mode diagnostique les retire sans consommateur d'images ni envoi
XR. Le pool et les lectures GPU sont vérifiés séparément sur des images
synthétiques ; les poses XR et le raccordement au présentateur restent à faire.

Les [premières observations de buffers et de liaisons de ressources](docs/RESOURCE_BINDINGS.md)
préparent le raccordement aux dessins du jeu. Elles conservent identités et
bornes observées, ainsi que les déclarations de layouts et les offsets fournis
par le pilote. Les sorties opaques de `vkGetDescriptorEXT` sont aussi conservées
dans les captures privées avec leur source observée, sans prouver leur usage
effectif par le GPU ni autoriser de reprise du jeu.
Le suivi ajoute les allocations, offsets de liaison, mappings et opérations de
cache observées, sans lire la mémoire ni prouver sa visibilité GPU.
Dans la [fixture propriétaire](docs/RESOURCE_BINDINGS.md#copie-stable-et-résolution-dans-la-fixture-propriétaire),
une copie stable du descriptor buffer permet maintenant de retrouver les deux
sources liées à quatre dessins et un dispatch. Le contrôle d'offset incorrect
retrouve la mauvaise paire de buffers, confirmée par le résultat compute.
Cette capture appartient au programme de test ; son équivalent dans Diablo
reste à implémenter.
La [copie GPU bornée](docs/RESOURCE_BINDINGS.md#copie-gpu-des-descripteurs-et-des-buffers-de-caméra)
récupère aussi ces descripteurs et les six buffers de caméra de la fixture,
sans modifier l'usage des sources. Les 9 456 octets correspondent aux sources ;
une copie provenant d'une mauvaise ressource est refusée. Le transport GPU
est commun aux plateformes ; son raccordement aux ressources du jeu reste ouvert.

La [preuve d'intégration de la couche Vulkan](docs/LAYER_REPLAY_POC.md) raccorde
maintenant les commandes d'une application au moteur de reprise : deux passes
originales produisent deux vues privées, comparées exactement à des références
GPU. L'image originale et la passe suivante restent identiques. Ce premier
chemin traverse réellement le chargeur Vulkan ; son admission est limitée aux
octets exacts de notre scène synthétique. Il ne reprend pas encore Diablo.
La preuve initiale décrite sur
cette page précède le raccordement SteamVR ci-dessus.

Le [helper peut maintenant être construit avec la seule projection](docs/PROJECTION_ONLY_HELPER.md),
après preuve que ses quatre arguments de matrice d'objet sont inutilisés. Les
dix vertex candidats, dont six auparavant refusés, passent la transformation et
le contrôle statique d'écritures. Les références GPU synthétiques restent
exactes ; aucun dessin du jeu n'est encore repris.

Un [contrat d'écritures du module complet](docs/SHADER_WRITE_CONTRACT.md) vérifie
maintenant les instructions de toutes les fonctions après validation SPIR-V.
Les dix-sept anciens candidats et les vertex transformés satisfont ce
prérequis statique. Les sorties,
ressources, alias et dépendances restent à auditer avant toute reprise du jeu.

Le [helper stéréo peut maintenant être compilé sans télémétrie](docs/STEREO_HELPER.md).
Ses écritures, atomiques et barrières de diagnostic sont alors absentes du
SPIR-V. Les essais GPU retrouvent les mêmes références géométriques pour les
deux yeux. Cela simplifie l'audit du helper ; les effets du shader original et
les ressources des passes du jeu restent à vérifier avant leur reprise.

Le [modèle des effets GLSL](docs/GLSL_EFFECTS.md) classe maintenant les opérations
étendues des anciens candidats et détecte les écritures par pointeur de
Modf/Frexp. Les inconnus restent bloquants. Cette analyse statique n'autorise
aucune reprise du jeu ; ses ressources et dépendances restent à résoudre.

Les [variantes privées neutralisent maintenant la télémétrie du helper](docs/REPLAY_SPECIALIZATION.md)
par leurs constantes de spécialisation. Les compteurs et le ring restent intacts
pendant la reprise synthétique, même si la création source fournit des adresses
actives ; un contrôle séparé écrit bien aux mêmes adresses. Cela ne remplace pas
l'audit des effets du shader complet et ne produit pas encore d'images de Diablo.

Le [contrat statique explicite](docs/STATIC_STATE.md) permet de reprendre un
pipeline dont la liste dynamique complète est vide, avec restauration des
états. Les contrôles GPU sans déclarations parasites passent. Un contrôle
renforcé révèle toutefois un écart sur le cas GPL statique avec déclarations
parasites ; ce cas reste une anomalie ouverte et ne constitue pas une preuve
positive. Les ressources doivent toujours être auditées séparément.

Le [calcul des déclarations dynamiques GPL](docs/GPL_DYNAMIC_STATES.md) filtre
les états par propriétaire et conserve le contrat dans les graphes privés. La
fixture reprend maintenant ses dessins avec cette liste calculée, y compris
avec des déclarations parasites : les références GPU restent exactes. Ce
résultat reste synthétique et ne raccorde aucun dessin du jeu.

Le [lanceur isolé de paires](docs/PAIRED_CAPTURE.md) prépare les deux paquets,
vérifie le préfixe et les empreintes, borne la session et ferme ses seuls
processus marqués. Ses essais ouvrent Battle.net ; aucun nouveau processus ou
rendu de Diablo n'a été confirmé. Les traces d'utilitaires ne sont pas prises
pour des preuves du jeu.

Le [diagnostic de paires dans la couche Vulkan](docs/PAIRED_PIPELINES.md) observe
les créations originales et compile les variantes privées de chaque œil, sans
les dessiner. Les essais GPU couvrent les pipelines monolithiques, GPL et les
identifiants issus d'un cache préparé. Ce raccordement reste testé hors de Diablo.

Le [chemin synchrone des bibliothèques GPL](docs/LIBRARY_VARIANTS.md) construit
deux graphes privés et les conserve par lease jusqu'à leur dernière utilisation.
Deux contrôles GPU distincts vérifient leur indépendance des sources et le témoin
GPL original avec ses bibliothèques maintenues en vie. Le cas d'un témoin GPL
utilisé après destruction de ses bibliothèques reste non validé sur cet hôte.

Le [module de recettes de pipelines](docs/PIPELINE_RECIPES.md) copie les
paramètres et octets shader pour créer les deux variantes d'œil. La scène GPU
de contrôle les utilise après destruction des modules sources ; ses références
restent exactes. Ce module n'est pas encore raccordé au flux de Diablo.

Le [recensement statique des accès mémoire](docs/SHADER_EFFECTS.md) prépare
l'audit des passes préparatoires du jeu. Ses contrôles détectent les écritures
de buffer, image, adresse GPU et la télémétrie de notre helper. Il n'autorise
aucune reprise et ne produit pas encore de rendu VR de Diablo.

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
Le mode à passes séparées conserve aussi les couleurs et la profondeur entre
un clear initial et une continuation avec `LOAD`. Il peut exporter les vues de
contrôle pour inspection. Aucun flux de commandes du jeu n'est encore rejoué.
Voir [les résultats, la propriété des ressources et les limites](docs/STEREO_TARGETS.md).

## Étape 11 : reprise automatique d'un flux graphique de contrôle

Un moteur enregistre maintenant les états et dessins directs/indexés, les
rejoue pour chaque œil puis rétablit l'état d'origine. Sur RTX 5090, les vues
reprises correspondent aux références indépendantes. Un dessin supplémentaire
sans rebind vérifie la restauration. Les passes incomplètes, non auditées ou
sans continuité d'attachements sont refusées. Le code commun est inclus dans
les vérifications Linux et Windows.

Le mode avec descriptor buffers capture aussi les liaisons et offsets des
graphiques et du calcul. Après la reprise, un calcul témoin sans rebind retrouve
ses propres constantes ; un offset volontairement incorrect est détecté. Les
références d'images utilisent des descripteurs traditionnels indépendants.

Cette preuve utilise une scène originale ; le moteur n'est pas encore raccordé
au flux de Diablo. Les variantes de descripteurs/états propres à VKD3D, les dépendances des
passes du jeu et la soumission SteamVR restent à implémenter.
Voir [la preuve de reprise et ses limites](docs/GRAPHICS_REPLAY.md).

La reprise couvre aussi douze états de la table principale de VKD3D, avec
viewports/scissors à compteur, strides dynamiques et valeurs requises par
pipeline. La fixture lit de vrais sommets et vérifie un dessin suivant dont
l'écriture de profondeur reste désactivée sans rebind. Les trois autres états
et les variantes non couvertes restent refusés.
[Audit, preuve GPU et limites](docs/DYNAMIC_STATE.md).

Le diagnostic relève maintenant les déclarations des pipelines et les clear
values. L'analyse des captures existantes retrouve une initialisation de la
profondeur antérieure à celle des couleurs, avec encore des `LOAD` non résolus.
Elle prépare la reprise d'une chaîne de passes ; aucun scope du jeu n'est
encore autorisé pour la reprise. [Méthode et résultats](docs/PASS_DEPENDENCIES.md).

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
