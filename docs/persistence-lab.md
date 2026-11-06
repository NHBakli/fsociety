# Étude de la persistance dans le laboratoire — issue #4

Cette étude porte sur un **programme de démonstration autonome**, sans accès
réseau, sans collecte de données et sans modification des paramètres de
démarrage de Windows. Elle sert à observer ce qui se passe lorsqu'un programme
autorisé est lancé à l'ouverture de session dans la VM du laboratoire. Elle ne
concerne pas l'agent du projet.

## Mécanismes étudiés

| Mécanisme Windows | Moment du lancement | Observation possible |
| --- | --- | --- |
| Dossier « Démarrage » | Ouverture de session de l'utilisateur | Entrée visible dans l'Explorateur et dans Autoruns. |
| Clé `Run` | Ouverture de session, à chaque fois | Entrée visible dans le Registre et dans Autoruns. |
| Tâche planifiée | Déclencheur configuré, par exemple ouverture de session | Tâche visible dans le Planificateur de tâches et dans Autoruns. |
| Service automatique | Démarrage du système, selon sa configuration | Service visible dans la console Services et dans Autoruns. |

Ces mécanismes ont des conditions de déclenchement différentes. **Redémarrer
avec Windows** et **redémarrer après l'arrêt inattendu d'un processus** sont
deux comportements distincts : un simple lancement à l'ouverture de session
ne démontre pas une récupération après arrêt du processus. Les services et
les tâches peuvent avoir des politiques de récupération distinctes, qui
doivent être évaluées séparément.

Sources Microsoft : [clés Run et RunOnce](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys),
[Planificateur de tâches](https://learn.microsoft.com/en-us/windows/win32/taskschd/about-the-task-scheduler),
[services Windows](https://learn.microsoft.com/en-us/windows/win32/services/about-services)
et [Autoruns](https://learn.microsoft.com/en-us/sysinternals/downloads/autoruns/).

## Programme témoin

[`implant/tests/persistence_demo.c`](../implant/tests/persistence_demo.c) est un
programme Windows qui écrit une ligne avec la date, l'heure et son identifiant
de processus dans `fsociety-persistence-demo.log`, dans le dossier temporaire
de l'utilisateur. Il affiche le chemin exact du fichier lorsqu'il est lancé
manuellement. Il se termine immédiatement après l'écriture. Il ne crée aucune
entrée de démarrage automatique.

Compilation sur Linux avec MinGW-w64 :

```sh
x86_64-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
    implant/tests/persistence_demo.c -o /tmp/fsociety-persistence-marker.exe
```

Le fichier `.exe` obtenu est destiné uniquement à la VM Windows du
laboratoire. Avant l'essai, ouvrir le code source et vérifier qu'il ne fait
que l'écriture décrite ci-dessus.

## Protocole de vérification dans la VM

1. Partir du snapshot propre du laboratoire et noter la date, l'heure, le
   compte utilisé et l'état initial des entrées de démarrage dans Autoruns.
2. Exécuter une fois le programme témoin manuellement. Vérifier qu'une ligne
   apparaît dans le fichier dont il affiche le chemin.
3. Configurer dans la VM un lancement visible du **programme témoin seulement**
   à l'ouverture de session, avec un mécanisme Windows autorisé pour le test.
   Noter le nom de l'entrée et son emplacement afin de pouvoir la retirer.
4. Redémarrer la VM et ouvrir la même session. Vérifier qu'une nouvelle ligne,
   avec une heure et un identifiant de processus différents, apparaît dans le
   fichier. Comparer aussi l'entrée observée dans Autoruns avant et après.
5. Si une politique de récupération après arrêt inattendu est étudiée, la
   tester séparément et noter explicitement ses résultats. La vérification
   après redémarrage de la VM ne suffit pas à prouver cette propriété.

Le résultat du redémarrage doit être relevé **sur la VM**. Une compilation
croisée réussie sur Linux ne prouve ni le lancement automatique ni le
comportement après redémarrage.

### Fiche de résultats

| Observation | Valeur à relever dans la VM |
| --- | --- |
| Snapshot initial et date du test | À compléter |
| Mécanisme et nom de l'entrée de démonstration | À compléter |
| Première ligne du journal, lancement manuel | À compléter |
| Ligne ajoutée après redémarrage | À compléter |
| Entrée visible dans Autoruns | À compléter |
| Résultat d'un éventuel test d'arrêt inattendu | À compléter ou « non testé » |
| État après nettoyage | À compléter |

## Traces observables et nettoyage

Un lancement automatique laisse une configuration visible dans l'outil Windows
correspondant et dans Autoruns. Le programme témoin laisse également un
exécutable copié dans la VM et son fichier journal temporaire. Ces traces
permettent de vérifier le test et d'identifier ce qui doit être retiré.

Après l'essai, supprimer l'entrée de démarrage créée pour le test, supprimer
l'exécutable témoin et son fichier journal, puis vérifier dans Autoruns que
l'entrée a disparu. Restaurer le snapshot propre pour retrouver l'état initial
de la VM. Noter dans la fiche de résultats que le nettoyage a été vérifié.
