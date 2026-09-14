# Plan d'action fsociety — Architecture, fonctionnalités & définitions

---

## 1. Architecture globale

### 1.1 Vue d'ensemble

```
┌──────────────────────────────┐         ┌──────────────────────────────┐
│        ATTAQUANT             │         │           CIBLE              │
│        (Linux)               │         │         (Windows)            │
│                              │         │                              │
│  ┌────────────────────────┐  │         │  ┌────────────────────────┐  │
│  │    fsociety-c2         │  │         │  │   fsociety-loader      │  │
│  │    (contrôleur)        │  │         │  │   (stager)             │  │
│  │                        │  │         │  │                        │  │
│  │  - Serveur socket      │  │         │  │  - Anti-VM / anti-debug│  │
│  │  - Gestion sessions    │  │         │  │  - Download payload    │  │
│  │  - Crypto RSA + AES    │  │         │  │  - Injection mémoire   │  │
│  │  - CLI opérateur       │  │         │  │  - Obfuscation strings │  │
│  │  - Logger              │  │         │  └───────────┬────────────┘  │
│  └───────────┬────────────┘  │         │              │               │
│              │               │         │              ▼               │
│              │               │         │  ┌────────────────────────┐  │
│              │               │         │  │   fsociety-implant     │  │
│              │               │         │  │   (payload principal)  │  │
│              │               │         │  │                        │  │
│              │               │         │  │  ┌──────────────────┐  │  │
│              │               │         │  │  │ mrrobot (shell)  │  │  │
│              │               │         │  │  ├──────────────────┤  │  │
│              │               │         │  │  │ whiterose        │  │  │
│              │               │         │  │  │  (évasion +      │  │  │
│              │               │         │  │  │   syscalls)      │  │  │
│              │               │         │  │  ├──────────────────┤  │  │
│              │               │         │  │  │ darkarmy         │  │  │
│              │               │         │  │  │  (mvt latéral)   │  │  │
│              │               │         │  │  ├──────────────────┤  │  │
│              │               │         │  │  │ fivenine         │  │  │
│              │               │         │  │  │  (persistance)   │  │  │
│              │               │         │  │  ├──────────────────┤  │  │
│              │               │         │  │  │ stage2           │  │  │
│              │               │         │  │  │  (commandes      │  │  │
│              │               │         │  │  │   avancées)      │  │  │
│              │               │         │  │  ├──────────────────┤  │  │
│              │               │         │  │  │ keylog / rdp /   │  │  │
│              │               │         │  │  │ crack / pth /    │  │  │
│              │               │         │  │  │ loot / phish /   │  │  │
│              │               │         │  │  │ privsec          │  │  │
│              │               │         │  │  └──────────────────┘  │  │
│              │               │         │  │                        │  │
│              │               │         │  │  - Crypto session      │  │
│              │               │         │  │  - Comm HTTP/DNS       │  │
│              │               │         │  │  - Watchdog            │  │
│              │               │         │  │  - Nettoyage logs      │  │
│              │               │         │  └───────────┬────────────┘  │
│              │               │         │              │               │
└──────────────┼───────────────┘         └──────────────┼───────────────┘
               │                                        │
               │        Canal chiffré (HTTPS/DNS)       │
               └────────────────────────────────────────┘
                    RSA (échange de clé) + AES (session)
```

### 1.2 Flux d'une intrusion

```
1. DÉPLOIEMENT
   Attaquant ──► fsociety-loader.exe ──► VM Windows cible
                 (exécution manuelle dans le lab)

2. STAGING
   fsociety-loader ──► Vérifie anti-VM/anti-debug
                    ──► Contacte fsociety-c2 (HTTPS)
                    ──► Télécharge fsociety-implant
                    ──► Injecte en mémoire (pas de disque)
                    ──► Exécute

3. ÉTABLISSEMENT
   fsociety-implant ──► Échange de clés RSA avec C2
                     ──► Session AES établie
                     ──► Enregistre persistance (fivenine)
                     ──► Lance watchdog

4. COMMANDES
   Opérateur ──► fsociety-c2 CLI ──► commande chiffrée
                                    ──► fsociety-implant
                                    ──► exécution (mrrobot, keylog, etc.)
                                    ──► résultat chiffré
                                    ──► fsociety-c2

5. NETTOYAGE
   Opérateur ──► commande cleanup ──► rollback Event Logs
                                   ──► suppression artefacts
                                   ──► (démonstration avant/après)
```

### 1.3 Composants et responsabilités

