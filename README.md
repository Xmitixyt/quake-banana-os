# 🍌 Quake for Banana OS

[![Banana OS](https://img.shields.io/badge/OS-Banana%20OS%200.6-yellow.svg)](https://github.com/qcom-toolbox/Banana-OS)
[![Version](https://img.shields.io/badge/version-1.09%20Full-green.svg)]()
[![Arch](https://img.shields.io/badge/arch-x86__64%20%7C%20i686-blue.svg)]()

Portage natif de **Quake (1996)** d'id Software pour **[Banana OS](https://github.com/qcom-toolbox/Banana-OS)** (SDK API v14/v15), incluant la **version complète** (`pak0.pak` + `pak1.pak`, 4 épisodes complets), le support **multi-touches matériel** (`banana_key_down`) et un moteur de rendu logiciel optimisé 64-bit.

---

## 🎮 Fonctionnalités

- **Fenêtrage natif Banana OS (`bwin`)** :
  - Upscaling 64-bit déroulé ultra-rapide (`320x240` $\rightarrow$ `640x480`).
  - Fenêtre **redimensionnable** (`bwin_resizable`, mode 1x `320x240`, 2x `640x480` ou taille libre).
- **Support Multi-Touches matériel (SDK API v14+ `banana_key_down`)** :
  - Lecture directe des scancodes matériels PS/2 à chaque frame : déplacements diagonaux (<kbd>Z</kbd>+<kbd>Q</kbd>/<kbd>D</kbd> ou <kbd>W</kbd>+<kbd>A</kbd>/<kbd>D</kbd>) + course (<kbd>Shift</kbd>) + saut (<kbd>Espace</kbd>) + tir simultanés sans blocage.
  - Touche <kbd>Échap</kbd> instantanée pour ouvrir le menu.
  - Rétrocompatibilité automatique avec les noyaux Banana OS antérieurs (< API v14).
- **Contrôles Clavier (AZERTY & QWERTY) & Souris (`autoexec.cfg` intégré)** :
  - <kbd>Z</kbd> / <kbd>W</kbd> / <kbd>↑</kbd> : Avancer
  - <kbd>S</kbd> / <kbd>↓</kbd> : Reculer
  - <kbd>Q</kbd> / <kbd>A</kbd> : Pas chassé à gauche (Strafe Left)
  - <kbd>D</kbd> : Pas chassé à droite (Strafe Right)
  - <kbd>←</kbd> / <kbd>→</kbd> : Tourner à gauche / droite
  - <kbd>Espace</kbd> / <kbd>Clic droit</kbd> : Sauter / Nager
  - <kbd>Shift</kbd> : Courir
  - <kbd>Clic gauche</kbd> / <kbd>Ctrl</kbd> : Tirer
  - **Souris (`+mlook` actif par défaut)** : Visée libre 360°
  - **Molette de souris (`bwin_wheel`)** : Arme suivante / précédente
  - <kbd>1</kbd>–<kbd>8</kbd> : Sélection directe des armes
- **Audio 16-bit Stéréo continu (`snd_banana.c`)** :
  - Mixage multi-canaux synchronisé sur `soundtime` / `paintedtime` avec pré-buffer `_snd_mixahead 0.15` sans coupure ni blocage DMA.
- **Optimisations VirtualBox / QEMU** :
  - Boucle principale non-bloquante (`banana_yield()` sans `hlt` CPU stall).
  - Cache de surfaces (`surfcache`) étendu à **2 Mo** et hunk mémoire de **24 Mo**.
  - Compilation `-O3 -ffast-math -funroll-loops` avec `UNALIGNED_OK=1` et `memcpy`/`memset` 64-bit.
- **Architecture Universelle** : paquet `.bpk` contenant à la fois `app-x86_64` et `app-i686`.

---

## 📥 Installation directe sur Banana OS depuis GitHub

Ouvrez un terminal dans **Banana OS** :

```bash
# 1. Supprimer une éventuelle ancienne version
rm -f quake.bpk*

# 2. Télécharger directement depuis GitHub
wget https://raw.githubusercontent.com/Xmitixyt/quake-banana-os/main/quake.bpk

# 3. Installer le paquet
pkg install quake.bpk

# 4. Lancer Quake
quake
```

*(Astuce VirtualBox : désactivez l'intégration automatique de la souris avec <kbd>Ctrl Droite</kbd> + <kbd>I</kbd> pour capturer la souris dans le jeu).*

---

## 🛠️ Compilation à partir des sources

```bash
python build_quake.py
```

---

## 📜 Licence

- Moteur Quake original © id Software (GNU GPL v2).
