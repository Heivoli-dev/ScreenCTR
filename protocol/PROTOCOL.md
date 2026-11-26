# Protocole ScreenCTR — version 1

Référence d'implémentation : `protocol/include/screenctr_protocol.h` + `protocol/src/sc_protocol.c`
(C99, partagé par la console, l'application Windows, la fausse console et les tests).
Tous les entiers sont **little-endian**.

## Canaux

| Canal | Transport | Sens | Port par défaut |
|---|---|---|---|
| Contrôle / session | TCP | PC ↔ console (la console écoute) | 6464 |
| Vidéo | UDP | console → PC (le PC écoute) | 6465 |
| Commandes | UDP | PC → console (la console écoute) | 6466 |
| Relais Rosalina | UDP local | sysmodule → Rosalina (`127.0.0.1`) | 4950 |

Les commandes ne passent jamais par le canal vidéo : une rafale d'images ne retarde pas
une pression de bouton.

## 1. Contrôle (TCP)

Trame : `u16 len` (taille de `type` + charge utile, ≤ 257) · `u8 type` · charge utile.

| Type | Nom | Sens | Charge utile |
|---|---|---|---|
| 0x01 | HELLO | PC→C | `u32 magic "SCTR"`, `u16 version`, `u16 flags` (bit0 = demande le contrôle), `u16 port vidéo du PC`, `char nom[32]`, `u8 id_client[16]` (aléatoire, généré à l'installation du PC) |
| 0x02 | AUTH_PENDING | C→PC | — (l'utilisateur doit accepter sur la console) |
| 0x03 | WELCOME | C→PC | `u32 jeton de session`, `u16 version`, `u16 port commandes`, `u8 contrôle autorisé`, `u8 modèle` |
| 0x04 | REJECT | C→PC | `u8 raison` (1 version, 2 refusé, 3 occupé, 4 délai dépassé) |
| 0x10 | SET_QUALITY | PC→C | `u8 qualité JPEG 10–95`, `u8 FPS max (0 = illimité)`, `u8 écrans (bit0 haut, bit1 bas)`, `u8 chroma (0 = 4:2:0, 1 = 4:4:4)` |
| 0x11 | SET_INPUT | PC→C | `u8 activé` |
| 0x20 | PING | ↔ | `u32 seq`, `u32 horodatage ms de l'émetteur` |
| 0x21 | PONG | ↔ | copie exacte du PING |
| 0x30 | STATS | C→PC | `u16 fps×10 [2]`, `u16 µs d'encodage [2]`, `u32 octets/s`, `u16 images inchangées non envoyées`, `u16 réservé` |
| 0x7F | BYE | ↔ | — |

### Séquence

```
PC                                   Console
 |---- TCP connect :6464 ------------->|
 |---- HELLO ------------------------->|  menu ScreenCTR : « Autoriser PC-xxx ? A = oui, B = non »
 |<--- AUTH_PENDING -------------------|  (+ « Autoriser le contrôle ? » si flags bit0)
 |<--- WELCOME (jeton) ----------------|  le jeu reprend, la diffusion démarre
 |<=== UDP vidéo :6465 ================|
 |==== UDP commandes :6466 ===========>|
 |---- PING / PONG (1 Hz) ------------>|  latence aller-retour
 |<--- STATS (1 Hz) -------------------|
 |---- BYE ou fermeture TCP ---------->|  arrêt diffusion + relâchement de toutes les commandes
```

Un PC déjà autorisé (son `id_client` est mémorisé dans `sd:/3ds/ScreenCTR/config.ini`)
peut se reconnecter sans nouvelle confirmation si `auto_accept = on`. Limite assumée : sur
un réseau local hostile, un tiers qui intercepte le HELLO peut rejouer l'identifiant ; le
réseau local est considéré comme de confiance (comme NTR, HzMod et Rosalina, qui n'ont
aucune authentification).
Délai de confirmation : 30 s → REJECT 4.

## 2. Vidéo (UDP)

Chaque image d'écran est un JPEG baseline complet, découpé en fragments de 1380 octets
(datagramme ≤ 1400 octets, sous la MTU Wi-Fi). En-tête de 20 octets :

| Octets | Champ |
|---|---|
| 0–1 | magic `0x4353` ("SC") |
| 2 | version (1) |
| 3 | écran (0 haut 400×240, 1 bas 320×240) |
| 4–7 | jeton de session (les datagrammes au mauvais jeton sont ignorés) |
| 8–11 | numéro d'image (par écran, croissant, comparaison modulo 2³²) |
| 12–13 | index du fragment |
| 14–15 | nombre de fragments |
| 16–19 | horodatage de capture (ms, horloge console) |

Règles du récepteur : une seule image en cours par écran ; un numéro plus récent abandonne
l'image incomplète précédente ; les fragments en retard sont ignorés. Pas de
retransmission : une image perdue est remplacée par la suivante (latence minimale).
Un écran qui ne change plus est renvoyé tel quel (même JPEG, nouveau numéro d'image, sans
réencodage) à +100 ms, +300 ms puis toutes les secondes : un client qui a perdu un
fragment se resynchronise vite, sans coût d'encodage pour la console.

## 3. Commandes (UDP, 28 octets)

| Octets | Champ |
|---|---|
| 0–1 | magic `0x4953` ("SI") |
| 2 | version |
| 3 | drapeaux (bit0 = écran tactile appuyé) |
| 4–7 | jeton de session |
| 8–11 | numéro de séquence (les paquets plus anciens sont ignorés) |
| 12–15 | boutons (actif haut) : A, B, SELECT, START, →, ←, ↑, ↓, R, L, X, Y, ZL, ZR, HOME, POWER |
| 16–19 | Circle Pad x, y (`s16`, −32767…32767, +y = haut) |
| 20–23 | C-Stick x, y (idem) |
| 24–27 | tactile x (0–319), y (0–239) |

Le PC envoie l'état complet à 125 Hz et immédiatement à chaque changement (un paquet
perdu est corrigé 8 ms plus tard, sans accumulation d'état).
**Sécurité :** la console relâche tout (paquet neutre envoyé à Rosalina) si aucun paquet
valide n'arrive pendant 250 ms, à la fermeture de session, si le contrôle est désactivé, et
à l'arrêt du sysmodule.

## 4. Conversion vers Rosalina InputRedirection (20 octets)

Faite par `sc_input_to_rosalina()` côté console :

| Octets | Contenu | Neutre |
|---|---|---|
| 0–3 | boutons HID bits 0–11, **actif bas** | `0x00000FFF` |
| 4–7 | tactile `(1<<24) | (y12<<12) | x12` | `0x02000000` |
| 8–11 | Circle Pad `(y12<<12) | x12`, centre 0x7FF | `0x007FF7FF` |
| 12–15 | C-Stick `(y8<<24) | (x8<<16) | (ZL/ZR<<8) | 0x81` | `0x80800081` |
| 16–19 | HOME (bit0), POWER (bit1), POWER long (bit2) | `0` |

Les valeurs neutres sont reprises du code source de Rosalina ; l'ordre des axes du
C-Stick et les bits ZL/ZR sont **à confirmer sur console** (voir `docs/STATUS.md`).
