# ScreenCTR — État d'avancement (itération 1)

**Règle du projet : aucune fonctionnalité n'est déclarée fonctionnelle sans test.**
Cette itération a été développée dans un environnement Linux **sans devkitARM, sans Qt 6,
sans SDL3, sans compilateur Windows et sans console**. Tout ce qui dépend de ces outils
est écrit mais **non compilé et non testé**.

Légende :
- ✅ **Testé** — exécuté ici, tests automatisés reproductibles (`scripts/run_tests.sh`).
- 🟡 **Écrit, non compilé** — code complet, jamais passé par son vrai compilateur.
- 🔴 **Non fait**.

## Ce qui est testé

| Composant | Preuve |
|---|---|
| ✅ Protocole (sérialisation, contrôle TCP, en-têtes vidéo, commandes) | `tests/test_common.c` |
| ✅ Réassemblage des fragments (désordre, doublons, retard, bouclage des numéros) | `tests/test_common.c` |
| ✅ Encodeur JPEG maison | Décodé par libjpeg (Pillow) : **même PSNR que libjpeg à 0,1 dB près**, tailles à ±0,5 % (`tests/check_jpeg.py`) |
| ✅ Conversion des 5 formats de framebuffer 3DS + rotation | Aller-retour sans perte (RGBA8, BGR8), erreur ≤ quantification (RGB565, RGB5A1, RGBA4) |
| ✅ Conversion vers le format Rosalina InputRedirection | Valeurs neutres identiques à celles du code source de Rosalina |
| ✅ Configuration console (combinaison de touches, PC mémorisés) | `tests/test_common.c` |
| ✅ Dispositions F1–F5 + personnalisée, conversion souris → tactile | `tests/test_pc_core.cpp` |
| ✅ Profils de manette, zones mortes, gâchettes → ZL/ZR, clavier AZERTY/QWERTY | `tests/test_pc_core.cpp` |
| ✅ FPS, latence (RTT lissé), pertes, score de qualité | `tests/test_pc_core.cpp` |
| ✅ **Chaîne complète console → PC** avec la fausse console | `tests/e2e_test.py` : poignée de main, autorisation, 30 FPS, JPEG décodés, contenu vérifié, ping, qualité, relais des commandes, **relâchement automatique en 251 ms**, relâchement sur déconnexion, refus d'un 2e PC, refus sur console. 17/17 sans perte et avec 5 % de perte simulée |
| ✅ Commande FFmpeg de l'enregistrement MP4 | `tests/recorder_pipe_test.py` (mêmes arguments que `Recorder.cpp`) |
| ✅ Code console : syntaxe et types | Contre des en-têtes libctru *de substitution* (`tests/stubs/`) — **ne remplace pas** une compilation devkitARM |

Mesure indicative (PC x86, **non représentative de l'ARM11**) : conversion + encodage
d'un écran 400×240 en 1,6 ms.

## Ce qui est écrit mais pas encore compilé / testé

| Composant | Fichiers | Ce qui doit être vérifié |
|---|---|---|
| 🟡 Sysmodule 3DS | `console/source/*` | Compilation devkitARM ; voir le plan de test matériel |
| 🟡 Lanceur 3DS | `console/launcher/` | Compilation ; `NS_LaunchTitle` depuis le Homebrew Launcher |
| 🟡 RSF (droits du sysmodule) | `console/screenctr.rsf` | Catégorie, accès core 2, mappage VRAM / registre HID, SVC Luma |
| 🟡 Application Windows Qt 6 | `pc/src/*` | Compilation MSVC, fonctionnement réel |
| 🟡 Manettes SDL3 | `pc/src/GamepadManager.cpp` | Xbox, DualShock 4, DualSense, génériques |
| 🟡 CI GitHub | `.github/workflows/build.yml` | Premier passage (versions d'outils) |

## Hypothèses techniques à valider sur la New 2DS XL (par ordre de risque)

1. **Bouclage local `127.0.0.1` sur la pile réseau 3DS** (relais vers Rosalina). Repli
   prévu : intégrer le patch HID de Rosalina dans ScreenCTR.
2. **Bits ZL/ZR et ordre des axes du C-Stick** dans le mot IR de Rosalina.
3. **Pause du jeu** par `svcControlProcess(PROCESSOP_SCHEDULE_THREADS)` et dessin du menu
   dans le framebuffer (mappage `svcMapProcessMemoryEx`).
4. **Fin de transfert DMA** détectée par `svcGetDmaState` (comportement New 3DS).
5. **Détection du menu HOME au premier plan** via `APT:U` en IPC directe.
6. **Budget mémoire** (tas de 2,6 Mio en région BASE) avec les jeux les plus gourmands.
7. **Performances réelles** : objectif 30 FPS sur l'écran du haut.

## Limitations connues (assumées)

- Écran supérieur capturé en 2D (œil gauche) ; le mode 800 px « wide » n'est pas géré.
- Démarrage du sysmodule par un lanceur à chaque allumage (pas de démarrage automatique
  sans modifier Luma3DS).
- Le contrôle à distance nécessite d'activer InputRedirection dans Rosalina.
- La combinaison d'ouverture du menu ne peut pas utiliser ZL/ZR (lus par un autre module).
- Manettes absentes de la base SDL : ajouter `gamecontrollerdb.txt` à côté de l'exécutable.
- Avec de fortes pertes Wi-Fi, chaque fragment perdu coûte une image entière (5 % de pertes
  de fragments ≈ 22 FPS au lieu de 30). Piste : correction d'erreurs (parité XOR) en phase 12.
- Pas de caméra virtuelle pour OBS : capture de fenêtre avec l'interface masquée.

## Étapes du cahier des charges

| # | Étape | État |
|---|---|---|
| 1 | Faisabilité | ✅ `docs/FEASIBILITY.md` (mécanismes vérifiés dans les sources de Rosalina et ChirunoMod) |
| 2 | Structure du projet | ✅ |
| 3 | Capture des écrans | 🟡 console / ✅ conversion + encodage |
| 4 | Protocole réseau | ✅ |
| 5 | Récepteur Windows | 🟡 application / ✅ réception validée par le client de test |
| 6 | Sysmodule et raccourci | 🟡 |
| 7 | Transmission des commandes | 🟡 console et PC / ✅ protocole, relais, sécurité |
| 8 | Contrôle tactile | 🟡 / ✅ conversion des coordonnées |
| 9 | Interface définitive | 🟡 |
| 10 | Dispositions personnalisables | 🟡 / ✅ calculs |
| 11 | Captures et enregistrements | 🟡 / ✅ FFmpeg |
| 12 | Optimisation | 🔴 nécessite des mesures sur console |
| 13 | Fichiers d'installation | 🟡 scripts + CI, aucun binaire produit |
