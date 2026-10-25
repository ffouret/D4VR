# Spécialisation sans télémétrie des variantes privées

Les variantes stéréo privées ne doivent pas hériter des écritures de diagnostic
du helper. `ReplaySpecialization` est maintenant utilisé par les recettes
monolithiques et les graphes GPL, pour le seul stage vertex transformé reconnu.
L'appel original et les autres stages conservent leurs spécialisations.

| SpecId du helper ABI 1 | Valeur dans la copie privée |
| --- | --- |
| 1, adresse des compteurs uint64 | 0 |
| 2, adresse du ring uint64 | 0 |
| 3, taille du ring uint32 | 1 |
| 4, adresse du paquet d'œil uint64 | Adresse propre à cet œil |

Le helper retourne avant ses atomiques et stores lorsque l'une des adresses
est nulle ou que le ring contient moins de 64 mots. Les trois conditions sont
imposées explicitement, même si la création source fournit des adresses actives.
Le module SPIR-V n'est pas réécrit ; ses instructions de télémétrie restent
présentes. Le [recensement statique](SHADER_EFFECTS.md), couvrant aussi le code
inaccessible, continue donc de les relever.

Les autres entrées, dont l'identifiant de shader 0 et les constantes du shader
original, sont conservées avec leurs octets. Les constantes remplacées sont
ajoutées dans une zone possédée, sans écraser les anciennes données : une autre
entrée peut partager leur offset. Les deux yeux ont des propriétaires séparés,
non copiables, conservés jusqu'au retour synchrone de création des pipelines.
Le contrôle historique d'un seul œil garde sa télémétrie ; il n'utilise pas ce
nouveau composant.

## Bornes et refus

Le composant accepte au plus 64 entrées sources et 4 Kio de données. La sortie
compte au plus 68 entrées et 4 128 octets. Les IDs remplacés doivent avoir la
largeur attendue ; doublons, tailles nulles, offsets hors limites, pointeurs
manquants et adresses d'œil nulles ou mal alignées sont refusés. Un échec de
variante ne modifie pas l'appel original. Le contrat suppose un véritable
vertex transformé avec l'ABI connue : un tag seul ne prouve pas l'ensemble de
ses effets ni l'identité exacte de son code.

Les tests C++ contrôlent les données temporaires détruites après copie, les
offsets partagés, les limites, les tailles des quatre IDs réservés, la
préservation des constantes et les refus. Les tests d'intégration vérifient
les deux chemins de création et la transmission de l'appel original. Le nouveau
test est exécuté sous Windows sans pilote et avec ASan/UBSan sous Linux.

## Preuve GPU avec adresses actives

`--replay-telemetry` donne à la création source deux vraies adresses GPU et un
ring de 4 096 mots. Les variantes privées doivent les neutraliser. À chaque
soumission principale, les deux compteurs commencent à zéro, leurs gardes et
le ring portent un motif distinct. Après reprise des dessins, les 64 octets de
compteurs/gardes et les 16 384 octets du ring restent exacts. Sur huit
soumissions, cela vérifie 131 584 octets, avec une barrière vers l'hôte et une
attente explicite de fence.

Un contrôle séparé rend ensuite un triangle avec le helper et la télémétrie
**activée aux mêmes adresses**. Il alloue bien 48 mots, compte une invocation
et écrit 48 mots du ring ; les gardes restent intactes. Ce contrôle empêche de
confondre l'absence d'écriture avec des adresses inutilisées ou une collecte
qui ne fonctionnerait pas. Sa cible est indépendante des images comparées.

Trois groupes couvrent GPL, recettes avec descriptor buffers et recettes avec
sets traditionnels. Deux autres groupes vérifient la régression du témoin GPL
original à douze états dynamiques et du contrat statique sans déclarations
parasites. Cela fait 29 processus, dont neuf contrôles négatifs intentionnels.
Les deux yeux, les références couleur/profondeur/stencil, CLEAR → LOAD, le
dessin suivant, les offsets de calcul, les métadonnées et Steam Runtime restent
vérifiés. [Mesures, sources et empreintes](proofs/replay-specialization-v1.json).

Pour reproduire, ajouter `--replay-telemetry` à
`tools/stereo_mrt_validate.py --recorded-replay --split-passes`, avec
`--pipeline-recipes` ou `--library-pipelines`. Les shaders peuvent utiliser les
positions générées ou un vrai vertex buffer avec `--dynamic-state`. Pour isoler
les écritures privées, le témoin original de ces trois contrôles est
monolithique et non instrumenté ; `--library-original-live` et `--static-state`
sont donc exclus dans ce mode. Leurs régressions sont des groupes distincts.

## Portée pour Diablo

**Désactiver cette télémétrie ne prouve pas que le shader complet est sans
effets externes.** Les écritures éventuelles du code original, les instructions
étendues, les descripteurs et leurs alias restent à auditer avec le code exact
et toutes ses spécialisations. Aucun champ d'autorisation du moteur n'est
renseigné automatiquement par cette politique. Les dépendances LOAD, barrières,
états fixes et durées de vie restent nécessaires.

La couche de diagnostic gelée précédemment reste inchangée. Aucun nouveau
binaire de couche n'est validé comme paquet de session ni déployé ici. Aucun
nouvel essai de Diablo/SteamVR n'est effectué. La reprise n'est toujours pas
raccordée aux dessins du jeu et aucune image du jeu n'est soumise au casque.
L'anomalie GPL statique précédente et son expérience privée restent distinctes ;
elles ne sont pas réessayées ni corrigées par cette spécialisation. Le backend
D3D12 natif Windows reste absent.

Références : [spécialisations Vulkan](https://docs.vulkan.org/refpages/latest/refpages/source/VkSpecializationInfo.html),
[helper original du projet](../native/probe_gpu/stereo_transform.vert).
