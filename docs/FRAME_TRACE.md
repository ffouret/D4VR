# Relevé des passes à la demande

Le mode `--frame-trace` complète les compteurs Vulkan par un relevé des sorties
couleur/profondeur et des pipelines utilisés. Il prépare le choix des passes
à adapter au rendu par œil. Il ne copie aucune texture, ne modifie aucun shader
et ne crée aucune session XR. Le casque peut rester déconnecté.

## Informations relevées

Les créations d'images et de vues fournissent le format, les dimensions, les
usages, le sous-ensemble de mip/layers et les opérations load/store/resolve des
attachements de rendu dynamique. Les identifiants sont des compteurs locaux,
sans adresse Vulkan brute. Une image sans création observée, notamment une
image de swapchain, garde `image_known=false` et l'identifiant 0.

La création des pipelines associe chaque stage à une empreinte FNV-1a64 et à
la taille du SPIR-V. Les bibliothèques de pipelines graphiques et les modules
SPIR-V fournis directement dans les informations de stage sont pris en compte.
Un shader réutilisé depuis le cache peut fournir uniquement un identifiant de
module. Celui-ci est conservé séparément, sans inventer une empreinte de code.
Les modules absents et bibliothèques inconnues restent signalés. Les snapshots survivent à la destruction des modules,
bibliothèques, vues et images ; la réutilisation d'un handle crée un nouvel ID.
Les commandes générées sur GPU ne sont pas attribuées au pipeline lié sur CPU.

Le relevé conserve les scopes de rendu, les dispatchs et les occurrences
d'exécution de buffers secondaires. Les dessins sont agrégés par pipeline dans
chaque scope : ce format **n'est pas une suite de commandes rejouable**.
Les lectures d'images, descripteurs et barrières ne sont pas suivis ; les
attachements seuls ne constituent pas un graphe complet de dépendances.
Les passes classiques conservent leur zone et leurs comptes, sans attachements.

## Déclenchement

Construire et valider la couche comme décrit dans [VULKAN_TRACE.md](VULKAN_TRACE.md).
Le bundle doit porter une preuve `frame_trace_fixture_passed=true` associée
à l'empreinte exacte du binaire. Ajouter `--frame-trace` au lancement isolé :

```sh
python3 tools/vulkan_trace.py run --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --bundle /travail/couche-validee --runtime-bundle /travail/bundle-geometrie-valide --output /travail/session-neuve --frame-trace
```

Une fois en partie sur le moniteur, depuis un autre terminal :

```sh
python3 tools/frame_trace.py request /travail/session-neuve
python3 tools/frame_trace.py report /travail/session-neuve/frame-PID-DEVICE-1.json --output /travail/rapport-passes.json --shader-directory /travail/catalogue/shaders --camera-chain /travail/catalogue/camera-chain.json
```

Pour les shaders mis en cache, l'outil natif facultatif construit un index privé
à partir du catalogue local. Il utilise son propre périphérique et ne soumet
aucune commande GPU. Fournir le nom exact du GPU utilisé pour la capture :

```sh
work/build/d4vr_shader_identifiers /travail/catalogue/shaders /travail/identifiants-neufs.json "NVIDIA GeForce RTX 5090"
python3 tools/frame_trace.py report /travail/session-neuve/frame-PID-DEVICE-1.json --output /travail/rapport-passes.json --identifier-catalog /travail/identifiants-neufs.json --camera-chain /travail/catalogue/camera-chain.json
```

Cet exécutable est construit avec `D4VR_BUILD_GPU_PROBE_FIXTURE=ON`. L'index doit
partager le même UUID d'algorithme d'identification que la capture, sinon
l'analyse refuse le rapprochement. Cet UUID décrit l'algorithme du pilote,
pas l'identité matérielle privée du GPU. Un identifiant égal peut désigner deux
définitions dont les différences n'affectent pas la compilation ; l'association
reste un candidat, sans preuve de stricte identité des octets.

La demande est écrite atomiquement. Une présentation arme le relevé ; la
présentation suivante le ferme. Il contient les soumissions CPU observées entre
ces appels, avec les résultats des appels au pilote. Une soumission encore en
cours retarde l'écriture. Ce périmètre **ne garantit pas une trame GPU complète
ni l'ordre d'exécution entre queues**. Sans extension swapchain, la fixture
capture une seule soumission. Les soumissions échouées restent visibles dans
le brut et sont exclues des comptes d'exécution du rapport.

Le rapprochement facultatif avec le catalogue local utilise FNV et la taille :
il fournit des candidats, pas une preuve cryptographique d'identité. Une matrice
trouvée dans l'analyse statique ne prouve ni sa valeur courante ni le rôle
visible de la passe. Les rapports conservent `scene_pass_identified=false` et
`game_stereo_submitted=false`.

## Bornes et validation

