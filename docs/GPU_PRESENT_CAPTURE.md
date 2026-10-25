# Producteur d'images GPU à partir de la présentation

`CaptureMode::gpuStream` fournit les images privées de `Capture` avec une
identité de génération, un œil sélectionné, un numéro de présentation, les
compteurs de dessins sélectionnés et la queue de production. La copie est
insérée au même BLIT de présentation déjà utilisé par le diagnostic du jeu.
Les paramètres, usages et layouts des images de l'application restent ceux de
son rendu d'origine. Les images privées restent en `TRANSFER_SRC_OPTIMAL`.

Ce chemin ne crée ni buffer de lecture, ni mapping, ni copie vers le CPU. Le
mode historique `cpuSnapshots` reste le défaut pour les captures diagnostiques.
Une image porte la provenance d'une présentation ; elle ne porte aucune pose XR
inférée et ne prétend pas appartenir au même instant qu'une image d'un autre œil.

## Durée de vie et limites

Quatre emplacements au maximum sont conservés, dans un budget de 256 Mio. Les
séquences acceptées sont croissantes. Chaque réutilisation reçoit un nouvel
identifiant de lease : un ancien identifiant ne peut pas libérer la nouvelle
image qui occupe le même emplacement. Les dimensions et le format peuvent
nécessiter une nouvelle allocation, uniquement après retrait sûr de l'ancienne.

Un emplacement ne devient réutilisable qu'après :

- fin de la fence de capture du producteur ;
- reset ou libération valide des commandes de production qui le référencent ;
- livraison au consommateur de l'image dont la présentation est valide ;
- retrait explicite de la lease et fin de sa fence de retrait.

Les commandes abandonnées ou les présentations sans provenance utilisable
peuvent être écartées après fin de leurs références et du travail GPU connu.
Une présentation arrivée avant la fence conserve son identité initiale. Son
image ne doit pas être recyclée au début de la frame suivante avant que le
consommateur ait pu la recevoir.

Lorsque tous les emplacements sont occupés, `record` refuse la capture sans
attendre. Il n'ajoute alors aucune opération de capture et le BLIT original
continue normalement. Une erreur de soumission du marqueur laisse l'emplacement
indisponible plutôt que de publier ou réutiliser une image non clôturée.

`presentedGpu` livre chaque lease une seule fois, après une présentation réussie
ou sous-optimale, un intervalle de dessins d'un seul œil correspondant, une
provenance de queue connue et la fin du producteur. L'ordre de livraison dépend des emplacements et de la disponibilité des fences,
et ne garantit pas l'ordre des présentations. Le consommateur doit choisir selon
les identifiants d'origine, trier si nécessaire et retirer les frames devenues
périmées ; il ne doit pas avancer artificiellement leur date. Les intervalles mixtes,
présentations échouées et familles inconnues ne produisent aucune lease.

Le consommateur soumet ses lectures sur la même queue, puis appelle `retire`
sous la synchronisation externe de cette queue. Un marqueur vide avec une fence
appartenant à `Capture` clôture ses lectures. Le consommateur ne peut plus
utiliser l'image après une demande valide de retrait, même si le marqueur
échoue. Ses commandes doivent être à usage unique et ne pas être resoumises.
Un identifiant ou une queue rejetés ne modifient pas la propriété de la lease.
Les accès venant d'une autre queue ne sont pas intégrés par cette interface.

Ces contraintes suivent les règles de
[cycle de vie des commandes Vulkan](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html)
et de [signalement des fences](https://docs.vulkan.org/spec/latest/chapters/synchronization.html).
La destruction exige la fin de tous les travaux du périphérique, comme le
composant historique. `retiringLeases` compte les retraits enregistrés jusqu'à
réutilisation, y compris les marqueurs déjà terminés.

## Essai de la couche

`D4VR_GPU_CAPTURE_DRY_RUN=1`, avec le répertoire de capture séquentielle et les
deux paquets de caméra immuables, active le diagnostic du flux. La couche génère
les séquences, émet des métadonnées de lease et soumet leur retrait sur la queue
de présentation si elle correspond à celle de production. Elle arrête de
demander de nouvelles captures après 128 leases livrées ; quelques captures
déjà enregistrées peuvent encore finir. Une queue différente laisse les
emplacements protégés et finit par imposer la limite de capacité.

**Ce mode n'a aucun consommateur d'images du jeu : il ne lit pas les leases et
n'envoie rien à OpenXR.** Il mesure la production et le recyclage réels. Les
métadonnées déclarent explicitement le mode diagnostique, l'absence de pose XR
connue et l'absence d'envoi au casque.

## Validation

La [preuve agrégée](proofs/gpu-present-capture-v1.json) sépare les contrôles
synthétiques d'un éventuel essai de Diablo. Le programme Vulkan propriétaire
exerce 37 leases, quatre allocations et 37 réutilisations, avec de vraies fences
maintenues en cours côté producteur et consommateur. Il compare les pixels
reçus à des couleurs variant par génération, vérifie le maintien de l'image
source et teste les références de commandes, les anciens identifiants et les
présentations invalides. Les appels de lecture CPU du producteur sont comptés
et restent à zéro.

Le programme WSI traverse le chargeur et la couche réels sur une fenêtre X11,
en natif et dans Steam Runtime. Les images originales et les dessins ne sont
pas répétés ; les images restaurées correspondent exactement. Les tests du
mode historique restent inclus. Un test CPU de propriété, sans pilote, couvre
les fins de travaux, la capacité, le budget, les marqueurs échoués et la
conservation d'une image en attente de livraison. Il est ajouté aux vérifications
Linux, Windows et ASan/UBSan.

Le raccordement à `ProjectionSession`, le cadencement XR indépendant du thread
du jeu, les paquets de pose réellement appliqués et la cohérence des deux yeux
restent à réaliser. Les reçus stricts du présentateur ne doivent pas être
satisfaits en réétiquetant ces images successives.

## Essai réel de Diablo IV

Le diagnostic a été exécuté dans Diablo IV 3.2.3.73883, personnage sélectionné
immobile, sans casque. Il a livré 114 leases à 2 560 × 1 440 : 49 pour l'œil
gauche, puis 65 pour le droit. Les présentations d'origine vont de 24 175 à
24 290, avec 664 à 683 dessins sélectionnés par intervalle. Les 114 marqueurs
de retrait ont été acceptés, sans erreur de présentation signalée par la couche.

Les identifiants de capture et de présentation sont uniques. Trois inversions
adjacentes d'ordre de livraison ont été observées ; les identifiants d'origine
sont conservés. Le choix de frames courantes et l'écartement des frames tardives
restent une obligation du consommateur à raccorder.

Cet essai ne lit pas les pixels des leases dans un consommateur et ne produit
aucune preuve de leur contenu après copie ; cette vérification est réalisée
sur les images synthétiques propriétaires. L'essai ne charge pas la validation
Vulkan dans le jeu. Les compteurs finaux du pool et la fin de chacune des
fences de retrait ne sont pas relevés dans le jeu : le processus ne fournit pas
de trace de `vkDestroyDevice`. Les limites du pool sont imposées par le code
et vérifiées dans les contrôles séparés, sans mesure du coût global GPU du jeu.

Le sélecteur a été remis en mode normal, le jeu et Battle.net sont sortis, et
les exécutables, bibliothèques d'origine et 262 exports ont été vérifiés. Les
captures d'interface et journaux de cette session restent privés. Aucune image
du jeu n'est envoyée à SteamVR, aucune pose XR courante n'est appliquée et les
deux groupes d'yeux restent successifs.
