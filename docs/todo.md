# fsociety — TODO

Roadmap du projet C2. Cochée = fait, vide = à faire.

---

## ✅ Fait

- [x] Handshake RSA-2048 + AES-256-GCM
- [x] Framing TCP correct (lecture exacte header + payload)
- [x] Session stable (plus de déconnexion immédiate)
- [x] Keepalive ping/pong (30 s / timeout 60 s)
- [x] Cross-compilation implant Windows depuis Linux (MinGW)
- [x] Makefile pour `c2/` et `implant/`

---

## 🎯 Priorité 1 — Boucle interactive (shell)

### Côté serveur (`c2/src/main.c`)
- [ ] Ajouter `STDIN_FILENO` dans le `select()` principal
- [ ] Variable `active_client` (défaut : premier client connecté)
- [ ] Lire une ligne depuis stdin avec `fgets()`
- [ ] Envoyer la ligne via `fso_server_send_secure(MSG_CMD, ...)`
- [ ] Gérer les commandes locales :
    - [ ] `/list` — liste les clients connectés
    - [ ] `/use N` — sélectionner le client N
    - [ ] `/quit` — quitter proprement
    - [ ] `/help` — afficher l'aide
- [ ] Afficher un prompt `c2> ` quand un client est actif
- [ ] Ignorer les lignes vides

### Côté implant (`implant/src/main.c`)
- [ ] Dans `case MSG_CMD:` : parser le payload (cmd_id + args)
- [ ] Exécuter avec `_popen(cmd, "r")` (Windows)
- [ ] Lire la sortie dans un buffer
- [ ] Envoyer le résultat via `fso_conn_send_secure(MSG_RESULT, ...)`
- [ ] Gérer les erreurs d'exécution (`_popen` retourne NULL)
- [ ] Limiter la taille du résultat (chunking si > 64 Ko)

### Format des messages
- [ ] Définir le format de `MSG_CMD` : `[cmd_id 1B][args...]`
- [ ] Définir le format de `MSG_RESULT` : `[cmd_id 1B][status 1B][output...]`
- [ ] Utiliser `fso_build_cmd()` et `fso_build_result()` (déjà dans protocol.c)

---

## 🎯 Priorité 2 — Reconnexion automatique

### Côté implant
- [ ] Appeler `fso_conn_reconnect()` quand `fso_conn_recv_secure` échoue
- [ ] Reprendre le handshake complet après reconnexion
- [ ] Backoff exponentiel : 5s → 10s → 20s → ... → 300s max
- [ ] Persister l'état de session (aes_key) entre reconnexions ? (optionnel)

### Côté serveur
- [ ] Nettoyer proprement les clients déconnectés
- [ ] Réutiliser les slots libres (`FSO_MAX_CLIENTS`)

---

## 🎯 Priorité 3 — Multi-clients

### Côté serveur
- [ ] Variable `active_client` gérée dynamiquement
- [ ] Commande `/list` : afficher `#0  192.168.153.128  (session OK)`
- [ ] Commande `/use N` : sélectionner un client
- [ ] Commande `/broadcast <cmd>` : envoyer à tous
- [ ] Commande `/kill N` : déconnecter un client
- [ ] Afficher `[client #N]` avant chaque réponse
- [ ] Timestamp sur les logs

### Côté implant
- [ ] Rien à faire (déjà prêt)

---

## 🎯 Priorité 4 — Transfert de fichiers

### Protocole
- [ ] Utiliser `MSG_CHUNK` (déjà défini dans protocol.h)
- [ ] Format chunk : `[transfer_id 4B][chunk_index 4B][total_chunks 4B][data_len 2B][data...]`
- [ ] Ajouter un `MSG_CHUNK_ACK` pour la fiabilité

### Commandes
- [ ] `upload <local> <remote>` — envoyer un fichier au client
- [ ] `download <remote> <local>` — récupérer un fichier du client
- [ ] Afficher la progression (`[####----] 40%`)
- [ ] Vérifier l'intégrité (hash SHA-256 en fin de transfert)

### Côté implant (Windows)
- [ ] `fopen()` / `fread()` / `fwrite()` pour lire/écrire
- [ ] Créer les dossiers si nécessaire

---

## 🎯 Priorité 5 — Modules offensifs

### Keylogger (`CMD_KEYLOG_*`)
- [ ] `CMD_KEYLOG_START` : installer un hook clavier (`SetWindowsHookEx`)
- [ ] `CMD_KEYLOG_STOP` : retirer le hook
- [ ] `CMD_KEYLOG_DUMP` : renvoyer le buffer des touches capturées
- [ ] Buffer persistant (fichier dans `%APPDATA%`)

