# Variantes synchrones des bibliothèques de pipelines

`native/vulkan/library_variants.*` traite les créations de pipelines
monolithiques ou assemblés avec GPL. Le module construit un graphe
privé par œil pendant que les paramètres sources sont encore valides. **La couche Vulkan le raccorde maintenant à un diagnostic explicite de
création seule, validé hors jeu. Il ne fournit aucune autorisation de reprise
des dessins.** Voir [le diagnostic de paires](PAIRED_PIPELINES.md).

## Contrat de création

L'adaptateur doit d'abord transmettre l'appel original une seule fois, puis
appeler `observe` pour chaque handle original créé, avant de revenir à
l'application. Le module ne transmet pas lui-même l'appel original. Il reprend
les informations vivantes de création et conserve les champs et chaînes de
stage, notamment les identifiants de cache. Il ne reconstitue pas du SPIR-V à
partir d'un candidat de catalogue. Les deux créations privées utilisent un cache
et un allocator nuls ; les sorties de creation feedback de l'application sont
retirées des copies pour ne pas les réécrire.

Le callback `MarkedStage` doit reconnaître un vertex shader **déjà transformé
avec l'ABI stéréo 1**, par son module ou un identifiant vérifié de ce module.
Cette identité est fournie par l'adaptateur ; elle n'est pas déduite d'un handle.
Le module copie les spécialisations du seul stage vertex reconnu et lui donne
le SpecId 4 de chaque œil. Les autres stages et états sont conservés pendant
l'appel synchrone, sans déréférencer leurs paramètres CPU après son retour.

Les morceaux possédés par chaque bibliothèque sont suivis séparément : entrée
de sommets, pré-rastérisation, fragment et sorties. Les liens privés remplacent
chaque bibliothèque connue par celle du même œil. Des morceaux qui se recouvrent,
une bibliothèque inconnue ou un exécutable utilisé comme bibliothèque sont
refusés. Un lien complet doit couvrir les quatre morceaux et inclure le vertex
transformé. Un lien sans état propre ne lit pas ses pointeurs de stage ou d'état
ignorés, même si les comptes et pointeurs non utilisés contiennent des valeurs
non déréférençables. Les flags 64 bits priment sur les flags historiques.

## Durées de vie et refus

Chaque nœud possède ses deux nouveaux handles et garde ses enfants privés en
vie. Un `Lease` conserve le graphe après retrait des associations originales ou
destruction du registre. `remove` ne détruit jamais un pipeline de l'application.
La réutilisation d'un handle source ne remplace pas un ancien graphe encore loué.
Une suppression pendant une création en cours empêche une inscription tardive.

Le quota compte aussi les nœuds conservés par les leases, pas seulement ceux
encore indexés. Les limites sont 512 nœuds par défaut (configuration au plus
4 096), profondeur 8, 4 bibliothèques liées, 16 stages, 8 nœuds pNext et
64 entrées/4 Kio de spécialisation par stage avant ajout de l'adresse d'œil.
Une seconde limite borne le nombre cumulé d'appels privés : 4 096 par défaut,
512 dans le diagnostic de couche. Détruire les sources ne réinitialise pas ce
budget. Les bibliothèques possédant une pré-rastérisation non marquée sont
refusées avant compilation ; les morceaux neutres restent copiés sous quota.
Les échecs partiels sont nettoyés. Un `VK_PIPELINE_COMPILE_REQUIRED` reste un
refus de variante : aucun remplacement ou retry ne modifie l'appel original.

Le propriétaire doit retenir les leases jusqu'à la fin des usages GPU et tous
les libérer avant le device. Le module n'attend ni ne soumet de travail et ne
suit pas les fences ou queues. Les layouts et objets originaux gardent leur
gestion de durée de vie par l'application. Le [contrat dynamique par partie](GPL_DYNAMIC_STATES.md)
est maintenant conservé pour les états reconnus. Les autres états GPL, contrats
de commandes, descripteurs, alias, lectures/écritures et barrières restent à
auditer. La création de pipelines ne prouve pas leur admissibilité à la reprise.
Les extensions racines non couvertes, render passes classiques, multiview,
tessellation/geometry/mesh et flags non modélisés sont refusés.

## Deux contrôles GPU distincts

La fixture originale construit trois bibliothèques (morceaux 1, 6 et 8), puis
un exécutable avec optimisation de liaison. Les deux modules shader propres à
ces bibliothèques sont détruits avant la liaison finale. Le registre est détruit
avant le rendu ; les graphes privés sont retenus uniquement par leur lease.

- `--library-pipelines` détruit aussi les bibliothèques et l'exécutable sources
  avant le rendu. Le flux original témoin est monolithique et indépendant.
  Les deux yeux utilisent exclusivement les exécutables du graphe privé loué.
- `--library-pipelines --library-original-live` transmet pour témoin
  l'exécutable GPL original et conserve ses bibliothèques jusqu'à la fin des
  commandes GPU. Cela vérifie séparément le rendu original GPL préservé et la
  restauration de ses états après les deux yeux.

Chaque mode passe six exécutions sur RTX 5090, dont Steam Linux Runtime,
validation Vulkan, métadonnées et les deux contrôles négatifs. Les seize vues
correspondent aux références couleur/profondeur/stencil, sans différence mesurée.
Les 272 commandes restaurées, le dessin suivant et les 32 mots du calcul restent
exacts. Cinq régressions du mode recettes monolithiques passent également.

Le relevé conserve correctement les déclarations GPL sans prétendre résoudre
leur liste effective. Le contrat de douze états de cette fixture est connu
explicitement ; aucune union brute de listes du jeu n'est autorisée. Les tests
C++ et leur version ASan/UBSan dans la CI Linux couvrent ownership, liens,
spécialisations, identifiants simulés, feedback, quotas, recyclage et échecs
partiels. Windows exécute les tests C++ communs ; les 100 tests Python passent.
Dans cette preuve initiale, les stages par identifiant étaient testés sans
pilote. Le nouveau [diagnostic de couche](PAIRED_PIPELINES.md) ajoute une
réussite GPU avec cache préparé ; les cache misses restent refusés.

## Limite native observée

Un premier essai utilisait aussi le **témoin GPL original après destruction de
ses bibliothèques sources**. Le processus est sorti avec le code 139 pendant
`vkCmdBindPipeline`, dans la pile du pilote 615.78.08. La cause exacte n'est pas
établie et ce cas n'est pas déclaré fonctionnel. Le binaire de cet essai n'a pas
été archivé ; l'empreinte du journal privé figure dans les preuves.

Les deux contrôles publiés rendent explicites leurs durées de vie différentes ;
ils ne prouvent pas ce troisième cas. Aucun contournement du pilote ni retard
sur les destructions de l'application n'a été ajouté au module. La source
VKD3D upstream étudiée utilise ce motif de bibliothèques, mais le runtime a des
modifications locales non reconstituées : ce n'est pas un audit complet des
créations réellement utilisées par Diablo.

[Mesures et limites](proofs/library-variants-v1.json). Aucun nouvel essai du jeu
ni envoi SteamVR n'est associé à cette étape. Le raccordement des créations
est ajouté dans le [diagnostic de paires](PAIRED_PIPELINES.md), testé hors jeu.
Restent les essais réels, le raccordement des commandes, l'audit des ressources
et dépendances, les passes suivantes, poses prédites et soumission SteamVR. Le
backend D3D12 natif Windows reste absent.

Références : [états et bibliothèques Vulkan](https://docs.vulkan.org/spec/latest/chapters/pipelines.html),
[GPL](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_graphics_pipeline_library.html),
[structure de liaison](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineLibraryCreateInfoKHR.html).
