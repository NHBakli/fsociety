# Notes d'architecture : Injection de code et furtivité

## 1. Contexte et objectif

Pour répondre aux critères de furtivité exigés par le projet (ne pas apparaître dans le gestionnaire des tâches), l'implant doit cesser d'être un processus autonome visible. Il doit s'exécuter à l'intérieur d'un processus Windows légitime. Cette technique s'appelle l'**Injection de processus** (Process Injection).

L'objectif à terme est d'implémenter le **Process Hollowing**, la technique de référence utilisée par les APT (Advanced Persistent Threats). Cependant, le développement se fait par étapes.

---

## 2. État actuel : Injection basique (CreateRemoteThread)

### 2.1. Principe
L'implant est lancé (via la tâche planifiée `fivenine`). Au démarrage, il vérifie s'il a été lancé avec des arguments. S'il est lancé sans argument (cas d'un démarrage automatique), il cherche un processus légitime déjà en cours d'exécution (par exemple `explorer.exe`), s'y injecte, puis quitte son propre processus.

### 2.2. Mécanisme technique
1. **Recherche du PID** : Utilisation de `CreateToolhelp32Snapshot` et `Process32Next` pour trouver l'ID du processus cible (ex: `explorer.exe`).
2. **Ouverture du processus** : `OpenProcess` avec les droits `PROCESS_ALL_ACCESS`.
3. **Allocation mémoire distante** : `VirtualAllocEx` pour réserver de l'espace dans le processus cible.
4. **Écriture** : `WriteProcessMemory` pour écrire le chemin de l'exécutable dans la mémoire du processus cible.
5. **Création du thread distant** : `CreateRemoteThread` qui exécute `WinExec` (une fonction de `kernel32.dll` déjà chargée dans le processus cible) avec le chemin de l'implant.
6. **Fermeture** : L'implant originel se termine (`return 0`).

### 2.3. Avantages et Limites
- **Avantages** : L'exécution est masquée sous le nom d'un processus légitime. Facile à implémenter.
- **Limites** : 
  - Le processus injecté (l'implant) tourne toujours avec son propre code. Il est donc possible de voir deux processus (le parent et l'enfant) avec des connexions réseau similaires.
  - Cette méthode est **hautement détectée** par les EDR modernes (signature de `CreateRemoteThread` + `WinExec`).

---

## 3. Étape suivante : Process Hollowing (Vraie furtivité)

### 3.1. Principe
Le Process Hollowing (ou "RunPE") consiste à créer un processus légitime (ex: `svchost.exe`), à "vider" sa mémoire (supprimer son code d'origine), puis à écrire le code de l'implant à la place. 
Le processus résultant est **invisible** dans le gestionnaire des tâches (il porte le nom du processus légitime), et son code sur le disque est celui d'origine (rien d'anormal).

### 3.2. Défis techniques
L'implémentation en C pur nécessite de :
1. Lire les en-têtes **PE (Portable Executable)** de l'implant pour comprendre sa structure.
2. Créer le processus cible en mode **Suspendu** (`CREATE_SUSPENDED`).
3. Utiliser des appels API non documentés comme `NtUnmapViewOfSection` pour vider la mémoire du processus cible.
4. Écrire l'image PE de l'implant dans la mémoire allouée (`VirtualAllocEx` + `WriteProcessMemory`).
5. Gérer les **relocalisations** (si l'adresse mémoire n'est pas celle prévue).
6. **Reprendre** l'exécution (`ResumeThread`).

### 3.3. Complexité
C'est un défi de taille (environ 200 à 300 lignes de code C bas niveau). L'équipe doit valider la faisabilité avant de s'y engager, car cela peut consommer beaucoup de temps de développement.

---

## 4. Recommandations pour la suite

1. **Court terme** : Conserver l'injection basique actuelle pour la démo. Elle prouve que le concept fonctionne.
2. **Moyen terme** : Implémenter une version simplifiée du Process Hollowing en utilisant la bibliothèque `ntdll.dll` et en manipulant les structures PE manuellement. 
3. **Alternative** : Utiliser la technique du "Process Doppelgänging" (qui utilise les transactions NTFS), plus moderne mais tout aussi complexe.

**Note pour le jury** : L'équipe est consciente que l'injection par `CreateRemoteThread` est détectable. Le choix du Process Hollowing pour la version finale démontre une volonté d'atteindre un niveau de furtivité réaliste, proche de celui des menaces avancées.