| Composant | Rôle | Langage | OS cible |
|---|---|---|---|
| **fsociety-c2** | Contrôleur : serveur d'écoute, gestion sessions, CLI, crypto, logs | C | Linux |
| **fsociety-implant** | Payload principal : communication, persistance, modules, watchdog | C | Windows |
| **fsociety-loader** | Stager : anti-VM, download, injection mémoire, obfuscation | C | Windows |
| **common** | Code partagé : protocole, crypto commune, types | C | Les deux |

### 1.4 Protocole de communication

```
┌─────────────────────────────────────────────────────┐
│                  PAQUET fsociety                    │
├──────────┬──────────┬──────────┬───────────────────┤
│  MAGIC   │  TYPE    │  LENGTH  │    PAYLOAD        │
│ (4 oct.) │ (1 oct.) │ (4 oct.) │  (chiffré AES)    │
├──────────┴──────────┴──────────┴───────────────────┤
│                                                     │
│  MAGIC  : 0x66 0x73 0x6F 0x63  ("fsoc")            │
│  TYPE   : 0x01 = CMD, 0x02 = RESULT, 0x03 = PING,  │
│           0x04 = PONG, 0x05 = KEYX, 0x06 = AUTH    │
│  LENGTH : taille du payload chiffré                │
│  PAYLOAD: données chiffrées AES-256-GCM            │
│                                                     │
└─────────────────────────────────────────────────────┘

Échange initial :
  1. Implant → C2 : KEYX (clé publique RSA de l'implant)
  2. C2 → Implant : AUTH (clé AES chiffrée avec RSA)
  3. Session établie : tout en AES-256-GCM
```

### 1.5 Arborescence monorepo

```
fsociety/                              # UN SEUL repo Git
├── .gitignore
├── .gitattributes
├── README.md
├── LICENSE
├── Makefile                           # Build racine
│
├── c2/                                # fsociety-c2
│   ├── Makefile
│   ├── include/
│   └── src/
│       ├── main.c
│       ├── server.c / server.h
│       ├── session.c / session.h
│       ├── crypto.c / crypto.h
│       ├── protocol.c / protocol.h
│       ├── commands.c / commands.h
│       ├── ui.c / ui.h
│       └── logger.c / logger.h
│
├── implant/                           # fsociety-implant
│   ├── Makefile
│   ├── include/
│   └── src/
│       ├── main.c
│       ├── comm.c / comm.h
│       ├── crypto.c / crypto.h
│       ├── fivenine.c / fivenine.h    # persistance
│       ├── whiterose.c / whiterose.h  # évasion + syscalls
│       ├── injection.c / injection.h
│       ├── utils.c / utils.h
│       └── modules/
│           ├── mrrobot.c              # shell
│           ├── keylog.c
│           ├── rdp.c
│           ├── crack.c
│           ├── pth.c
│           ├── loot.c
│           ├── phish.c
│           ├── darkarmy.c             # mouvement latéral
│           ├── privsec.c
│           └── stage2.c               # whatever
│
├── loader/                            # fsociety-loader
│   ├── Makefile
│   ├── include/
│   └── src/
│       ├── main.c
│       ├── download.c / download.h
│       ├── inject.c / inject.h
│       ├── evasion.c / evasion.h
│       └── utils.c / utils.h
│
├── common/                            # Code partagé
│   ├── Makefile
│   ├── include/
│   │   ├── protocol.h
│   │   ├── crypto_common.h
│   │   └── types.h
│   └── src/
│       ├── crypto_common.c
│       └── protocol.c
│
├── build/                             # Toolchains & scripts
│   ├── Makefile.linux
│   ├── Makefile.mingw
│   └── toolchain-mingw.cmake
│
├── lab/                               # Environnement de test
│   ├── manifest.md
│   ├── setup-vm.md
│   └── snapshots/
│
├── docs/                              # Documentation
│   ├── install.md
│   ├── usage.md
│   ├── architecture.md
│   ├── MITRE_mapping.md
│   ├── blue_team_notes.md
│   ├── demo_script.md
│   └── troubleshooting.md
│
├── slides/                            # Présentation
│   └── .gitkeep
│
└── scripts/                           # Scripts utilitaires
    ├── build-all.sh
    ├── clean.sh
    └── test-lab.sh
```

### 1.6 Gitflow monorepo

```
main                          # Version stable, démontrable
│
├── develop                   # Intégration continue
│   │
│   ├── feature/c2-core
│   ├── feature/implant-comm
│   ├── feature/loader-evasion
│   ├── feature/module-keylog
│   ├── feature/module-darkarmy
│   ├── feature/whiterose-syscall
│   ├── feature/fivenine-persistence
│   ├── feature/stage2-commands
│   └── feature/docs-mitre
│
├── release/v1.0              # Préparation de la démo
│
└── hotfix/fix-crypto-bug     # Correction urgente
```

