# Dépendances et références

## NVIDIA Streamline

`native/third_party/streamline/` contient uniquement les neuf en-têtes publics
nécessaires du SDK Streamline 2.12.0, sa licence et un manifeste de provenance.

Source : [NVIDIA-RTX/Streamline](https://github.com/NVIDIA-RTX/Streamline),
révision `e8aaa6eaac968711fb62473d4ae8256dde20919b`.

Copyright NVIDIA, sous licence MIT incluse dans
`native/third_party/streamline/license.txt`. Aucun composant Nsight Perf ni
binaire NVIDIA n'est redistribué. Les empreintes du manifeste correspondent
aux fichiers amont ; `.gitattributes` préserve leurs octets et fins de ligne.

## Godot

Le lanceur télécharge séparément le moteur officiel Godot 4.7.2, sous licence
MIT et licences de ses dépendances. Aucun binaire Godot n'est versionné ici.
Voir [les mentions légales Godot](https://godotengine.org/license/).

## Référence Overlord

Overlord est étudié comme référence technique. Aucun code de ce projet GPLv3
n'a été copié dans D4VR.

## Outils d'analyse GPU

DXC (Microsoft), glslang et SPIRV-Tools (Khronos) sont des outils externes de
préparation et ne sont pas redistribués. Les fixtures HLSL/GLSL, leur LLVM IR
et leur SPIR-V encodé en texte sont des créations originales D4VR. Aucun shader
extrait de Diablo IV n'est inclus. Le helper de diagnostic est une implémentation
originale du protocole du ring VKD3D ; aucun fichier source VKD3D n'est incorporé.
Voir [les versions et la procédure](docs/CAMERA_OBSERVER.md).

## Interfaces OpenXR et Vulkan

Le diagnostic natif emploie les en-têtes et loaders externes OpenXR (Khronos) et
Vulkan. Aucun de ces fichiers ou binaires n'est incorporé au dépôt. Les shaders
de contrôle et le code d'application natif sont des créations originales D4VR.
Voir [la procédure et les versions](docs/NATIVE_XR.md).