### Capture d'écran
- [ ] `CMD_SCREENSHOT` : capturer l'écran (`BitBlt` + `GDI+`)
- [ ] Encoder en PNG ou JPEG
- [ ] Envoyer via chunking

### Persistance (`CMD_PERSIST`)
- [ ] Registre : `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- [ ] Tâche planifiée (`schtasks`)
- [ ] Dossier Startup
- [ ] Service Windows (plus avancé)

### Reconnaissance (`CMD_SYSCALL` ?)
- [ ] Infos système : hostname, OS, user, IP publique
- [ ] Processus en cours (`EnumProcesses`)
- [ ] Liste des utilisateurs connectés

### Élévation de privilèges (`CMD_PRIVSEC`)
- [ ] Vérifier les privilèges actuels (`SeDebugPrivilege`, etc.)
- [ ] UAC bypass (si pertinent)
- [ ] Token manipulation

---

## 🎯 Priorité 6 — Évasion / Furtivité (avancé)

### Communication
- [ ] Chiffrer les métadonnées (taille des paquets, timing)
- [ ] Canal HTTPS au lieu de TCP brut
- [ ] Fallback DNS / ICMP si TCP bloqué
- [ ] Malleable C2 profiles (style Cobalt Strike)

### Binaire
- [ ] Chiffrement / packer du binaire
- [ ] Obfuscation (OLLVM, junk code)
- [ ] Anti-debug (IsDebuggerPresent, NtQueryInformationProcess)
- [ ] Anti-VM (détection VMware, VirtualBox)

### Injection
- [ ] Injection dans un process légitime (`CreateRemoteThread`, APC)
- [ ] Process hollowing
- [ ] DLL sideloading

### Évasion EDR/AV
- [ ] AMSI bypass (patch `AmsiScanBuffer`)
- [ ] ETW patching (`EtwEventWrite`)
- [ ] Unhooking ntdll
- [ ] Direct syscalls (Hell's Gate, Halo's Gate)

---

## 🎯 Priorité 7 — Qualité de vie

### Logs
- [ ] Niveaux de log (DEBUG, INFO, WARN, ERROR)
- [ ] Fichier de log persistant (`c2.log`)
- [ ] Rotation des logs
- [ ] Timestamps précis

### Interface
- [ ] Couleurs ANSI dans le terminal
- [ ] Menu interactif au démarrage
- [ ] Historique des commandes (`readline`)

### Robustesse
- [ ] Gestion propre des signaux (`SIGPIPE`, `SIGINT`, `SIGTERM`)
- [ ] Timeout sur chaque opération
- [ ] Limite de taille sur chaque buffer
- [ ] Validation stricte des entrées

### Tests
- [ ] Test unitaire du protocole (`fso_pack` / `fso_unpack`)
- [ ] Test du framing (envoi de gros paquets fragmentés)
- [ ] Test de fuzz sur le parser
- [ ] CI (GitHub Actions) pour compiler sur Linux + Windows

---

## 🎯 Priorité 8 — Documentation

- [ ] `README.md` : description, build, usage
- [ ] `docs/PROTOCOL.md` : spécification du protocole (types de messages, formats)
- [ ] `docs/ARCHITECTURE.md` : vue d'ensemble (C2, implant, common)
- [ ] `docs/DEBUG_SESSION.md` : récap du debug (déjà fait)
- [ ] Diagramme de séquence du handshake (PlantUML ou ASCII)

---

## 🐛 Bugs connus / dette technique

- [ ] `implant/src/rsa_der.c` : `der_tlv_size` définie mais non utilisée (warning)
- [ ] `#pragma comment(lib, ...)` ignoré par MinGW (cosmétique)
- [ ] `server.c` : `fso_unpack` → `fso_unpack_header` (corrigé, vérifier les autres usages)
- [ ] Vérifier tous les `fso_unpack()` restants dans le code — ils exigent header + payload entier
- [ ] Ajouter `-Werror` partout pour éviter les warnings silencieux
- [ ] `crypto_common.c` (OpenSSL) : messages d'erreur parfois faux (ligne `return FSO_CRYPTO_ERR_ENCRYPT; /* ou DECRYPT */`)

---

## 📅 Ordre d'attaque recommandé

1. **Boucle interactive** (le plus utile, ~1 h)
2. **Reconnexion auto** (~30 min)
3. **Multi-clients** (~1 h)
4. **Upload / Download** (~2 h)
5. **Info système + keylogger** (~1 jour)
6. **Persistance** (~1 jour)
7. **Le reste** (furtivité, CI, doc...)

---

## 🎯 Prochaine session

**Objectif** : boucle interactive minimale
**Fichiers à modifier** : `c2/src/main.c`, `implant/src/main.c`
**Test** : `whoami` tapé dans le serveur → affiché dans le serveur après aller-retour chiffré