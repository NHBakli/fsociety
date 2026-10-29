#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════
 * Les états possibles du parseur quand on lit le fichier
 * ══════════════════════════════════════════════════════════ */
enum { NORMAL, CHAINE, COMMENTAIRE_LIGNE, COMMENTAIRE_BLOC };

/* ══
 * ecrire_dead_code — écrit un bloc de code mort POLYMORPHE
 * Chaque appel produit un bloc différent (nombre et type
 * d'opérations variables, choisis au hasard).
 * ═══════════════════════════════════════════════════════════ */
static void ecrire_dead_code(FILE *out) {
    int faux = rand();
    if (faux == 42) faux++;   /* la valeur piège doit être != 42 */

    /* ouverture du if faux */
    fprintf(out, " if (piege == %d) { int x = %d;", faux, rand());

    /* nombre d'opérations : entre 3 et 8 (choisi au hasard) */
    int nb = 3 + rand() % 6;

    for (int i = 0; i < nb; i++) {
        int decalage = 1 + rand() % 31;   /* entre 1 et 31 */

        switch (rand() % 3) {
        case 0:
            /* multiplication + addition */
            fprintf(out, " x = x * %d + %d;", rand() | 1, rand());
            break;
        case 1:
            /* xor avec décalage à droite */
            fprintf(out, " x ^= x >> %d;", decalage);
            break;
        case 2:
            /* rotation de bits */
            fprintf(out, " x = (x << %d) | (x >> %d);", decalage, 32 - decalage);
            break;
        }
    }

    /* fermeture du if */
    fprintf(out, " }");
}

/* ═══════════════════════════════════════════════════════════
 * main — lit entree.c, écrit sortie.c en injectant du dead code
 * aux bons endroits (début de fonction uniquement).
 * ═══════════════════════════════════════════════════════════ */
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage : %s entree.c sortie.c\n", argv[0]);
        return 1;
    }

    FILE *in  = fopen(argv[1], "r");
    FILE *out = fopen(argv[2], "w");
    if (!in || !out) { perror("fopen"); return 1; }

    /* initialisation du générateur aléatoire avec l'heure actuelle
       → chaque compilation produit un fichier différent */
    srand(time(NULL));

    /* on pose la variable piège en haut du fichier de sortie */
    fprintf(out, "static volatile int piege = 42;\n");

    int etat    = NORMAL;
    int prec    = 0;   /* le caractère juste avant celui qu'on lit */
    int dernier = 0;   /* dernier caractère "important" en mode NORMAL */
    int c;

    while ((c = fgetc(in)) != EOF) {
        fputc(c, out);   /* on recopie TOUJOURS le caractère */

        switch (etat) {
        case NORMAL:
            /* détection d'entrée dans un commentaire ou une string */
            if (prec == '/' && c == '/')      { etat = COMMENTAIRE_LIGNE; }
            else if (prec == '/' && c == '*') { etat = COMMENTAIRE_BLOC;  }
            else if (c == '"')                { etat = CHAINE;            }
            else if (c == '{') {
                /* on injecte UNIQUEMENT si le dernier caractère important
                   est ')' → ça distingue "int main() {" (fonction)
                   de "= { 0 }" (initialisation de tableau) */
                if (dernier == ')') ecrire_dead_code(out);
            }

            /* on mémorise le dernier caractère "important"
               (on ignore les espaces et les '/') */
            if (!isspace(c) && c != '/') dernier = c;
            break;

        case CHAINE:
            /* un \" échappé ne ferme pas la chaîne */
            if (prec == '\\') { /* on ne fait rien */ }
            else if (c == '"') { etat = NORMAL; }
            break;

        case COMMENTAIRE_LIGNE:
            /* se termine à la fin de ligne */
            if (c == '\n') etat = NORMAL;
            break;

        case COMMENTAIRE_BLOC:
            /* se termine par la séquence */
            if (prec == '*' && c == '/') etat = NORMAL;
            break;
        }

        prec = c;
    }

    fclose(in);
    fclose(out);
    return 0;
}
