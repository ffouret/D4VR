# Capture et reprise contrôlées des dessins

`native/vulkan/graphics_replay` enregistre maintenant un flux graphique limité,
le reproduit dans les deux cibles privées, puis rétablit l'état graphique
d'origine. La preuve GPU utilise uniquement les sources originales de la
fixture. **Ce moteur n'est pas raccordé à la couche chargée par Diablo IV :
aucun dessin du jeu n'est encore rejoué ni envoyé à SteamVR.**

Les [mesures agrégées et empreintes des sources](proofs/graphics-replay-v1.json)
permettent de rattacher les résultats ci-dessous au binaire de contrôle testé.
La [preuve des descriptor buffers](proofs/descriptor-replay-v1.json) complète
ce premier relevé avec un calcul témoin après la reprise.
Les [états dynamiques supplémentaires](DYNAMIC_STATE.md) ajoutent les compteurs,
strides et changements de profondeur, avec contrats de pipeline explicites.

## Enregistrement et émission

Un objet `Graphics` correspond à un enregistrement de command buffer. Chaque
méthode transmet immédiatement l'appel original une fois, puis conserve une
copie des données nécessaires : pipeline, descriptor sets et offsets dynamiques,
push constants, viewports, scissors, référence stencil, index et vertex buffers,
dessins directs ou indexés. Les tableaux et octets temporaires sont possédés
par le moteur ; leurs adresses CPU peuvent être réutilisées après l'appel.

L'état établi avant `BeginRendering` forme le préfixe du scope. Les changements
et dessins suivants sont conservés dans leur ordre. Après l'`EndRendering`
original, un scope admissible est émis une fois pour chaque œil, avec le pipeline
spécifique à cet œil et son paquet GPU immuable. Les deux vues utilisent les
mêmes arguments et ressources de scène. Aucun dessin original n'est supprimé.

Le moteur réémet ensuite l'historique des commandes d'état avec les pipelines
originaux, sans réémettre les dessins. Cette première implémentation privilégie
la fidélité et conserve des états redondants. Elle ne constitue pas une mesure
de performance ni une stratégie finale pour les trames du jeu.

Les handles restent empruntés. Le moteur ne soumet pas de travail, n'attend
aucune queue et ne détruit aucun objet du renderer. Les cibles possèdent leurs
propres images et allocations, selon le contrat décrit dans
[STEREO_TARGETS.md](STEREO_TARGETS.md). L'appelant doit garantir les durées de
vie, la propriété de queue et la fin du travail avant réutilisation.

## Admission et refus

Un pipeline doit être enregistré avec ses deux variantes, son layout et les
états qu'il consomme. Le contrat exige un audit excluant les écritures externes
et l'aliasing des attachements avec les entrées des shaders. Ces propriétés ne
sont déduites ni d'un handle ni d'une compilation réussie. Les variantes doivent
conserver la même interface et le même état fixe que l'original.

L'adaptateur doit signaler par `unsupported()` toute commande dont les effets
ne sont pas modélisés : écritures de ressources, requêtes, rendu conditionnel,
variantes de descripteurs non couvertes, autres états dynamiques, commandes indirectes ou secondaires,
etc. Ce signal interdit la reprise du reste de cet enregistrement. Le module
n'intercepte pas automatiquement toute l'API Vulkan ; cette couverture est une
condition préalable au futur raccordement au jeu.

Les autres refus couvrent les pipelines inconnus/non audités, états requis
incomplets, signatures d'attachements différentes et scopes sans dessin.
Les seuls scopes admis ont quatre couleurs et une profondeur/stencil, une seule
couche, sans multiview, resolve, extension `pNext` ni flags spéciaux. Leurs
dimensions et formats doivent correspondre aux cibles privées. Les changements
de layout de descripteurs invalident conservativement tous les sets suivis :
aucune compatibilité de layouts distincts n'est supposée.

