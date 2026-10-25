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
