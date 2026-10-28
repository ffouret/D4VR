# Observation du renderer Vulkan

La couche explicite `VK_LAYER_D4VR_trace` prépare le raccordement entre VKD3D
et la chaîne native OpenXR. Elle relève les versions, extensions et commandes
réellement soumises. Le mode d'observation ne crée pas de session XR et ne rejoue aucune commande.
Les arguments, résultats et synchronisations Vulkan d'origine sont conservés.

## Pourquoi observer les commandes indirectes

Le journal VKD3D du jeu annonce l'activation de
`VK_EXT_device_generated_commands` pour les opérations ExecuteIndirect. Cette
extension peut générer les commandes de dessin sur le GPU. Un adaptateur qui
ne traiterait que les appels CPU à `vkCmdDraw` pourrait donc manquer de la
géométrie. Le prochain relevé doit mesurer les voies employées dans le jeu.
Il ne suffit pas à identifier la passe principale ou à établir une stratégie
complète de stéréo.

## Informations et limites

La couche transmet les appels aux fonctions suivantes et compte leurs
exécutions lors des soumissions réussies : dessins directs, indexés, indirects,
indirects avec compteur, commandes générées EXT/NV, dispatchs compute, débuts de
passes classiques et de rendu dynamique. Les buffers secondaires référencés
sont agrégés, y compris lorsqu'ils sont exécutés plusieurs fois. Une nouvelle
séquence d'enregistrement ou un reset efface les anciens compteurs ; les
libérations de buffers et destructions de pools retirent leur état.

Les comptes correspondent aux **commandes de l'API**, pas au nombre de dessins
internes à un buffer indirect ou généré. Les commandes mesh et autres voies non
interceptées ne sont pas déduites. L'analyse ne lit ni textures, ni tokens GPU,
ni données privées du jeu. Les groupes de queues et extensions sont relevés
à la création du périphérique ; les exigences OpenXR devront ensuite être
comparées à ce périphérique réel.

Chaque processus écrit son propre JSONL, créé en mode 0600 dans un dossier
privé. Le journal contient au maximum 512 enregistrements. Les huit premières
soumissions sont détaillées ; les huit premières présentations, puis une sur
600 jusqu'à 144 000, donnent un instantané cumulatif. Une destruction normale
du périphérique fournit les comptes finaux. Sans cet événement, le rapport
signale une couverture partielle. Les bornes internes sont de 32 768 buffers
suivis par périphérique, 4 096 références secondaires par buffer et huit niveaux
d'agrégation. Une absence ou troncature reste indiquée dans les compteurs.

Il n'y a pas d'attente GPU supplémentaire. Les compteurs CPU utilisent un mutex ;
leur coût en partie n'est pas encore mesuré. Une validation des pixels sur une
fixture ne garantit pas les performances ou chaque image du jeu.

## Vérifications effectuées

Sur la RTX 5090 et le pilote 615.78.08 :

- couche effectivement chargée, avec les cinq soumissions de la fixture
  d'observation retrouvées et aucun buffer inconnu ;
- les 16 384 octets de l'image de contrôle restent identiques et les 32 valeurs
  de matrices sont récupérées exactement ;
- fixture géométrique : parallaxe conservée à −11,4232 et −5,72029 pixels aux
  distances de deux et quatre unités ; télémétrie et projection hors cible vérifiées ;
- même fixture d'observation exécutée dans Steam Linux Runtime 4 : chargement
  de la couche et conservation des pixels confirmés ;
- tests sans pilote : négociation du loader, relais exact des arguments DGC,
  alias KHR, buffers secondaires réexécutés, soumission échouée et durée de vie
  des enregistrements.

Le programme de contrôle demande Vulkan 1.2.0 et le GPU expose Vulkan 1.4.351.
Ces versions appartiennent à la fixture ; elles ne préjugent pas de celles de
Diablo IV. Les contrôles sur le jeu sont enregistrés séparément après la session.

Un mode facultatif ajoute le relevé des attachements et pipelines à la demande.
Sa fixture demande Vulkan 1.3 pour le rendu dynamique ; les comparaisons de
pixels passent aussi dans Steam Runtime. Voir [FRAME_TRACE.md](FRAME_TRACE.md).

## Relevé en partie du 24 octobre 2026

