## 📄 `docs/DEBUG_SESSION.md`

# fsociety — Récap de la session de debug

**Date** : 17 septembre 2026
**Objectif** : faire tenir une session chiffrée stable entre le C2 (Linux) et l'implant (Windows VM)

---

## 🎯 Résultat final

Session chiffrée **stable** :
- Handshake RSA + AES-256-GCM OK
- Message chiffré reçu en clair côté implant
- **Plus de déconnexion immédiate**

### Logs de succès

**Serveur Linux :**
```
[+] Client #0 connecté : 192.168.153.128:49711
[*] MSG_KEYX reçu (294 octets DER)
[+] Clé AES chiffrée (256 octets)
[+] MSG_AUTH envoyé (267 octets)
[+] Client #0 : session chiffrée établie
[+] Message chiffré envoyé au client #0
```

**Client Windows :**
```
[+] Session sécurisée active
[*] En attente de commandes du C2...
[C2] type=0x01 seq=0, 24 octets
hello from C2 (chiffré)
```

---

## 🐛 Bugs identifiés et corrigés

### Bug 1 — Short read TCP

**Symptôme** : le client se déconnecte immédiatement après le handshake,
`fso_unpack (header) échoué`, `Lecture en-tête échouée (got=0)`.

**Cause** : `recv()` ne garantit pas de tout recevoir en un seul appel.
Un seul `recv()` peut :
- recevoir **moins** que prévu (fragmentation TCP)
- recevoir **plus** que prévu (coalescence de plusieurs paquets)

Résultat : désynchronisation complète du flux.

**Correction** : ajout de deux fonctions qui bouclent jusqu'à obtenir
exactement N octets :

- `fso_server_recv_exact(srv, idx, buf, len)` → `c2/src/server.c`
- `fso_conn_recv_exact(conn, buf, len)`     → `implant/src/comm.c`

Utilisées partout à la place des `recv()` nus :
- `fso_server_recv_secure`  (serveur)
- `fso_handshake`           (serveur)
- `fso_conn_recv_secure`    (client)
- `recv_auth`               (client)

### Bug 2 — `fso_unpack` exige header + payload entier

**Symptôme** : `fso_unpack (header) échoué` alors que les 11 octets reçus
étaient corrects.

**Cause** : `fso_unpack()` contenait la vérification :

    if (in_size < FSO_HEADER_SIZE + length) return -1;

Donc on ne pouvait pas lui passer **juste** les 11 octets d'en-tête
pour connaître la taille du payload.

**Correction** : ajout d'une fonction dédiée `fso_unpack_header()` qui
parse uniquement l'en-tête sans exiger le payload.

- Déclarée dans `common/include/protocol.h`
- Implémentée dans `common/src/protocol.c`
- Utilisée dans `c2/src/server.c`, `c2/src/handshake.c`,
  `implant/src/comm.c`, `implant/src/main.c`

### Bug 3 — `fso_handshake` faisait un seul `recv()`

**Cause** : la fonction lisait 2048 octets d'un coup, pouvant :
- rater une partie du `MSG_KEYX`
- avaler un paquet suivant (ex : `MSG_AUTH` du client suivant dans une
  session multi-clients)

**Correction** : refactor pour lire exactement :
1. 11 octets d'en-tête (`recv_exact`)
2. `header.length` octets de payload (`recv_exact`)

### Bug 4 — Texte parasite dans `protocol.h`

**Symptôme** : erreur de compilation.

**Cause** : un copier-coller raté avait laissé :

    } fso_cmd_t;if (in_size < FSO_HEADER_SIZE + length) return -1; a

après la définition de `fso_cmd_t`.

**Correction** : suppression du texte parasite.

---

## 📁 Fichiers modifiés

| Fichier | Modification |
|---|---|
| `common/include/protocol.h` | + prototype `fso_unpack_header`, nettoyage parasite |
| `common/src/protocol.c` | + implémentation `fso_unpack_header` |
| `c2/include/server.h` | + prototype `fso_server_recv_exact` |
| `c2/src/server.c` | + `fso_server_recv_exact`, `recv_secure` refactoré |
| `c2/src/handshake.c` | refactor complet avec `recv_exact` + `unpack_header` |
| `implant/include/comm.h` | + prototype `fso_conn_recv_exact` |
| `implant/src/comm.c` | + `fso_conn_recv_exact`, `recv_secure` refactoré |
| `implant/src/main.c` | `recv_auth` refactoré avec `recv_exact` + `unpack_header` |

---

## 🛠️ Build

### Serveur C2 (Linux)

```bash
cd ~/Documents/dev/fsociety/c2
rm -f *.o fsociety-c2
make
```

### Implant (cross-compile Windows depuis Linux)

`implant/Makefile` :

```makefile
CC      = x86_64-w64-mingw32-gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 \
          -Iinclude -I../common/include \
          -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN
LDFLAGS = -lws2_32 -lbcrypt -lcrypt32 -luser32

SRCS = $(wildcard src/*.c) ../common/src/protocol.c
OBJS = $(patsubst src/%.c,build/%.o,$(wildcard src/*.c)) \
       build/common_protocol.o

all: fsociety-implant.exe

fsociety-implant.exe: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/common_protocol.o: ../common/src/protocol.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build fsociety-implant.exe

.PHONY: all clean
```

Compilation :

```bash
cd ~/Documents/dev/fsociety/implant
make
```

Le `.exe` est généré dans `implant/fsociety-implant.exe`.

**Important** : le fichier `../common/src/protocol.c` **doit** être inclus
dans le build pour résoudre les symboles `fso_pack` / `fso_unpack` /
`fso_unpack_header`.

---

## 🚀 Test

1. **Linux** :
   ```bash
   cd ~/Documents/dev/fsociety/c2
   ./fsociety-c2
   ```

2. **Windows** (dans la VM, après avoir copié le `.exe`) :
   ```powershell
   cd $env:TEMP
   .\fsociety-implant.exe 192.168.153.1 8443
   ```

**Adresse IP** : `192.168.153.1` correspond à `vmnet1` (réseau host-only
VMware). Vérifier avec `ip a` ou `ifconfig` côté Linux.

---

## ✅ État actuel

- [x] Handshake RSA + AES-256-GCM fonctionnel
- [x] Session stable (pas de déconnexion)
- [x] Envoi d'un message chiffré serveur → client
- [ ] Boucle interactive (stdin → MSG_CMD, MSG_CMD → MSG_RESULT)
- [ ] Keepalive (MSG_PING / MSG_PONG)
- [ ] Multi-clients (sélection du client actif)
- [ ] Modules (shell, keylog, etc.)

---

## 📚 Leçon retenue

**Règle d'or en programmation socket TCP** :

> `recv()` ne garantit **jamais** de tout recevoir en un seul appel.
> Il faut toujours boucler jusqu'à avoir lu exactement le nombre
> d'octets attendu (header puis payload).

Corollaire : toujours vérifier la **taille attendue** avant de parser.
Un protocole réseau sérieux (HTTP/2, WebSocket, TLS, SSH…) utilise
toujours un **framing** explicite : `[header fixe][length][payload]`,
et lit chaque partie en mode exact.
