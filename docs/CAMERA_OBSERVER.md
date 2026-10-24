# Observation des transformations GPU

État du 24 octobre 2026, Diablo IV 3.2.3.73883 sur Bazzite. Ce diagnostic
récupère des constantes GPU ; le rendu stéréoscopique du jeu reste à réaliser.

## Analyse statique et première capture

`tools/shader_camera.py` désassemble les shaders capturés avec DXC et suit les
opérations SSA qui alimentent `SV_Position`. Il accepte uniquement quatre
produits scalaires partageant un même vecteur et quatre lignes consécutives
d'un buffer identifié. Il ne devine pas les index dynamiques ni les valeurs
écrites dans plusieurs branches.

Sur les 5 784 vertex shaders :

- 2 679 correspondent à `CBStatic`, b0/espace 0, octets 1152–1215 ;
- parmi eux, 2 113 prennent leur entrée dans une seconde matrice : `CBCommon`,
  b1/espace 0, octets 896–959 ;
- aucune erreur de désassemblage n'a été relevée.

Ces résultats décrivent la chaîne de calcul, sans identifier à eux seuls la
caméra de la scène. Les constantes par objet peuvent combiner plusieurs repères.

La première sonde réelle a produit 1 024 matrices complètes, issues de 46
shaders, dont 31 différentes à la précision du journal, sans groupe incomplet
ni ligne rejetée. 1 011 présentent la forme d'une projection perspective droite,
13 celle d'une transformation affine diagonale ou orthographique. Le groupe
principal, 908 échantillons, est compatible avec un champ vertical de 26° et
un rapport 16:9 ; ses petites variations de projection sont compatibles avec
un jitter temporel. Les autres usages comprennent potentiellement interfaces,
ombres, portraits et vues secondaires ; leur identité n'est pas établie.

La limite de 1 024 échantillons a été atteinte. Sans horodatage GPU, cette
capture ne prouve pas que les déplacements du joueur sont couverts. Il serait
incorrect d'appeler la matrice b0 une matrice monde-vue-projection complète.

## Instrumentation et validations

Le helper original `native/probe_gpu/probe_ring.vert` utilise le protocole du
ring de diagnostic VKD3D, à la révision `0bb64c56c8f0dcc`. Les constantes de
spécialisation 0–3 reçoivent le hash du shader, deux adresses GPU et la taille
du ring. Ce protocole interne peut changer : le lanceur vérifie le SHA-256 du
module VKD3D utilisé pour préparer le bundle.

`tools/spirv_probe.py` ajoute un appel d'observation avant les retours normaux
d'un vertex shader, puis lie ce programme avec le helper. Les sorties du shader
original sont conservées. Les buffers du jeu sont lus ; seules les zones de
diagnostic du ring sont écrites. Le processus emploie donc des shaders SPIR-V
instrumentés, même si les fichiers du jeu et les DLL NVIDIA restent d'origine.

Chaque programme passe `spirv-val`. La première série comptait 2 679 programmes
validés. La seconde se limite aux 2 113 programmes dont les deux multiplications
sont reconnues. Elle prélève les deux matrices dans la même invocation, avec un
intervalle de 65 536 invocations éligibles et une limite de 1 024 paires. Cela
étend la couverture par rapport au premier intervalle de 256 ; la durée réelle
dépend du nombre de draws et n'est pas garantie par un nombre de secondes.

Une fixture indépendante a été exécutée sur la RTX 5090 de la machine :

- les 32 valeurs envoyées dans deux buffers sont retrouvées avec leurs bits exacts ;
- les 16 384 octets de l'image 64 × 64 restent identiques avec et sans sonde ;
- le saut entre deux prélèvements et l'arrêt au budget sont vérifiés ;
- l'identité du shader et les numéros d'échantillon et de ligne sont vérifiés.

Ce résultat concerne la fixture originale. Il ne démontre ni l'identité de tous
les pixels du jeu ni l'absence de coût GPU dans le jeu.

Les tests Python vérifient les limites des buffers, les conflits de constantes
de spécialisation, la conservation des écritures d'origine, le parcours des
matrices et le regroupement des lignes de journal. Une paire incomplète ou des
lignes dupliquées ne sont jamais combinées en une matrice valide.

