# Déclarations dynamiques des graphes GPL

`native/vulkan/dynamic_states.hpp` filtre les déclarations selon la partie du
pipeline qui les possède. `LibraryNode::dynamicStates()` conserve le résultat
dans chaque graphe privé. **Ce calcul ne constitue ni un instantané complet de
l'état graphique, ni un audit des ressources, ni une autorisation de reprise.**

La [spécification Vulkan](https://docs.vulkan.org/spec/latest/chapters/pipelines.html#pipelines-graphics-subsets-dynamic-state)
impose d'ignorer les déclarations étrangères aux parties créées. Le document
1.3.275 consulté est épinglé par commit et empreinte dans la preuve. La table
dynamique courante confirme la correspondance utilisée pour les quatorze états
déjà gérés par notre moteur de reprise.

| Partie GPL | États reconnus |
| --- | --- |
| Entrée de sommets, bit 1 | Topologie, stride des bindings, primitive restart |
| Pré-rastérisation, bit 2 | Viewport/scissor historiques et WITH_COUNT, depth bias et son activation |
| Fragment, bit 4 | Référence/masque d'écriture stencil, depth bounds, activation de l'écriture de profondeur |
| Sorties fragment, bit 8 | Constantes de mélange |

Une déclaration étrangère à la partie est écartée ; elle ne peut rendre
dynamique un état statique d'une autre bibliothèque. Une création de liaison
sans partie propre ignore entièrement son pointeur `pDynamicState`. Les états
partagés entre plusieurs parties, comme fragment shading rate ou sample
locations, et les autres états non modélisés rendent le contrat inconnu.

## Propriété et refus

Le module copie au plus 64 entrées de la déclaration source et ne retient que
les états reconnus applicables. Le graphe combine les snapshots de ses enfants,
avec une couverture disjointe des quatre parties. Les parties manquantes ou
chevauchantes, doublons, modes de viewport/scissor contradictoires, extensions
et flags non couverts restent inconnus. Une bibliothèque partielle peut
transmettre son snapshot à ses parents sans prétendre être complète.

Un contrat complet vide représente un pipeline statique. Le mode explicite
`DynamicDeclaration::complete` de `Graphics` distingue désormais ce cas du
chemin historique ; les contrats inconnus sont refusés. La preuve de cette
page utilise un contrat non vide de trois ou douze états. Le
[contrôle statique renforcé](STATIC_STATE.md) mesure séparément une anomalie
avec des déclarations parasites et un propriétaire statique : sa validité
théorique ne suffit pas à établir l'état fixe effectivement rendu. Les conditions qui rendent certains
états inutiles pour un dessin, les valeurs courantes, les ressources et les
durées de vie GPU restent des vérifications distinctes.

Une liste inconnue ne fait pas échouer la seule compilation des paires : le
diagnostic de création peut conserver leurs handles, sans les déclarer aptes à
la reprise. Les leases conservent les listes copiées avec les graphes après
destruction des déclarations CPU sources et du registre.

## Vérifications

Deux groupes de six validations GPU utilisent maintenant le contrat calculé
par le graphe pour enregistrer la reprise. Les bibliothèques d'entrée et de
sortie contiennent des déclarations parasites de viewport et scissor. Les blocs
bruts atteignent treize entrées et leur union contiendrait quatorze états ; le
contrat utilisé en contient exactement douze. Il correspond à la déclaration
indépendante de la fixture monolithique.

- Le graphe privé est rendu après destruction de tous ses objets sources et du
  registre, avec un témoin monolithique indépendant.
- Le témoin GPL original conserve ses bibliothèques sources jusqu'à la fin du
  GPU ; le graphe privé et ses snapshots restent indépendants du registre.

Dans chaque groupe, les seize vues correspondent aux références couleur,
profondeur et stencil. Les 272 commandes restaurées, le dessin suivant et les
32 mots du calcul restent exacts. Steam Runtime, validation Vulkan, absence de
métadonnées sans opt-in et les contrôles négatifs passent. Cinq régressions du
chemin de recettes monolithiques passent également, soit dix-sept processus de
validation en incluant les erreurs intentionnelles correctement détectées.

Les tests C++ couvrent aussi un propriétaire statique malgré une déclaration
parasite, les données temporaires, l'héritage, les parties incomplètes et les
refus. Ils sont ajoutés à Windows et aux contrôles ASan/UBSan Linux. Les 108
tests Python passent. [Mesures, sources et empreintes](proofs/gpl-dynamic-states-v1.json).

Pour reproduire, ajouter `--library-pipelines --library-state-decoys` au mode
`--recorded-replay --dynamic-state` du validateur `stereo_mrt_validate.py`.
`--library-original-live` sélectionne le témoin GPL conservant ses sources ; son
absence sélectionne le graphe privé indépendant. Les autres paramètres sont
ceux du [validateur des graphes GPL](LIBRARY_VARIANTS.md).

## Limites pour Diablo

Les anciennes métadonnées de trame restent volontairement brutes : leur flag
GPL `effective_dynamic_states_known` demeure faux. Le nouveau contrat est
disponible dans les nœuds privés créés synchroniquement ; il ne reconstitue pas
les graphes inconnus d'anciennes captures. La couche gelée et validée auparavant
reste inchangée. Aucun nouveau binaire de couche n'a été validé sur GPU avec ce
calcul, et aucun nouvel essai de Diablo ou de SteamVR n'a été effectué.

Restent les autres états et conditions du jeu, l'audit des descripteurs, alias,
`LOAD` et barrières, puis le raccordement d'une reprise admissible et sa gestion
des ressources en vol. Le problème natif antérieur d'un témoin GPL utilisé après
destruction de ses bibliothèques n'est pas résolu par ce calcul et n'est pas
réessayé. Les deux preuves de durée de vie restent distinctes. Le backend D3D12
natif Windows demeure absent.
