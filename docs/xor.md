# Transformation XOR — issue #6

Cette note accompagne [`common/src/xor.c`](../common/src/xor.c) et ses
[`tests`](../common/tests/test_xor.c). L'objectif est de comprendre une
transformation réversible d'octets, puis d'en évaluer les limites pour la
protection de données.

## Fonctionnement

Pour l'octet d'indice `i`, `xor_transform` calcule :

```text
sortie[i] = entree[i] XOR cle[i modulo longueur_cle]
```

La clé est répétée si elle est plus courte que l'entrée. Le XOR possède la
propriété `(A XOR B) XOR B = A` : un second appel avec la même clé et la même
longueur restaure les octets d'origine. Par exemple, `0x41 XOR 0x01 = 0x40`,
puis `0x40 XOR 0x01 = 0x41`.

La fonction traite des octets et une longueur explicite, et non des chaînes
terminées par `\0`. Un octet `0x00` au milieu de l'entrée est donc traité
normalement. L'entrée et la sortie peuvent être le même tampon. La fonction
renvoie `false` si la clé est absente ou vide, ou si un tampon est absent alors
que la longueur est non nulle. Une longueur nulle est acceptée avec une clé
valide, même si les tampons d'entrée et de sortie sont `NULL`.

## Vérification

Depuis la racine du dépôt :

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
    common/tests/test_xor.c common/src/xor.c -o /tmp/test_xor
/tmp/test_xor
```

Le résultat attendu est `All XOR tests passed`. Les tests couvrent :

| Cas | Ce qui est vérifié |
| --- | --- |
| Un octet | Le résultat connu `0x41 XOR 0x01 = 0x40` et le décodage. |
| Données binaires | `0x00`, `0xFF` et une clé répétée produisent les octets attendus. |
| Tailles variables | L'aller-retour fonctionne de 0 à 9 octets ; la zone après la longueur demandée reste intacte. |
| Même tampon | Deux transformations successives restaurent le contenu initial. |
| Arguments invalides | Les appels sont refusés sans modifier l'octet de sortie de contrôle. |

## Pourquoi ce n'est pas un chiffrement sûr

Le fait de retrouver les données d'origine prouve la **réversibilité**, pas
la **confidentialité**. Dans cet exemple, si l'on connaît `0x41` et son
résultat `0x40`, on retrouve immédiatement l'octet de clé :
`0x41 XOR 0x40 = 0x01`. Avec une clé répétée, cette observation peut révéler
d'autres positions protégées par le même octet de clé.

Le XOR ne fournit pas non plus d'**intégrité** : modifier un bit de la sortie
modifie le même bit après décodage, sans que `xor_transform` puisse le
détecter. La fonction n'authentifie ni les données ni leur origine. Elle ne
doit donc pas être utilisée seule pour protéger des communications ou des
données sensibles.

## Alternative pour des données à protéger

Un chiffrement authentifié, tel qu'**AES-GCM**, associe la confidentialité à
une vérification d'intégrité. Le destinataire vérifie un tag d'authentification
avant d'accepter les données déchiffrées. Contrairement à `xor_transform`, ce
type d'algorithme exige aussi une gestion rigoureuse des clés et un **nonce
unique pour chaque chiffrement avec la même clé**. Réutiliser cette paire
clé/nonce compromet la sécurité de GCM. Une implémentation réelle doit utiliser
une bibliothèque cryptographique reconnue plutôt qu'une construction maison.

Références : [NIST SP 800-38D](https://csrc.nist.gov/pubs/sp/800/38/d/final)
pour la définition de GCM et [RFC 5116](https://www.rfc-editor.org/rfc/rfc5116.html)
pour l'interface des algorithmes de chiffrement authentifié et la contrainte
sur la réutilisation du nonce.
