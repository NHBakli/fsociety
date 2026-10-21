# Tests du laboratoire

Trois programmes : `client.c` (agent), `server.c` (relais), `new_ctrl.c`
(contrôleur). `common.h` regroupe les fonctions TLS et réseau partagées.
Les diagnostics sont directement dans `client.c`. `test-mel` reste inchangé.

Le Makefile compile les programmes ; `make-certs.sh` prépare les certificats.
Il n'y a plus de fichiers de diagnostics séparés ni de script de tests Python.

## 1. Ouvrir Ubuntu/WSL

Dans PowerShell :

```powershell
wsl -d Ubuntu
```

Puis, dans Ubuntu :

```sh
cd /mnt/c/Users/foupo/Desktop/Projet_virology/fsociety/new_shell
```

## 2. Installer les outils une première fois

```sh
sudo apt update
sudo apt install build-essential libssl-dev openssl
```

## 3. Compiler et préparer les certificats

```sh
make
make certs
```

Si `certs/` existe déjà avec des certificats valides, lancer uniquement `make`.
Le script refuse de remplacer des certificats existants ; leur validité est
de 30 jours. Clés et exécutables sont ignorés par Git.

## 4. Démarrer le relais

Dans un premier terminal Ubuntu, depuis `new_shell` :

```sh
./server
```

Attendu : serveur en écoute sur `127.0.0.1`, ports 4444 et 5555.

## 5. Démarrer l'agent

Ouvrir un deuxième terminal Ubuntu, revenir dans le même dossier et lancer :

```sh
./client
```

Attendu : `Serveur et agent authentifies`.

## 6. Tester les trois diagnostics

Dans un troisième terminal Ubuntu, depuis le même dossier :

```sh
./new_ctrl
```

Choisir `1`, `2` et `3` successivement. Attendu : nom de la machine,
nom/version du système, puis heure UTC. Chaque réponse commence par `OK`.
Choisir `0` pour quitter le menu.

## 7. Vérifier le refus d'une action inconnue

Le contrôleur accepte uniquement les trois diagnostics. Par exemple :

```sh
./new_ctrl 127.0.0.1 id
```

Attendu : message d'usage et code d'échec, sans demande envoyée.

Pour tester aussi le refus du relais, envoyer un diagnostic inconnu avec
le certificat du contrôleur :

```sh
printf 'id\n' | openssl s_client -quiet -connect 127.0.0.1:5555 -CAfile certs/ca.crt -cert certs/controller.crt -key certs/controller.key
```

Attendu : `READY`, puis `REFUSED diagnostic inconnu`.
Le contrôle du catalogue est également présent dans l'agent : aucun texte
reçu n'est exécuté par un shell.

## 8. Vérifier une déconnexion

Dans le terminal de l'agent, appuyer sur Ctrl+C. Dans celui du contrôleur :

```sh
./new_ctrl 127.0.0.1 hostname
```

Attendu : `ERROR agent deconnecte` ou une erreur de connexion/délai.
Relancer `./client`, puis répéter : le résultat doit à nouveau commencer par `OK`.

## 9. Vérifier le refus d'authentification

Garder le relais actif. Se connecter sans certificat client :

```sh
openssl s_client -quiet -connect 127.0.0.1:5555 -CAfile certs/ca.crt </dev/null
```

Attendu : alerte TLS et refus dans la console du relais, sans résultat de
diagnostic. Aucun mot de passe ni secret n'est inscrit dans le code.

## 10. Conserver les observations

Noter les résultats et les heures UTC des consoles. Dans Wireshark, capturer
le loopback avec `tcp.port == 4444 || tcp.port == 5555` : les diagnostics et
résultats sont chiffrés après la négociation TLS. Les diagnostics lisent des
informations système directement et ne créent aucun processus enfant.

Cette procédure vérifie Linux/WSL. La compilation et les observations sur la
VM Windows du ticket restent à valider. Le code contient les branches Windows,
mais un exécutable compilé sous WSL est un programme Linux.

## Repères pour la suite

Le relais traite un seul agent et un diagnostic à la fois. Les délais limitent
l'inactivité réseau par opération : 5 secondes sur le relais, 10 sur le contrôleur.
Une coupure silencieuse au repos est détectée lors du prochain diagnostic.
Relancer manuellement l'agent après une déconnexion.

Pour une future démo sur plusieurs machines du lab, passer l'IPv4 du relais
à chaque programme. Distribuer seulement `ca.crt` et le certificat/la clé du
rôle concerné : `agent`, `server` ou `controller`. Conserver `ca.key` sur la
machine de préparation.