**Convention de commits** : `type(scope): message`
- `feat(c2): add multi-session handling`
- `feat(implant): implement fivenine persistence`
- `feat(module): add keylog via SetWindowsHookEx`
- `fix(crypto): correct AES key derivation`
- `docs(mitre): add T1055 mapping`
- `build(makefile): add mingw cross-compile`

---

## 2. Fonctionnalités du projet

### 2.1 Core obligatoire (exigences du sujet)

| # | Fonctionnalité | Description | Module | MITRE ATT&CK |
|---|---|---|---|---|
| 1 | **Exécution furtive** | L'implant doit rester indétectable par l'antivirus du lab (Windows Defender). | `whiterose` | T1027 |
| 2 | **Shell distant** | Spawn d'un shell à distance sur la cible. | `mrrobot` | T1059 |
| 3 | **Tunnel chiffré** | Communication via un protocole existant (HTTP/HTTPS, DNS, etc.). | `comm` | T1071.001 / T1071.004 |
| 4 | **Extraction de credentials** | Vol de credentials depuis la machine compromise (LSASS, SAM). | `elliot` | T1003 |
| 5 | **Persistance** | Survivre à un reboot et redémarrer même si le process est tué. | `fivenine` | T1547.001 / T1053.005 |
| 6 | **Résilience** | Watchdog qui relance l'implant s'il s'arrête. | `killswitch` | T1547 |
| 7 | **Chiffrement des communications** | Chiffrement asymétrique (RSA) pour l'échange de clé, symétrique (AES) pour la session. Prouvable via Wireshark. | `crypto` | T1573 |
| 8 | **Nettoyage des traces** | Rollback des logs système, réseau et commandes (pas de wipe). | `redwheelbarrow` | T1070.001 |

### 2.2 Commandes avancées (build targets du sujet)

| # | Commande | Intent | Module | MITRE ATT&CK | Blue Team Note |
|---|---|---|---|---|---|
| 9 | `keylog` | Démarrer, arrêter ou dumper une capture des frappes clavier. | `keylog.c` | T1056.001 | Hooks clavier suspects, drivers inhabituels. |
| 10 | `rdp` | Activer ou désactiver le service Remote Desktop sur la cible. | `rdp.c` | T1021.001 | Changements de registre RDP, connexions sortantes. |
| 11 | `crack` | Lancer un cracking de hash contre le matériel récupéré. | `crack.c` | T1110.002 | CPU/GPU anormal, wordlists, tentatives échouées. |
| 12 | `pth` | Réutiliser un hash récupéré pour s'authentifier (Pass-the-Hash). | `pth.c` | T1550.002 | Auth NTLM anormales, accès LSASS. |
| 13 | `loot` | Localiser et exfiltrer des fichiers sensibles (password stores, clés SSH, configs). | `loot.c` | T1005 / T1041 | Volume sortant anormal, destinations inhabituelles. |
| 14 | `phish` | Aider à produire un leurre de harvesting de credentials depuis le contexte compromis. | `phish.c` | T1566 | Analyse emails/pièces jointes, sensibilisation. |
| 15 | `propagate` | Se déplacer latéralement vers d'autres machines du réseau. | `darkarmy.c` | T1021 / T1570 | Scans réseau, SMB anormal, processus distants. |
| 16 | `privsec` | Identifier et exploiter une mauvaise configuration ou vulnérabilité locale pour élever les privilèges. | `privsec.c` | T1068 / T1548 | Tokens, UAC bypass, processus élevés. |
| 17 | `syscall` | Invoquer un appel système directement depuis userland pour bypasser les hooks userland. | `whiterose.c` | T1106 | Détection comportementale EDR. |
| 18 | `shell` | Offrir un shell basé sur des commandes built-in pour éviter de spawn de nouveaux processus. | `shell.c` | T1059 | Commandes intégrées suspectes, absence de processus enfants. |
| 19 | `whatever` | Être créatif et surprendre (ex. exfiltration DNS, stéganographie). | `stage2.c` | — | À définir. |

### 2.3 Bonus envisagés

