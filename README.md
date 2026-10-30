# fsociety

**Projet scolaire de cybersécurité, en cours de développement.** Ce dépôt sert
à étudier, dans un laboratoire isolé et autorisé, la communication entre
plusieurs programmes, les tests logiciels et les limites de certaines
techniques de protection des données. Le travail est accompagné de tests et
de notes destinés à expliquer les choix techniques lors de la démonstration
du projet.

## Cadre du projet

Les expériences sont prévues uniquement sur des machines de laboratoire dont
l'utilisation est autorisée. Les données de démonstration doivent être
fictives ; aucune donnée personnelle ou aucun identifiant réel ne doit être
collecté. Le [manifeste du laboratoire](lab/manifest.md) décrit
l'environnement de test et ses contraintes.

Le dépôt est un **prototype pédagogique**. Le
[plan d'action](docs/action_plan.md) présente aussi des idées et des objectifs
qui ne sont pas nécessairement implémentés. La présence d'un dossier ou d'une
fonction dans ce plan ne signifie pas qu'elle est opérationnelle dans le code
actuel.

## Organisation du dépôt

| Dossier | Rôle actuel |
| --- | --- |
| `server/` | Programme serveur expérimental qui coordonne les connexions. |
| `controller/` | Interface en ligne de commande pour les essais du laboratoire. |
| `implant/` | Programme agent expérimental et module de démonstration. |
| `loader/` | Ébauche d'un composant distinct ; son développement est en cours. |
| `common/` | Code partagé, dont la transformation XOR et ses tests. |
| `lab/` | Description de l'environnement de laboratoire. |
| `docs/` | Explications et notes du projet. |

## Exemple étudié : XOR

La fonction `xor_transform` illustre une transformation réversible d'octets.
Les tests vérifient l'encodage, le décodage, les données binaires, plusieurs
longueurs et les arguments invalides. Depuis la racine du dépôt, avec GCC :

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
    common/tests/test_xor.c common/src/xor.c -o /tmp/test_xor
/tmp/test_xor
```

Le résultat attendu est `All XOR tests passed`. La note
[Transformation XOR](docs/xor.md) explique le code et pourquoi le XOR seul
n'est pas un chiffrement adapté à la protection de communications.

## État du développement

Le code, les tests et la documentation évoluent par issues. Les composants
ne forment pas encore une application complète et portable. En particulier,
le Makefile de l'agent utilise actuellement un framework propre à macOS ; la
construction de l'ensemble du dépôt dépend donc de la plateforme et des
travaux restant à réaliser. Le test XOR ci-dessus peut être exécuté
indépendamment des autres composants.
