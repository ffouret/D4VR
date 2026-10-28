# Effets mémoire des instructions GLSL étendues

Le recensement accepte maintenant une grammaire GLSL.std.450 explicite et un
modèle limité à dix-sept instructions. Il précise un point de l'audit des
shaders ; **il n'autorise aucune reprise et ne prouve pas l'absence de tous les
effets externes d'un module**.

## Périmètre explicite

| Instructions | Effet mémoire modélisé |
| --- | --- |
| FAbs, Floor, Fract, Sin, Cos, Exp2, Log2, Sqrt, InverseSqrt | Résultat uniquement |
| Fma, NMin, NMax, NClamp | Résultat uniquement |
| ModfStruct, FrexpStruct | Résultat structuré uniquement |
| Modf, Frexp | Résultat et écriture par un opérande pointeur |

Ces treize opérations arithmétiques couvrent celles trouvées dans les candidats
préparatoires. Les formes structurées et par pointeur ont des contrôles
supplémentaires. « Résultat uniquement » concerne les effets mémoire ; le
modèle ne garantit pas une arithmétique identique entre pilotes.

Les noms, opcodes, nombres d'opérandes IdRef et absence de quantificateurs sont
vérifiés contre la grammaire fournie. Ajouter des lignes à cette grammaire
n'étend pas le modèle : les autres instructions restent inconnues. Un set
inconnu, un import absent, une variante non couverte d'OpExtInst ou une
destination de pointeur inconnue bloque le statut d'audit des instructions
étendues. Les arités incorrectes, IDs nuls et imports dupliqués sont refusés.

La grammaire est bornée à 128 Kio et 512 instructions. Celle des contrôles vient
de SPIRV-Headers au commit `fbf86836a9b1966586faf7be7bb8a6440e156d62`,
SHA-256 `5d72e48247569f9b77a17a849db624350abc9d26f7f72b153bbcf15c37238239`.
Le modèle et les rapports portent son empreinte. Aucun téléchargement implicite
n'est réalisé par les outils d'analyse ou de validation.

## Écritures et statuts distincts

Modf et Frexp peuvent écrire par pointeur **sans OpStore externe à l'appel**.
Le rapport suit la classe de stockage de cette destination, y compris les
pointeurs dérivés et paramètres typés. Les compteurs d'écritures étendues sont
séparés des stores/copies/initialisations du recensement historique. Un compteur
non atomique cumule ces deux familles ; les atomiques restent inventoriées à
part. Les destinations Function/Private/Output restent distinctes des autres
classes et des destinations inconnues.

`extended_instruction_effects_audited` signifie que les effets de ces appels
étendus sont modélisés et leurs destinations connues. **Il peut donc être vrai
pour une instruction qui écrit dans un SSBO.** Il ne signifie pas « sans
effets externes ». Les champs `external_memory_writes_excluded`,
`bound_descriptor_images_resolved`, `fixed_function_blend_and_masks_known` et
`replay_allowed` restent faux. Le module n'est pas validé par le recensement ;
`spirv-val` est une vérification séparée. Toutes les fonctions sont parcourues,
même lorsqu'une spécialisation rend leurs branches inaccessibles.

Sans `--glsl-grammar`, le comportement historique est conservé : les appels
étendus sont inventoriés sans ce modèle et leur statut d'audit reste faux.

## Contrôles et anciens candidats

Neuf contrôles originaux sont construits et passent spirv-val, sans exécution
GPU. Les cinq contrôles mémoire historiques restent couverts. Les quatre
nouveaux vérifient les quinze opérations à résultat seul, Modf/Frexp écrivant
directement dans un SSBO, leurs écritures Function/Private, et un DebugPrintf
valide mais non modélisé. Le cas SSBO contient zéro store externe explicite et
deux écritures étendues : il démontre la détection du chemin qui manquait au
premier recensement. Aucun contrôle n'autorise la reprise.

Les 117 tests Python couvrent aussi les destinations inconnues, noms de sets,
opcodes non modélisés même présents dans la grammaire, formes d'instruction
étendues non prises en charge, données mal formées et candidats ambigus. La CI
Linux construit et valide les neuf contrôles ; les tests Python passent aussi
sous Windows.

Les deux anciens relevés préparatoires ont respectivement 17/7 candidats et
20/8 associations de stage. Leurs 507/151 appels étendus appartiennent à
13/4 opérations modélisées, sans écriture étendue par pointeur. Il n'y a plus
d'effet étendu inconnu parmi ces candidats. L'union compte dix-sept modules,
tous revérifiés avec spirv-val ; les ensembles se recouvrent et leurs nombres
ne s'additionnent pas. L'association par catalogue/identifiant ou FNV ne prouve
toujours pas leur identité exacte dans le pipeline du jeu. Les deux/un LOAD
préparatoires non résolus restent inchangés.

Les [mesures agrégées et empreintes](proofs/glsl-effects-v1.json) séparent les
contrôles synthétiques des analyses privées. Aucun shader, identifiant de
module/pipeline ou journal du jeu n'est publié.

## Utilisation et suite

```sh
python3 tools/shader_effects.py /travail/frame.json --scopes 12 14 \
  --shader-directory /travail/shaders --identifier-catalog /travail/catalogue.json \
  --grammar /sdk/spirv.core.grammar.json \
  --glsl-grammar /sdk/extinst.glsl.std.450.grammar.json \
  --output /travail/effets-prives.json
```

Le code exact transformé, toutes les spécialisations, les autres instructions,
les descripteurs et alias, les états fixes et les dépendances d'attachements
restent à auditer. La [neutralisation de télémétrie](REPLAY_SPECIALIZATION.md)
n'enlève pas ses instructions du SPIR-V brut. Il reste ensuite à raccorder des
passes admissibles et leurs traitements suivants, avec durées de vie GPU,
poses prédites et soumission SteamVR. Aucun nouvel essai de Diablo/SteamVR ni
binaire de couche n'est associé à cette étape ; aucun dessin du jeu n'est
repris. Les originaux du jeu et de NVIDIA sont préservés, le relais Streamline
refusé reste désactivé et le backend D3D12 natif Windows demeure absent.

Références : [instruction set GLSL.std.450](https://registry.khronos.org/SPIR-V/specs/unified1/GLSL.std.450.html),
[grammaire épinglée](https://github.com/KhronosGroup/SPIRV-Headers/blob/fbf86836a9b1966586faf7be7bb8a6440e156d62/include/spirv/unified1/extinst.glsl.std.450.grammar.json),
[DebugPrintf GLSL](https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_debug_printf.txt).
