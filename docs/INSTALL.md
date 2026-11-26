# Installation

> Version 0.1.0 : pas encore validée sur console. Voir `docs/STATUS.md`.

## Console (New 3DS, New 3DS XL, New 2DS XL avec Luma3DS)
1. Copier `ScreenCTR.cia` sur la carte SD et l'installer avec FBI.
2. Copier `ScreenCTR-Launcher.3dsx` dans `sd:/3ds/`.
3. Pour le contrôle depuis le PC : menu Rosalina (L+Bas+SELECT) → activer **InputRedirection**.

À chaque démarrage de la console : ouvrir le Homebrew Launcher et lancer **ScreenCTR Launcher**
une fois. ScreenCTR tourne ensuite en arrière-plan, même en changeant de jeu.

### Utilisation
- **L+R+START** (modifiable dans `sd:/3ds/ScreenCTR/config.ini`) : pause du jeu + menu ScreenCTR
  avec l'adresse IP et le port.
- Demande de connexion d'un PC : **A** autoriser, **Y** autoriser et mémoriser, **B** refuser,
  puis une seconde question pour le contrôle à distance.
- Dans le menu : **B** reprendre, **Y** arrêter la diffusion, **X** contrôle à distance oui/non,
  **SELECT** quitter ScreenCTR.

`config.ini` :
```ini
hotkey = L+R+START
remote_input = on        # off : contrôle à distance totalement désactivé
auto_accept = on         # reconnexion sans confirmation des PC mémorisés
quality = 70
fps_cap = 30
chroma = 420
port = 6464
```

## Windows 10 / 11 (64 bits)
Décompresser le dossier `ScreenCTR` et lancer `ScreenCTR.exe`. Autoriser l'accès réseau
privé lorsque Windows le demande (ports UDP 6465 entrant).

| Raccourci | Action |
|---|---|
| F1 / F2 / F3 | Superposés / côte à côte / grand + petit |
| F4 / F5 | Écran supérieur / inférieur seul |
| F8 · Ctrl+E | Disposition personnalisée · éditeur |
| F6 · F7 | Capture PNG · enregistrement MP4 |
| F11 · Ctrl+H | Plein écran · masquer l'interface (OBS) |
| Ctrl+I | Contrôle à distance oui/non |

Tous les raccourcis et toutes les touches se modifient dans Paramètres.
