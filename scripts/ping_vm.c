#include <stdio.h>   // Affichage, lecture de l'adresse et resultat du ping.
#include <string.h>  // Verification des caracteres et recherche de "TTL=".
#include <windows.h> // Attente avec Sleep et mesure du temps avec GetTickCount.

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Utilisation : %s <IPv4_VM>\n", argv[0]);
        return 1;
    }

    // Accepter uniquement une IPv4 composee de quatre nombres entre 0 et 255.
    unsigned int a, b, c, d;
    char fin;
    if (strspn(argv[1], "0123456789.") != strlen(argv[1]) ||
        sscanf_s(argv[1], "%3u.%3u.%3u.%3u%c", &a, &b, &c, &d, &fin, 1u) != 4 ||
        a > 255 || b > 255 || c > 255 || d > 255) {
        return 1;
    }

    // Construire la commande avec les nombres valides, sans texte libre.
    char ip[16];
    char commande[80];
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a, b, c, d);
    snprintf(commande, sizeof(commande), "ping.exe -4 -n 1 -w 1000 %s", ip);
    printf("Ping vers %s toutes les 10 secondes. Ctrl+C pour arreter.\n", ip);

    while (1) {
        DWORD debut = GetTickCount();
        char ligne[256];
        int succes = 0;

        // Un seul ping IPv4, avec une attente maximale d'une seconde.
        FILE *ping = _popen(commande, "r");
        if (ping == NULL) {
            perror("Impossible de lancer le ping");
            return 1;
        }

        // Sous Windows, une reponse IPv4 reussie contient "TTL=".
        while (fgets(ligne, sizeof(ligne), ping) != NULL) {
            if (strstr(ligne, "TTL=") != NULL) {
                printf("[SUCCES] %s", ligne);
                succes = 1;
            }
        }
        _pclose(ping);

        if (!succes) {
            printf("[ECHEC] %s ne repond pas au ping.\n", ip);
        }
        fflush(stdout);

        // Inclure le temps du ping dans les 10 secondes.
        DWORD duree = GetTickCount() - debut;
        if (duree < 10000) {
            Sleep(10000 - duree);
        }
    }
}
