# fsociety — Manifeste de laboratoire

## 1. Objectif

Ce document décrit l'environnement de test dans lequel fsociety est développé,
compilé et démontré. Il permet de reproduire le lab à l'identique et de
justifier les choix techniques devant le jury.

---

## 2. Vue d'ensemble

| Composant | Rôle | OS | IP |
|---|---|---|---|
| **Attaquant** | C2, compilation, analyse | Linux (Ubuntu) | 192.168.153.1 |
| **Cible** | Implant, loader, tests | Windows 10 22H2 | 192.168.153.128 |

Réseau : **host-only (VMnet1)**, isolé d'Internet.

---

## 3. Machine attaquante

| Élément | Valeur |
|---|---|
| OS | Ubuntu (Linux natif) |
| Interface réseau | `vmnet1` (VMware Host-only) |
| IP | 192.168.153.1 |
| Masque | 255.255.255.0 |
| Outils | gcc, make, mingw-w64, openssl, wireshark, tcpdump, git |
| Rôle | Compilation du C2, hébergement du C2, capture réseau |

---

## 4. Machine cible

| Élément | Valeur |
|---|---|
| OS | Windows 10 22H2 |
| Build | 19045 |
| Architecture | x86_64 |
| VBS | **Activé** |
| HVCI | **Activé** |
| Windows Defender | **Actif** |
| Interface réseau | Ethernet0 (Host-only VMnet1) |
| IP | 192.168.153.128 |
| Masque | 255.255.255.0 |
| Outils | Sysinternals (Procmon, Process Explorer), Wireshark (optionnel) |

---

## 5. Justification des choix

### 5.1 Windows 10 22H2

- Version stable et largement documentée.
- Les numéros de syscall sont connus et stables (build 19045).
- Boot rapide en VM, peu gourmand en ressources.
- Permet de tester la furtivité contre Defender dans des conditions réalistes.

### 5.2 VBS/HVCI activés

- Représente un défi technique plus crédible.
- Force l'implant à s'adapter aux protections modernes.
- Démontre une compréhension des limites du userland.
- La désactivation depuis le C2 est une technique signature détectable → non retenue.

### 5.3 Defender actif

- Le sujet exige que l'implant reste indétectable par l'antivirus du lab.
- Tester contre Defender permet de valider la furtivité réelle.
- Les échecs sont documentés et mitigés.

### 5.4 Réseau host-only (VMnet1)

- Isolation totale d'Internet.
- Aucun risque de fuite hors du lab.
- Conforme aux exigences légales et éthiques du projet.
- Permet la communication hôte ↔ VM pour les tests et la démo.

---

## 6. Configuration réseau

### 6.1 Interfaces

| Machine | Interface | IP | Rôle |
|---|---|---|---|
| PC hôte (Linux) | `vmnet1` | 192.168.153.1 | Host-only (VMware) |
| VM Windows | `Ethernet0` | 192.168.153.128 | Cible |
| PC hôte (Wi-Fi) | `wlo1` | 172.20.10.6 | Réseau physique (hors lab) |
| PC hôte (NAT) | `vmnet8` | 192.168.219.1 | NAT (non utilisé) |

### 6.2 Pare-feu VM Windows

Windows bloque ICMP par défaut. Pour permettre les tests de connectivité
(ping) depuis l'hôte, la règle suivante a été appliquée dans la VM :

```powershell
New-NetFirewallRule -DisplayName "Allow ICMPv4-In" -Protocol ICMPv4 -IcmpType 8 -Action Allow
```

**Justification** : cette règle autorise uniquement les requêtes d'écho
entrantes (ICMPv4 type 8), nécessaires pour valider la connectivité du lab.
Elle ne réduit pas la surface d'attaque de manière significative dans un
environnement isolé host-only.

### 6.3 Vérification

- Depuis l'hôte : `ping 192.168.153.128` → réponse OK.
- Depuis la VM : `ping 192.168.153.1` → réponse OK.

---

## 7. Snapshots

| Nom | Contenu | Utilisation |
|---|---|---|
| `clean` | Windows 10 22H2 fraîchement installé, VBS/HVCI ON, Defender ON, réseau host-only, pare-feu ICMPv4-In autorisé | Point de départ |
| `infected` | Après déploiement de l'implant | Tests de persistance, rollback |
| `demo` | Configuration finale avant la démo | Démonstration live |

**Procédure de restauration** : VMware → Snapshots → Restaurer.

---

## 8. Procédure de reconstruction

1. Créer une VM Windows 10 22H2 (8 CPU, 6 Go RAM, 60 Go disque).
2. Installer Windows, appliquer les mises à jour jusqu'au build 19045.
3. Vérifier VBS/HVCI : `msinfo32` → "Virtualization-based security: Running".
4. Vérifier Defender : Windows Security → Protection active.
5. Configurer la carte réseau en **Host-only (VMnet1)**.
6. Configurer l'IP fixe : `192.168.153.128`, masque `255.255.255.0`.
7. Autoriser ICMPv4-In via PowerShell (voir section 6.2).
8. Vérifier le ping depuis l'hôte Linux (`192.168.153.1`).
9. Créer le snapshot `clean`.

---

## 9. Contraintes légales

- Toutes les attaques sont menées **uniquement** dans ce lab isolé.
- Aucune cible externe n'est visée.
- Consentement explicite et écrit de toutes les parties impliquées.
- Toute utilisation hors de ce cadre est illégale.

---

## 10. Références

- [MITRE ATT&CK](https://attack.mitre.org/)
- [Microsoft WinAPI Docs](https://learn.microsoft.com/en-us/windows/win32/api/)
- [VBS/HVCI Documentation](https://learn.microsoft.com/en-us/windows/security/hardware-security/enable-virtualization-based-protection-of-code-integrity)