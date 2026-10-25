# Première paire géométrique par le rendu original

Le mode expérimental prépare deux images successives avec deux positions de
caméra. Il conserve le moteur de rendu original pour obtenir une première
preuve visible avant la reprise complète de ses ressources. **Une première paire
réelle de Diablo IV a été capturée**, en 2560 × 1440. Après un second essai
négatif et la correction du filtre de shaders, le troisième essai retrouve
un décalage reproductible sur 21 des 36 zones de terrain suivies. La couverture
reste partielle et ce résultat ne constitue pas encore du Diablo en VR.

## Sélection et capture

Deux paquets GPU privés restent immuables. Les variantes de pipelines pointent
respectivement vers le paquet gauche et le paquet droit. Le fichier privé
`eye.selection` choisit la variante lors de l'enregistrement du command buffer :
`0` gauche, `1` droite, `2` pipeline original. Une sélection absente ou invalide
conserve le pipeline original. Les variantes sélectionnées restent possédées
jusqu'au reset ou à la libération du command buffer ; leurs ressources sont
libérées après la fin du travail GPU, au nettoyage du device.

Cette activation explicite change le pipeline lié, sans doubler les dessins.
Elle n'utilise ni écriture dans un paquet en cours de lecture GPU, ni attente
globale du device. Les autres modes de la couche restent passifs ou conservent
leur comportement précédent.

La capture attend un BLIT de présentation complet vers une image de swapchain
connue. Elle reproduit cette seule lecture BLIT dans une image privée, puis
copie cette image privée dans un buffer cohérent. Le layout, les usages et le
contenu de la source restent inchangés. Une soumission vide avec fence, placée
après la soumission originale sur sa queue extérieurement synchronisée,
confirme la fin du travail avant lecture CPU. Les ressources insérées restent
vivantes même après reset du command buffer. Aucun pointeur mappé du jeu n'est lu.

Le mode accepte seulement un command buffer primaire `ONE_TIME_SUBMIT`, un pool
non protégé, une région couleur complète, un mip et une couche. Il refuse les
chemins inconnus, les images sparse/protégées et les formats non pris en charge.
Les limites sont quatre captures, 256 Mio d'allocations privées et 4096 × 4096
pixels. Les formats acceptés sont RGBA/BGRA8 et les deux UNORM10:10:10:2. La
conversion PNG n'est pas un tone mapping HDR calibré.

