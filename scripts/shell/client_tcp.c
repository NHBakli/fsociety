#define _WIN32_WINNT 0x0600 // Active les fonctions reseau Windows utilisees ici.
#include <winsock2.h>      // Connexion TCP, envoi et reception.
#include <ws2tcpip.h>      // Conversion d'une adresse IPv4 avec InetPtonA.
#include <stdio.h>         // Affichage des messages dans le terminal.

#pragma comment(lib, "ws2_32.lib") // Bibliotheque reseau pour le compilateur MSVC.

#define PORT 5000

int main(int argc, char *argv[])
{
    WSADATA wsa;
    SOCKET connexion = INVALID_SOCKET;
    struct sockaddr_in serveur = {0};
    const char message[] = "BONJOUR";
    char reponse[2];
    DWORD delai = 5000;
    int position = 0;
    int resultat = 1;
    int erreur;

    if (argc != 2) {
        fprintf(stderr, "Utilisation : %s <IPv4_serveur>\n", argv[0]);
        return 1;
    }

    erreur = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (erreur != 0) {
        fprintf(stderr, "[ERREUR] Initialisation reseau : %d.\n", erreur);
        return 1;
    }

    serveur.sin_family = AF_INET;
    serveur.sin_port = htons(PORT);
    if (InetPtonA(AF_INET, argv[1], &serveur.sin_addr) != 1) {
        fprintf(stderr, "[ERREUR] Adresse IPv4 invalide.\n");
        goto nettoyage;
    }

    connexion = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connexion == INVALID_SOCKET) {
        fprintf(stderr, "[ERREUR] Creation de la connexion : %d.\n", WSAGetLastError());
        goto nettoyage;
    }

    // Limiter l'attente des envois et receptions a cinq secondes.
    if (setsockopt(connexion, SOL_SOCKET, SO_SNDTIMEO,
                   (const char *)&delai, sizeof(delai)) == SOCKET_ERROR ||
        setsockopt(connexion, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&delai, sizeof(delai)) == SOCKET_ERROR) {
        fprintf(stderr, "[ERREUR] Configuration du delai : %d.\n", WSAGetLastError());
        goto nettoyage;
    }

    printf("Connexion vers %s sur le port TCP %d...\n", argv[1], PORT);
    fflush(stdout);
    if (connect(connexion, (struct sockaddr *)&serveur, sizeof(serveur)) == SOCKET_ERROR) {
        fprintf(stderr, "[ERREUR] Connexion impossible : %d.\n", WSAGetLastError());
        goto nettoyage;
    }

    // TCP peut envoyer le message en plusieurs morceaux : envoyer les sept octets.
    while (position < (int)sizeof(message) - 1) {
        int nombre = send(connexion, message + position,
                          (int)sizeof(message) - 1 - position, 0);
        if (nombre == SOCKET_ERROR) {
            fprintf(stderr, "[ERREUR] Envoi impossible : %d.\n", WSAGetLastError());
            goto nettoyage;
        }
        if (nombre == 0) {
            fprintf(stderr, "[ERREUR] Envoi interrompu.\n");
            goto nettoyage;
        }
        position += nombre;
    }
    printf("Message envoye : BONJOUR\n");

    // Lire exactement les deux octets de la reponse, meme s'ils arrivent separement.
    position = 0;
    while (position < (int)sizeof(reponse)) {
        int nombre = recv(connexion, reponse + position, (int)sizeof(reponse) - position, 0);
        if (nombre == SOCKET_ERROR) {
            fprintf(stderr, "[ERREUR] Reception impossible : %d.\n", WSAGetLastError());
            goto nettoyage;
        }
        if (nombre == 0) {
            fprintf(stderr, "[ERREUR] Le serveur a ferme la connexion avant la reponse complete.\n");
            goto nettoyage;
        }
        position += nombre;
    }

    if (reponse[0] != 'O' || reponse[1] != 'K') {
        fprintf(stderr, "[ERREUR] Reponse inattendue : le serveur doit envoyer OK.\n");
        goto nettoyage;
    }
    printf("[SUCCES] Reponse recue : OK\n");
    resultat = 0;

nettoyage:
    // Liberer la connexion et les ressources reseau avant de quitter.
    if (connexion != INVALID_SOCKET) {
        closesocket(connexion);
    }
    WSACleanup();
    return resultat;
}
