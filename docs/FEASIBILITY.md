# ScreenCTR — Étude de faisabilité technique

*Heivoli Network — Two screens. One connection.*

Ce document vérifie, point par point, que chaque exigence du cahier des charges est réalisable
sur New Nintendo 3DS / New 2DS XL sous Luma3DS, et fixe les choix techniques retenus.
Chaque point est classé :

- **PROUVÉ** : mécanisme déjà utilisé avec succès par un projet open source existant.
- **PLAUSIBLE** : techniquement cohérent, mais jamais vérifié tel quel ; à valider sur console.
- **RISQUÉ** : obstacle connu, solution de repli prévue.

## 1. Capture des deux écrans — PROUVÉ

**Mécanisme retenu** (le même que HzMod / ChirunoMod, GPLv3) :

1. `GSPGPU_ImportDisplayCaptureInfo()` renvoie, pour chaque écran, l'adresse virtuelle du
   framebuffer affiché, son format (RGBA8, BGR8, RGB565, RGB5A1, RGBA4) et son pas (stride).
2. Si l'adresse est en VRAM (`0x1F000000–0x1F600000`), la source DMA est notre propre
   processus (`CUR_PROCESS_HANDLE`, la VRAM y est mappée) ; sinon le framebuffer est dans la
   mémoire linéaire de l'application au premier plan, et on ouvre ce processus avec
   `svcOpenProcess()` (PID obtenu via `APT_GetAppletManInfo` / `APT_GetAppletInfo` /
   `NS`/`PM`).
3. `svcStartInterProcessDma()` copie le framebuffer dans un tampon de notre processus,
   puis `svcFlushProcessDataCache()` avant lecture par le CPU.

**Contraintes :**
- Les framebuffers 3DS sont stockés tournés de 90° (colonnes de 240 pixels) : la rotation est
  faite pendant la conversion couleur → YCbCr, sans passe supplémentaire.
- En 3D stéréoscopique, on ne capture que l'œil gauche (exigence « 400 × 240 en 2D »).
- Applications en mode « HIMEM » : ChirunoMod a besoin d'un lanceur spécifique sur Old 3DS ;
  sur New 3DS, la mémoire de base reste suffisante (à vérifier sur quelques jeux lourds).

## 2. Performances (30 FPS, deux écrans) — PLAUSIBLE

- New 3DS : 4 cœurs ARM11 à 804 MHz, cache L2. Le cœur 2 est utilisable par un sysmodule
  (ChirunoMod y place son thread réseau, priorité 0x10). Sur New 2DS XL, ce cœur est encore
  plus libre (pas de module QTM pour la 3D avec suivi du visage).
- Charge : 400×240 + 320×240 = 172 800 pixels/image → 5,2 Mpx/s à 30 FPS.
- HzMod annonce environ 60 FPS max sur New 3DS pour un écran. NTR atteint ~30 FPS sur
  deux écrans. **Objectif réaliste : 30 FPS haut + 30 FPS bas, qualité JPEG ~70, 4:2:0.**
- Économies : on n'encode un écran que s'il a changé (empreinte FNV-1a du framebuffer) —
  l'écran inférieur est souvent statique ; un écran inchangé est renvoyé sans réencodage.
- Pas de NEON sur ARM11 (ARMv6K) : encodeur JPEG entièrement entier (DCT AAN), écrit pour
  ScreenCTR et testé sur PC. libjpeg-turbo (portlib devkitPro, utilisé par ChirunoMod) reste
  une alternative si les mesures sur console sont meilleures.

**À mesurer sur console :** temps de capture DMA, temps d'encodage par écran, débit Wi-Fi
réel du New 2DS XL (802.11n limité, ~1–2 Mo/s utiles en UDP).

## 3. Exécution en arrière-plan / sysmodule — PROUVÉ avec réserve

- HzMod/ChirunoMod tournent comme processus système autonome (titre CIA, catégorie
  sysmodule) démarré par un petit lanceur. Démarrage automatique au boot : **non garanti**
  sans modification de Luma3DS.
- **Choix retenu (phase 1)** : sysmodule autonome `ScreenCTR` (CIA) + lanceur `ScreenCTR
  Launcher` à lancer une fois par session depuis le menu HOME. Il survit au changement de jeu.
- **Phase ultérieure (option)** : fork de Luma3DS/Rosalina (GPLv3) pour un démarrage
  automatique au boot. Décision à prendre après validation de la phase 1.

