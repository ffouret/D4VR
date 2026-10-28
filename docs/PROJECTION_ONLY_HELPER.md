# Construction du helper avec la seule projection

Les six vertex auparavant refusés n'exposent pas le second buffer uniforme
attendu par l'instrumentation. Le helper stéréo récupère pourtant la position
depuis la sortie clip et la projection courante : ses quatre arguments de
matrice d'objet ne sont pas utilisés. Le nouveau mode supprime cette lecture
ajoutée après vérification du helper exact. Il ne déduit aucune liaison ou
matrice absente du shader original.

## Contrat explicite

`--projection-only-helper` exige `--stereo --helper-no-telemetry`. Le helper est
lu une fois ; une copie dans un répertoire temporaire privé est validée avec
spirv-val. Les mêmes octets servent aux contrôles structurels et à la liaison.
Le fichier fourni n'est pas relu après validation.

Le contrôle sans télémétrie reste requis. Le contrôle supplémentaire retrouve
la fonction exportée et ses dix paramètres de l'ABI 1 : un pointeur Function
vers uint32, puis neuf pointeurs Function vers float4. Les paramètres 5 à 8
doivent être sans usage dans tout le corps, sans raccourci de portée ou de
spécialisation. Le scan de mots est conservateur : une collision de littéral
peut provoquer un refus supplémentaire. Une lecture ou transmission de ces
arguments reste bloquante, même si le helper passe spirv-val.

Le constructeur conserve les dix arguments et initialise les quatre locaux
inutilisés à un float4 nul défini. Ces valeurs ne sont pas présentées comme
une matrice du jeu. Le helper ne peut pas les lire. Les accès ajoutés ne lisent
plus que les quatre lignes de projection ; les accès et stores du shader
source restent présents. Un shader source utilisant encore son buffer de vue
en conserve donc le besoin. Le type et l'étendue de la projection restent
vérifiés et le module final passe spirv-val.

Le marqueur `D4VR_HELPER_PROJECTION_ONLY_1` décrit cette construction. L'ABI,
le SpecId 4 et le calcul du helper restent identiques. Ce marqueur ne prouve
pas l'absence d'effets du code original, ni l'identité ou l'éligibilité d'une
passe. Le mode historique continue d'exiger ses deux sources ; aucun repli
automatique n'est ajouté quand une source manque.

```sh
python tools/spirv_probe.py work/original.vert.spv \
  --stereo --helper-no-telemetry --projection-only-helper \
  --helper work/helper-no-telemetry.spv --tools /sdk/bin \
  --output work/projection-only.vert.spv
```

## Contrôles

Cinq constructions synthétiques couvrent deux shaders à un buffer, deux
shaders à deux buffers et un shader source écrivant un SSBO. Les premiers
n'exposent réellement aucun descripteur set 3/binding 1 et sont refusés par
le mode historique. Le nouveau mode ne crée aucun accès supplémentaire au
second descripteur dans les autres shaders et préserve le store SSBO original.
Un helper modifié pour lire object0 passe spirv-val, mais sa construction dans
ce mode est refusée. Les 141 tests Python incluent six nouveaux tests d'ABI,
d'usage des paramètres, de descripteurs et de préservation des accès sources.
La CI Linux reproduit les contrôles ; les tests Python passent aussi sous Windows.

La fixture GPU accepte une variante synthétique à un buffer : la matrice de
vue de la scène de contrôle y occupe les lignes 56 à 59 du buffer caméra,
la projection les lignes 72 à 75. Ce rangement est propre au test. Il ne
représente aucune hypothèse de rangement ajoutée aux shaders du jeu. Le module
ne déclare pas le second buffer ; les layouts de la fixture peuvent conserver
leurs bindings inutilisés.

Les essais GPU couvrent les graphes GPL privés et recettes monolithiques à un
buffer, les descriptor buffers et sets traditionnels, la construction à deux
buffers et le chemin historique. Cela fait 29 processus, dont neuf contrôles
négatifs attendus. Les vingt processus positifs comparent chacun seize vues à
des références indépendantes : couleurs/stencil exacts et différence de
profondeur mesurée nulle. La disparité est de 7,293568 pixels à la distance 2
contre 3,641940 à 4, avec 2 738 texels visibles dans un seul œil.

CLEAR → LOAD, les cibles originales et dessins suivants restent vérifiés.
Les cas avec descriptor buffers retrouvent aussi les 32 mots compute suivants.
Les 131 584 octets de compteurs, gardes et ring restent intacts par processus
positif ; le contrôle de diagnostic séparé écrit bien 48 mots aux mêmes adresses.
Validation Khronos, métadonnées et Steam Runtime sont inclus.

Les anciens candidats comprennent dix vertex. Tous peuvent maintenant être
transformés hors du jeu avec ce mode, y compris les six auparavant refusés.
Les dix modules transformés et les dix-sept modules originaux passent le
contrat validé d'absence d'écritures hors mémoire locale/sorties. L'identité
avec les octets réellement utilisés par les pipelines actifs reste non prouvée.
Les [mesures agrégées et empreintes](proofs/projection-only-helper-v1.json)
conservent le code, les identifiants et journaux du jeu hors du dépôt.

## Suite

Aucune couche de session gelée ni aucun original du jeu/NVIDIA n'est remplacé.
Aucun lancement de Diablo ou essai casque n'est effectué. Les anomalies GPL
antérieures ne sont pas réessayées ; le relais Streamline refusé reste désactivé.
Les deux/un LOAD préparatoires, ressources/alias, états fixes et durées de vie
restent à résoudre. Il faut établir l'identité du code actif puis raccorder les
passes admissibles et traitements suivants, poses prédites et soumission
SteamVR. Le backend D3D12 natif Windows reste absent. **Diablo n'est toujours
pas rendu en VR.**
