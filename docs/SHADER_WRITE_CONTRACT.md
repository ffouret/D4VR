# Contrat d'écritures du module complet

`shader_write_contract.py` vérifie un prérequis statique : l'absence d'écritures
hors mémoire propre à l'invocation et sorties normales du shader. Le contrat
porte sur **toutes les fonctions du module**, y compris les fonctions jamais
appelées et branches désactivées par défaut. Il n'autorise aucune reprise.

## Conclusion bornée et validation

L'analyse exige une liste explicite d'opcodes examinés. Elle couvre les
déclarations, calculs de valeurs, lectures d'images retenues, contrôle de flot,
stores, copies et initialisations. Les instructions étendues utilisent le
modèle GLSL explicite, notamment les écritures par pointeur de Modf/Frexp.
Leur destination est classée comme celle des stores/copies : Function, Private
et Output sont séparés des buffers, adresses physiques et destinations inconnues.

Capacités, extensions, modes d'exécution et décorations sont également limités.
Les conversions d'adresse ne peuvent produire qu'un pointeur physique ; les
casts produisant d'autres pointeurs et les dérivations non résolues sont
bloquants. Les opcodes inconnus, formes étendues non couvertes, atomiques,
barrières, écritures d'image et appels non résolus ne sont pas admis. Le
contrat est volontairement conservateur, même pour un atomique de lecture.
Il accepte un seul point d'entrée vertex ou fragment, sous GLSL450 avec
adressage Logical ou PhysicalStorageBuffer64.

La grammaire core et celle de GLSL sont épinglées par SHA-256 au commit Khronos
`fbf86836a9b1966586faf7be7bb8a6440e156d62`. Les noms et la présence de résultats
typés sont comparés à la table explicite. Ajouter des lignes à une grammaire
ne rend pas les nouvelles instructions admissibles. Les bornes restent
16 Mio et 250 000 instructions par shader.

| Champ | Signification |
| --- | --- |
| `static_non_output_external_writes_excluded` | Toutes les instructions satisfont le modèle statique ; validation encore requise |
| `module_validation_passed` | spirv-val accepte exactement les octets analysés dans l'environnement indiqué |
| `validated_non_output_external_writes_excluded` | Les deux conditions précédentes sont vraies |
| `replay_allowed` | Toujours faux |

La validation travaille sur un instantané privé des octets déjà lus, sans
relire le chemin fourni après le contrôle. Le rapport identifie ces octets,
les grammaires, l'environnement, les sources du modèle et le validateur par empreinte.
Les valeurs de spécialisation ne sont pas utilisées pour éliminer des branches.
Un helper de diagnostic reste donc refusé malgré ses adresses nulles par défaut.
Le marqueur du helper sans télémétrie ne confère aucune permission particulière.

Cette conclusion suppose un module valide et une exécution définie. Elle ne
prouve ni les bornes des ressources à l'exécution, ni les effets des sorties
couleur/profondeur/stencil, ni leurs alias. Les descripteurs, états fixes,
dépendances LOAD, barrières et durées de vie restent des contrôles distincts.
Le recensement historique conserve ses champs d'autorisation à faux. Aucun
champ d'éligibilité du moteur C++ n'est renseigné par ce nouvel outil.

## Contrôles et candidats privés

Vingt-huit cas synthétiques exercent les deux helpers, modules originaux et
transformés, textures et input attachments, stores par adresse physique,
écritures buffer/image/atomiques, GLSL pur et Modf/Frexp, copies et diagnostic
non modélisé. Treize satisfont la conclusion validée ; les quinze autres sont
refusés comme prévu. Les copies externes, atomiques et barrières d'une fonction
jamais appelée sont détectées. Une copie vers une variable locale reste admise.

Un cas contient seulement des opcodes couverts mais un point d'entrée invalide :
le verdict statique seul est positif, spirv-val échoue et la conclusion validée
reste fausse. Un OpNop explicitement examiné est admis ici, alors que le contrat
plus petit du helper l'écarte. Ces cas distinguent couverture, validation et
permission de reprise. Les 135 tests Python comprennent dix nouveaux tests.
La CI Linux reproduit les vingt-huit cas ; les tests Python passent aussi sous
Windows. Aucun shader de ces contrôles n'est exécuté sur le GPU à cette étape.

Les anciens relevés contiennent 17/7 candidats et 20/8 associations de stage.
Leur union compte dix-sept modules : dix vertex et sept fragments. **Tous les
dix-sept passent spirv-val et le nouveau contrat.** Quatre vertex peuvent être
transformés par l'injecteur existant avec le helper sans télémétrie ; leurs
modules transformés passent également les deux contrôles. Six autres vertex
sont refusés, faute d'un des buffers uniformes attendus. Aucune liaison ni
matrice n'est devinée pour contourner ce refus.
La [construction ultérieure avec la seule projection](PROJECTION_ONLY_HELPER.md)
prouve que les arguments secondaires sont inutilisés dans le helper, puis
supprime cette dépendance ajoutée. Elle couvre les dix vertex, sans modifier
les résultats de la preuve historique liée ici.

Les ensembles se recouvrent : les nombres des relevés ne s'additionnent pas.
Il s'agit des octets des anciens candidats du catalogue, pas d'une preuve
d'identité avec les modules des pipelines actifs. Les deux/un LOAD préparatoires
restent non résolus. Les [mesures agrégées](proofs/shader-write-contract-v1.json)
ne publient aucun code, identifiant de module/pipeline ou journal privé du jeu.

## Utilisation et suite

```sh
python tools/shader_write_contract.py work/transformed.vert.spv \
  --validator /sdk/bin/spirv-val --target-env vulkan1.3 \
  --grammar /sdk/spirv.core.grammar.json \
  --glsl-grammar /sdk/extinst.glsl.std.450.grammar.json \
  --output work/write-contract.json
```

Le rapport est privé, mode 0600. Aucun téléchargement n'est implicite. Les
contrôles reproductibles sont dans `shader_write_validate.py` avec les mêmes
entrées explicites et un compilateur GLSL.

Les originaux du jeu/NVIDIA et le paquet de diagnostic gelé restent inchangés.
Aucun nouveau binaire de couche, lancement de Diablo, essai GPU ou casque n'est
effectué ici. Le relais Streamline refusé reste désactivé. Les anomalies GPL
précédentes restent distinctes. Restent l'identité exacte du code actif,
l'adaptation des vertex refusés sur des données prouvées, les ressources/alias,
états fixes et dépendances, puis la reprise admissible des passes et traitements
suivants, poses prédites et soumission SteamVR. L'adaptateur D3D12 natif Windows
reste absent. **Diablo n'est toujours pas rendu en VR.**

Référence : [instructions SPIR-V Khronos](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html).