| # | Fonctionnalité | Description | MITRE ATT&CK |
|---|---|---|---|
| 20 | **Screenshot** | Capture d'écran à distance. | T1113 |
| 21 | **Clipboard** | Vol du presse-papiers. | T1115 |
| 22 | **Webcam** | Accès à la caméra. | T1125 |
| 23 | **Port scan** | Découverte des services ouverts sur le réseau interne. | T1046 |
| 24 | **Upload / Download** | Transfert de fichiers entre attaquant et cible. | T1105 |
| 25 | **Domain fronting** | Masquer le trafic C2 via un CDN légitime. | T1090.004 |
| 26 | **DNS tunneling** | Exfiltration via DNS. | T1048.003 |
| 27 | **Anti-VM** | Détection d'environnement d'analyse (VM). | T1497.001 |
| 28 | **Anti-debug** | Détection de débogueur. | T1622 |

### 2.4 Récapitulatif par module Mr. Robot

| Module | Fonctionnalités couvertes |
|---|---|
| **mrrobot** | Shell distant, shell built-in |
| **whiterose** | Évasion, anti-VM, anti-debug, syscalls directs, unhooking |
| **darkarmy** | Mouvement latéral, propagation |
| **fivenine** | Persistance (registre Run, tâche planifiée, service) |
| **killswitch** | Watchdog, résilience |
| **elliot** | Extraction de credentials (LSASS, SAM) |
| **redwheelbarrow** | Nettoyage des traces (rollback logs) |
| **stage2** | Commandes créatives (DNS exfil, stéganographie) |
| **keylog** | Keylogger |
| **rdp** | Activation/désactivation RDP |
| **crack** | Cracking de hash |
| **pth** | Pass-the-Hash |
| **loot** | Exfiltration de fichiers |
| **phish** | Génération de leurre |
| **privsec** | Élévation de privilèges |

---

## 3. Phases du projet

| Phase | Objectif | Livrable |
|---|---|---|
| **Phase 0** | Lab, manifeste, squelette | VM + Makefile + repo Git |
| **Phase 1** | Loader / stager | fsociety-loader.exe |
| **Phase 2** | Implant principal | fsociety-implant.exe |
| **Phase 3** | C2 | fsociety-c2 |
| **Phase 4** | Modules avancés | keylog, darkarmy, etc. |
| **Phase 5** | Nettoyage traces | rollback Event Logs |
| **Phase 6** | Tests & démo | AV, Wireshark, reboot, kill |

---

## 4. Livrables finaux

- [ ] Outil fonctionnel démontré en live sur VM Windows.
- [ ] README complet (installation, usage, ressources, sources).
- [ ] Mapping MITRE ATT&CK + blue team notes pour chaque capacité.
- [ ] Dépôt Git propre (monorepo, 3+ branches, commits cohérents).
- [ ] Code maintenable (pas de duplication, fonctions claires).
- [ ] Présentation avec slides.
- [ ] Manifeste de lab reproductible.

---

## 5. Points de vigilance

- **Légal** : usage strictement limité au lab, consentement écrit.
- **Mémoire C** : `malloc`/`free` rigoureux, pas de fuites, pas de use-after-free.
- **Sécurité C** : `strncpy`, `snprintf`, validation des entrées.
- **Erreurs WinAPI** : toujours vérifier `GetLastError`.
- **Dépendances** : OpenSSL côté C2, custom côté implant.
- **Obfuscation** : strings XOR, stack strings, imports dynamiques.
- **Compilation** : `-O2 -s -Wall -Wextra`, strip des symboles.
- **Furtivité** : tester régulièrement contre Windows Defender.

---

## 6. Définitions des termes techniques