Le propriétaire confirme avoir joué dans le décor avec une image normale.
Le lancement se termine avec le code 0 ; Diablo IV et Battle.net sont fermés,
et les empreintes originales de l'exécutable et de la DLL NVIDIA sont conformes.
Quatre processus produisent un journal ; un seul crée un périphérique graphique,
la RTX 5090. Ses 47 enregistrements restent sous le budget.

Le dernier instantané couvre 13 800 appels de présentation, à environ 134,8
secondes depuis le début du journal. Il contient 34 301 789 commandes de dessin
indexé, 2 043 178 dessins directs, 1 854 816 dispatchs et 82 788 dispatchs
indirects. Les 2 287 902 débuts de rendu sont du rendu dynamique. Aucun buffer
inconnu, aucune liste secondaire tronquée et aucun appel de commandes générées
EXT/NV ne sont relevés dans ces totaux, malgré l'activation de l'extension EXT.
Cela décrit cette session ; d'autres scènes peuvent employer d'autres voies.

Le processus ne transmet pas d'événement `vkDestroyDevice` à la couche avant
sa fermeture. Les nombres sont donc les derniers totaux observés, pas un compte
final garanti de toute la session. Ils ne mesurent ni le nombre de primitives
GPU ni le coût du traçage. Aucune image de Diablo n'a été soumise à SteamVR.

## Exigences de liaison SteamVR

Le contrôle natif `--requirements` interroge séparément la liaison
`XR_KHR_vulkan_enable`, adaptée aux objets Vulkan créés par une application.
Il crée une instance OpenXR pour lire les exigences, sans instance Vulkan,
périphérique Vulkan ou session XR. SteamVR annonce Vulkan 1.0 à 1.2 comme plage
connue, cinq extensions d'instance et huit de périphérique.

Le périphérique du jeu demande déjà les huit noms d'extension requis. Les
instances Vulkan 1.3 présentes dans le même processus demandent les cinq noms
d'instance. Le format initial de trace ne relie pas chaque périphérique à une
instance précise : cette association, le GPU demandé par le runtime et la
synchronisation de queue restent à vérifier. La version maximale annoncée est
une version testée ; les versions plus récentes nécessitent une évaluation
distincte. La comparaison des noms seule ne prouve aucune liaison fonctionnelle.

```sh
XR_RUNTIME_JSON=/chemin/SteamVR/steamxr_linux64.json work/native-xr/d4vr_xr_readiness --requirements > /travail/exigences.json
python3 tools/vulkan_trace.py compare-requirements --trace-directory /travail/session --requirements /travail/exigences.json --output /travail/comparaison.json
```

La soumission Vulkan directe via OpenVR est également documentée par Valve :
elle accepte une description des textures et des objets Vulkan de l'application.
C'est une voie d'intégration à évaluer, sans prototype de soumission du jeu à ce stade.

## Liaison temporaire au périphérique du jeu

Une option séparée vérifie maintenant l'acceptation des objets Vulkan empruntés
à l'application. Elle est désactivée dans le mode d'observation et requiert un
build `D4VR_TRACE_WITH_XR_BINDING=ON`, avec les en-têtes OpenXR. Le loader natif
est chargé dynamiquement uniquement pour ce contrôle ; aucun loader OpenXR n'est
requis pour l'observation normale. Son empreinte est épinglée au build testé.

La couche conserve l'instance et le périphérique d'origine, identifie la queue
par `vkGetDeviceQueue`/`vkGetDeviceQueue2`, puis attend sa première soumission
sur une famille graphique. Elle tente une seule liaison par périphérique. Le
mutex des données de trace est relâché pendant les appels au runtime ; un garde
par thread empêche les tentatives récursives. L'appel reste dans le périmètre
de synchronisation de la queue assuré par l'application.

Le runtime et la couche peuvent voir des handles physiques distincts : le
loader utilise des handles de trampoline et de terminateur. La couche lit
l'UUID matériel par la table de dispatch suivante et compare celui du handle
public renvoyé par OpenXR via le loader public. La valeur de l'UUID n'est jamais
écrite dans le journal. Une absence d'identité ou d'extension requise arrête
le contrôle avant la création de session. La liaison utilise le handle public
attendu par le runtime après cette vérification.

