# Première paire géométrique par le rendu original

Le mode expérimental prépare deux images successives avec deux positions de
caméra. Il conserve le moteur de rendu original pour obtenir une première
preuve visible avant la reprise complète de ses ressources. **Aucune paire de
Diablo n'a encore été capturée avec ce mode.** Les preuves ci-dessous utilisent
uniquement nos programmes de contrôle.

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

## Essai privé préparé

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
