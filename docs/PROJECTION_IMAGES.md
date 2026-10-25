# Images GPU et présentation OpenXR

Le présentateur possède maintenant une entrée `presentProjectionImages` pour
un producteur d'images Vulkan. Il peut recevoir deux images privées, au lieu
d'imposer le moteur de reprise figée. L'ancien `presentReplay` utilise cette
même entrée via un adaptateur ; son inversion verticale reste conservée.
**Ce raccordement est testé avec nos images synthétiques. Aucun adaptateur de
Diablo ni nouvel envoi au casque n'est encore réalisé.**

## Cycle d'une paire

Après localisation des vues OpenXR, le présentateur crée une demande contenant
une séquence, l'heure de présentation prévue, la famille de queue et les deux
paquets de caméra. La séquence avance à chaque demande, même si la précédente
ne produit aucune image. Le producteur reçoit le command buffer privé, après
fin de la fence du travail privé précédent. Il y enregistre ses opérations et
retourne deux images laissées en `TRANSFER_SRC_OPTIMAL`.

Le producteur reste propriétaire des ressources jusqu'à la fin de la fence
suivante. Ce chemin exige une dépendance GPU établie sur la même queue ; il
n'intègre pas de producteur concurrent venant d'une autre queue. Les copies
du présentateur ne passent pas par une lecture CPU. Les dimensions et le format
sont fixés à la création des swapchains ; le mode n'ajoute aucun anneau d'images
ni allocation GPU par frame dans le présentateur.

Les deux reçus doivent identifier les yeux 0 et 1, deux images distinctes,
la séquence et l'heure demandées, une même scène non nulle, et la famille de
queue, le format et les dimensions attendus. Une paire incohérente ou absente
termine une frame OpenXR vide **avant toute acquisition de swapchain**. Une paire
acceptée est copiée sur le GPU ; la fence doit finir avant les releases OpenXR.
Les poses et FOV des vues soumises restent ceux de la demande correspondante.

Ces reçus contrôlent la cohérence déclarée du producteur. Ils ne démontrent pas
que celui-ci a réellement rendu la scène avec ces matrices : cette obligation
reste à tester pour chaque producteur. Modifier l'étiquette d'une ancienne
image pour la faire accepter serait incorrect. Les captures actuelles de Diablo
sont successives, sans pose XR demandée ni scène simultanée prouvée ; elles ne
peuvent donc pas être soumises honnêtement à cette interface comme une paire
courante. Le transport seul ne remplace pas le rendu géométrique du jeu.

## Session pilotable par étapes

`ProjectionSession` conserve la session XR entre les appels. `advance()` démarre
au plus une frame : il localise les vues et appelle le producteur, puis rend la
main. Les appels suivants acquièrent et attendent les deux images, soumettent
une copie privée, puis interrogent sa fence avant release et `xrEndFrame`.
Une seule paire et un seul command buffer privé peuvent être en cours.

| Retour | État conservé et suite |
| --- | --- |
| `idle` | Aucune frame commencée ; l'appelant peut continuer son travail. |
| `imagesPending` | Commandes du producteur conservées ; les attentes de swapchain utilisent un timeout nul. |
| `submitted` / `gpuPending` | Fence privée en cours ; aucun nouvel appel du producteur ni reset du pool. |
| `presented` / `skipped` | Frame terminée ; une nouvelle demande peut être commencée. |
| `stopped` / `failed` | Session terminale ; aucun nouvel appel du producteur. |

`advance(false)` poursuit uniquement une frame déjà commencée. Il n'appelle
jamais `xrWaitFrame`, même après une présentation terminée. Le callback, les
poses, l'heure et les reçus restent ceux de cette frame pendant les attentes.
Un `XR_TIMEOUT_EXPIRED` conserve la même acquisition ; il ne vaut pas une
attente réussie et ne permet ni copie ni release. Voir les règles de
[Khronos pour cette attente](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrWaitSwapchainImage.html).
La fin GPU est interrogée avec
[`vkGetFenceStatus`](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetFenceStatus.html).

Les appels, y compris la construction, la destruction et ceux du runtime,
exigent une sérialisation par l'appelant et un accès exclusif à la queue
empruntée. Les descripteurs de liaison et de callbacks sont copiés ; leurs
ressources et captures doivent rester vivantes jusqu'à destruction de la
session. Aucun thread ni verrou concurrent n'est ajouté.