Une session et une swapchain 64 × 64 sont créées, leurs images Vulkan sont
énumérées, puis les objets OpenXR temporaires sont détruits. Aucun objet Vulkan
emprunté n'est détruit ou remplacé. La session n'est jamais démarrée ; aucune
image n'est acquise, dessinée ou soumise. Les commandes de l'application sont
ensuite transmises normalement, que le contrôle ait réussi ou échoué.

La fixture `borrowed_binding_vulkan.cpp` crée ses propres objets Vulkan 1.3 avec
les noms d'extensions provenant d'un relevé frais du runtime. Elle émet une
soumission sans commande, puis vérifie qu'elle peut fermer normalement ses
objets après le contrôle. Le test réussit en natif et dans Steam Runtime 4 :
UUID concordant, extensions présentes, session acceptée et trois images.
Les durées relevées sont respectivement de 121 et 100 ms. Les pixels et la
parallaxe des fixtures GPU habituelles restent conformes avec ce build.

Le contrôle réel sur Diablo IV réussit également : instance Vulkan 1.3,
identité matérielle concordante malgré deux handles différents, session acceptée
et trois images OpenXR. La création et destruction temporaires prennent environ
2,22 secondes, au démarrage. **Ce résultat confirme l'acceptation d'une session
sur le périphérique du jeu ; il ne produit aucune vue stéréo de Diablo.**
La couche libère immédiatement ses objets OpenXR et ne conserve pas une session
active. Le rejeu géométrique, les ressources par œil et la synchronisation des
futures trames de rendu restent à implémenter.

```sh
python3 tools/vulkan_trace.py run --xr-binding --xr-loader /chemin/libopenxr_loader.so --xr-runtime /chemin/SteamVR/steamxr_linux64.json --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --bundle /travail/couche-validee --runtime-bundle /travail/bundle-geometrie-valide --output /travail/session-neuve
```

Le lanceur exige une preuve de réussite de la fixture empruntée et vérifie le
SHA-256 du loader testé avant ce mode. SteamVR et la liaison du casque doivent
être actifs pendant le démarrage. Le champ `xr_binding_probe` du rapport
conserve le résultat ; `game_stereo_submitted` reste faux.

## Construire et lancer

```sh
cmake -S native -B work/vulkan-trace -DD4VR_BUILD_VULKAN_TRACE=ON
cmake --build work/vulkan-trace
ctest --test-dir work/vulkan-trace --output-on-failure
```

L'option est désactivée par défaut. Cette couche cible actuellement Linux et
requiert les en-têtes Vulkan, sans lien vers le loader dans la bibliothèque.
Le cœur stéréo reste portable ; le futur backend D3D12 natif Windows est distinct.

Le lancement isolé utilise `tools/vulkan_trace.py`. `stage` fige le binaire et
le manifest après vérification de l'empreinte du build testé et des preuves
GPU/runtime. `run` vérifie les originaux du jeu, refuse une session Battle.net
déjà active, puis épingle une copie privée du lanceur Lutris au runtime VKD3D
déjà validé. Seules ses variables d'environnement activent la couche ; aucune
installation dans le système ou substitution de bibliothèque du jeu n'a lieu.
Les overrides de shaders sont explicitement retirés pour ce relevé.

```sh
python3 tools/vulkan_trace.py stage --layer-dir /travail/build --proof /travail/validation/proof.json --output /travail/couche-neuve
python3 tools/vulkan_trace.py run --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --bundle /travail/couche-neuve --runtime-bundle /travail/bundle-geometrie-valide --output /travail/session-neuve
```

Entrer en partie et se déplacer environ une minute sur le moniteur, puis fermer
le jeu et Battle.net. `trace-report.json` conserve les derniers totaux connus
et l'indication de fermeture propre par périphérique. La commande `report`
permet de reconstruire ce rapport après une interruption. Les journaux privés,
binaires compilés et relevés du jeu restent hors du dépôt.

## Interfaces officielles

- [Interface entre le loader Vulkan et les couches](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md)
- [Exécution de commandes générées EXT](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdExecuteGeneratedCommandsEXT.html)
- [Exigences de version OpenXR/Vulkan](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrGraphicsRequirementsVulkanKHR.html)
- [Soumission Vulkan à SteamVR via OpenVR](https://github.com/ValveSoftware/openvr/wiki/Vulkan)
- [Identité du périphérique Vulkan](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceIDProperties.html)
