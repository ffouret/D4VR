# Déclarations complètes et pipelines statiques

`replay::Pipeline::dynamicDeclaration` distingue désormais trois contrats :

| Mode | Signification |
| --- | --- |
| `legacy` (défaut) | Compatibilité : une liste vide utilise les champs viewport/scissor/stencil historiques ; une liste non vide est complète. |
| `complete` | Liste complète, y compris vide : aucun setter dynamique n'est alors requis. |
| `unknown` | Enregistrement du pipeline refusé, même si la liste contient des valeurs. |

Les valeurs d'énumération invalides, états non couverts, doublons et mélanges
viewport/scissor classique et WITH_COUNT sont refusés. Une liste complète vide
ne désactive pas les vérifications de descripteurs, constantes, index ou vertex
buffers. Elle ne satisfait pas les deux audits de ressources : les écritures
externes et les alias d'attachements doivent toujours être exclus séparément.

La fixture transfère maintenant la liste **et sa complétude** depuis le graphe
GPL possédé. Un graphe partiel ou inconnu ne peut pas devenir statique par simple
copie de sa liste vide. Les variantes doivent conserver l'état fixe et les
interfaces de leur original ; ce nouveau mode ne prouve pas cette équivalence.

## Ordre des commandes

Selon la [spécification Vulkan](https://docs.vulkan.org/spec/latest/chapters/pipelines.html#pipelines-dynamic-state),
une liaison qui fixe un état invalide sa valeur dynamique antérieure. Un retour
à un pipeline dynamique exige de rétablir les valeurs après cette liaison.
Le moteur applique cette invalidation même pour un contrat complet vide. Le
préfixe, les scopes et la restauration conservent l'ordre des setters et des
liaisons ; la restauration ne réémet pas les dessins.

Les tests C++ couvrent le contrat vide avec les anciens champs laissés à leurs
valeurs par défaut, les ressources manquantes, l'absence d'audit, les contrats
inconnus et le retour statique → dynamique avec ou sans réinitialisation. Les
anciens appelants conservent leur comportement. Ces tests appartiennent à la
CI Windows sans pilote et au groupe ASan/UBSan Linux.

## Contrôle GPU et anomalie isolée

Le mode `--static-state` de la fixture fixe viewport, scissor et référence
stencil dans les pipelines. Avant la liaison statique, il enregistre des valeurs
dynamiques volontairement différentes : viewport 13×17, scissor 11×7 et stencil
91. Les dessins n'appellent ensuite aucun setter de ces états. Le témoin qui
suit la reprise ne rétablit aucun état par lui-même.

Le stencil fixe vaut 1 pour les deux objets. Ses octets sont comparés aux
références et limités à 0/1. Pour le recensement de visibilité, les couleurs
indépendantes des objets identifient la surface proche et la surface éloignée ;
les anciennes fixtures dynamiques continuent d'utiliser leurs références
stencil distinctes 1/2. Les références couleur/profondeur restent indépendantes.

Les graphes statiques sans déclarations parasites et les recettes monolithiques
sont validés séparément. Deux modes de durée de vie GPL restent distincts :
objets sources détruits avant le rendu privé, ou témoin original conservant ses
bibliothèques jusqu'à la fin du GPU. Les contrôles incluent les deux yeux,
CLEAR → LOAD, le flux original, le dessin suivant, le calcul témoin et Steam
Runtime. Les quatre groupes représentent 24 processus, dont huit contrôles
négatifs intentionnels. Les 16 vues de chaque contrôle positif correspondent
aux références : aucun écart couleur, profondeur ou stencil. Le dessin suivant
et le flux original correspondent chacun sur 10 485 760 octets définis, et les
32 mots du calcul sont exacts. [Résultats et empreintes](proofs/static-state-v1.json).

**Une anomalie reste ouverte sur le cas statique avec déclarations parasites.**
Le contrôle initial sans valeurs dynamiques préalables passait ; le contrôle
renforcé produit un écart couleur. Retirer les seules déclarations étrangères
aux parties GPL fait passer le même contrôle. Aucune erreur de validation Vulkan
n'a été observée dans les relevés ciblés. Cela isole une interaction, sans en
établir la cause ni certifier le comportement d'autres pilotes. Ce cas n'est
pas ajouté aux preuves positives. Les déclarations filtrées ne suffisent donc
pas à autoriser sa reprise ; leur conformité théorique ne prouve pas l'état
fixe effectivement rendu. Aucun changement du pilote ni des créations
originales de l'application n'est effectué pour contourner cet écart.

Une expérience privée filtre les déclarations ignorées avant la création de
nos copies. Elle corrige les vues privées isolées, mais le dessin suivant du
témoin GPL original présente encore un écart après restauration. Ce changement
est donc conservé hors des sources publiées ; il ne constitue pas une solution.
Les empreintes de cette expérience et de ses échecs sont distinguées dans la
preuve des sources finalement publiées. Les originaux ne sont pas modifiés.

Les déclarations parasites avec propriétaire WITH_COUNT restent couvertes par
une régression distincte à douze états dynamiques. Leur preuve n'est pas
étendue au propriétaire statique.

## Reproduction et limites

Ajouter `--static-state` au validateur `tools/stereo_mrt_validate.py`, avec
`--recorded-replay --split-passes`, les shaders `stereo_mrt.vert` transformés et
le fragment MRT. `--library-pipelines` choisit les graphes GPL ; ajouter
`--library-original-live` pour conserver les bibliothèques du témoin original.
`--pipeline-recipes` choisit les recettes monolithiques. Les descriptor buffers
restent optionnels, avec leur shader de calcul. `--dynamic-state` est exclusif
avec `--static-state`. Ajouter `--library-state-decoys` au cas statique sert à
reproduire l'anomalie ; le validateur doit alors signaler l'écart.

Il n'y a aucun nouvel essai de Diablo ou de SteamVR dans cette étape. La couche
de diagnostic gelée n'est pas reconstruite et le moteur n'est toujours pas
raccordé aux dessins du jeu. L'autre limite native GPL, après destruction des
bibliothèques d'un témoin original, n'est pas réessayée. Restent les autres
états/conditions, ressources, dépendances `LOAD`, synchronisations et durées de
vie, puis le rendu des passes et leur soumission SteamVR. L'adaptateur D3D12
natif Windows demeure absent. **Les images testées sont synthétiques.**
