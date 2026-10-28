# Premières observations des ressources de dessin

La couche passive observe maintenant la création/destruction des buffers,
les résultats de `vkGetBufferDeviceAddress` (core/KHR), les liaisons de descriptor
buffers, leurs offsets et les liaisons classiques de descriptor sets. Les
événements draw/dispatch situent ces observations dans l'enregistrement CPU.
Elle observe aussi les layouts de groupes et de pipelines, ainsi que les
tailles et offsets de bindings demandés au pilote.
`vkGetDescriptorEXT` conserve maintenant une copie bornée des octets retournés
avec l'identité observée du buffer ou de la vue d'image source.
Les arguments et résultats originaux traversent chaque interception une fois.

**Ces observations ne rendent aucun dessin de Diablo et n'autorisent aucune
reprise.** Les descripteurs actifs, le contenu courant des buffers/textures et
les alias de mémoire physique restent inconnus. L'observateur copie uniquement
les sorties de requêtes CPU ; il n'ajoute aucune commande GPU.

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

## Layouts et requêtes du pilote

Chaque création réussie de descriptor set layout ou de pipeline layout reçoit
une identité de génération. Les snapshots possèdent les tableaux de bindings
(numéro, type, nombre, stages, flags), les groupes référencés et les plages de
push constants. Les entrées nulles explicites des layouts partiels sont
distinguées d'un objet non observé. Une chaîne inconnue, une définition coupée
ou des immutable samplers non résolus laissent `definition_complete=false`.
Ce champ décrit uniquement la déclaration observée ; il ne valide aucune
compatibilité entre layouts ni aucun contenu de ressource.

`vkGetDescriptorSetLayoutSizeEXT` et
`vkGetDescriptorSetLayoutBindingOffsetEXT` traversent la couche une fois. Les
valeurs retournées créent une nouvelle révision privée des métadonnées, sans
requête supplémentaire. Les futures liaisons référencent cette révision, même
si le pipeline layout avait été créé avant la requête. Les observations déjà
enregistrées restent figées. Détruire un set layout ne retire pas sa définition
des pipeline layouts qui le décrivaient ; recréer le même handle crée une
autre identité.

Les événements d'offsets et de liaisons classiques référencent
`pipeline_layouts` par `layout_snapshot_id`. `layout_identity_captured=true`
signifie qu'une identité est observée ; une définition incomplète est aussi
marquée tronquée dans l'événement. Les pipelines monolithiques et les parties
GPL propriétaires de shaders conservent leur déclaration de layout ; le
layout effectif d'un assemblage GPL n'est pas résolu par ces métadonnées.
Les tables `descriptor_set_layouts` et `pipeline_layouts` sont dédupliquées
par révision lors de la sérialisation. Aucun handle, pointeur ou contenu de
descripteur n'y est écrit. Ces copies ne prolongent aucune durée de vie GPU.

Les budgets conservateurs sont 4 096 bindings par set, 32 groupes et plages de
push constants par pipeline, 65 536 allocations de snapshots/objets et un
million de mots de métadonnées cumulés par device. Un dépassement ferme la
résolution des layouts, même si des objets sont ensuite libérés, afin de ne
présenter aucune ancienne révision comme une observation à jour.

Références Vulkan : [bindings et immutable samplers](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorSetLayoutBinding.html),
[pipeline layouts](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineLayoutCreateInfo.html),
[offsets fournis par le pilote](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDescriptorSetLayoutBindingOffsetEXT.html).

## Sorties exactes de GetDescriptor

L'interception copie la source typée avant l'appel original et les octets
retournés après cet appel, sans modifier ses arguments, destination ou résultat.
Elle lit uniquement le membre d'union désigné par `type`. Les buffers conservent
leur identité/plage observée, l'offset et la range demandée ; le format ignoré
des uniform/storage buffers n'est pas lu. Les texel buffers gardent leur format.
Les images conservent les identités de vue/image et le layout demandé. Les
sources nulles, inconnues ou ambiguës restent distinctes. Les samplers sont
signalés présents ou absents, sans inventer leur identité. Les types non
pris en charge gardent seulement leur type et leurs octets opaques.

Le catalogue compare les **octets entiers** puis la source observée pour
dédupliquer les requêtes connues. Aucune empreinte faible n'est utilisée.
Des encodages identiques associés à deux générations de ressource, ou à des
samplers non résolus, restent plusieurs candidats. Une recherche retourne
tous les candidats exacts ; elle n'autorise jamais une reprise. Le catalogue
est un préfixe borné des requêtes observées depuis l'activation, sans preuve
que leurs sorties figurent dans les descriptor buffers lus par une frame.

