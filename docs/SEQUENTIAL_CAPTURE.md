# Première paire géométrique par le rendu original

Le mode expérimental prépare deux images successives avec deux positions de
caméra. Il conserve le moteur de rendu original pour obtenir une première
preuve visible avant la reprise complète de ses ressources. **Une première paire
réelle de Diablo IV a été capturée**, en 2560 × 1440. Elle reste successive et
partiellement transformée ; ce résultat ne constitue pas encore du Diablo en VR.

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
swapchain. L'emploi effectif de ce chemin dans le runtime Lutris épinglé reste
à vérifier pendant le premier essai du jeu. Son chemin de présentation par
shader n'est pas pris en charge ici.

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

## Procédure d'essai privée

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
