## 1. Objectif

Couvrir les traces laissées par l'implant dans les logs système, réseau et commandes, sans éveiller les soupçons.

---

## 2. Principe : rollback, pas wipe

Le sujet l'exige : *"by rolling them back rather than wiping everything"*.

- **Rollback** : restaurer un état antérieur cohérent.
- **Wipe** : tout effacer → trou suspect, alerte immédiate.

---

## 3. Les trois surfaces

### 3.1 Logs système (Windows Event Log)

**Ne pas** :
- `wevtutil cl` (efface tout),
- Supprimer les fichiers `.evtx`.

**Plutôt** :
- Identifier les événements générés par l'implant,
- Restaurer une sauvegarde antérieure ciblée,
- Ou retirer les entrées spécifiques via API.

**Démonstration** : avant/après avec `Get-WinEvent`.

### 3.2 Logs réseau

- Côté C2 : ne pas logger les IP en clair,
- Côté implant : pas de logs locaux,
- Côté pare-feu : utiliser HTTPS (port 443) pour se fondre dans le trafic,
- Pas de connexions en rafale.

### 3.3 Logs de commandes

- PowerShell : `ConsoleHost_history.txt` dans `%APPDATA%\Microsoft\Windows\PowerShell\PSReadLine\`,
- Cmd : pas de log natif, mais tracé par les EDR,
- **Effacer** ces fichiers après usage, ou ne pas utiliser PowerShell (shell built-in).

---

## 4. Les pièges à éviter

| Piège | Raison |
|---|---|
| Effacer tous les logs | Un log vide est plus suspect qu'un log normal |
| `wevtutil cl` | Commande connue, tracée |
| Supprimer immédiatement les fichiers temporaires | Apparition/disparition suspecte |
| Modifier les timestamps | Incohérence détectable |
| Oublier la mémoire | L'implant peut être récupéré en RAM |

---

## 5. Approche recommandée

1. **Ne pas générer de traces inutiles** : API directes, pas de PowerShell, pas de commandes visibles.
2. **Rollback ciblé** : restaurer les logs à un état antérieur, pas les vider.
3. **Nettoyage différé** : attendre la fin de la mission.
4. **Cohérence** : pas de trous suspects dans les logs.
5. **Démonstration** : avant/après avec captures.

---

## 6. Validation

- Comparer les logs avant/après avec `Get-WinEvent`,
- Vérifier la cohérence des timestamps,
- Vérifier l'absence de fichiers temporaires,
- Documenter la méthode et les limites.

---

## 7. En résumé

| Surface | Méthode |
|---|---|
| Logs système | Rollback ciblé, pas wipe |
| Logs réseau | HTTPS, pas de logs côté C2, pas de rafale |
| Logs commandes | Effacer `ConsoleHost_history.txt`, ou shell built-in |
