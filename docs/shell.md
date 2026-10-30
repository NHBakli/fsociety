# Shell

Ce guide permet de compiler et tester les diagnostics dans son propre laboratoire.
Seuls `hostname`, `os` et `time` sont autorisés ; l'agent refuse les autres actions.
Partir d'un clone du dépôt ; sauf indication contraire, les commandes sont lancées
depuis sa racine. Les trois programmes ont chacun leur `main()` et les Makefiles
existants restent inchangés.

| Rôle | Source |
|---|---|
| Serveur | [server/src/new_server.c](../server/src/new_server.c) |
| Contrôleur | [controller/src/new_ctrl.c](../controller/src/new_ctrl.c) |
| Agent | [implant/src/new_client.c](../implant/src/new_client.c) |
| Fonctions communes | [common/src/new_common.h](../common/src/new_common.h) |

## 1. Installer les outils

Sous Ubuntu/Debian :

```sh
sudo apt update
sudo apt install gcc libssl-dev openssl
```

Sous Windows x64, installer [MSYS2](https://www.msys2.org/), puis ouvrir le terminal
**MSYS2 UCRT64**. Installer GCC et le
[paquet OpenSSL UCRT64](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-openssl) :

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-openssl
```

Dans ce terminal, rejoindre la racine du clone avant de poursuivre.
Les commandes de compilation Windows ci-dessous s'exécutent dans UCRT64.

## 2. Compiler les trois programmes

Sous Ubuntu/Debian :

```sh
mkdir -p build
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Icommon/src server/src/new_server.c -o build/new_server -lssl -lcrypto
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Icommon/src controller/src/new_ctrl.c -o build/new_ctrl -lssl -lcrypto
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Icommon/src implant/src/new_client.c -o build/new_client -lssl -lcrypto
```

Sous Windows, dans UCRT64 :

```sh
mkdir -p build
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Icommon/src server/src/new_server.c -o build/new_server.exe -lssl -lcrypto -lws2_32 -ladvapi32
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Icommon/src controller/src/new_ctrl.c -o build/new_ctrl.exe -lssl -lcrypto -lws2_32 -ladvapi32
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Icommon/src implant/src/new_client.c -o build/new_client.exe -lssl -lcrypto -lws2_32 -ladvapi32
```

Un exécutable Linux ne fonctionne pas sous Windows : compiler pour le système
qui exécutera chaque programme, sur cette machine ou une autre du même système.

## 3. Créer une seule génération de certificats pour le laboratoire

Depuis la racine du clone, sous Linux ou dans UCRT64 :

```sh
(cd build && sh ../scripts/new_make-certs.sh)
```

Le [script](../scripts/new_make-certs.sh) crée `build/certs/`, avec une validité
de 30 jours. Il refuse de remplacer un dossier `certs/` existant.
Lancer cette génération **une seule fois pour tout le laboratoire** :
toutes les machines doivent utiliser la même autorité `ca.crt`.
Générer séparément les certificats sur chaque machine empêcherait leur confiance mutuelle.

Pour distribuer les rôles, copier uniquement les fichiers nécessaires dans un
sous-dossier `certs/` à côté de l'exécutable concerné :

| Programme | Fichiers à placer dans son dossier `certs/` |
|---|---|
| Serveur | `ca.crt`, `server.crt`, `server.key` |
| Contrôleur | `ca.crt`, `controller.crt`, `controller.key` |
| Agent | `ca.crt`, `agent.crt`, `agent.key` |

Ne jamais transférer `ca.key` : cette clé reste sur la machine de génération.
Pour l'essai local suivant, le dossier généré contient déjà tous les rôles.

## 4. Vérifier d'abord sur une seule machine

Ouvrir trois terminaux, chacun avec `build/` comme dossier de travail.
Lancer dans cet ordre, un programme par terminal :

```sh
# Terminal 1
./new_server 127.0.0.1
# Terminal 2
./new_client 127.0.0.1
# Terminal 3
./new_ctrl 127.0.0.1
```

Sous Windows dans UCRT64, utiliser les mêmes commandes avec le suffixe `.exe`.
Dans PowerShell, la syntaxe est `.\new_server.exe 127.0.0.1`,
`.\new_client.exe 127.0.0.1` et `.\new_ctrl.exe 127.0.0.1`.
Pour PowerShell, copier auparavant depuis le dossier `ucrt64/bin/` de son installation
MSYS2 les deux DLL OpenSSL
`libssl-3-x64.dll` et `libcrypto-3-x64.dll` à côté des exécutables ;
UCRT64 permet déjà de les trouver via son environnement.

L'agent doit afficher `Serveur et agent authentifies`.
Dans le contrôleur, choisir `1` (nom de la machine), `2` (système),
`3` (heure UTC), puis `0` pour quitter. Les réponses réussies commencent par
`OK` ; sous Windows, le système est affiché comme `Windows build ...`.
Le résultat décrit la machine de l'agent. Ces programmes ne nécessitent
normalement aucun droit administrateur pour cet essai sur localhost.

## 5. Passer à deux machines : serveur et VM

Choisir une interface de laboratoire joignable entre les deux machines.
Aucune configuration NAT, de partage ou d'hyperviseur particulière n'est supposée.
Relever l'adresse IPv4 du serveur avec `ipconfig` sous Windows ou `ip addr` sous Linux.

L'exemple ci-dessous utilise **192.168.56.1** : remplacer cette adresse partout
par l'adresse réelle du serveur. Celui-ci écoute uniquement sur l'adresse passée :
le contrôleur, même installé sur le serveur, doit donc utiliser cette même adresse.

Sur la machine du serveur, depuis son dossier contenant `certs/` :

```sh
./new_server 192.168.56.1
```

Dans la VM Windows, préparer un dossier contenant `new_client.exe`, les DLL
`libssl-3-x64.dll` et `libcrypto-3-x64.dll`, ainsi que `certs/ca.crt`,
`certs/agent.crt` et `certs/agent.key`. Utiliser les certificats de l'étape 3
et un exécutable Windows compilé à l'étape 2 ; aucun partage permanent n'est requis.
Ouvrir PowerShell dans ce dossier :

```powershell
.\new_client.exe 192.168.56.1
```

Depuis le dossier du contrôleur et de ses propres certificats :

```sh
./new_ctrl 192.168.56.1
```

Adapter les deux dernières commandes hôte à `.\new_server.exe` et
`.\new_ctrl.exe` si l'hôte est sous Windows/PowerShell.
Autoriser si nécessaire TCP **4444** (agent) et **5555** (contrôleur) sur le serveur,
uniquement pour le réseau du laboratoire, en conservant le pare-feu actif.

## 6. Vérifier les résultats et les erreurs

| Essai | Résultat attendu |
|---|---|
| Menu `1` / `hostname` | `OK` suivi du nom de la machine de l'agent |
| Menu `2` / `os` | `OK` suivi du système ; numéro de build sous Windows |
| Menu `3` / `time` | `OK` suivi de la date et de l'heure UTC |
| Fermer l'agent puis demander un diagnostic | Réponse `ERROR` indiquant la déconnexion |
| Arrêter le serveur puis demander un diagnostic | Erreur de connexion dans le contrôleur |
| Certificat absent, ou certificat d'un autre rôle | Démarrage ou authentification refusé |
| Argument inconnu : `./new_ctrl 127.0.0.1 inconnu` | Affichage de l'usage et refus de l'argument |

Utiliser l'adresse réelle et le suffixe `.exe` pour adapter le dernier essai.
Tester les certificats dans une copie du dossier du rôle concerné, puis reprendre
les fichiers corrects pour continuer. Ces résultats sont à relever dans son propre laboratoire.

Sous Ubuntu/WSL, la génération sans remplacement, les trois diagnostics, le refus
d'un argument inconnu et les deux cas de déconnexion ont été vérifiés sur localhost.

## 7. Conserver les observations réseau et processus

Dans Wireshark, capturer sur l'interface de laboratoire pour deux machines.
Commencer la capture avant de lancer l'agent et le contrôleur.
Pour un essai local, choisir l'interface **loopback** ; sous Windows, Npcap fournit
« Adapter for loopback traffic capture ». Voir la
[documentation Wireshark](https://wiki.wireshark.org/CaptureSetup/Loopback).
Filtre d'affichage : `tcp.port == 4444 || tcp.port == 5555`.
Si nécessaire, utiliser « Décoder comme… > TLS » pour ces ports.

Relever les connexions, leur établissement TLS et les paquets `Application Data` :
le contenu des diagnostics est chiffré. Sur chaque machine, relever aussi l'heure
et le PID, par exemple avec le Gestionnaire des tâches ou `Get-Process` sous Windows.
Relever le processus parent avec [Process Explorer](https://learn.microsoft.com/en-us/sysinternals/downloads/process-explorer)
ou un outil système adapté.
Les trois diagnostics fixes ne lancent aucun shell.

Sauvegarder captures d'écran, fichiers `.pcapng` et journaux dans un sous-dossier
daté de [lab/preuves](../lab/preuves/), distinct des preuves déjà présentes :
[agent](../lab/preuves/01-agent-vm.png),
[serveur](../lab/preuves/02-serveur.png) et
[contrôleur](../lab/preuves/03-controleur-resultats.png).

## 8. Résoudre les problèmes courants

- **Certificats introuvables** : le dossier de travail doit contenir `certs/`, avec les fichiers du bon rôle.
- **DLL absente sous Windows** : placer les deux DLL OpenSSL près de l'exécutable, ou lancer depuis UCRT64.
- **Port occupé** : arrêter l'ancienne instance du laboratoire avant de relancer le serveur.
- **Connexion impossible** : vérifier l'IPv4 d'écoute du serveur, l'adresse utilisée par les deux clients et les règles du pare-feu.
- **TLS refusé** : vérifier le rôle du certificat, l'autorité commune et les horloges ; après expiration, créer un laboratoire neuf avec une nouvelle génération, puis redistribuer les fichiers des rôles.
