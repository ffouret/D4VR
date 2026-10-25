# Recettes de pipelines pour les variantes par œil

Le module commun `native/vulkan/pipeline_recipe.*` conserve les paramètres CPU
et les octets exacts des shaders d'un pipeline complet. Il construit deux
pipelines privés à partir de cette recette. Il prépare l'adaptateur réel ;
**il n'est pas branché sur la couche de Diablo et n'autorise pas la reprise**.

Le [module synchrone de bibliothèques](LIBRARY_VARIANTS.md) complète ce chemin
pour GPL, pendant que les objets et paramètres sources sont vivants. Il ne
change pas les restrictions du présent module de recettes monolithiques.

## Propriété et durée de vie

`Recipe::capture` reçoit les informations de création et un lookup des octets
exacts des modules shader. Il copie noms d'entrée, tableaux de sommets,
viewports/scissors statiques, sample masks, formats, blend/logic/write masks,
profondeur/stencil, déclarations dynamiques et données de spécialisation.
Aucun pointeur CPU ni handle de shader source n'est conservé. Le handle de
pipeline layout reste emprunté ; son maintien en vie incombe à l'adaptateur.

`Recipe::stereo` reçoit le vertex shader transformé et les deux adresses de
paquets d'œil distinctes, non nulles et alignées à 16 octets. Il vérifie le tag
ABI 1 et un SpecId 4 uint64, recrée deux modules shader temporaires, puis crée
les deux pipelines. Les spécialisations originales sont copiées ; SpecId 4 est
remplacé ou ajouté pour chaque œil. Une utilisation originale de cet ID sans
tag stéréo est refusée. Les modules temporaires sont détruits après compilation.

L'objet `Variants` détruit seulement les pipelines qu'il possède. Une création
partielle en échec est nettoyée. Il n'appelle ni submit ni wait, et ne détruit
aucun objet emprunté. Le propriétaire doit garder les variantes et leur layout
jusqu'à la fin de leur dernière utilisation GPU, puis les libérer avant le device.
Le module ne suit pas les trames en vol.

Les octets source doivent provenir du module exact. Un candidat d'un catalogue
ou un identifiant du pilote ne remplit pas ce contrat. La recette ne certifie
pas l'équivalence d'interface du shader transformé, ses effets externes ou les
alias de ressources. Le [recensement statique](SHADER_EFFECTS.md) ne fournit pas
cette autorisation ; les états de spécialisation et la télémétrie doivent aussi
être audités. Le futur adaptateur devra conserver le passage original lorsque
la capture d'une recette est refusée ou qu'une dépendance manque.

## Périmètre initial

Seuls les pipelines monolithiques vertex + fragment en rendu dynamique sont
acceptés. Les flags 64 bits priment sur les flags historiques. Les seules
extensions de création couvertes sont RenderingCreateInfo et Flags2. Les états
dynamiques acceptés sont ceux déjà gérés par le moteur de reprise. Les listes
viewport/scissor ignorées ne sont pas lues ; WITH_COUNT normalise les comptes
ignorés à zéro. Les pointeurs tessellation et base pipeline ignorés sont retirés.
Les états depth/stencil ou blend ignorés faute d'attachement ne sont pas lus.

GPL, pipelines dérivés, render pass classique, multiview, rasterizer discard,
stages inline/identifiants sans modules exacts et extensions non couvertes
restent refusés. Le refus de GPL précède tout accès à ses pointeurs de stage ou
d'état potentiellement ignorés. Aucun fallback n'invente une description.

Une recette limite chaque shader à 4 Mio, les couleurs à 8, les viewports et
scissors à 16, les bindings à 32, les attributs à 64, les états dynamiques à 32,
les entrées de spécialisation à 64 et leurs données à 4 Kio par stage avant
l'ajout du paquet d'œil. Les doublons de stages, états dynamiques et constantes
sont refusés. Le contrôle des octets couvre l'en-tête et les tailles
d'instructions ; il ne remplace pas `spirv-val` ni la validation du pilote.

## Preuve synthétique

La fixture MRT accepte `--pipeline-recipes` avec `--recorded-replay`. Elle
capture la recette dans la création du pipeline de contrôle, puis détruit les
trois modules shader sources et les tableaux d'octets des deux stages originaux
avant la création des variantes. Le code transformé reste fourni à cet appel.
Les variantes automatiques remplacent les deux
pipelines d'œil auparavant construits directement. Les références continuent
à utiliser les pipelines originaux indépendants.

Sur RTX 5090, les seize vues finales correspondent aux références : quatre
couleurs, profondeur et stencil sans différence mesurée. Les continuations
CLEAR/LOAD, descriptor buffers, douze états dynamiques, 272 restaurations,
dessin témoin suivant et calcul de 32 mots restent exacts. Quatre exécutions
positives et deux contrôles négatifs passent, dont Steam Runtime et la couche
de métadonnées. Cinq exécutions du chemin manuel précédent passent aussi.

Le test C++ sans pilote couvre destruction des sources CPU, spécialisations,
flags2, pointeurs ignorés empoisonnés, refus GPL/extensions, échecs de shaders
et création partielle de pipelines. La CI l'exécute sous Linux et Windows ;
un contrôle Linux supplémentaire utilise ASan et UBSan. Les 100 tests Python
restent valides. [Mesures et empreintes](proofs/pipeline-recipes-v1.json).

Aucune passe du jeu n'est reprise ici. Restent le lookup sûr des modules réels,
GPL/états non couverts, l'audit ressources/lectures/barrières et l'adaptateur
complet de commandes, puis les passes suivantes et la soumission SteamVR avec
poses prédites et gestion des files/trames en vol. Le backend D3D12 natif Windows
reste absent.

Références : [création graphique](https://docs.vulkan.org/refpages/latest/refpages/source/VkGraphicsPipelineCreateInfo.html),
[viewport et états ignorés](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineViewportStateCreateInfo.html),
[spécialisation](https://docs.vulkan.org/refpages/latest/refpages/source/VkSpecializationInfo.html).
