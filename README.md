# 🍌 Quake for Banana OS (Bêta)

[![Banana OS](https://img.shields.io/badge/OS-Banana%20OS%200.6-yellow.svg)](https://github.com/qcom-toolbox/Banana-OS)
[![Status](https://img.shields.io/badge/status-v1.1%20Bêta%20(capture%20souris)-orange.svg)](https://github.com/Xmitixyt/quake-banana-os/releases/tag/v1.1-beta)
[![Arch](https://img.shields.io/badge/arch-x86__64%20%7C%20i686-blue.svg)]()

Portage natif de **Quake (1996)** d'id Software (basé sur le moteur original `WinQuake` / `idquake`) pour **[Banana OS](https://github.com/qcom-toolbox/Banana-OS)**, un système d'exploitation x86/x86_64 développé *from scratch* avec environnement de fenêtrage GUI (`bwin`).

Ce projet rejoint la collection des portages classiques sur Banana OS aux côtés de **[DOOM for Banana OS](https://github.com/Xmitixyt/doom-banana-os)**, en franchissant le cap d'un moteur **100% 3D temps réel** (BSP, lightmaps dynamiques, modèles alias `.mdl`, physique 3D et machine virtuelle QuakeC `progs.dat`).

> [!NOTE]
> **Statut actuel : Version v1.1 Bêta**  
> Intègre le nouveau support natif de capture relative de la souris via l'API v20 du SDK Banana OS (`bwin_capture_mouse`, `BANANA_EV_MOUSE_REL`).

---

## 🛠️ Ce qui a été réalisé sur ce portage (Architecture & Optimisations)

### 1. Couche d'abstraction matérielle (HAL) Banana OS
Trois pilotes natifs ont été écrits sur mesure pour interfacer le moteur Quake avec l'API noyau de Banana OS (`__banana`) :
- **[`sys_banana.c`](sys_banana.c) — Système, Mémoire, Fichiers & Horloge** :
  - Gestionnaire de descripteurs de fichiers VFS (`Sys_FileOpenRead`, `Sys_FileOpenWrite`, `Sys_FileSeek`, `Sys_FileRead`) compatible avec le système de fichiers en mémoire/disque de Banana OS.
  - Allocation dynamique du *hunk* mémoire de **24 Mo** (avec repli automatique à 16 Mo / 12 Mo).
  - Boucle principale cadencée à **72 Hz** via `banana_ticks()` et `banana_yield()` **sans jamais appeler `banana_sleep(1)`** : évite l'instruction `sti; hlt` qui gèle les interruptions d'horloge pendant 15 à 30 ms sous VirtualBox.
- **[`vid_banana.c`](vid_banana.c) — Rendu Vidéo 64-bit & Capture Relative de la Souris (SDK v20)** :
  - **Capture relative native de la souris (`bwin_capture_mouse`)** : masque automatiquement le curseur de la souris (matériel GPU et logiciel), capture le pointeur de la VM (`vmtools_capture_pointer`), empêche les clics accidentels en arrière-plan et reçoit les deltas relatifs purs (`BANANA_EV_MOUSE_REL`) pour une rotation illimitée à 360° sans blocage aux bords.
  - Rendu logiciel 3D interne en `320x240` avec **Z-buffer 16-bit** et **cache de surfaces (`surfcache`) étendu de 512 Ko à 2 Mo** (alloué dynamiquement sur le tas pour alléger le binaire ELF de 550 Ko et éliminer le *thrashing* des lightmaps).
  - **Blitter 64-bit déroulé (`palette64`)** : pré-calcul de paires de pixels 32-bit `0x00RRGGBB` dans des registres 64-bit (`uint64_t`) et copie déroulée par 4 vers le framebuffer `bwin` en `640x480` (2x), `320x240` (1x natif) ou redimensionnement libre en virgule fixe `16.16`.
  - Fenêtre redimensionnable (`bwin_resizable`) et prise en charge de la molette (`bwin_wheel`).
- **[`snd_banana.c`](snd_banana.c) — Moteur Audio DMA Continu** :
  - Mixage stéréo 16-bit multi-canaux avec spatialisation gauche/droite.
  - Synchronisation exacte de la plage `[last_paintedtime, paintedtime)` sur `soundtime` avec pré-buffer `_snd_mixahead 0.15` (150 ms) et `S_ExtraUpdate` actif.
  - Correction du seuil `banana_audio_queued_ms()` (qui inclut les ~300 ms d'avance `AHEAD` du ring buffer matériel AC'97 / Intel HDA de Banana OS), éliminant toute coupure sonore ou répétition de buffer lors des chargements de niveaux.

### 2. Support Multi-Touches Matériel (SDK API v14 / v15)
Tout comme sur **[DOOM for Banana OS](https://github.com/Xmitixyt/doom-banana-os)**, Quake exploite la fonction **`banana_key_down(key)`** introduite dans l'API v14 du SDK :
- **Zéro blocage typematic** : lecture directe de la matrice de scancodes matériels PS/2 du noyau à chaque frame.
- Permet de maintenir simultanément **Avancer (<kbd>Z</kbd>/<kbd>W</kbd>) + Strafer (<kbd>Q</kbd>/<kbd>A</kbd> ou <kbd>D</kbd>) + Courir (<kbd>Shift</kbd>) + Sauter (<kbd>Espace</kbd>) + Tirer (<kbd>Ctrl</kbd> / Souris)**.
- Touche <kbd>Échap</kbd> instantanée (libère proprement la capture souris pour basculer vers le bureau/menu si désiré, puis un clic dans la fenêtre reprend la capture).

### 3. Optimisations du Moteur Logiciel (`WinQuake`) & `libc` SDK
- **Intégration du SDK Banana OS v20** : support complet des nouvelles bibliothèques `pthread`, `socket`, `ufd`, `stdio` et `mman`.
- **Compilation `-O3 -ffast-math -funroll-loops -fno-builtin`** pour les deux architectures (`x86_64` avec `-mno-red-zone` pour les interruptions Ring 0, et `i686` avec support `divdi3` 64-bit).
- **Optimisation de `d_scan.c` & `quakedef.h`** : activation de `UNALIGNED_OK 1` sur x86/x86_64, déroulement par 8 pixels de `D_DrawSpans8` (perspective texture mapping) et mise en registres locaux de `D_DrawTurbulent8Span` (eau/lave/téléporteurs).
- **Version Complète incluse (`pak0.pak` + `pak1.pak`)** : les 4 épisodes complets, toutes les armes et toutes les maps sont intégrés dans `quake.bpk`.

---

## 🎮 Contrôles Clavier (AZERTY & QWERTY) & Souris

Le fichier [`autoexec.cfg`](autoexec.cfg) intégré configure automatiquement les contrôles FPS modernes au démarrage :

| Action | Touche / Souris |
| :--- | :--- |
| **Avancer** | <kbd>Z</kbd> / <kbd>W</kbd> / <kbd>↑</kbd> |
| **Reculer** | <kbd>S</kbd> / <kbd>↓</kbd> |
| **Pas chassé gauche (Strafe Left)** | <kbd>Q</kbd> / <kbd>A</kbd> |
| **Pas chassé droite (Strafe Right)** | <kbd>D</kbd> |
| **Tourner gauche / droite** | <kbd>←</kbd> / <kbd>→</kbd> |
| **Visée libre 360° (`+mlook`)** | Mouvement de la **Souris (capture relative)** |
| **Tirer** | **Clic Gauche** / <kbd>Ctrl</kbd> |
| **Sauter / Nager** | <kbd>Espace</kbd> / **Clic Droit** |
| **Courir (Speed)** | <kbd>Shift</kbd> |
| **Changer d'arme** | **Molette Souris** (Haut / Bas) ou <kbd>1</kbd>–<kbd>8</kbd> |
| **Menu / Libérer Souris** | <kbd>Échap</kbd> |

---

## 📥 Installation Rapide sur Banana OS

Ouvrez un terminal dans Banana OS (`startx` si vous êtes en mode texte) :

```bash
# 1. Nettoyer les éventuels anciens téléchargements
rm -f quake.bpk*

# 2. Télécharger directement depuis la Release GitHub v1.1-beta (ou la branche main)
wget https://github.com/Xmitixyt/quake-banana-os/releases/download/v1.1-beta/quake.bpk
# (Alternative directe : wget https://raw.githubusercontent.com/Xmitixyt/quake-banana-os/main/quake.bpk)

# 3. Installer le paquet
pkg install quake.bpk

# 4. Lancer Quake
quake
```

> [!TIP]
> **Souris sous VirtualBox** : Désactivez l'intégration automatique de la souris avec <kbd>Ctrl Droite</kbd> + <kbd>I</kbd> pour permettre une rotation à 360° fluide sans blocage aux bords de la fenêtre.

---

## 🔗 Collection des Jeux Portés sur Banana OS

- 🍌 **[DOOM for Banana OS](https://github.com/Xmitixyt/doom-banana-os)** (`doom.bpk`) — Portage basé sur `doomgeneric` avec multi-touches SDK v14, plein écran et audio 16 canaux.
- 🍌 **[Quake for Banana OS](https://github.com/Xmitixyt/quake-banana-os)** (`quake.bpk`) — Portage 3D temps réel basé sur `WinQuake` (Version Complète `pak0`+`pak1`) avec multi-touches SDK v14/v15, blitter 64-bit et audio DMA continu.

---

## 🛠️ Compilation à partir des sources

La chaîne de compilation utilise **Clang/LLVM** (`clang` + `ld.lld`) et l'outil `bpkg` du SDK Banana OS :

```bash
python build_quake.py
```

Le script compile les cibles `app-x86_64` et `app-i686`, empaquette `id1/pak0.pak`, `id1/pak1.pak` et `id1/autoexec.cfg`, et vérifie l'archive finale `quake.bpk` avec `bpkg check`.

---

## 📜 Licence

- Moteur Quake original © id Software (GNU GPL v2).
- Couche d'abstraction Banana OS (`sys_banana.c`, `vid_banana.c`, `snd_banana.c`) : GPL v2.