| Terme | Définition | Lien |
|---|---|---|
| **C2** (Command and Control) | Infrastructure centrale utilisée par un attaquant pour communiquer avec les machines compromises. C'est le hub depuis lequel il envoie des commandes et reçoit des données. | [Fortinet](https://www.fortinet.com/fr/resources/cyberglossary/command-and-control-attacks) |
| **Implant** | Programme malveillant déployé sur la machine victime. Il établit la communication avec le C2, exécute les commandes et maintient la persistance. | — |
| **Stager** | Petit morceau de code dont le seul rôle est de se connecter au C2 et de télécharger le payload principal (le stage). | [LabEx](https://labex.io/fr/tutorials/kali-understand-and-use-staged-vs-stageless-payloads-in-metasploit-594038) |
| **Payload** | Charge utile : le code qui effectue réellement l'action malveillante (shell, keylogger, exfiltration). | — |
| **Shellcode** | Code binaire exécutable, historiquement conçu pour lancer un shell. Contraintes : pas d'octets nuls. | [Wikipédia](https://browse.library.kiwix.org/content/wikipedia_fr_all_maxi/Shellcode) |
| **IAT** (Import Address Table) | Table dans un exécutable Windows listant les fonctions importées des DLL. Les analystes la lisent pour savoir quelles API le binaire utilise. | — |
| **Obfuscation de strings** | Cacher les chaînes sensibles (URLs, noms d'API) dans le binaire pour éviter l'analyse statique. | [Malwarebytes](https://blog.malwarebytes.com/cybercrime/2017/05/explained-malware-obfuscation/) |
| **Injection mémoire** | Charger et exécuter du code directement en RAM sans écrire sur disque. | [MITRE T1055](https://attack.mitre.org/techniques/T1055/) |
| **Process injection** | Exécuter du code dans l'espace d'adressage d'un autre processus vivant pour masquer l'exécution. | [MITRE T1055](https://attack.mitre.org/techniques/T1055/) |
| **Unhooking** | Restaurer les bytes originaux des API `ntdll` hookées par l'AV/EDR. | — |
| **Hook** | Interception d'un appel de fonction pour en modifier le comportement (utilisé par les EDR). | — |
| **EDR** (Endpoint Detection and Response) | Solution de sécurité qui surveille le comportement des processus en temps réel. | [CrowdStrike](https://www.crowdstrike.fr/cybersecurity-101/endpoint-security/what-is-edr/) |
| **Syscall direct** | Appeler le noyau Windows directement sans passer par les API userland hookées. | [Outflank](https://outflank.nl/blog/2019/06/19/red-team-tactics-combining-direct-system-calls-and-srdi-to-bypass-av-edr/) |
| **DNS tunneling** | Technique d'exfiltration qui encode des données dans des requêtes DNS pour contourner les firewalls. | — |
| **Chiffrement asymétrique** | Chiffrement à clé publique/privée (RSA, ECC). La clé publique chiffre, la clé privée déchiffre. | — |
| **Chiffrement symétrique** | Même clé pour chiffrer et déchiffrer (AES). Plus rapide, utilisé pour la session. | — |
| **Keylogger** | Logiciel qui enregistre les frappes clavier. | [Norton](https://fr.norton.com/blog/malware/what-is-a-keylogger) |
| **RDP** (Remote Desktop Protocol) | Protocole de bureau à distance de Microsoft. | — |
| **Cracking** | Retrouver un mot de passe en clair à partir de son hash. | — |
| **Pass-the-Hash** | Réutiliser un hash NTLM volé pour s'authentifier sans casser le mot de passe. | [LeMagIT](https://www.lemagit.fr/definition/Quest-ce-quune-attaque-pass-the-hash) |
| **Exfiltration** | Vol et transfert de données hors du réseau de la victime. | — |
| **Mouvement latéral** | Se déplacer d'une machine compromise vers d'autres machines du réseau. | — |
| **Élévation de privilèges** | Passer d'un compte standard à un compte administrateur ou SYSTEM. | — |
| **Rootkit** | Malware conçu pour se cacher profondément dans le système et maintenir un accès persistant. | [Avira](https://www.avira.com/fr/blog/rootkit) |
| **Zero-day** | Faille inconnue de l'éditeur, sans correctif disponible. | [Kaseya](https://www.kaseya.com/fr/blog/what-is-zero-day-vulnerability/) |
| **MITRE ATT&CK** | Base de connaissances recensant les tactiques et techniques des attaquants. | [MITRE](https://attack.mitre.org/) |
| **Gitflow** | Modèle de gestion de branches Git (main, develop, feature, release, hotfix). | [Atlassian](https://www.atlassian.com/fr/git/tutorials/comparing-workflows/gitflow-workflow) |
| **Monorepo** | Pratique qui consiste à regrouper plusieurs projets dans un seul dépôt Git. | [Atlassian](https://www.atlassian.com/fr/git/tutorials/monorepos) |
| **Submodule Git** | Mécanisme Git permettant d'inclure un dépôt dans un autre (non utilisé ici). | [Git docs](https://git-scm.com/book/fr/v2/Utilitaires-Git-Sous-modules) |
| **Use-after-free** | Bug mémoire où un pointeur est utilisé après libération de la mémoire. | — |
| **Wireshark** | Analyseur de paquets réseau. | — |
| **Watchdog** | Mécanisme de surveillance qui relance automatiquement un processus s'il s'arrête. | — |
| **Domain fronting** | Technique utilisant un domaine légitime (CDN) pour masquer le trafic C2. | — |
| **Rollback** | Restauration des logs à un état antérieur plutôt que suppression totale. | — |
| **Wipe** | Suppression complète des logs (très suspect, souvent détecté). | — |
| **Anti-VM** | Techniques pour détecter si le code s'exécute dans une machine virtuelle. | — |
| **Anti-debug** | Techniques pour détecter un débogueur attaché au processus. | — |