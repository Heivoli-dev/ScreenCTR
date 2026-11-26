# Plan de test sur console (New 2DS XL + Luma3DS)

À suivre dans l'ordre : chaque étape isole une hypothèse. En cas d'échec, envoyer
`sd:/3ds/ScreenCTR/log.txt`, le numéro de l'étape et ce qui s'affiche.

Prérequis : Luma3DS à jour, carte SD sauvegardée, PC et console sur le même réseau.

## Étape 0 — Compilation
- [ ] `scripts/build_console.sh` (ou la CI) produit `ScreenCTR.cia` et `ScreenCTR-Launcher.3dsx`.
- [ ] La CI Windows produit `ScreenCTR.exe` ; `ctest` passe.

## Étape 1 — Démarrage
- [ ] Installer `ScreenCTR.cia` avec FBI, copier le lanceur dans `sd:/3ds/`.
- [ ] Lancer le lanceur depuis le Homebrew Launcher : « ScreenCTR démarré (PID …) ».
- [ ] `log.txt` contient `ScreenCTR 0.1.0 starting` puis `listening on 6464`.
- [ ] `config.ini` a été créé dans `sd:/3ds/ScreenCTR/`.

## Étape 2 — Réseau sans vidéo
- [ ] Sur le PC, la fausse console n'est PAS lancée. Connexion de ScreenCTR.exe à l'IP de la console.
- [ ] La console affiche la demande d'autorisation (étape 3 si non) ; « B » → le PC affiche « Connexion refusée ».

## Étape 3 — Raccourci, pause et menu (dans un jeu)
- [ ] L+R+START : le jeu se fige et le menu apparaît sur l'écran du bas, avec l'IP et le port.
- [ ] B : le menu disparaît, l'écran du bas retrouve exactement son contenu, le jeu reprend.
- [ ] Même test dans le menu HOME.
- [ ] Si le jeu se fige sans menu : noter le jeu ; `log.txt` doit contenir `overlay unavailable` et le jeu doit reprendre seul.

## Étape 4 — Diffusion
- [ ] Autoriser avec A : le jeu reprend, les deux écrans s'affichent sur le PC.
- [ ] Barre d'état du PC : noter FPS haut / bas et temps d'encodage console (ms).
- [ ] Changer d'écran dans le jeu, aller au menu HOME, revenir : l'image suit.
- [ ] Session de 30 minutes : pas de plantage, mémoire stable.
- [ ] Menu ScreenCTR → Y : la diffusion s'arrête, le PC affiche « Diffusion arrêtée depuis la console ».

## Étape 5 — Contrôle à distance
- [ ] Activer InputRedirection dans Rosalina (L+Bas+SELECT).
- [ ] Reconnecter en autorisant le contrôle. Bouton A de la manette → action A en jeu.
  *Si rien ne se passe : hypothèse « bouclage 127.0.0.1 » invalide (le noter).*
- [ ] Tester chaque bouton, la croix, le Circle Pad (les 4 directions), le C-Stick, ZL, ZR, HOME.
  *Noter toute inversion d'axe ou ZL/ZR permutés.*
- [ ] Cliquer sur l'écran du bas dans la fenêtre PC : le toucher correspond à l'endroit cliqué (coins inclus).
- [ ] Les boutons physiques de la console fonctionnent pendant le contrôle à distance.
- [ ] Maintenir A sur la manette puis débrancher le câble réseau du PC / couper le Wi-Fi du PC : A doit être relâché en moins d'une demi-seconde.
- [ ] Fermer ScreenCTR.exe en maintenant une direction : relâchée.

## Étape 6 — PC
- [ ] F1 à F5, F8, Ctrl+E (éditeur), F11, Ctrl+H, F6 (PNG), F7 (MP4 lisible dans VLC).
- [ ] OBS : capture de la fenêtre « ScreenCTR » avec l'interface masquée.
- [ ] Manettes : Xbox, DualShock 4, DualSense, une manette générique.
- [ ] Reconnexion automatique : éteindre le Wi-Fi de la console 10 s puis le rallumer.
