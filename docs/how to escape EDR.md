## 1. Objectif

Permettre à l'implant fsociety de s'exécuter et de mener ses actions sans déclencher les solutions de détection endpoint (EDR) déployées sur la cible.

---

## 2. Comprendre l'EDR

Un EDR surveille en temps réel :
- Les appels API (userland et kernel),
- Les créations de processus et leurs chaînes parent-enfant,
- Les injections mémoire,
- Les accès aux ressources sensibles (LSASS, registre, etc.),
- Les connexions réseau,
- Les modifications de fichiers et de registre.

Il ne se contente pas de regarder *ce qui* est appelé, mais *comment* et *dans quel contexte*.

---

## 3. Architecture d'exécution hiérarchique

Principe : placer les opérations à haut risque dans des sous-processus ou des processus délégués, tandis que l'agent principal reste simple et inoffensif.

### Niveau 1 — Agent principal

- Rôle : maintenir le canal C2, heartbeat, coordination.
- API utilisées : ordinaires (nom d'utilisateur, infos système, réseau).
- Ne contient aucun shellcode.
- Ne fait rien de malveillant directement.

### Niveau 2 — Sous-module

- Rôle : exécuter les commandes risquées (shell, keylog, etc.).
- Méthode : fork d'un processus enfant, transfert de la tâche.
- Chaîne parent-enfant propre.
- Durée de vie courte.

### Niveau 3 — Délégation

- Rôle : opérations les plus dangereuses (credentials, persistance, keylogger).
- Méthode : injection dans un processus Windows légitime (`rundll32.exe`, `svchost.exe`).
- Les indicateurs pointent vers le processus légitime, pas vers l'implant.

---

## 4. Techniques de furtivité

### 4.1 Obfuscation statique

- Strings XOR ou stack strings,
- Résolution dynamique d'imports (`LoadLibrary` + `GetProcAddress`),
- Pas d'imports suspects dans la IAT,
- Strip des symboles à la compilation,
- Compilation `-O2 -s`.

### 4.2 Évasion comportementale

- Éviter les API surveillées (`CreateRemoteThread`, `VirtualAllocEx`, `WriteProcessMemory`),
- Utiliser des alternatives (`NtCreateThreadEx`, `QueueUserAPC`, `SetThreadContext`),
- Syscalls directs pour les opérations sensibles,
- Unhooking des API `ntdll` avant usage.

### 4.3 Vérification de l'environnement

- Vérifier IP publique, nom d'utilisateur, nom d'hôte, nom de domaine,
- Auto-suppression si la vérification échoue,
- Date d'autodestruction,
- Détection de services cloud et d'outils d'analyse.

### 4.4 Timing

- Éviter les actions en dehors des heures de travail,
- Espacer les commandes,
- Éviter les pics d'activité.

---

## 5. Ce qu'il faut éviter

| Piège | Raison |
|---|---|
| Appeler `CreateRemoteThread` directement | Signature connue |
| Utiliser PowerShell sans raison | Tracé, surveillé |
| Écrire sur disque | Artefact persistant |
| Communiquer en clair | Détectable par IDS |
| Agir immédiatement après le déploiement | Suspect |
| Utiliser des outils connus (Mimikatz, etc.) | Signatures |

---

## 6. Validation

- Test contre Windows Defender en lab,
- Test contre un EDR gratuit (ex. Sysmon + règles),
- Analyse des logs générés,
- Documentation des échecs et mitigations.