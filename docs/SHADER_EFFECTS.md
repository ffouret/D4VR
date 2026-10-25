# Recensement statique des accès mémoire des shaders

`tools/shader_effects.py` rapproche les stages d'un relevé privé du catalogue
local, puis recense les instructions SPIR-V des modules candidats. Il prépare
l'audit des passes du jeu. **Il n'autorise jamais leur reprise.** Aucun shader,
journal ou identifiant de pipeline du jeu n'est publié avec les résultats.

## Informations relevées

Les types de résultats fournis par la grammaire Khronos permettent d'identifier
la classe de stockage des pointeurs des `OpLoad`, `OpStore`, `OpCopyMemory` et
`OpCopyMemorySized`. Les initialisations de variables comptent comme des
écritures. Cela couvre aussi un pointeur typé issu d'un paramètre, d'une copie,
d'un accès, d'un select ou d'un phi, sans retrouver la ressource liée.

Le rapport sépare les écritures Output/Function/Private, les autres classes
connues et les destinations inconnues. Il relève séparément les instructions
image, atomiques et barrières, les déclarations SubpassData, les lectures
d'attachement explicites et les pointeurs PhysicalStorageBuffer. Les locations
de sortie directes sont des **déclarations**, pas une preuve d'écriture effective.
Les sorties en blocs ne sont pas entièrement résolues.

Le recensement couvre toutes les fonctions, y compris celles qui sont
inaccessibles. Les écritures derrière une condition ou un SpecId par défaut
inactif restent visibles. Les opcodes inconnus et instructions mémoire
non modélisées sont signalés. Les instructions étendues sont inventoriées,
sans certifier leurs effets dans le mode historique. L'option
[`--glsl-grammar`](GLSL_EFFECTS.md) ajoute un modèle explicite de dix-sept
instructions, dont les écritures par pointeur de Modf/Frexp. Aucun binaire n'est
exécuté par cet outil.

## Limites pour la reprise

Une association par identifiant du pilote, ou par FNV et taille, reste un
candidat. Le fichier local est revérifié contre le catalogue et son SHA-256
figure dans le rapport privé ; cela ne prouve pas l'identité exacte du module
utilisé par le jeu. Plusieurs candidats restent distincts. Un stage sans
candidat ou une bibliothèque manquante reste explicite.

Le recensement n'est pas un validateur SPIR-V et ne prouve pas l'absence d'effets
externes. Il ne résout ni les descripteurs/alias d'images, ni le mélange et les
masques d'écriture du pipeline, ni l'ordre GPU et les lectures antérieures.
Les champs `external_memory_writes_excluded`, `bound_descriptor_images_resolved`
et `replay_allowed` restent faux, même si aucun store externe explicite n'est
trouvé. Ce rapport ne remplit donc pas les autorisations du moteur de reprise.

Le helper stéréo contient lui-même une télémétrie par adresse GPU. Le contrôle
la retrouve malgré ses adresses de spécialisation nulles par défaut. Un futur
audit de variantes devra examiner **le shader transformé et ses spécialisations**,
pas seulement le shader d'origine ; le présent outil ne réduit pas ces branches.
Les [variantes privées](REPLAY_SPECIALIZATION.md) forcent maintenant les
constantes 1/2/3 à 0/0/1. Les contrôles GPU valident l'absence d'écriture dans
les buffers de télémétrie, sans transformer ce recensement en preuve d'absence
de tout effet externe du shader complet.

## Utilisation privée

La grammaire est un fichier externe explicite, sans téléchargement implicite.
La validation utilise celle de SPIRV-Headers au commit
`fbf86836a9b1966586faf7be7bb8a6440e156d62`, SHA-256
`dd2a7c48d3131c9d0c78ad669ab4765048e6fd63e1fe277a0858f5458febdc68`.
Le rapport identifie la grammaire effectivement fournie.

```sh
python3 tools/shader_effects.py /travail/frame.json --scopes 12 14 \
  --shader-directory /travail/shaders \
  --identifier-catalog /travail/identifiants.json \
  --grammar /sdk/spirv.core.grammar.json --output /travail/effets-prives.json
```

La sélection est limitée à 64 scopes, 256 pipelines, 4 096 associations de
stage, 512 modules candidats et 256 Mio de code. Le fichier privé est écrit
en mode 0600. Les anciens relevés restent utilisables.

## Mesures réalisées

Dans les deux anciens relevés, les passes préparant la profondeur et le stencil
ont respectivement 20 et 8 associations de stage, couvrant 17 et 7 modules
candidats distincts. Toutes ont un candidat unique ; l'union compte 17 modules.
Chaque module passe séparément `spirv-val --target-env vulkan1.3`.

Le recensement n'y trouve aucun store explicite vers une classe externe,
destination de store inconnue, image écrite, instruction atomique, déclaration
d'input attachment ou opcode inconnu. Les fragments candidats déclarent une
sortie en location zéro. Dans ce premier recensement, douze puis cinq modules utilisaient des
instructions étendues non auditées. Le [modèle GLSL ultérieur](GLSL_EFFECTS.md)
classifie désormais leurs appels connus, sans transformer les autres champs
de refus en autorisation. Les états de mélange/masques et les ressources réellement
liées restent inconnus : les deux puis un LOAD non résolus restent inchangés.
Ces observations ne prouvent ni le rôle visible de ces passes ni leur reprise.

Cinq contrôles originaux sont compilés et validés sans GPU : lecture de texture,
input attachment, stores de buffer/image et atomique, store par adresse GPU,
et télémétrie du helper stéréo. Ils exercent les détections positives et restent
refusés pour toute autorisation automatique. Les dix nouveaux tests Python
couvrent aussi copies mémoire, pointeurs dérivés, ambiguïtés, catalogue altéré,
opcodes inconnus et limites de sélection. La suite compte 100 tests.

Les [preuves agrégées](proofs/shader-effects-v1.json) séparent contrôles
synthétiques et analyse statique des candidats du jeu. La CI Linux compile et
valide les contrôles ; les tests Python passent également sous Windows. Aucun
nouvel essai de Diablo ou rendu en VR n'est associé à cette étape.

Références : [grammaire épinglée](https://github.com/KhronosGroup/SPIRV-Headers/blob/fbf86836a9b1966586faf7be7bb8a6440e156d62/include/spirv/unified1/spirv.core.grammar.json),
[instructions SPIR-V](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html),
[états du framebuffer Vulkan](https://docs.vulkan.org/spec/latest/chapters/framebuffer.html).