## 4. Raccourci L+R+START, pause du jeu, menu sur la console — PLAUSIBLE

- Lecture des touches : notre sysmodule lit l'état HID (service `hid:SPVR` ou mémoire
  partagée HID) toutes les 16 ms. Combinaison configurable (`/3ds/ScreenCTR/config.ini`).
- Pause : `svcControlProcess(handle, PROCESSOP_SCHEDULE_THREADS, 1, 0)` — SVC étendu de
  Luma3DS, celui que Rosalina utilise pour figer le jeu quand son menu s'ouvre.
- Affichage du menu : jeu figé, on écrit directement dans le framebuffer de l'écran
  inférieur (DMA inverse ou `svcMapProcessMemoryEx`), en sauvegardant puis restaurant son
  contenu. Police 8×8 intégrée.
- Risque : interaction avec le menu Rosalina s'il est ouvert en même temps (à tester).
- Sécurité : si le menu ne peut pas être dessiné, le jeu est immédiatement relancé (jamais
  de jeu figé derrière un menu invisible).

## 5. Contrôle de la console depuis le PC — PROUVÉ (Rosalina) / PLAUSIBLE (relais local)

- Rosalina (Luma3DS) contient **InputRedirection** : il patche le module `hid` (et `ir`
  pour le C-Stick/ZL/ZR) et écoute en UDP sur le port **4950** des paquets de 20 octets :
  boutons (actif bas, neutre `0xFFF`), tactile (neutre `0x02000000`), Circle Pad (neutre
  `0x007FF7FF`), C-Stick/ZL/ZR (neutre `0x80800081`), HOME/POWER.
- **Défaut constaté dans le code source de Rosalina : aucun délai d'expiration**, les
  dernières valeurs reçues restent appliquées si le PC se déconnecte. Le cahier des charges
  exige le contraire.
- **Choix retenu** : le PC n'envoie jamais directement à Rosalina. Il envoie ses commandes au
  sysmodule ScreenCTR (port UDP 6466), qui vérifie la session et l'autorisation, puis relaie
  vers Rosalina sur `127.0.0.1:4950`. ScreenCTR envoie un paquet neutre dès 250 ms sans
  paquet, à la fermeture de session, et à l'arrêt.
- **À vérifier sur console** : que la pile réseau 3DS (`soc:U`) accepte le bouclage local
  `127.0.0.1`. **Repli** : intégrer dans ScreenCTR le patch HID de Rosalina (GPLv3,
  compatible avec la licence du projet).
- Commandes physiques : Rosalina garde des emplacements distincts pour les valeurs
  physiques et distantes ; la fusion des deux est à confirmer sur console.
- Prérequis utilisateur : activer InputRedirection dans le menu Rosalina (L+Bas+SELECT →
  Paramètres réseau). ScreenCTR l'indique dans son menu.

## 6. Écran tactile — PROUVÉ

Format Rosalina : `(1<<24) | (y<<12) | x`, x et y ramenés sur 0–4095. Conversion PC :
position de la souris dans le rectangle affiché de l'écran inférieur → pixel 320×240 →
échelle 12 bits. Testé ici (fonction pure).

## 7. Application Windows — PROUVÉ (technologies standard)

Qt 6 (Widgets), SDL3 (manettes Xbox/DualShock/DualSense/génériques), FFmpeg (MP4 H.264).
Compatibilité OBS : capture de fenêtre (mode « interface masquée ») ; une sortie caméra
virtuelle n'est pas prévue en phase 1.

## 8. Réseau — PROUVÉ

- TCP 6464 : session (hello, autorisation, qualité, ping/latence, fin).
- UDP 6465 : vidéo console → PC, images JPEG fragmentées (≤ 1400 octets/datagramme).
- UDP 6466 : commandes PC → console, indépendant de la vidéo, 125 Hz + à chaque changement.
- Aucun serveur externe. Voir `protocol/PROTOCOL.md`.

## 9. Licence

Les mécanismes de capture et d'injection proviennent de projets GPLv3 (Luma3DS,
ChirunoMod). ScreenCTR est publié sous **GPL-3.0-or-later**, ce qui permet de reprendre du
code de ces projets si le repli le nécessite, avec crédit.

## 10. Ce qui ne peut PAS être vérifié dans l'environnement de développement actuel

L'environnement de développement de cette première itération ne dispose ni de devkitARM, ni
de Qt 6, ni de SDL3, ni d'un compilateur Windows, ni d'une console. Voir `docs/STATUS.md`
pour l'état exact, composant par composant.