Le [chemin BLIT de VKD3D-Proton étudié](https://github.com/HansKristian-Work/vkd3d-proton/blob/ef9a02e873715c9e2da0d6db8a236aca8380b303/libs/vkd3d/swapchain.c)
rend cette capture possible sans ajouter `TRANSFER_SRC` aux images de
swapchain. Les captures réelles confirment l'emploi de ce chemin dans les
sessions du runtime Lutris épinglé. Son chemin de présentation par shader
n'est pas pris en charge ici.

## Preuves actuelles

La [preuve synthétique](proofs/sequential-capture-v1.json) comprend :

- un test GPU du composant : copie exacte de 49 152 octets, source et image
  originale préservées, ressources conservées après reset ;
- trois exécutions avec une vraie fenêtre X11 et un swapchain Vulkan : mode
  actif natif, désactivé, actif dans Steam Runtime, sous validation Vulkan de
  la synchronisation ;
- dans chaque mode actif, deux images de 262 144 octets égales aux références
  rendues indépendamment par le client, sans différence ;
- des disparités de 14,608 pixels à profondeur 2 et 7,313 à profondeur 4,
  confirmant le décalage géométrique dépendant de la profondeur ;
- une image normale identique avant et après les sélections, et aucune capture
  ni modification de pixels lorsque le mode est désactivé ;
- 162 tests Python, le test natif de relais, les six contrôles de reprise et
  les onze contrôles du chemin d'interception existant à 1024 par œil.

Ces essais ne lancent ni Diablo ni le casque. Ils ne corrigent pas les erreurs
Vulkan internes à SteamVR déjà décrites dans la preuve du casque.

## Premier essai réel de Diablo IV

La [preuve réelle agrégée](proofs/sequential-diablo-v1.json) correspond au binaire
du commit `24e19a9`, avec le runtime Lutris épinglé et le jeu 3.2.3.73883. Deux
captures du même processus/device ont été obtenues aux présentations 1997 et
2130, en BGRA8 : 14 745 600 octets chacune. Les intervalles comportent
respectivement 229 et 216 commandes de dessin utilisant les variantes privées,
avec les masques d'œil attendus 1 et 2. La sélection revient à 2 après la prise.

Les images montrent une même zone du jeu et un changement de point de vue du
terrain. L'animation du personnage et des messages d'interface changent aussi.
Ces images seules ne séparent donc pas toute la disparité géométrique du
mouvement temporel. Le plafond de 512 nœuds privés est atteint : la couverture
des pipelines est partielle, malgré zéro échec de création pilote dans les
observations publiées. Il faut résoudre cette limite avant de prétendre couvrir
le monde complet. Les 1 536 refus observés ne sont pas tous attribués à une
cause individuelle dans ce relevé.

Tous les processus de la session sont ensuite sortis ; l'exécutable, la
bibliothèque originale et ses exports correspondent encore aux références.
La destruction Vulkan du device n'a pas été observée. La validation Khronos
n'était pas activée dans le jeu : ce relevé n'est pas une preuve de synchronisation
du jeu sans erreur. Aucune image n'a été envoyée au casque lors de cet essai.
Les pixels réels restent privés, hors dépôt ; seuls hashes et résultats agrégés
sont publiés.

## Correction préparée après ce relevé

Le mode successif utilise désormais un plafond explicite de 4 096 nœuds et
4 096 tentatives de création, avec un compteur séparé `budget_rejections`.
Chaque manifeste d'image contient aussi les compteurs du registre au moment
de l'export, même si le programme ne détruit pas explicitement son device.
Les autres modes conservent leur quota précédent. Ce changement supprime le
plafond de 512 du premier essai ; il ne prouve pas encore que tous les dessins
du jeu seront couverts. Le pilote conserve la gestion de ses allocations
internes, dont le coût ne se déduit pas du seul nombre de pipelines.

`request --repeat` prend gauche/droite puis gauche/droite, dans la limite des
quatre captures privées. Le rapport conserve la première paire et identifie
les deux images suivantes explicitement, sans écraser silencieusement un œil.
Les séquences, formats et ordres de présentation doivent correspondre. La
répétition prépare une inspection de la stabilité du décor ; elle ne prouve pas
automatiquement son immobilité.

La [preuve de cette correction](proofs/sequential-coverage-v2.json) couvre
164 tests Python, les tests natifs de relais et de propriété, y compris 4 096
paires simulées sans GPU avec libération différée par lease. Le client WSI
effectue maintenant quatre captures : 1 048 576 octets égaux aux références
dans chaque cas actif natif et Steam Runtime, sous validation de synchronisation.
Ces essais supplémentaires restent synthétiques ; le premier essai réel
ci-dessus utilise toujours l'ancien binaire `24e19a9`.

## Second essai réel : résultat négatif sur le terrain suivi

La [preuve réelle répétée](proofs/sequential-diablo-v2.json), testée au commit
`d00a0e9`, contient quatre images de 2560 × 1440 : yeux 0, 1, 0, 1 aux
présentations 2229, 2248, 2266 et 2284. Les compteurs au moment de l'export
montrent 711 à 795 nœuds privés, zéro refus de budget et zéro échec de création
pilote. Le plafond précédent est donc levé pour cet essai. Les autres refus
et la sélection effective de toutes les géométries restent à résoudre.

L'analyse locale suit 35 patches contrastés du terrain dans les couples
gauche/droite, gauche/retour gauche, puis retour gauche/retour droite. Ils
retrouvent tous un décalage de zéro pixel aux coordonnées analysées. Les images
sont réduites de moitié pour cette mesure ; un effet inférieur à deux pixels
ou sur d'autres géométries n'est pas exclu. Les patches de 7 × 7 échantillons
espacés de deux pixels utilisent l'erreur absolue après retrait de la moyenne,
une recherche de ±40 pixels horizontalement et ±8 verticalement, un contraste
moyen d'au moins 2, une erreur inférieure à 0,8 fois ce contraste et un rapport
de 1,5 face au meilleur candidat distinct. Ce contrôle exploratoire ne prouve
pas toute la scène. Il **ne valide pas la stéréo du décor** et le mouvement
visible du premier essai ne doit pas être présenté comme une preuve acquise.

Fabien a aussi signalé que le premier PNG ne montrait que l'interface. La cause
est vérifiée : 6 665 235 pixels sur 7 372 800 de cette paire ont un alpha nul,
malgré des couleurs de décor présentes. L'alpha du backbuffer composé ne
représente pas la transparence d'une capture d'écran. L'export force désormais
l'alpha des seuls aperçus PNG à 255. Les RGB et les payloads bruts, avec leurs
hashes d'origine, restent inchangés. Un test décode le PNG produit avec des
couleurs de terrain à alpha nul pour vérifier leur visibilité et la préservation
du fichier brut. Les aperçus des deux essais sont régénérés.

Les processus du second essai sont sortis et les vérifications de l'exécutable,
de la bibliothèque d'origine et de ses exports correspondent encore aux
références. Aucun nouvel essai casque n'a été effectué. La prochaine étape
porte sur les shaders et matrices des dessins de terrain, avant toute
présentation du résultat comme une réussite VR.

## Procédure d'essai privée

Le constructeur propose maintenant `camera_bundle.py --stereo
--projection-only-helper`. Le filtre précédent exigeait à la fois la projection
finale dans le buffer principal et une matrice d'objet dans un second buffer.
Il écartait 566 candidats dont la projection finale était pourtant reconnue :
551 sans chaîne d'entrée directement reconnue et 15 avec une matrice dans le
buffer principal. Le helper n'utilise pas ses arguments d'objet ; le mode avec
projection seule, déjà contrôlé séparément, permet de supprimer cette exigence.

Le [nouveau bundle privé validé](proofs/projection-bundle-coverage-v1.json)
contient 2 679 modules transformés, tous vérifiés par spirv-val. Les identités
sources et le runtime restent vérifiés. Aucun accès ajouté ne lit une matrice
d'objet ; les accès du shader original restent présents. Le filtre de projection
numérique n'est pas modifié et les positions non reconnues restent hors sélection.
Les 167 tests Python incluent une régression de sélection sans second buffer.
La fixture GPU vérifie onze images contre des références indépendantes, les
paquets invalides, le jitter et les changements de pose. Dans le nouveau mode,
les buffers de diagnostic restent intacts. Le contrôle historique conserve sa
télémétrie et un témoin négatif détecte bien un shader qui écrit dans ces buffers.
Les quatre processus utilisent la validation de synchronisation, dont un dans
Steam Runtime. Ces contrôles sont synthétiques ; leur essai réel ultérieur
est décrit ci-dessous.

## Troisième essai réel : disparité reproductible sur une partie du terrain

La [preuve agrégée du troisième essai](proofs/sequential-diablo-v3.json) teste
le bundle du commit `16715d6`, avec le même binaire natif que le second essai.
Le lancement de Battle.net et l'entrée dans le monde avec le personnage déjà
sélectionné sont effectués par l'interface habituelle, après autorisation
humaine du contrôle de saisie KDE. Le personnage reste immobile ; aucun casque
n'est nécessaire pour cet essai.

Quatre images 2560 × 1440 du même processus/device correspondent aux yeux
0, 1, 0, 1 et aux présentations 34130, 34427, 34724 et 35022. Elles comptent
537, 552, 538 et 540 commandes de dessin sélectionnées. Le registre compte
3 386 à 3 521 nœuds privés sur un plafond de 4 096, sans refus de budget ni
échec de création pilote. Les 3 039 autres refus ne sont pas attribués
individuellement par ce relevé.

La même méthode exploratoire que précédemment accepte 36 des 40 patches
contrastés. **21 ont un décalage horizontal non nul, de −4 à −18 pixels,
identique dans les deux paires gauche/droite.** Les 36 patches retrouvent zéro
déplacement au retour au même œil, et zéro déplacement vertical entre yeux.
La médiane horizontale de l'ensemble est −6 pixels. La variation spatiale du
décalage, avec une partie du terrain proche autour de −16 pixels et des zones
lointaines autour de −4 à −8 pixels, concorde avec une transformation
géométrique ; il ne s'agit pas d'une mesure indépendante de profondeur.
Fabien signale également avoir vu le décalage pendant l'essai.

**Quinze patches restent fixes.** La sélection de tous les dessins du monde
n'est donc pas prouvée et la scène complète n'est pas validée pour le casque.
Les captures sont successives ; l'animation du personnage et les effets
atmosphériques changent encore. La mesure à deux pixels ne couvre ni tous les
objets ni les mouvements inférieurs à cette résolution. Les pixels et shaders
restent privés ; le dépôt ne contient que les hashes et résultats agrégés.
La sélection revient à `2` après les quatre prises. La validation Khronos
n'est pas activée dans le jeu et aucun nouvel essai SteamVR n'a été effectué.

La suite porte sur les géométries restées fixes, puis sur deux yeux complets au
même instant et leur envoi à SteamVR avec suivi de tête. Le backend Windows
D3D12 et les erreurs de validation SteamVR précédemment observées restent ouverts.

`tools/sequential_capture.py prepare` vérifie le binaire testé, le bundle de
shaders déjà validé, la version du jeu et le lanceur Lutris complet. Il crée
un lanceur et des caches privés hors du jeu, avec sélection originale `2`.
`launch` lance cette session et relève les seuls processus portant son marqueur.
Une fois le joueur immobile dans le décor, `request` sélectionne chaque œil,
laisse deux secondes de stabilisation, puis demande une capture. Le script
revient à `2`, y compris en cas d'échec.

`report` exige deux images ordonnées du même processus/device, un intervalle
ne contenant que l'œil sélectionné parmi les dessins suivis et un PID observé
comme Diablo IV dans cette session. Les images, journaux, encodages et shaders
du jeu restent privés. Les seuls hashes et résultats agrégés seront publiés.

Les compteurs suivent les dessins conventionnels utilisant une variante ; ils
ne prouvent pas que toute la géométrie du monde est couverte. Les deux images
sont successives : animations, TAA et objets mobiles peuvent différer. Ce mode
ne produit ni paire au même instant, ni suivi de tête, ni envoi des images du
jeu à SteamVR. Une paire réelle réussie devra ensuite être inspectée avant
d'être présentée au casque. Le backend Windows D3D12 reste à implémenter ; le
composant de capture Vulkan est du C++ commun, le client WSI de test utilise X11.
