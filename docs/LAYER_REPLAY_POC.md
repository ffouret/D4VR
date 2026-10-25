# Première reprise des dessins à travers la couche Vulkan

La nouvelle couche expérimentale relie enfin les interceptions Vulkan au moteur
`Graphics`. L'application de contrôle ne lie ni n'appelle ce moteur : elle
enregistre ses commandes Vulkan habituelles. Après chaque passe admissible,
D4VR reprend ses dessins dans ses deux cibles privées, puis restaure les états
du flux original. Les copies de sortie sont ajoutées avant `EndCommandBuffer`.
La lecture CPU attend un `DeviceWaitIdle` réussi après soumission réussie.

**Ce résultat est synthétique. Aucun dessin de Diablo IV, aucune pose de casque
et aucune soumission SteamVR ne sont exercés par cette preuve.** Les deux
triangles à des distances différentes servent à contrôler géométrie et
occlusion, pas à représenter un décor du jeu.

## Admission fermée du contrôle

`D4VR_BUILD_LAYER_REPLAY_POC=ON` produit une bibliothèque et un manifest séparés
de la couche de diagnostic utilisée précédemment. L'activation requiert aussi
`D4VR_LAYER_REPLAY_POC=1` et le nom d'application du client de contrôle. Aucun
paquet de jeu n'est remplacé ou installé.

Le générateur compile nos sources vertex/fragment et inclut leurs octets dans
les deux programmes. La couche compare les **octets complets fournis à la
création des modules actifs**, sans empreinte approximative, catalogue de
candidats, marqueur seul ou fichier d'admission modifiable à l'exécution.
Un changement de nom de débogage SPIR-V, sans changement visuel, suffit à
refuser l'admission. Les créations originales et leurs résultats sont conservés.

Ces sources ne lisent que l'indice de sommet et 32 octets de push constants.
La variante stéréo lit en plus son propre paquet d'œil immuable. Il n'existe
aucun descripteur, buffer de sommets, texture, adresse source ou effet externe à
auditer dans ce profil fermé. C'est cette propriété des sources exactes qui
permet de renseigner les deux prérequis de `Graphics` pour ce contrôle. Ce
profil ne fournit **aucune permission pour les shaders du jeu**.

La couche capture le layout sans descripteurs et sa plage de push constants,
les octets des modules, le pipeline et ses déclarations viewport/scissor, puis
les commandes bind/push/draw/rendering. Les sources de pipelines sont possédées
avant destruction des modules originaux. Les copies privées utilisent la
recette existante et ses constantes de télémétrie neutralisées. Les formats,
images, sous-ressources et étendues des attachements originaux sont vérifiés.

Le profil exige un client de contrôle à un seul thread, une seule queue et des
enregistrements primaires one-time. Les commandes non modélisées empoisonnent
la reprise de l'enregistrement. Les barrières et copies d'image vers buffer
peuvent traverser le flux hors d'une passe ; elles restent bloquantes à
l'intérieur. Des wrappers sont générés pour toute la surface `vkCmd*` du
header compilé : 215 wrappers de refus dans le build testé, en complément des
commandes déjà interceptées et des quatre nouveaux états modélisés. Les autres
appels continuent vers le maillon suivant. Les fonctions privées utilisent
directement ce maillon, sans récursion dans les interceptions.

Chaque enregistrement possède ses cibles, variantes, paquets et staging,
jusqu'au reset/free légal de son command buffer ou à la destruction du device.
Une seconde passe de l'application utilise les états restaurés sans les
réinitialiser. Le contrôle détruit les modules originaux avant l'enregistrement.
Un layout détruit est retiré des recettes pouvant être clonées ultérieurement.
Les allocations sont bornées à huit enregistrements et huit recettes ; une
exception de diagnostic bloque la suite de l'admission sans traverser l'ABI C.

## Résultat sur Bazzite et RTX 5090

Les [preuves et empreintes](proofs/layer-replay-poc-v1.json) couvrent six
processus : positif natif, commande inconnue, module visuellement identique mais
aux octets différents, activation absente, autre client et positif dans Steam
Runtime. Le chargement des couches aux niveaux instance et device est confirmé.
Khronos, avec validation de synchronisation activée, ne rapporte aucune erreur
dans ces six contrôles.

Chaque positif reprend deux passes : quatre dessins originaux, huit copies
privées. Les quatre MRT, les trois octets définis de chaque profondeur D24 et le
stencil des deux yeux correspondent aux références GPU enregistrées séparément :
**2 621 440 octets définis comparés, zéro différence**. Le quatrième octet des
copies D24 est indéfini et est exclu des comparaisons. Les deux cibles originales,
dont la passe témoin suivante, retrouvent également leurs références avec zéro
différence. Les vues gauche et droite diffèrent sur 3 590 pixels du premier MRT.

Les quatre contrôles de refus ne produisent aucune copie privée et conservent
les pixels originaux. La commande graphique non modélisée est légale dans le
flux original : une liaison ultérieure du pipeline statique invalide sa valeur
dynamique inutilisée. La couche refuse quand même cet historique inconnu.

Les trois modules SPIR-V générés passent aussi `spirv-val`. La compilation
hôte stricte et le test CPU du relais original passent. La CI compile cette
nouvelle couche et son client sous Linux ; les composants communs existants
continuent d'être vérifiés sous Linux et Windows. Les contrôles GPU ci-dessus
sont locaux et ne doivent pas être attribués à la CI.

## Reproduire

Avec les headers Vulkan et glslang disponibles dans l'environnement de build :

```sh
cmake -S native -B work/layer-poc -DD4VR_BUILD_LAYER_REPLAY_POC=ON
cmake --build work/layer-poc --parallel 2
python tools/layer_poc_validate.py \
  --layer work/layer-poc \
  --client work/layer-poc/d4vr_layer_replay_client \
  --validation-layer /chemin/vers/la-couche-khronos \
  --runtime /chemin/vers/SteamLinuxRuntime_4/run \
  --output work/nouvelle-preuve-privee
```

Le vérificateur exige un nouveau répertoire, isole les variables du processus
et borne chaque exécution à 60 secondes. Les images PPM, données GPU et journaux
restent dans ce répertoire privé ; seuls agrégats et empreintes sont publiés.

## Prochaine liaison

Le [raccordement OpenXR de la scène figée](FROZEN_REPLAY_XR.md) a depuis affiché
ces images dans SteamVR, avec des erreurs de validation encore ouvertes. Le
profil fermé sert à cette liaison hors du jeu. Pour Diablo,
restent indispensables l'identité exacte du code actif, les ressources et alias,
les états complets et les producteurs LOAD. L'ancienne couche de diagnostic
de paires reste gelée et aucune boucle de lancement Battle.net n'est ajoutée.
Le backend D3D12 natif Windows reste absent.