Chaque capture possède le préfixe connu au moment de ses envois CPU. Les
observations déjà capturées survivent aux nouvelles requêtes, à l'écrasement
de la destination et à la destruction des ressources. Elles ne prolongent
aucune durée de vie GPU. `descriptor_query_selection_proven` et
`descriptor_contents_captured` restent faux : les requêtes ne sont pas un
snapshot du contenu actif des descriptor buffers, et l'adresse CPU de sortie
n'est pas interprétée comme un emplacement GPU.

Les limites sont 4 096 entrées, 4 096 octets par sortie et un Mio d'octets dans
les entrées. Les duplications connues sont reconnues avant de consommer le budget.
Après dépassement ou échec, le catalogue est marqué tronqué et les recherches
ne retournent plus de candidats ; les copies historiques restent lisibles.
Il faudra mesurer cette couverture dans le jeu avant d'utiliser ce préfixe
pour une capture de ressources exploitable.

Les encodages peuvent contenir des adresses propres au pilote : ils restent
dans les fichiers de capture privés, hors du dépôt. Le résumé Python émet des
comptages sans recopier ces octets. Références Vulkan :
[GetDescriptor](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDescriptorEXT.html),
[source typée](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorDataEXT.html),
[adresse, range et format](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorAddressInfoEXT.html).

## Provenance mémoire observée

Le device récupère les propriétés mémoire réelles et `nonCoherentAtomSize`.
Les allocations réussies reçoivent une identité de génération ; les bindings
de buffers conservent cette identité, la taille logique et l'offset dans
l'allocation. Free/recréation d'un même handle ne réactive jamais une ancienne
liaison. Les snapshots sont des valeurs CPU : ils survivent aux changements
ultérieurs sans garder de ressource GPU en vie.

Les hooks couvrent Allocate/FreeMemory, BindBufferMemory/BindBufferMemory2
(core/KHR), Map/UnmapMemory, Flush/InvalidateMappedMemoryRanges. MapMemory2 et
UnmapMemory2 sont aussi interceptés avec leurs noms KHR/core lorsque les headers
de compilation prennent en charge `VK_KHR_map_memory2`. Un échec de batch bind
laisse toutes les liaisons concernées indéterminées : les statuts individuels
de maintenance6 ne sont pas décodés. Les chaînes d'allocation inconnues, imports,
bindings de groupes de devices ou paramètres de mapping non pris en charge
restent incomplets. Les liaisons d'images et sparse ne sont pas suivies ici.

`buffer_memory` décrit les buffers référencés par le préfixe de requêtes et les
liaisons instrumentées. Son scope est le **dernier envoi CPU enregistré** de la
capture, indépendamment de son résultat, et ne décrit pas un ordre d'exécution
GPU. Les lignes indiquent propriétés, offset de liaison, mapping/génération,
couverture du payload logique et derniers ranges de flush/invalidation. Un
mapping `VK_WHOLE_SIZE` est borné par la taille de l'allocation ; l'offset de
mapping et celui du buffer sont traités séparément. Aucun pointeur hôte ni
contenu de mémoire n'est sérialisé par ce suivi.

| Statut | Observation |
| --- | --- |
| 0 | Génération de buffer inconnue ou déjà retirée |
| 1 | Aucune liaison observée (ne conclut pas que le buffer est réellement non lié) |
| 2 | Liaison observée vers une allocation inconnue |
| 3 | Liaison et bornes logiques observées |
| 4 | Bornes de liaison incompatibles avec la taille observée |
| 5 | Registre incomplet après dépassement/échec |
| 6 | Allocation libérée, génération historique conservée |
| 7 | Résultat de liaison indéterminé |

Les propriétés HOST_VISIBLE/COHERENT proviennent du type mémoire réel ; elles
ne sont jamais déduites de l'usage du buffer. Les ranges de cache connus
respectent le mapping, l'allocation et l'alignement de l'atome non cohérent.
Les échecs ne sont pas comptés comme des opérations réussies ; unmap ne crée
aucun flush implicite. Remap/unmap invalident les anciens ranges connus tout
en conservant leurs compteurs historiques.

Même une ligne entièrement observée et mappée garde
`gpu_visibility_proven=false`. Les écritures CPU, accès GPU concurrents,
barrières, imports et alias physiques ne sont pas résolus. Les offsets/taille
portent sur le payload logique, pas sur toutes les contraintes d'alias des
memory requirements. Le registre est limité à 65 536 buffers et allocations
vivants, les tableaux d'API et captures à 4 096 entrées ; un dépassement est
explicite et ferme la résolution. Il reste à établir un contrat de capture
des octets avec durée de vie, quiescence et visibilité avant toute lecture.

