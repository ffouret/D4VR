# Premières observations des ressources de dessin

La couche passive observe maintenant la création/destruction des buffers,
les résultats de `vkGetBufferDeviceAddress` (core/KHR), les liaisons de descriptor
buffers, leurs offsets et les liaisons classiques de descriptor sets. Les
événements draw/dispatch situent ces observations dans l'enregistrement CPU.
Les arguments et résultats originaux traversent chaque interception une fois.

**Ces observations ne rendent aucun dessin de Diablo et n'autorisent aucune
reprise.** Le contenu des descripteurs, les identités des layouts, les images
référencées et les alias de mémoire physique restent inconnus. Le diagnostic ne
lit ni n'écrit le contenu des buffers et n'ajoute aucune commande GPU.

## Identité et bornes

Chaque création observée reçoit un identifiant de génération. Une adresse de
liaison est comparée aux plages d'adresses retournées pour ces buffers : les
métadonnées conservent l'identifiant, la taille, l'usage, l'offset relatif et les
octets restants, sans publier adresse GPU ni handle brut. Les plages sont
indexées avec un maximum de fin préfixé pour détecter aussi un grand buffer
chevauchant des plages plus petites. Les additions qui débordent sont exclues.

| Statut | Sens |
| --- | --- |
| 0 | Aucune plage observée correspondante |
| 1 | Une seule plage observée correspondante |
| 2 | Plusieurs plages observées se chevauchent |
| 3 | Adresse nulle |
| 4 | Registre incomplet après dépassement de budget ou échec |

Le statut 1 n'exclut pas les alias physiques ayant des adresses différentes,
ni les ressources provenant d'API non observées. `resource_aliases_excluded` et
`descriptor_contents_captured` restent donc faux. L'usage 64 bits de Flags2
remplace le champ historique ; une chaîne de création inconnue ou des flags
supplémentaires laissent `creation_chain_complete=false`.

La destruction retire la plage des prochaines recherches. Les captures déjà
enregistrées possèdent leurs valeurs et survivent aux resets et au recyclage
des handles. Cela conserve l'observation historique, sans prolonger la durée de
vie GPU du buffer.

## Événements bornés

`resource_events` suit l'ordre des seules observations instrumentées dans un
command buffer. `observed_event_index` n'est pas l'ordinal de toutes les commandes
Vulkan et n'établit aucun ordre entre queues. Les entrées indiquent le nombre
d'opérations enregistrées au moment de l'observation. Les liaisons classiques
restent explicitement visibles, avec leur bind point, sans inventer leur contenu.

Les limites sont 65 536 buffers, 32 plages/offsets par événement, 2 048 événements
par enregistrement et 32 768 événements vivants. Les dépassements sont marqués ;
un registre incomplet ne résout plus de propriétaire. Les échecs d'allocation de
ces nouvelles observations ferment leur collecte sans traverser l'ABI Vulkan.

## Vérification

Les [preuves synthétiques](proofs/resource-bindings-v1.json) couvrent les tests
CPU de durée de vie, chevauchement, overflow, Flags2, limites et relais exact,
ainsi que six processus GPU. Le premier envoi de la fixture contient quatre
liaisons vers un buffer observé, huit changements d'offset, quatre liaisons
classiques, huit dessins et un dispatch. Les références couleur/profondeur et
le témoin compute conservent leurs résultats. Le négatif d'offset ajoute un
changement valide mais incorrect, détecté par le résultat compute ; le négatif
de synchronisation conserve son erreur attendue. Sans opt-in, aucune capture
de frame n'est produite. La scène fermée de présentation reste vérifiée par ses
onze contrôles GPU à 1024.

La CI vérifie ces métadonnées sous Linux et Windows et ajoute le test de frame
aux contrôles ASan/UBSan. Le runtime ASan local est absent ; cette vérification
est effectuée en CI. Aucun nouvel essai dans Diablo n'est revendiqué ici.

Avant une nouvelle capture du jeu, il faut compléter les identités de layouts,
les descripteurs et la provenance des ressources qui y sont encodées. Les
observations présentes ne ferment pas les dépendances LOAD ni les états de
dessin requis par le moteur de reprise.