Chaque périphérique accepte quatre demandes. Une capture est bornée à 256
soumissions, 256 occurrences de buffers par soumission, huit niveaux de
secondaires et 8 192 opérations. L'enregistrement conserve au plus 2 048
opérations par buffer et 8 192 par périphérique, 512 variantes de pipeline par
scope, 65 536 variantes actives et 65 536 entrées par table de ressources.
Les limites de listes et d'opérations indiquent une troncature ; une ressource
non suivie reste inconnue. Le fichier JSON privé est créé en mode 0600, sans
écrasement, et limité à 8 Mio. Une sérialisation trop grande produit un événement
d'erreur au lieu d'un fichier partiel présenté comme complet.

Les tests locaux sur RTX 5090/pilote 615.78.08 comparent les images entières
avec et sans couche, en rendu classique Vulkan 1.2 et dynamique Vulkan 1.3.
Les quatorze exécutions incluent les sondes d'observation et de déplacement
géométrique, ainsi que Steam Linux Runtime 4 pour le rendu dynamique. Toutes
les empreintes de pixels concordent ; la parallaxe reste à −11,4232 et −5,72029
pixels à deux et quatre unités. Le relevé retrouve un dessin, le bon format,
les dimensions et les empreintes exactes des shaders synthétiques. Quatre
exécutions utilisent des pipelines créés depuis leurs seuls identifiants de
module ; le relevé les rapproche correctement de l'index calculé séparément.

Les tests C++ sans pilote vérifient aussi GPL, modules inline, réutilisation des
handles et des secondaires, reset, demandes consommées, soumissions en cours ou
échouées et récupération des budgets. La suite Python compte 73 tests locaux.

## Premier relevé dans Diablo IV

Deux demandes récupèrent respectivement 202 et 223 scopes de rendu dynamique,
160 et 162 dispatchs compute, sur 21 et 20 soumissions CPU entre deux appels de
présentation. Elles contiennent 2 875 et 2 816 commandes de dessin, sans
troncature, buffer inconnu ou pipeline manquant. Les textures couleur et
profondeur sont retrouvées, notamment un groupe d'attachements 1 707 × 960 et
des sorties 2 560 × 1 440. Cela ne suffit pas à prouver leur rôle visible.

Le propriétaire confirme une image normale en partie. Le lancement se termine
avec le code 0, les processus sont fermés et les empreintes originales du jeu
et de la DLL NVIDIA sont conformes. L'analyse du premier relevé retrouve le code
de seulement 10 stages sur 468 occurrences ; les cinq empreintes distinctes
correspondent à des candidats du catalogue. Le suivi des identifiants a été
ajouté après cette limite ; l'index natif local contient 10 114 shaders classiques.
Les fichiers de bibliothèques ray tracing du catalogue ne sont pas indexés par
ce programme et leurs pipelines ne sont pas suivis dans ce mode.

Une identification vérifiée de la passe de scène et le coût CPU du relevé dans
Diablo restent à établir. Aucune image du jeu n'est encore soumise au casque.

## Relevé avec identifiants de cache

La seconde version retrouve un identifiant ou une empreinte de code pour chacun
des 510 et 386 stages des pipelines référencés dans deux nouvelles demandes.
Le catalogue fournit un candidat pour 328 des 339 clés de shader distinctes du
premier relevé, et 262 des 267 du second. Aucun rapprochement n'est inventé pour
les clés restantes. Les UUID d'algorithme concordent.

Les relevés contiennent 193 et 149 scopes de rendu dynamique, 292 et 136
dispatchs, sur 19 soumissions chacun. Aucun buffer ni pipeline n'est manquant,
et aucune troncature n'est signalée. Dans les deux relevés, des vertex shaders
associés au déplacement géométrique prouvé précédemment écrivent dans le même
groupe de quatre attachements couleur et d'un depth/stencil 1 707 × 960.
Au total, six des huit identités de shaders de cette preuve précédente trouvent
des candidats dans ce groupe. Cette association désigne un groupe concret à
étudier pour la stéréo ; elle ne prouve ni le contenu visible des textures, ni
un déplacement actuel, ni une couverture complète du décor et du personnage.

Le propriétaire confirme une image normale pour cette seconde version. Le
lancement se termine avec le code 0, Diablo et Battle.net sont fermés, et les
empreintes de l'exécutable et de la DLL NVIDIA d'origine sont conformes.

- [Informations de rendu dynamique](https://docs.vulkan.org/refpages/latest/refpages/source/VkRenderingInfo.html)
- [Liaison des bibliothèques de pipelines](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineLibraryCreateInfoKHR.html)
- [Identifiants sans code SPIR-V](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineShaderStageModuleIdentifierCreateInfoEXT.html)
- [Calcul de l'identifiant d'un module](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetShaderModuleCreateInfoIdentifierEXT.html)
