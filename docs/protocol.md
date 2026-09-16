# Protocole fsociety

## 1. Objectif

Définir le format exact des échanges entre le C2 (Linux) et l'implant
(Windows), ainsi que le flux d'établissement de session.

---

## 2. Format des paquets

┌──────────┬──────────┬──────────┬──────────┬───────────────────┐
│  MAGIC   │  TYPE    │  SEQ_ID  │  LENGTH  │    PAYLOAD        │
│ (4 oct.) │ (1 oct.) │ (2 oct.) │ (4 oct.) │  (chiffré AES)    │
└──────────┴──────────┴──────────┴──────────┴───────────────────┘

| Champ | Taille | Description |
|---|---|---|
| **MAGIC** | 4 octets | `0x66 0x73 0x6F 0x63` ("fsoc") |
| **TYPE** | 1 octet | Type de message |
| **SEQ_ID** | 2 octets | Numéro de séquence (big-endian) |
| **LENGTH** | 4 octets | Taille du payload (big-endian) |
| **PAYLOAD** | variable | Données chiffrées AES-256-GCM |

**Taille max** : 1 Mo par paquet.

---

## 3. Types de messages

| Type | Valeur | Direction | Description |
|---|---|---|---|
| `MSG_CMD` | `0x01` | C2 → Implant | Commande |
| `MSG_RESULT` | `0x02` | Implant → C2 | Résultat |
| `MSG_PING` | `0x03` | C2 → Implant | Keepalive |
| `MSG_PONG` | `0x04` | Implant → C2 | Réponse keepalive |
| `MSG_KEYX` | `0x05` | Implant → C2 | Échange de clé |
| `MSG_AUTH` | `0x06` | C2 → Implant | Auth + clé AES |
| `MSG_ERROR` | `0x07` | Les deux | Erreur |
| `MSG_CHUNK` | `0x08` | Les deux | Morceau de fichier |

---

## 4. Flux d'établissement de session

Implant                              C2
│                                  │
│──── MSG_KEYX (clé publique) ────▶│
│                                  │
│◀─── MSG_AUTH (clé AES chiffrée) ──│
│                                  │
│──── MSG_PING ───────────────────▶│
│                                  │
│◀─── MSG_PONG ─────────────────────│
│                                  │
│      Session établie              │
│      (tout en AES-256-GCM)        │
│                                  │

---

## 5. Multi-commandes

Chaque paquet porte un `seq_id` (2 octets). Le C2 peut envoyer plusieurs
commandes en parallèle, l'implant répond avec le même `seq_id`. Le C2
associe les résultats aux commandes par ce numéro.

---

## 6. Keepalive

- **C2 → Implant** : `MSG_PING` toutes les 30 secondes.
- **Implant → C2** : `MSG_PONG` immédiat.
- Si le C2 ne reçoit pas de `PONG` pendant 60 s → session considérée morte.

---

## 7. Reconnexion

Si la session tombe, l'implant retente automatiquement :
- Délai initial : 5 s.
- Backoff exponentiel : ×2 à chaque échec.
- Plafond : 300 s (5 min).
- Reset du backoff après reconnexion réussie.

---

## 8. Gros fichiers (chunks)

Pour les fichiers > 1 Mo, découpage en chunks de 512 Ko max.

Payload d'un chunk :

┌──────────────┬──────────────┬──────────────┬──────────────┬──────────┐
│ TRANSFER_ID  │ CHUNK_INDEX  │ TOTAL_CHUNKS │ CHUNK_SIZE   │  DATA    │
│ (4 oct.)     │ (4 oct.)     │ (4 oct.)     │ (2 oct.)     │ variable │
└──────────────┴──────────────┴──────────────┴──────────────┴──────────┘

| Champ | Taille | Description |
|---|---|---|
| **TRANSFER_ID** | 4 octets | Identifiant unique du transfert |
| **CHUNK_INDEX** | 4 octets | Numéro du chunk (0-based) |
| **TOTAL_CHUNKS** | 4 octets | Nombre total de chunks |
| **CHUNK_SIZE** | 2 octets | Taille de ce chunk |
| **DATA** | variable | Données brutes |

---

## 9. Payload d'une commande

┌──────────────┬─────────────────────────────┐
│  CMD_ID      │  ARGS                       │
│  (1 octet)   │  (variable, UTF-8)          │
└──────────────┴─────────────────────────────┘

---

## 10. Payload d'un résultat

┌──────────────┬──────────────┬─────────────────┐
│  CMD_ID      │  STATUS      │  OUTPUT         │
│  (1 octet)   │  (1 octet)   │  (variable)     │
└──────────────┴──────────────┴─────────────────┘

---

## 11. Identifiants de commandes

| CMD_ID | Commande | Module |
|---|---|---|
| `0x01` | shell | mrrobot |
| `0x02` | keylog_start | keylog |
| `0x03` | keylog_stop | keylog |
| `0x04` | keylog_dump | keylog |
| `0x05` | rdp_enable | rdp |
| `0x06` | rdp_disable | rdp |
| `0x07` | crack | crack |
| `0x08` | pth | pth |
| `0x09` | loot | loot |
| `0x0A` | phish | phish |
| `0x0B` | propagate | darkarmy |
| `0x0C` | privsec | privsec |
| `0x0D` | syscall | whiterose |
| `0x0E` | shell_builtin | mrrobot |
| `0x0F` | cleanup | redwheelbarrow |
| `0x10` | persist | fivenine |
| `0x11` | whatever | stage2 |

---

## 12. Chiffrement

### 12.1 Échange de clé

- **RSA-2048**.
- L'implant génère une paire au premier lancement.
- Le C2 reçoit la clé publique, génère une clé AES-256, la chiffre
  avec RSA, l'envoie.

### 12.2 Session

- **AES-256-GCM**.
- **Clé** : 32 octets.
- **IV** : 12 octets (aléatoire par paquet).
- **Tag** : 16 octets.

Format du payload chiffré :

┌──────────┬──────────────────┬──────────┐
│  IV      │  CIPHERTEXT      │  TAG     │
│ (12 oct.)│  (variable)      │ (16 oct.)│
└──────────┴──────────────────┴──────────┘

---

## 13. Sérialisation

- **Endianness** : big-endian.
- **Encodage** : UTF-8.
- **Taille max** : 1 Mo par paquet.
- **Taille max chunk** : 512 Ko.

---

## 14. Gestion des erreurs

| Code | Signification |
|---|---|
| `0x00` | Succès |
| `0x01` | Commande inconnue |
| `0x02` | Erreur d'exécution |
| `0x03` | Erreur de déchiffrement |
| `0x04` | Timeout |
| `0x05` | Erreur réseau |
| `0x06` | Payload invalide |
| `0x07` | Erreur mémoire |
| `0x08` | Erreur de chunk |

---

## 15. Points de vigilance

- **Endianness** : toujours big-endian.
- **Validation** : vérifier MAGIC, TYPE, LENGTH.
- **Taille max** : rejeter > 1 Mo.
- **Timeout** : 60 s.
- **Retry** : 3 tentatives.
  - **Backoff** : 5 s → 300 s.