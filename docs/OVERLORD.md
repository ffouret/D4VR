# Ce qu'Overlord apporte à l'étude

Référence examinée le 24 octobre 2026 :
[Aeka0/Overlord](https://github.com/Aeka0/Overlord), commit
`322495f2ee8b7f120b7d285a11d998f63561d5fa`.

Le propriétaire indique que ce mod de Modern Warfare 2 Campaign Remastered
fonctionne sur son PC Bazzite avec son Quest 2. Le backend et la version qu'il
utilise restent à relever : le code courant du dépôt peut différer de sa copie.

## Observations dans les sources

| Observation | Source vérifiable au commit examiné |
| --- | --- |
| Le renderer se branche sur des fonctions et adresses propres au jeu H2 | [engine_stereo_renderer.cpp](https://github.com/Aeka0/Overlord/blob/322495f2ee8b7f120b7d285a11d998f63561d5fa/src/client/component/vr/engine_stereo_renderer.cpp#L62) |
| La session OpenXR utilise le périphérique D3D11 du jeu | [openxr_runtime.cpp](https://github.com/Aeka0/Overlord/blob/322495f2ee8b7f120b7d285a11d998f63561d5fa/src/client/component/vr/openxr_runtime.cpp#L780) |
| Le contrat de rendu impose des vues natives cohérentes et des ressources par œil | [Contrats VR](https://github.com/Aeka0/Overlord/blob/322495f2ee8b7f120b7d285a11d998f63561d5fa/docs/vr-runtime-rendering.md) |
| Le projet est sous licence GPLv3 | [Licence](https://github.com/Aeka0/Overlord/blob/322495f2ee8b7f120b7d285a11d998f63561d5fa/LICENSE) |

## Conséquences pour D4VR

L'exemple soutient la piste d'une intégration au moteur pour obtenir une vraie
stéréo. Les adresses et structures H2 ne constituent pas un adaptateur Diablo.
Le chemin graphique D3D12 de Diablo IV exige une recherche distincte.

La séparation entre adaptateur du jeu et présentation VR guide notre conception.
L'étude n'a importé aucun code d'Overlord et ne transforme pas ses essais sous
Windows en garantie de fonctionnement sous Lutris.

Une reprise future de code devra respecter sa licence et être distinguée du
code original MIT de D4VR.