Les clear values sont copiées et les opérations couleur, profondeur et stencil
restent distinctes. Un `LOAD` exige une initialisation privée préalable **et les
mêmes vues originales pour chaque aspect chargé**. Des formats identiques ne
suffisent pas. Un scope refusé invalide cette continuité. Un enregistrement
commençant par des `LOAD`, comme les candidats actuels du jeu, reste donc refusé.
Il faut d'abord retrouver et reprendre les producteurs de leur contenu.

Le journal limite chaque enregistrement à 8 192 commandes et à 1 Mio de nœuds
et données comptabilisés, hors structures auxiliaires. Le dépassement du budget
ou une erreur d'allocation désactive la reprise ; les appels originaux continuent
d'être transmis.

## Descriptor buffers

L'activation explicite de `DescriptorLimits` ajoute la capture de
`vkCmdBindDescriptorBuffersEXT` et `vkCmdSetDescriptorBufferOffsetsEXT`.
Sans activation, aucun pointeur de fonction de cette extension n'est demandé.
Le contrat du pipeline distingue les sets traditionnels des descripteurs en
buffers ; une liaison d'un autre type ne satisfait pas un état requis.
Les adresses, usages, indices et offsets 64 bits sont copiés, sans conserver
les tableaux CPU de l'appelant. Les chaînes `pNext`, push descriptors et
samplers immuables embarqués restent exclus. Les indices et alignements sont
contrôlés avec les limites fournies par l'adaptateur, jusqu'à 32 bindings.

Les bindings de buffers sont globaux aux bind points. Chaque rebind invalide
conservativement les offsets suivis ; ils doivent être réétablis. Les offsets
**graphiques et de calcul** et les sets traditionnels de ces deux bind points
font donc partie de l'historique restauré. L'adaptateur doit couvrir leurs
appels depuis le début de l'enregistrement. Une commande de descripteur pour
un autre bind point doit interdire la reprise. Les changements entre layouts
distincts conservent la politique de refus conservatrice.

Les octets contenus dans les buffers GPU et les ressources qu'ils référencent
ne sont pas copiés. Leurs durées de vie, lectures/écritures, plages et dépendances
doivent être auditées par l'adaptateur. Cette capture de liaisons n'autorise
aucune reprise arbitraire d'un heap du jeu.

La fixture ajoute `--descriptor-buffers --compute /travail/descriptor-echo.spv`
au mode de reprise. Les images de référence utilisent toujours des descriptor
sets traditionnels et le shader original. Deux UBO sont décrits dans trois
slots privés : la scène originale pour les graphiques, et deux caméras
indépendantes. Après la reprise, un dispatch de contrôle utilise sa propre
caméra, à un offset différent, **sans rebind de descripteur**. Les 32 mots lus
sur huit dispatches correspondent exactement aux UBO attendus. Le dessin
suivant sans rebind reste également conforme. Ce mode restaure 132 commandes
d'état, au lieu de 108 dans le chemin initial.

Un second contrôle négatif attribue volontairement au calcul l'offset de la
caméra graphique. Cet offset est valide pour Vulkan, mais ses données sont
incorrectes : les huit échantillons sont détectés comme erronés. La validation
Vulkan ne signale aucune erreur d'API pour ce cas. Le contrôle de référence
vérifie donc aussi les données, au-delà de la légalité des appels. Ce cas est
explicitement exclu des résultats positifs.

Sur RTX 5090, les trois exécutions positives (dont Steam Runtime) et les deux
contrôles négatifs passent. Le chemin traditionnel est revérifié séparément.
Le test GPU couvre ici des UBO en buffers ; il ne prouve pas encore des images,
samplers, descripteurs mutables ou heaps de VKD3D en cours de modification.

```sh
glslangValidator -V --target-env vulkan1.2 native/tests/descriptor_echo.comp -o /travail/descriptor-echo.spv
spirv-val --target-env vulkan1.2 /travail/descriptor-echo.spv
```

