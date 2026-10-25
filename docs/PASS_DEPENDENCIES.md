# Déclarations des pipelines et producteurs des attachements

La couche de diagnostic conserve maintenant les listes d'états dynamiques des
pipelines et les valeurs de clear des attachements. Elle **observe** les appels
originaux ; elle n'intègre pas encore le moteur de reprise dans Diablo et
n'ajoute aucun dessin ou envoi SteamVR.

## Métadonnées nouvelles

Chaque pipeline graphique possède un bloc `pipeline_state`. Un pipeline complet
sans bibliothèque peut identifier sa liste de déclarations dynamiques. Les
tableaux CPU sont copiés ; une destruction ou réutilisation des objets source
ne modifie pas les snapshots déjà enregistrés.

Pour GPL, les blocs restent séparés avec leur pipeline source et les sous-parties
qu'ils déclarent. Une union brute des listes ne prouve pas leur application
effective au pipeline lié : `effective_dynamic_states_known` reste donc faux
pour ces pipelines. Ce champ désigne seulement la **liste de déclarations**,
jamais les valeurs établies pendant l'enregistrement. Les flags de création
64 bits priment sur les flags historiques. Un create de liaison sans sous-partie
propre ne déréférence pas son pointeur `pDynamicState`, ignoré par Vulkan.

Les limites sont 64 états par bloc, 32 blocs par pipeline et un million de mots
de déclaration cumulés par périphérique. Une limite atteinte rend les
déclarations incomplètes. `graphics_state_snapshot_complete` reste faux : les
valeurs dynamiques, descripteurs, lectures et barrières ne constituent pas encore
un enregistrement complet du renderer.

Les clear values sont conservées en quatre mots 32 bits, sans conversion JSON
flottante. Couleur, profondeur et stencil restent distincts ; seuls les bits de
profondeur ou de stencil pertinents sont copiés pour ces deux aspects. Pour un
attachement sans clear observé, le champ vaut `null`. Les anciens relevés restent
lisibles et leurs valeurs manquantes restent explicitement inconnues.

## Recherche de producteurs

`tools/pass_dependencies.py` parcourt les utilisations d'attachements précédant
les scopes choisis. Il cherche un clear couvrant la zone consommée, sur la même
queue connue, avec la même image, vue et sous-ressource, aspect par aspect. Il
inclut les scopes intermédiaires qui réutilisent cet attachement et développe
leurs propres dépendances `LOAD`, jusqu'à 256 scopes. Un store abandonné, une
identité inconnue, une zone partielle ou un ordre secondaire aplati empêchent
d'établir ce candidat. Les identités distinctes de vues ne sont pas fusionnées.

Les résultats sont des **producteurs possibles dans la séquence de métadonnées
CPU**. Ils ne prouvent pas l'ordre GPU ni l'absence d'écritures par calcul,
transfert ou alias. Les lectures/barrières restent absentes. L'outil conserve
donc toujours `replay_allowed: false`, même si chaque `LOAD` trouve un clear.

```sh
python3 tools/pass_dependencies.py /travail/frame.json --scopes 12 14 --output /travail/dependances.json
```

## Résultats des deux anciens relevés de Diablo

L'étude des captures déjà réalisées retrouve une préparation de la profondeur
et du stencil **avant** le clear des quatre couleurs. Les trois scopes candidats
par capture avaient seulement été examinés comme des `LOAD` isolés. Le nouvel
outil retrouve deux racines de clear dans chacune des captures et des scopes
intermédiaires supplémentaires. Toutefois les passes préparatoires utilisent
elles-mêmes une couleur en `LOAD` sans clear antérieur retrouvé.

| Capture | Scopes candidats analysés | Scopes de la chaîne possible | LOAD non résolus |
| --- | --- | --- | --- |
| Première | 3, totalisant 171 dessins | 9 | 2 |
| Seconde | 3, totalisant 65 dessins | 8 | 1 |

Les clear values et déclarations dynamiques n'étaient pas enregistrées dans ces
captures. Le [bilan agrégé](proofs/real-pass-dependencies-v1.json) ne contient
aucun journal, shader ou image du jeu. Il ne prouve ni le rôle visible exact des
passes ni un flux rejouable. La nécessité des valeurs couleur antérieures exige
encore l'audit des lectures, masques et effets de bord des shaders.
Le [recensement statique des shaders candidats](SHADER_EFFECTS.md) complète cet
audit sans résoudre ces LOAD : les états de mélange et ressources liées restent
inconnus, et les variantes transformées doivent aussi être examinées.

## Validation de la couche hors du jeu

Le validateur de cibles stéréo accepte `--trace-layer /dossier/couche`. Il vérifie
que la couche chargée observe les cinq pipelines graphiques monolithiques de
la fixture, leurs déclarations (jusqu'à douze états) et 48 clear values dans le
premier enregistrement. Les références d'images, le dessin suivant et le calcul
témoin restent exacts. Le mode `metadata-disabled` vérifie qu'un fichier de
demande ne déclenche aucune capture sans activation explicite du diagnostic.

Quatre exécutions positives et deux contrôles négatifs passent, dont Steam
Runtime avec validation Vulkan de synchronisation. La régression classique,
dynamique et par identifiants de shaders passe aussi dans quatorze exécutions.
Les [mesures synthétiques](proofs/pass-metadata-validation-v1.json) identifient
le binaire testé. Les tests C++ couvrent GPL, flags 64 bits, pointeurs ignorés,
copies transitoires et clear values par aspect. Le test de métadonnées est aussi
inclus dans la CI Windows sans pilote. Les 90 tests Python passent.

Cette couche est préparée pour un prochain relevé isolé ; aucun nouvel essai du
jeu n'est annoncé ici. Le prochain raccordement doit encore auditer les états
réels et ressources, résoudre la chaîne d'initialisation, construire les
variantes de pipelines et reprendre une passe du jeu, puis composer et envoyer
les vues à SteamVR avec suivi de tête. Le backend D3D12 natif Windows reste absent.

Références : [sous-parties GPL](https://docs.vulkan.org/refpages/latest/refpages/source/VkGraphicsPipelineLibraryCreateInfoEXT.html),
[déclarations dynamiques](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineDynamicStateCreateInfo.html).
