# Diagnostic de création des paires dans la couche Vulkan

La couche `trace_layer.cpp` raccorde `LibraryVariants` aux créations originales,
pour compiler deux graphes privés sur le périphérique de l'application.
**Elle ne lie jamais ces nouveaux pipelines aux commandes, ne rejoue aucun
dessin et ne soumet aucune image à SteamVR. Ce raccordement est testé sur des
fixtures synthétiques ; aucun nouvel essai de Diablo n'a été effectué.**

## Activation et transmission

Les deux variables `D4VR_PAIR_LEFT_FILE` et `D4VR_PAIR_RIGHT_FILE` désignent des
paquets ABI 1 privés de 176 octets, immuables pendant la session. La couche exige
les identités gauche/droite et la même séquence. Le device doit déjà avoir
activé buffer device address. Cette option exclut `D4VR_EYE_STATE_FILE`, qui
modifie les créations originales pour l'ancien diagnostic d'un œil fixe. Une
configuration invalide laisse l'application continuer avec ses appels originaux
et écrit `paired_pipeline_error` ; elle n'active pas partiellement le mode.

L'appel original conserve ses paramètres, pointeurs, cache, allocator, sorties
et résultat. Après sa transmission unique, chaque sortie non nulle est observée
avant de revenir à l'application, même si le lot a retourné une erreur. Les
créations privées utilisent exclusivement les fonctions du maillon suivant,
sans récursion dans la couche. Les sorties de feedback originales sont exclues
des copies. Un échec privé ou `VK_PIPELINE_COMPILE_REQUIRED` ne change pas le
résultat original et ne déclenche aucun nouvel essai.

La sélection exige un vertex déjà transformé, portant le tag stéréo ABI 1 et le
SpecId 4. Les handles de modules sont suivis jusqu'à leur destruction ; les
identifiants du pilote proviennent uniquement de modules/code ainsi marqués.
Un stage inline marqué est accepté. Les listes sont limitées à 65 536 éléments,
les chaînes de stage à huit nœuds. Aucun candidat du catalogue des shaders du
jeu n'est considéré comme une identité vérifiée.

Les bibliothèques dont la pré-rastérisation n'est pas marquée sont refusées.
Les morceaux neutres d'un graphe GPL peuvent être copiés avant de connaître un
futur lien ; ils consomment le budget. Ce diagnostic limite les graphes privés
à 512 nœuds vivants et 512 appels de création cumulés par device. Une création
contient deux pipelines. Les destructions originales restent transmises sans
être retardées. Les graphes privés gardent leurs propres enfants en vie et sont
libérés avant leurs paquets et avant le device. Aucun usage GPU de ces paires
n'est introduit dans ce mode ; leur réutilisation future devra gérer fences,
command buffers et ressources en vol.

## Preuves distinctes

La [preuve agrégée](proofs/paired-pipelines-v1.json) identifie les sources,
binaires et rapports par empreinte. Deux groupes de huit cas passent sur RTX
5090 : monolithique puis GPL avec le témoin original et ses bibliothèques
conservés jusqu'à la fin du GPU. Dans ce dernier groupe, la couche observe 35
lots et 51 sorties, construit 36 paires de bibliothèques et 12 d'exécutables,
et refuse trois sources non marquées. Aucun échec privé ou cache miss n'est
signalé ; aucun nœud privé ne subsiste à la fermeture normale de la fixture.

Les images de la fixture continuent de correspondre aux références couleur,
profondeur et stencil. Ses seize vues proviennent de ses propres pipelines de
contrôle ; **elles ne démontrent pas le rendu des paires ajoutées par la couche**.
Les contrôles négatifs de dépendance manquante et d'offset de calcul erroné
restent détectés. Les cas sans activation et avec un seul paquet ne créent pas
de paires. Les métadonnées GPL laissent les états effectifs non résolus.

Trois essais supplémentaires couvrent un module puis des identifiants issus
d'un cache préparé, dont Steam Runtime. Le passage par identifiant crée quatre
paires d'exécutables, contre deux avec le code seul. Il démontre une réussite
avec cache chaud ; aucun succès de compilation par identifiant à froid n'est
annoncé. Le refus d'un cache miss est aussi testé sans pilote. Les quatorze
régressions de couche et dix-huit du diagnostic ancien à un œil sont séparées.

Le problème natif antérieur concernant un témoin GPL lié après destruction de
ses bibliothèques reste [documenté](LIBRARY_VARIANTS.md). Aucune temporisation
des destructions de l'application n'a été ajoutée pour le contourner.

## Reproduire hors jeu

Construire la couche avec `D4VR_BUILD_VULKAN_TRACE=ON` et la fixture avec
`D4VR_BUILD_GPU_PROBE_FIXTURE=ON`. Après compilation et transformation des shaders
originaux de fixture, le validateur accepte `--trace-layer /travail/couche` et
`--paired-pipelines`. Exemple à adapter aux chemins locaux :

```sh
python3 tools/stereo_mrt_validate.py --fixture /travail/d4vr_stereo_targets_fixture --original /travail/mrt-vertex.spv --instrumented /travail/mrt-vertex-instrumented.spv --fragment /travail/mrt.frag.spv --compute /travail/descriptor-echo.spv --split-passes --recorded-replay --descriptor-buffers --dynamic-state --library-pipelines --library-original-live --validation-layer /travail/validation --trace-layer /travail/couche --paired-pipelines --output /travail/essai-neuf
```

Le validateur crée ses deux paquets privés et purge les options héritées des
autres diagnostics. Les lanceurs de jeu existants purgent aussi ces nouvelles
variables. Aucun lanceur de capture du jeu n'active encore ce mode.

Restent l'observation réelle des créations du jeu, la résolution des états GPL,
l'audit des ressources et des `LOAD` préparatoires, une reprise admissible des
passes, les traitements suivants, les poses prédites et la soumission SteamVR.
Le composant de graphes reste portable ; le branchement Linux ne fournit pas
encore d'adaptateur D3D12 natif Windows.
