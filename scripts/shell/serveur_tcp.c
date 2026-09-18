#define _WIN32_WINNT 0x0600 // Rend disponible la conversion IPv4 InetPtonA.
#include <winsock2.h>      // Connexions TCP sous Windows.
#include <ws2tcpip.h>      // Conversion d'une adresse IPv4.
#include <stdio.h>         // Messages dans le terminal.
#include <string.h>        // Comparaison du message recu.

#pragma comment(lib, "ws2_32.lib") // Bibliotheque reseau de Windows.
#define PORT 5000

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Utilisation : %s <IPv4_locale_du_PC>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in adresse = {0};
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons(PORT);
    if (InetPtonA(AF_INET, argv[1], &adresse.sin_addr) != 1) {
        fprintf(stderr, "[ERREUR] Adresse IPv4 invalide.\n");
        return 1;
    }

    WSADATA wsa;
    int erreur = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (erreur != 0) {
        fprintf(stderr, "[ERREUR] Initialisation Winsock : %d.\n", erreur);
        return 1;
    }

    SOCKET serveur = INVALID_SOCKET;
    SOCKET client = INVALID_SOCKET;
    int resultat = 1;
    const char *etape = "creation de la socket";
    serveur = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serveur == INVALID_SOCKET) goto erreur_reseau;

    // Ecouter uniquement sur l'adresse locale donnee au lancement.
    etape = "association de l'adresse et du port (bind)";
    if (bind(serveur, (struct sockaddr *)&adresse, sizeof(adresse)) == SOCKET_ERROR)
        goto erreur_reseau;
    etape = "mise en ecoute (listen)";
    if (listen(serveur, 1) == SOCKET_ERROR) goto erreur_reseau;

    printf("[SERVEUR] En attente sur %s:%d (TCP). Ctrl+C pour arreter.\n", argv[1], PORT);
    fflush(stdout);
    etape = "acceptation du client (accept)";
    client = accept(serveur, NULL, NULL);
    if (client == INVALID_SOCKET) goto erreur_reseau;

    // Apres connexion, limiter chaque attente d'envoi/reception a 5 secondes.
    DWORD delai = 5000;
    etape = "configuration des delais";
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&delai, sizeof(delai)) == SOCKET_ERROR ||
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&delai, sizeof(delai)) == SOCKET_ERROR)
        goto erreur_reseau;

    // TCP peut livrer BONJOUR en plusieurs morceaux : lire ses 7 octets.
    char message[8] = {0};
    int recus = 0;
    while (recus < 7) {
        int n = recv(client, message + recus, 7 - recus, 0);
        etape = "reception du message (recv)";
        if (n == SOCKET_ERROR) goto erreur_reseau;
        if (n == 0) {
            fprintf(stderr, "[ERREUR] Client deconnecte avant le message complet.\n");
            goto fin;
        }
        recus += n;
    }
    if (strcmp(message, "BONJOUR") != 0) {
        fprintf(stderr, "[ERREUR] Message inattendu : BONJOUR attendu.\n");
        goto fin;
    }
    printf("[RECU] BONJOUR\n");

    // Repondre avec les 2 octets OK, puis terminer cet unique echange.
    int envoyes = 0;
    while (envoyes < 2) {
        int n = send(client, "OK" + envoyes, 2 - envoyes, 0);
        etape = "envoi de la reponse (send)";
        if (n == SOCKET_ERROR) goto erreur_reseau;
        if (n == 0) {
            fprintf(stderr, "[ERREUR] Envoi interrompu.\n");
            goto fin;
        }
        envoyes += n;
    }
    printf("[SUCCES] Reponse OK envoyee.\n");
    resultat = 0;
    goto fin;

erreur_reseau:
    fprintf(stderr, "[ERREUR] %s : Winsock %d.\n", etape, WSAGetLastError());
fin:
    if (client != INVALID_SOCKET) closesocket(client);
    if (serveur != INVALID_SOCKET) closesocket(serveur);
    WSACleanup();
    return resultat;
}