## Reproduire la préparation

Outils employés, conservés hors dépôt : DXC v1.9.2609 officiel Microsoft,
glslangValidator 15.1.0 et SPIRV-Tools 2025.1. Le téléchargement Linux de DXC a
été vérifié par son SHA-256 publié dans les métadonnées de release GitHub :
`96faadc7f5c282d2ffda49804beb4c3ee38127bc252b723234e3c5cdf7aa39a1`.

```sh
python3 tools/shader_camera.py --dxc /outils/dxc --shaders /capture/shaders --inventory /capture/inventory.json --output /capture/camera-chain.json
glslangValidator -V --target-env vulkan1.1 -Od native/probe_gpu/probe_ring.vert -o /travail/helper.spv
glslangValidator -V --target-env vulkan1.1 native/tests/probe_fixture.vert -o /travail/fixture.vert.spv
glslangValidator -V --target-env vulkan1.1 native/tests/probe_fixture.frag -o /travail/fixture.frag.spv
cmake -S native -B work/native-gpu -DD4VR_BUILD_GPU_PROBE_FIXTURE=ON
cmake --build work/native-gpu --target d4vr_gpu_probe_fixture
python3 tools/camera_bundle.py --shaders /capture/shaders --inventory /capture/inventory.json --candidates /capture/camera-chain.json --helper /travail/helper.spv --tools /outils/spirv --proton-path /chemin/GE-Proton --gpu-fixture /chemin/d4vr_gpu_probe_fixture --fixture-vertex /travail/fixture.vert.spv --fixture-fragment /travail/fixture.frag.spv --output /travail/bundle-neuf
```

La préparation exécute la fixture GPU et vérifie les empreintes DXIL/SPIR-V
avant de valider les substitutions. Le fichier texte `probe_fixture.spv.hex`
est la compilation glslang du shader synthétique du dépôt, pour les tests
Python hors GPU. Il ne provient pas de Diablo IV. De même, `camera_fixture.ll`
est le désassemblage DXC du HLSL synthétique adjacent.

## Lancement et rapport

```sh
python3 tools/camera_capture.py run --game-dir "/chemin/Diablo IV" --launcher /travail/battlenet.sh --bundle /travail/bundle-neuf --output /travail/session-neuve
python3 tools/camera_capture.py report /travail/session-neuve/camera.log --output /travail/session-neuve/matrices.json
```

Le lancement exige le jeu et Battle.net fermés, un jeu conforme au profil et les
DLL originales. La copie privée du lanceur fixe le GE-Proton vérifié et définit
`VKD3D_SHADER_OVERRIDE`, un ring de 16 Mio, un cache neuf et le journal. Aucun
réglage permanent de Lutris n'est écrit. Après fermeture du jeu et de Battle.net,
le lancement habituel n'utilise plus ces variables.

Rester immobile dix secondes en partie, marcher environ deux minutes, puis
quitter le jeu et Battle.net. Les observations CPU de taille du journal indiquent
quand ses octets deviennent visibles ; elles ne sont pas des horodatages GPU.
Le ring journalise les flottants à six décimales : le rapport compte les matrices
distinctes à cette précision seulement.

Les shaders extraits, bundles contenant du bytecode du jeu, caches et journaux
privés restent locaux. Ils sont exclus des archives sources et du dépôt.
L'observateur est propre à Linux/VKD3D ; les analyseurs et tests hors GPU sont
portables. Un adaptateur D3D12 Windows natif reste à implémenter, ainsi que les
deux rendus géométriques synchronisés, les historiques et la soumission SteamVR.

## Sources primaires

- [DXC v1.9.2609](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609)
- [Ring VKD3D, révision utilisée](https://github.com/HansKristian-Work/vkd3d-proton/blob/0bb64c56c8f0dcc/libs/vkd3d/debug_ring.c)
- [Substitution et spécialisation des shaders](https://github.com/HansKristian-Work/vkd3d-proton/blob/0bb64c56c8f0dcc/libs/vkd3d/state.c)
- [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools)
