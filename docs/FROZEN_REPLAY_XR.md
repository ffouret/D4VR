# Reprise figée vers SteamVR

Les images produites par l'interception Vulkan de la scène de contrôle sont
maintenant recalculées avec les poses OpenXR et soumises à SteamVR, sur le même
device Vulkan et la même queue. Le test réel du 25 octobre a soumis **891 paires
d'images suivies en dix secondes**. Le testeur confirme le relief et la réaction
aux mouvements de tête, mais signale une forte pixellisation à 256 × 256 par œil.

**Ce premier affichage reste synthétique et sa validation Vulkan échoue. Aucun
dessin de Diablo IV n'est rendu en VR.** Les observations, erreurs et contrôles
hors casque sont conservés dans les [preuves agrégées](proofs/frozen-replay-xr-v1.json).

## Chemin exécuté

`Graphics::replaySnapshot` conserve la dernière passe réussie dont tous les
attachements utilisent CLEAR. Après achèvement GPU, elle peut être reprise dans
un autre command buffer privé, sans répéter le dessin original ni restaurer les
états de son command buffer. Une passe LOAD ne fournit pas ce snapshot fermé.
Le propriétaire conserve les ressources d'entrée et les leases de pipelines.

Les deux paquets d'œil peuvent être actualisés après achèvement de toutes leurs
lectures GPU. Les deux mappings sont acquis avant la moindre écriture : l'échec
du second laisse les deux anciens paquets intacts. Leurs adresses BDA restent
stables. Les données sont immuables pendant chaque soumission.

L'adaptateur Linux `native/xr/replay_presenter.cpp` emprunte instance, device et
queue. Il vérifie l'UUID du GPU et les noms d'extensions requis, possède ses
objets XR et son pool/fence privés, puis exécute attente de frame, localisation
des yeux, mise à jour des paquets, reprise, copie et soumission de projection.
Le command buffer privé reçoit le callback de dispatch du chargeur Vulkan.
Le client reste bloqué dans son premier `DeviceWaitIdle` pendant cet essai borné :
il s'agit d'une scène figée, pas encore d'un adaptateur de jeu en temps réel.

SteamVR redemande la queue par l'API publique pendant la création de session.
Cette réentrée remplaçait la fonction de présentation pendant son exécution et
libérait sa capture. L'ancienne bibliothèque reproduit un SIGSEGV avec le runtime
factice, sans casque ; la correction conserve un propriétaire local et interdit
le remplacement après début de présentation.

## Contrôles hors casque

Onze processus GPU couvrent les refus existants, l'absence de chargeur XR, un
échec de session après réentrée, une préparation sans frame et deux frames avec
deux poses simulées, en environnement natif et Steam Runtime. Le runtime factice
alloue de vraies images Vulkan ; il n'utilise aucun casque ni compositeur.
Les couches D4VR et Khronos sont chargées aux niveaux instance et device, avec
validation de synchronisation : zéro erreur dans ces onze contrôles. Les pixels
originaux et les premières vues privées restent identiques aux références GPU.

Un contrôle GPU indépendant reprend huit snapshots dans d'autres command buffers
après actualisation des poses. Couleur, profondeur, stencil et témoin original
retrouvent leurs références. Le contrôle négatif sans dépendance signale bien
une erreur de synchronisation. Les tests CPU couvrent aussi les refus de reprise
et l'atomicité des mises à jour de paquets.

## Limites constatées dans SteamVR

Le client termine normalement et les références originales restent exactes,
mais le vérificateur strict refuse le test réel : 150 messages sur le tiling DRM,
six conflits WRITE_AFTER_WRITE lors de `vkCmdCopyImage` et deux destructions
d'objets encore utilisés. Leur attribution et leur correction restent ouvertes.
Notre adaptateur émet des `vkCmdBlitImage` ; cela aide à localiser les copies en
cause sans constituer, à lui seul, une preuve d'origine externe.

Le runtime effectue aussi ses propres soumissions sur le device emprunté : le
compteur global observé vaut 10 693, tandis que les dessins originaux restent
au nombre de dix. L'hypothèse du vérificateur « deux soumissions globales » doit
être remplacée par une mesure distinguant les commandes de l'application.
La sélection explicite des couches sur l'instance du client laisse les contrôles
du device emprunté actifs ; elle n'élimine pas les erreurs rapportées.

La prochaine étape est d'augmenter la résolution de contrôle et de résoudre ces
problèmes. Pour Diablo restent l'identité active des shaders, les ressources et
alias, les producteurs LOAD et la fermeture des états. Le backend D3D12 Windows
reste à développer ; les composants communs gardent leurs tests Windows.

## Reproduire hors casque

```sh
cmake -S native -B work/layer-poc -DD4VR_BUILD_LAYER_REPLAY_POC=ON
cmake --build work/layer-poc --parallel 2
python3 tools/layer_poc_validate.py \
  --layer work/layer-poc --client work/layer-poc/d4vr_layer_replay_client \
  --mock-loader work/layer-poc/libd4vr_replay_xr_mock.so \
  --validation-layer /chemin/vers/la-couche-khronos \
  --runtime /chemin/vers/SteamLinuxRuntime_4/run \
  --output work/nouvelle-preuve-privee
```

`tools/replay_xr_validate.py` prépare un essai réel borné, exige un nouveau
répertoire privé et échoue sur toute erreur Vulkan. Demander le casque uniquement
après compilation et contrôles hors casque, attendre le signal du testeur, lancer
immédiatement et annoncer la fin pour qu'il puisse laisser le casque en veille.