Références Vulkan : [mapping](https://docs.vulkan.org/refpages/latest/refpages/source/vkMapMemory.html),
[flush et absence de flush implicite à l'unmap](https://docs.vulkan.org/refpages/latest/refpages/source/vkFlushMappedMemoryRanges.html),
[liaisons multiples et échecs](https://docs.vulkan.org/refpages/latest/refpages/source/vkBindBufferMemory2.html).

## Copie stable et résolution dans la fixture propriétaire

`stereo_targets_vulkan --descriptor-receipt-output` exporte maintenant aussi
les octets de son propre descriptor buffer. Le programme isolé possède cette
allocation cohérente, écrit les six descripteurs avant le premier envoi et
n'ajoute ensuite aucune écriture CPU ou GPU dans ce stockage. Il copie les
octets avant les envois et vérifie leur égalité après chacun des huit fences
terminés, avant toute libération. La copie est bornée à un Mio. Les encodages
restent dans le même fichier privé que l'oracle des sorties GetDescriptor.

Ce contrat vient du code fermé de la fixture. L'égalité avant/après ne prouve
pas à elle seule l'absence d'écritures intermédiaires. Le registre de mappings
passif n'empêche ni unmap/free concurrent, ni les écritures CPU ou GPU ; il
ne doit pas servir à lire un pointeur du jeu. La couche passive n'a reçu aucun
accès mémoire supplémentaire et ses champs `descriptor_contents_captured` et
`descriptor_query_selection_proven` restent faux.

`tools/descriptor_contents.py FRAME OWNED_ORACLE` combine cette copie avec les
événements réels de la couche. Il additionne l'offset de l'adresse liée, celui
du set et celui du binding fourni par le pilote, puis compare tous les octets
du descripteur aux sorties GetDescriptor. Les correspondances ambiguës,
catalogues incomplets, offsets inconnus, plages hors limites et générations
libérées restent non résolus. Les tailles viennent des propriétés réellement
interrogées par la fixture, sans décodage du format opaque NVIDIA.

Le profil est volontairement limité à cette fixture : un stockage immuable,
un envoi capturé avec un seul command buffer, trois sets vides et deux UBOs
scalaires dans le set 3. Son oracle atteste que les six requêtes constituent
tous les descripteurs écrits dans ce stockage ; un unique candidat dans un
préfixe arbitraire du catalogue ne suffirait pas. Graphics et compute gardent
des offsets séparés ; une nouvelle liaison de buffers invalide les deux.
Les liaisons classiques ne sont pas décodées. Les générations de layouts
doivent être identiques ; les nouvelles révisions de requêtes conservent
cette identité. Aucune compatibilité entre layouts distincts n'est supposée.

La [preuve du contenu lié](proofs/descriptor-contents-v1.json) retrouve les deux
sources de quatre dessins et d'un dispatch dans le premier envoi capturé.
Les quatre dessins utilisant des sets classiques restent non résolus.
Un contrôle compute avec un offset valide mais incorrect retrouve exactement
la mauvaise paire ; le résultat GPU détecte aussi cette erreur. Le résumé
CLI contient seulement des comptages et aucun encodage. Cela identifie des
sources **liées** dans cette scène propriétaire, sans prouver les accès
dynamiques d'un shader ni copier les matrices/textures sources.

Il reste à construire une capture du jeu avec protection de durée de vie,
exclusion des écritures et visibilité démontrée, puis à établir la couverture
de ses descripteurs, les contenus des ressources, leurs alias et la fermeture
des passes LOAD avant de reprendre deux vues de Diablo. Cette preuve n'ajoute
aucun rendu de Diablo ni nouvel essai dans le casque.

Références Vulkan : [invalidation lors d'une nouvelle liaison](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindDescriptorBuffersEXT.html),
[offsets, mises à jour et visibilité](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetDescriptorBufferOffsetsEXT.html),
[tailles d'encodage](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDescriptorEXT.html).

## Copie GPU des descripteurs et des buffers de caméra

`native/vulkan/buffer_readback` ajoute le transport par adresses GPU existantes.
Un shader compute lit des mots de quatre octets vers un buffer privé cohérent.
Les sources gardent leur usage original : BDA est requis, TRANSFER_SRC ne l'est
pas. Le shader n'écrit aucune source et ne requiert ni descripteur, atomic,
accès 8 bits ou capability shaderInt64. Les 24 octets de push constants portent
les deux adresses et le nombre de mots. Le plan vérifie tout le batch avant la
première commande : alignements, tailles, overflow, usages, chevauchements VA
source/destination ou destinations, maximum 32 spans et un Mio au total.
Les alias physiques à adresses distinctes restent à exclure par l'appelant.

Le composant enregistre dans un command buffer compute **privé**, hors passe de
rendu. L'appelant conserve les ressources, exclut les writers, synchronise la
queue et les producteurs, soumet et attend le fence avant lecture/libération.
Les barrières couvrent les écritures host cohérentes et les producteurs de la
même queue, puis la visibilité des résultats vers l'hôte. Elles ne remplacent
pas une synchronisation entre queues ou une protection contre free/unmap/écriture
concurrente. Aucun accès à un pointeur mappé du jeu n'est ajouté à la couche.

Avec `--readback-compute`, la fixture copie immédiatement après son premier
fence terminé, avant de modifier les sources : 192 octets de descripteurs et
9 264 octets des six UBOs. Ce moment correspond au premier envoi capturé ; les
données ne sont pas prises après les mises à jour des cas suivants. Les copies
GPU sont comparées aux sources CPU possédées avant toute reprise des tests.
224 octets de garde restent intacts et les sept sources sont inchangées.
Le résolveur utilise ensuite la copie **GPU** des descripteurs pour retrouver
les sources liées aux quatre dessins et au dispatch. Les payloads de caméra
restent dans l'oracle privé, avec les encodages opaques.

La [preuve GPU](proofs/buffer-readback-v1.json) couvre sept processus. Le nouveau
contrôle négatif pointe vers une autre ressource valide : Vulkan reste sans
erreur, mais la comparaison des octets et le résolveur refusent la copie. Les
références couleur/profondeur/stencil et les témoins suivants restent inchangés.
La validation Vulkan ne garantit pas une couverture de tous les accès BDA ;
les comparaisons exactes, bornes et gardes apportent des contrôles séparés.
Les tests CPU de plan sont portables Linux/Windows et inclus dans ASan/UBSan CI.
La couche passive et son profil d'admission n'ont pas changé ; aucun dessin de
Diablo ni nouvel essai au casque ne résulte de cette étape.

Références : [plages des adresses GPU](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetBufferDeviceAddress.html),
[mémoire des PhysicalStorageBuffers](https://docs.vulkan.org/spec/latest/chapters/interfaces.html#interfaces-resources-layout).

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

Les [contrôles de layouts](proofs/resource-layouts-v1.json) ajoutent la durée de
vie des déclarations, les requêtes tardives, les révisions figées, les handles
recyclés, les chaînes inconnues et les entrées nulles. Dans les six processus
GPU, la fixture expose huit identités de set layouts et deux de pipeline
layouts ; les huit offsets et quatre liaisons classiques référencent tous
leur déclaration. Une taille et deux offsets de bindings sont observés. Les
mêmes contrôles négatifs, références graphiques/compute et régressions VR
restent vérifiés. Ce sont des preuves synthétiques, sans nouvel essai Diablo
ni nouvel essai humain dans le casque.

La CI vérifie ces métadonnées sous Linux et Windows et ajoute le test de frame
aux contrôles ASan/UBSan. Le runtime ASan local est absent ; cette vérification
est effectuée en CI. Aucun nouvel essai dans Diablo n'est revendiqué ici.

La [preuve des sorties de descripteurs](proofs/descriptor-receipts-v1.json)
compare les copies de la couche aux octets reçus par l'application GPU isolée :
six buffers et une vue de texture inutilisée, détruite avant le premier envoi.
La destination de cette dernière est écrasée après l'appel, sans modifier la
capture. Les mêmes six processus GPU et onze régressions du POC sont exécutés ;
le témoin compute et les références graphiques restent comparés. Ce contrôle
ne prouve pas la sélection active d'une texture par le jeu.

La [preuve de provenance mémoire](proofs/memory-bindings-v1.json) ajoute les
suballocations, propriétés non cohérentes synthétiques, mappings partiels,
réutilisations, erreurs et ranges de cache. Sur GPU, les sept buffers suivis
ont chacun une allocation observée et un payload entièrement mappé ; un flush
et une invalidation sont comptés. Les témoins graphiques, compute et les sept
encodages exacts restent comparés. Cela ne prouve ni la visibilité actuelle
des écritures ni l'absence d'alias. Aucun nouvel essai dans Diablo ou au casque.

Avant une nouvelle capture du jeu, il faut établir la capture des octets avec
sa durée de vie et sa visibilité, puis relier les encodages à leur emplacement
actif. Les
observations présentes ne ferment pas les dépendances LOAD ni les états de
dessin requis par le moteur de reprise.