**Ce découpage ne garantit pas un appel entièrement non bloquant :** le début
d'une frame appelle `xrWaitFrame`, qui cadence selon le runtime, et le callback
producteur ainsi que les autres appels du runtime peuvent prendre du temps.
La destruction termine les tâches en cours et peut attendre. L'ancien
`presentProjectionImages(..., seconds)` demeure un wrapper synchrone de
laboratoire. Le brancher directement sur le thread de rendu du jeu ne suffit
pas : l'intégration doit encore séparer le cadencement XR du travail de ce
thread et respecter la synchronisation de la queue.

Un arrêt reçu pendant une attente termine les travaux déjà soumis avant de
libérer leurs images. Si seule une image a été acquise, elle est attendue puis
libérée, sans acquisition supplémentaire. Le nettoyage après interruption ou
erreur libère aussi les acquisitions valides et termine la frame vide avant
la destruction des objets. Une perte fatale du runtime ou du périphérique ne
permet pas de garantir la réussite des appels de nettoyage.

## Preuves hors casque

La [preuve synthétique agrégée](proofs/projection-images-v1.json) décrit onze
processus de contrôle sur la RTX, dont un dans Steam Runtime. Le runtime XR
simulé alloue de vraies images Vulkan et compare leurs pixels après copie :

- 196 608 octets correspondent exactement aux motifs indépendamment attendus,
  avec deux yeux distincts, des couleurs variant par frame et un contrôle d'axe Y ;
- les paires périmées, scènes différentes, yeux inversés, familles de queue et
  formats incorrects sont refusés sans acquire ni release ;
- après une paire valide, une paire périmée est refusée ; après une absence,
  la suivante utilise une nouvelle séquence et la nouvelle heure de pose ;
- les acquires et releases sont équilibrés et les images sont vérifiées après
  la fence, avec les poses, FOV et l'heure attendus ;
- les tâches GPU que le faux runtime soumet après `xrEndFrame` sont terminées
  avant destruction de sa session ;
- onze contrôles de l'ancien chemin d'interception à 1 024 par œil passent
  aussi, avec images originales préservées et sans répétition des soumissions.

La validation Khronos de synchronisation est chargée dans tous ces processus,
sans message d'erreur observé. Un test CPU commun vérifie les reçus cohérents
et treize contrôles de refus. Il est inclus dans les cibles Linux, Windows et
ASan/UBSan. Les 170 tests Python existants passent aussi.

Le faux runtime ne constitue ni SteamVR ni un casque. Les erreurs internes
SteamVR précédemment observées restent ouvertes. Le producteur réel du jeu,
la reprise cohérente des ressources pour une pose XR, le rendu continu et le
backend Windows D3D12 restent à implémenter. Aucun nouvel essai de Diablo n'a
été nécessaire pour cette étape.

## Contrôles de la session par étapes

La [preuve du cycle coopératif](proofs/projection-pump-v1.json) ajoute douze
processus de contrôle, dont un dans Steam Runtime. Des événements Vulkan
maintiennent réellement la fence non signalée sur trois retours successifs
par paire. Le programme appelant avance entre les étapes et effectue des
soumissions GPU indépendantes avant les copies privées. Les motifs des deux
yeux sont vérifiés après présentation, avec les poses et l'heure d'origine.

Les contrôles couvrent les timeouts répétés, un arrêt avec GPU en cours, un
arrêt après une seule acquisition, la destruction avec une image encore
indisponible ou une soumission en cours, une erreur d'acquisition et une
exception du producteur. Les frames masquées ou sans pose valide n'appellent
pas le producteur. Les objets de configuration temporaires peuvent disparaître
sans invalider les copies détenues par la session.

Les onze contrôles de transport précédents et les onze contrôles de reprise
interceptée à 1 024 par œil passent avec ces sources. Au total, ces 34 processus
ne signalent aucune erreur Khronos de synchronisation. Les deux ensembles de
contrôles de transport comparent exactement 393 216 octets de pixels.
Ces résultats restent synthétiques et hors casque ; ils n'ajoutent aucune
preuve de rendu de Diablo dans SteamVR.
