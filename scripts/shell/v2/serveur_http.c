#define _WIN32_WINNT 0x0600
#include <winsock2.h> // Sockets TCP et UDP, fonctions Windows.
#include <stdio.h>    // Affichage des informations recues.
#include <string.h>   // Lecture des en-tetes HTTP.

#pragma comment(lib, "ws2_32.lib")
#define PORT_HTTP 5000
#define PORT_DECOUVERTE 5001
#define QUESTION "LAB_SYSINFO_V2_DISCOVER"
#define REPONSE "LAB_SYSINFO_V2_SERVER"
#define LIMITE 2048

// Accuser reception ou signaler une requete incorrecte.
static void repondre(SOCKET client, int succes)
{
    const char *texte = succes
        ? "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK"
        : "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    int restant = (int)strlen(texte);
    while (restant > 0) {
        int n = send(client, texte, restant, 0);
        if (n <= 0) break;
        texte += n;
        restant -= n;
    }
}

// Lire un POST /infos complet : TCP peut livrer le message en plusieurs morceaux.
static void recevoir_infos(SOCKET client)
{
    char tampon[2 * LIMITE + 1];
    char *fin_entetes = NULL;
    int recus = 0, longueur = -1, hote = 0;
    DWORD delai = 5000;
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char *)&delai, sizeof(delai)) ||
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (char *)&delai, sizeof(delai))) return;

    // La ligne vide separe les en-tetes HTTP des informations systeme.
    while (fin_entetes == NULL) {
        if (recus == LIMITE) goto incorrect;
        int n = recv(client, tampon + recus, LIMITE - recus, 0);
        if (n <= 0) return;
        recus += n;
        tampon[recus] = '\0';
        fin_entetes = strstr(tampon, "\r\n\r\n");
        if (memchr(tampon, '\0', fin_entetes ? (size_t)(fin_entetes - tampon) : (size_t)recus))
            goto incorrect;
    }

    int debut_corps = (int)(fin_entetes - tampon) + 4;
    char *ligne = strstr(tampon, "\r\n");
    *ligne = '\0';
    if (strcmp(tampon, "POST /infos HTTP/1.1") != 0) goto incorrect;
    ligne += 2;

    // Content-Length indique le nombre exact d'octets du contenu a lire.
    while (ligne < fin_entetes) {
        char *fin = strstr(ligne, "\r\n");
        *fin = '\0';
        char *valeur = strchr(ligne, ':');
        if (valeur == NULL || valeur == ligne) goto incorrect;
        *valeur++ = '\0';
        if (strspn(ligne, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+-.^_" "\x60" "|~") != strlen(ligne))
            goto incorrect;
        while (*valeur == ' ' || *valeur == '\t') ++valeur;
        char *dernier = fin;
        while (dernier > valeur && (dernier[-1] == ' ' || dernier[-1] == '\t')) --dernier;
        *dernier = '\0';

        if (_stricmp(ligne, "Content-Length") == 0) {
            if (longueur != -1 || *valeur == '\0') goto incorrect;
            longueur = 0;
            for (; *valeur; ++valeur) {
                if (*valeur < '0' || *valeur > '9') goto incorrect;
                longueur = longueur * 10 + (*valeur - '0');
                if (longueur > LIMITE) goto incorrect;
            }
        } else if (_stricmp(ligne, "Host") == 0) {
            if (++hote > 1 || *valeur == '\0') goto incorrect;
        } else if (_stricmp(ligne, "Transfer-Encoding") == 0 || _stricmp(ligne, "Expect") == 0) {
            // Ce serveur de laboratoire accepte seulement un corps de taille connue.
            goto incorrect;
        }
        ligne = fin + 2;
    }
    if (hote != 1 || longueur <= 0) goto incorrect;
    while (recus < debut_corps + longueur) {
        int n = recv(client, tampon + recus, debut_corps + longueur - recus, 0);
        if (n <= 0) return;
        recus += n;
    }

    // La date et l'heure sont deja presentes dans le releve du client.
    putchar('\n');
    for (int i = 0; i < longueur; ++i) {
        unsigned char c = (unsigned char)tampon[debut_corps + i];
        // Afficher le texte en neutralisant les commandes de controle du terminal.
        putchar((c >= 32 && c != 127) || c == '\n' || c == '\t' ? c : '?');
    }
    fflush(stdout);
    repondre(client, 1);
    return;

incorrect:
    repondre(client, 0);
}

int main(void)
{
    WSADATA wsa;
    SOCKET serveur = INVALID_SOCKET, decouverte = INVALID_SOCKET;
    struct sockaddr_in adresse = {0};
    int erreur = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (erreur != 0) {
        fprintf(stderr, "Initialisation reseau impossible : %d.\n", erreur);
        return 1;
    }

    // Ecouter sur toutes les interfaces : aucune adresse IP a saisir.
    adresse.sin_family = AF_INET;
    adresse.sin_addr.s_addr = htonl(INADDR_ANY);
    adresse.sin_port = htons(PORT_HTTP);
    serveur = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serveur == INVALID_SOCKET ||
        bind(serveur, (struct sockaddr *)&adresse, sizeof(adresse)) == SOCKET_ERROR ||
        listen(serveur, SOMAXCONN) == SOCKET_ERROR) goto erreur_reseau;

    adresse.sin_port = htons(PORT_DECOUVERTE);
    decouverte = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (decouverte == INVALID_SOCKET ||
        bind(decouverte, (struct sockaddr *)&adresse, sizeof(adresse)) == SOCKET_ERROR) goto erreur_reseau;

    SetConsoleOutputCP(CP_UTF8);
    puts("Serveur pret : HTTP 5000, decouverte UDP 5001. Ctrl+C pour arreter.");
    fflush(stdout);
    while (1) {
        // Attendre une demande de decouverte UDP ou une connexion HTTP.
        fd_set lectures;
        FD_ZERO(&lectures);
        FD_SET(serveur, &lectures);
        FD_SET(decouverte, &lectures);
        if (select(0, &lectures, NULL, NULL, NULL) == SOCKET_ERROR) goto erreur_reseau;

        if (FD_ISSET(decouverte, &lectures)) {
            char message[64];
            struct sockaddr_in client = {0};
            int taille = sizeof(client);
            int n = recvfrom(decouverte, message, sizeof(message), 0, (struct sockaddr *)&client, &taille);
            if (n == (int)sizeof(QUESTION) - 1 && memcmp(message, QUESTION, sizeof(QUESTION) - 1) == 0)
                sendto(decouverte, REPONSE, sizeof(REPONSE) - 1, 0, (struct sockaddr *)&client, taille);
        }
        if (FD_ISSET(serveur, &lectures)) {
            SOCKET client = accept(serveur, NULL, NULL);
            if (client == INVALID_SOCKET) goto erreur_reseau;
            recevoir_infos(client);
            closesocket(client);
        }
    }

erreur_reseau:
    fprintf(stderr, "Erreur reseau : %d.\n", WSAGetLastError());
    if (serveur != INVALID_SOCKET) closesocket(serveur);
    if (decouverte != INVALID_SOCKET) closesocket(decouverte);
    WSACleanup();
    return 1;
}
