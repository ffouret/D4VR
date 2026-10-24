# Relevé DirectX 12 du 24 octobre 2026

Le relais Streamline étant refusé par la signature du jeu, un deuxième chemin
d'observation utilise les diagnostics déjà présents dans VKD3D-Proton. Aucun
fichier exécutable de Diablo IV ni de NVIDIA n'est remplacé pour ce relevé.

## Procédure

Le script de lancement autonome a été généré par la version installée de Lutris,
avec `lutris --output-script 1`. L'identifiant 1 correspond ici à Battle.net ;
il faut choisir celui de son installation et générer le script dans un dossier
de travail, car Lutris écrit `<slug>.sh` dans son répertoire courant.

Le lanceur exporté utilise `/usr/bin/umu-run` et `PROTONPATH=GE-Proton`, avec
le préfixe Battle.net existant. Les tests Wine du relais utilisaient séparément
DW-Proton ; ce runner de test ne doit pas être confondu avec le lanceur du jeu.

Fermer le jeu et Battle.net avant de démarrer cette session. Un Battle.net déjà
ouvert ne recevrait pas les nouvelles variables. L'outil refuse cet état plutôt
que d'annoncer une capture qui ne serait pas activée.

```sh
python3 tools/shader_capture.py run --game-dir "/chemin/Diablo IV" --launcher "/chemin/battlenet.sh" --output "/chemin/capture-neuve"
```

L'outil vérifie les empreintes du jeu et de la DLL NVIDIA originale, puis prépare
un dossier neuf hors du jeu. Pour ce lancement seulement, il définit :

- `VKD3D_SHADER_DUMP_PATH` : dossier des bytecodes relevés ;
- `VKD3D_SHADER_CACHE_PATH` : cache de travail neuf pour ce relevé ;
- `VKD3D_DEBUG=info` et `VKD3D_LOG_FILE` : version et diagnostic du renderer.

Les chemins utilisent le lecteur Wine `Z:`. Le démarrage peut être plus long
avec ce cache temporaire. Le journal du lanceur reste privé dans le dossier de
travail et ne doit pas être publié : les outils de lancement peuvent y écrire
des données de session. Le dépôt et les livrables ne contiennent ni ce journal
ni les shaders extraits du jeu.

Entrer en partie et se déplacer une trentaine de secondes, puis quitter le jeu
et Battle.net. Les variables disparaissent avec ces processus ; le lancement
Lutris habituel et son cache n'ont pas été reconfigurés.

```sh
python3 tools/shader_capture.py status "/chemin/capture-neuve"
python3 tools/shader_inventory.py "/chemin/capture-neuve/shaders" --output "/chemin/inventory.json"
```

Le lanceur de capture est spécifique à Linux/VKD3D. L'analyseur de fichiers est
portable et testé sur Linux et Windows. Il n'implémente pas de collecte D3D12
native Windows ; cette partie du futur adaptateur reste à écrire.

## Résultats mesurés

Le processus Battle.net a reçu les variables de relevé et de cache temporaire.
Le renderer a annoncé VKD3D-Proton `3.1.0`, build `0bb64c56c8f0dcc+`.
À la fin du lancement :

| Élément | Nombre |
| --- | ---: |
| Fichiers DXIL | 10 249 |
| Fichiers SPIR-V | 10 377 |
| Fichiers de signature racine | 5 |
| Total brut | 20 631 fichiers, 365 534 722 octets |
| Fichiers DXIL/SPIR-V analysés | 20 626 |
| Erreurs de structure détectées | 0 |
| Vertex shaders DXIL | 5 784 |
| Pixel shaders DXIL | 4 222 |
| Compute shaders DXIL | 108 |
| Bibliothèques DXIL | 135 |
| Vertex shaders avec entrée `POSITION` non système | 5 754 |

Les formats DXIL et SPIR-V représentent souvent un même shader avant et après
traduction. Ces nombres ne s'additionnent donc pas en autant de programmes de
jeu distincts. Un programme peut être compilé ou chargé à l'avance sans dessiner
le décor pendant la séquence testée.

## Portée de l'analyse

L'analyseur vérifie les limites des conteneurs et instructions. Il relève les
étapes, modèles de shaders, signatures et ressources déclarées. Il ne lit pas
les valeurs présentes en mémoire GPU et n'exécute pas les bytecodes.

Une entrée `POSITION` aide à sélectionner des candidats, mais des interfaces,
ombres et objets hors caméra peuvent aussi en déclarer une. Les ressources
SPIR-V déclarées ne suffisent pas à nommer un buffer « caméra ». Il faut encore
corréler un draw de la scène, son programme et ses constantes, puis tester un
rendu géométrique distinct pour chaque œil.

La suite de cette étude est décrite dans [l’observation des transformations GPU](CAMERA_OBSERVER.md).

## Sources des formats et diagnostics

- [Diagnostics de shaders VKD3D-Proton](https://github.com/HansKristian-Work/vkd3d-proton#advanced-shader-debugging)
- [Conteneur et signatures DXIL](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/DxilContainer/DxilContainer.h)
- [Instructions et valeurs SPIR-V](https://github.com/KhronosGroup/SPIRV-Headers/blob/main/include/spirv/unified1/spirv.hpp)