Ajouter `--descriptor-buffers --compute /travail/descriptor-echo.spv` à la
commande de validation ci-dessous. Le validateur exige alors les mesures du
calcul suivant et le contrôle négatif d'offset incorrect.

Références : [bindings globaux de buffers](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindDescriptorBuffersEXT.html),
[offsets par bind point](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetDescriptorBufferOffsetsEXT.html),
[sets traditionnels par bind point](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindDescriptorSets.html).

## Preuve GPU

Le mode `--recorded-replay --split-passes` de
`d4vr_stereo_targets_fixture` utilise le moteur pour enregistrer les dessins de
l'application de contrôle. Les variantes par œil ne sont plus dessinées
explicitement par la fixture. Les constantes sont écrites en deux morceaux puis
leur mémoire CPU est effacée ; les tableaux de viewport/scissor sont également
modifiés après enregistrement.

Pour chaque exécution positive sur RTX 5090 :

- 12 scopes capturés produisent 24 copies de dessins dans les yeux et 16 vues
  finales, dont une continuation `CLEAR` puis `LOAD`.
- Les quatre sorties couleur correspondent aux références indépendantes sur
  16 777 216 octets ; 1 048 576 valeurs de profondeur et autant de valeurs stencil
  correspondent également, sans différence mesurée.
- Le moteur réémet 108 commandes d'état d'origine. Un dessin témoin **sans aucun
  rebind graphique** correspond à une référence explicitement configurée, sur
  10 485 760 octets définis de couleur, profondeur et stencil.
- Le flux original transmis correspond à un rendu indépendant non enregistré,
  sur 10 485 760 octets définis. Ses lectures avant/après les rendus de référence
  restent identiques.

Les références stéréo utilisent le shader original avec les transformations de
caméra calculées sur CPU. Elles ne réutilisent pas la commande de reprise pour
construire l'image attendue. Les données D24 indéfinies sont exclues ; la
comparaison de profondeur accepte au plus quatre unités de quantification et
mesure ici zéro. Les témoins d'état/isolation exigent une égalité stricte.

Le validateur lance trois contrôles positifs, dont Steam Linux Runtime avec la
validation de synchronisation, et un contrôle négatif de dépendance manquante.
Ce dernier doit déclencher `SYNC-HAZARD-WRITE-AFTER-WRITE` et n'est jamais annoncé
comme une image positive validée. Exemple avec les shaders construits selon
[STEREO_TARGETS.md](STEREO_TARGETS.md) :

```sh
python3 tools/stereo_mrt_validate.py --fixture /build/d4vr_stereo_targets_fixture --original /travail/mrt.vert.spv --instrumented /travail/mrt.stereo.spv --fragment /travail/mrt.frag.spv --validation-layer /chemin/manifest-khronos --runtime /chemin/SteamLinuxRuntime_4/run --output /travail/reprise-neuve --recorded-replay --split-passes
```

`d4vr_graphics_replay_tests` vérifie sans pilote les copies de données transitoires,
arguments indexés, offsets dynamiques, modifications partielles de constantes,
restauration des états et refus. Ses assertions restent actives en Release.
Il est inclus dans CTest avec `D4VR_BUILD_STEREO_TARGET_TESTS=ON` et des headers
Vulkan, sans bibliothèque de pilote. La CI compile et exécute ce code commun
sur Linux et Windows ; le test GPU reste propre à la machine de référence.

## Travail restant pour Diablo

Il faut compléter l'adaptateur pour les variantes de descripteurs et états
dynamiques utilisés par VKD3D, auditer les lectures/écritures et durées de vie des ressources,
retrouver l'initialisation des attachements `LOAD`, et construire des variantes
de pipelines du jeu. Les multiples queues et trames en vol exigent une gestion
explicite des ressources et des dépendances. La composition des passes suivantes,
les poses prédites et la soumission des images du jeu à SteamVR restent à
implémenter. Le backend D3D12 natif sous Windows reste également absent.
