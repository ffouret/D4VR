# Helper géométrique sans télémétrie

`stereo_transform.vert` accepte `D4VR_DIAGNOSTIC_TELEMETRY=0`. La compilation
retire le ring, les compteurs, leurs constantes 0 à 3, la fonction de relevé et
son appel. Le paquet d'œil, l'ABI stéréo et le calcul géométrique sont conservés.
La valeur par défaut reste 1 : les diagnostics historiques gardent leurs
prélèvements. Aucune couche de session gelée n'est remplacée.

| Module de helper compilé | Diagnostic par défaut | Sans télémétrie |
| --- | --- | --- |
| SpecIds | 0, 1, 2, 3, 4 | 4 |
| Stores vers mémoire externe | 9 | 0 |
| Atomiques | 3 | 0 |
| Barrières mémoire | 2 | 0 |
| Lecture du paquet d'œil | Oui | Oui |

Ces nombres portent sur les instructions de toutes les fonctions du module,
avant spécialisation. Les stores locaux et vers la sortie restent présents.
Les coordonnées sont transformées avant rastérisation, sans lecture d'image
couleur/profondeur pour reconstruire une vue.

## Construction et contrat borné

```sh
glslangValidator -V --target-env vulkan1.1 -Od \
  -DD4VR_DIAGNOSTIC_TELEMETRY=0 native/probe_gpu/stereo_transform.vert \
  -o work/helper-no-telemetry.spv
python tools/spirv_probe.py work/original.vert.spv --stereo \
  --helper-no-telemetry --helper work/helper-no-telemetry.spv \
  --tools /usr/bin --output work/transformed.vert.spv
```

Le nouveau drapeau exige le mode stéréo, valide le helper avec `spirv-val`, puis
applique un contrat structurel explicite. Il est borné à 128 Kio et 8 192
instructions. Seuls les opcodes examinés, FAbs de GLSL.std.450 et le SpecId 4
uint64 initialement nul sont admis. Les stores doivent viser Function/Output ;
les variables, conversions de pointeurs, appels, capacités et extensions sont
également limités. Toute nouvelle forme du compilateur exige une révision du
contrat. Le module final lié passe aussi `spirv-val`.

Le marqueur `D4VR_HELPER_NO_TELEMETRY_1` décrit **le helper fourni avant
liaison**. Il ne garantit ni l'identité ni l'absence d'effets du shader original.
Un contrôle conserve une écriture SSBO du shader source : le module transformé
porte bien le marqueur, mais son store externe reste compté. Aucun champ
`external_memory_writes_excluded` ou `replay_allowed` n'est activé. Les graphes
privés continuent d'imposer leur politique de spécialisation ; les entrées
privées inutilisées ne réintroduisent pas de code de diagnostic.

## Vérifications

Dix modules de contrôle passent la compilation/liaison et `spirv-val` : les
deux helpers et quatre shaders sources transformés chacun avec les deux
helpers. Le helper de diagnostic est refusé lorsqu'on exige la variante sans
télémétrie. Un helper valide contenant un OpNop supplémentaire est aussi
refusé : un opcode inconnu n'est pas accepté par défaut. Les 125 tests Python
incluent huit nouveaux tests du contrat, de ses bornes et des refus.
La CI Linux reproduit cette construction ; les tests Python et composants
communs sont aussi vérifiés sous Windows.

Les essais GPU utilisent une scène originale, avec quatre cas et deux epochs
par cas. Trois groupes testent la nouvelle variante : graphes GPL privés,
recettes monolithiques avec descriptor buffers et recettes avec sets classiques.
Un groupe supplémentaire reprend le helper de diagnostic pour vérifier sa
régression. La validation Khronos, le contrôle de synchronisation, le contrôle
d'offset compute, les métadonnées et Steam Runtime sont inclus.

Cela fait 23 processus, dont sept contrôles négatifs intentionnels. Les seize
processus positifs comparent chacun 16 vues : zéro octet couleur/stencil
différent et zéro différence de profondeur mesurée. Les références sont
indépendantes et utilisent le shader source. La disparité est de 7,293568 pixels
à la distance 2 contre 3,641940 à 4 ; 2 738 texels sont visibles dans un seul
œil. Les cibles originales, CLEAR → LOAD et le dessin suivant restent vérifiés.
Les cas avec descriptor buffers vérifient aussi les 32 mots compute suivants.

Pour chacun de ces processus positifs, 131 584 octets de compteurs, gardes et
ring restent intacts pendant les huit soumissions principales. Le nouveau
paramètre `--telemetry-control-shader` permet ensuite d'utiliser un module de
diagnostic séparé aux mêmes adresses : il écrit bien 48 mots du ring. Ce module
n'est jamais utilisé pour les variantes stéréo comparées. Un 24e processus,
qui donne volontairement le helper sans télémétrie à ce contrôle positif,
échoue avec le message attendu. Il est compté séparément comme huitième
contrôle négatif. [Mesures et empreintes](proofs/stereo-helper-v1.json).

La comparaison entre variantes repose sur ces références indépendantes ; les
tests ne prouvent pas une équivalence universelle des shaders, des pilotes ou
de tous les pixels de Diablo.

## Travail restant

Aucun nouveau processus de Diablo, rendu VR du jeu ou essai casque n'est lancé.
Les shaders et journaux du jeu restent privés. Les originaux du jeu/NVIDIA et
le paquet de diagnostic gelé restent inchangés. Le relais Streamline refusé
n'est pas repris. Les anomalies GPL précédentes ne sont pas réessayées ici.

L'audit complet du code exact du jeu, ses spécialisations, descripteurs/alias,
états fixes, dépendances LOAD, barrières et durées de vie GPU reste nécessaire.
Il faut ensuite raccorder les passes admissibles et leurs traitements suivants,
les poses prédites et la soumission SteamVR. Le backend D3D12 natif Windows
reste à implémenter. **Diablo n'est toujours pas rendu en VR.**
