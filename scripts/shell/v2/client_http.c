#define _WIN32_WINNT 0x0600 // Fonctions Windows disponibles depuis Vista.
#include <winsock2.h>      // Decouverte du serveur avec une socket UDP.
#include <ws2tcpip.h>      // Interfaces reseau et conversion des adresses.
#include <windows.h>       // Informations systeme et attente.
#include <lmcons.h>        // Taille maximale du nom d'utilisateur Windows.
#include <winhttp.h>       // Envoi des requetes HTTP.
#include <stdio.h>         // Creation du texte et messages dans le terminal.
#include <string.h>        // Longueur du message avec strlen.

#pragma comment(linker, "/SUBSYSTEM:WINDOWS") // Eviter l'ouverture d'une console Windows lors de l'execution.
#pragma comment(linker, "/ENTRY:mainCRTStartup") // Eviter l'ouverture d'une console Windows lors de l'execution.
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define PORT 5000
#define PORT_DECOUVERTE 5001
#define INTERVALLE_MS 30000
#define QUESTION "LAB_SYSINFO_V2_DISCOVER"
#define REPONSE "LAB_SYSINFO_V2_SERVER"

// Demander l'adresse du serveur sur chaque reseau IPv4 local actif.
static int trouver_serveur(WCHAR *serveur, size_t capacite, char *ip_client)
{
    SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    INTERFACE_INFO interfaces[64];
    DWORD taille_interfaces = 0;
    DWORD i;
    struct sockaddr_in local = {0};
    int diffusion = 1;
    int envois = 0;
    int trouve = 0;
    ULONGLONG limite;

    if (udp == INVALID_SOCKET) {
        fprintf(stderr, "[ERREUR] Creation de la socket UDP : %d.\n", WSAGetLastError());
        return 0;
    }
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(udp, SOL_SOCKET, SO_BROADCAST, (const char *)&diffusion,
                   sizeof(diffusion)) == SOCKET_ERROR ||
        bind(udp, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR ||
        WSAIoctl(udp, SIO_GET_INTERFACE_LIST, NULL, 0, interfaces, sizeof(interfaces),
                 &taille_interfaces, NULL, NULL) == SOCKET_ERROR) {
        fprintf(stderr, "[ERREUR] Preparation de la decouverte UDP : %d.\n", WSAGetLastError());
        goto fin;
    }

    for (i = 0; i < taille_interfaces / sizeof(interfaces[0]); ++i) {
        struct sockaddr_in destination = {0};
        ULONG adresse = interfaces[i].iiAddress.AddressIn.sin_addr.s_addr;
        ULONG masque = interfaces[i].iiNetmask.AddressIn.sin_addr.s_addr;

        if (!(interfaces[i].iiFlags & IFF_UP) ||
            !(interfaces[i].iiFlags & IFF_BROADCAST) ||
            (interfaces[i].iiFlags & IFF_LOOPBACK) ||
            interfaces[i].iiAddress.AddressIn.sin_family != AF_INET ||
            adresse == INADDR_ANY || masque == 0 || masque == INADDR_BROADCAST) {
            continue;
        }
        destination.sin_family = AF_INET;
        destination.sin_port = htons(PORT_DECOUVERTE);
        destination.sin_addr.s_addr = adresse | ~masque;
        if (sendto(udp, QUESTION, (int)sizeof(QUESTION) - 1, 0,
                   (struct sockaddr *)&destination, sizeof(destination)) != SOCKET_ERROR) {
            ++envois;
        }
    }
    if (envois == 0) {
        fprintf(stderr, "[ERREUR] Aucun reseau IPv4 utilisable pour la decouverte.\n");
        goto fin;
    }

    // Ignorer les autres messages UDP pendant une attente de deux secondes.
    limite = GetTickCount64() + 2000;
    while (GetTickCount64() < limite) {
        fd_set lecture;
        struct timeval attente = {0, 100000};
        struct sockaddr_in source = {0};
        int taille_source = sizeof(source);
        char reponse[128];
        int disponible;
        int recus;

        FD_ZERO(&lecture);
        FD_SET(udp, &lecture);
        disponible = select(0, &lecture, NULL, NULL, &attente);
        if (disponible == SOCKET_ERROR) {
            fprintf(stderr, "[ERREUR] Attente de la decouverte : %d.\n", WSAGetLastError());
            goto fin;
        }
        if (disponible == 0) continue;
        recus = recvfrom(udp, reponse, sizeof(reponse), 0,
                         (struct sockaddr *)&source, &taille_source);
        if (recus == (int)sizeof(REPONSE) - 1 && source.sin_family == AF_INET &&
            source.sin_port == htons(PORT_DECOUVERTE) &&
            memcmp(reponse, REPONSE, sizeof(REPONSE) - 1) == 0 &&
            InetNtopW(AF_INET, &source.sin_addr, serveur, capacite) != NULL) {
            int taille_local = sizeof(local);

            // Lire l'IP locale choisie par Windows pour joindre ce serveur.
            if (connect(udp, (struct sockaddr *)&source, sizeof(source)) == SOCKET_ERROR ||
                getsockname(udp, (struct sockaddr *)&local, &taille_local) == SOCKET_ERROR ||
                InetNtopA(AF_INET, &local.sin_addr, ip_client, 16) == NULL) {
                fprintf(stderr, "[ERREUR] Lecture de l'IP du client : %d.\n", WSAGetLastError());
                goto fin;
            }
            if (local.sin_addr.s_addr == htonl(INADDR_ANY)) {
                fprintf(stderr, "[ERREUR] IP du client indisponible.\n");
                goto fin;
            }
            trouve = 1;
            break;
        }
    }

fin:
    closesocket(udp);
    return trouve;
}

// Un FILETIME contient un compteur de temps reparti sur deux nombres de 32 bits.
static ULONGLONG temps_cpu(FILETIME temps)
{
    return ((ULONGLONG)temps.dwHighDateTime << 32) | temps.dwLowDateTime;
}

// Mesurer l'utilisation moyenne du CPU pendant une seconde.
static int mesurer_cpu(double *pourcentage)
{
    FILETIME repos1, noyau1, utilisateur1;
    FILETIME repos2, noyau2, utilisateur2;
    ULONGLONG total, repos;

    if (!GetSystemTimes(&repos1, &noyau1, &utilisateur1)) {
        fprintf(stderr, "[ERREUR] Lecture du CPU : %lu.\n", GetLastError());
        return 0;
    }
    Sleep(1000);
    if (!GetSystemTimes(&repos2, &noyau2, &utilisateur2)) {
        fprintf(stderr, "[ERREUR] Lecture du CPU : %lu.\n", GetLastError());
        return 0;
    }

    // Le temps du noyau inclut deja le repos : on le soustrait une seule fois.
    total = (temps_cpu(noyau2) - temps_cpu(noyau1)) +
            (temps_cpu(utilisateur2) - temps_cpu(utilisateur1));
    repos = temps_cpu(repos2) - temps_cpu(repos1);
    if (total == 0 || repos > total) {
        fprintf(stderr, "[ERREUR] Mesure CPU indisponible.\n");
        return 0;
    }
    *pourcentage = 100.0 * (double)(total - repos) / (double)total;
    return 1;
}

// Reunir uniquement les six informations demandees, en texte lisible.
static int preparer_infos(char *texte, size_t capacite, const char *ip_client)
{
    WCHAR nom_windows[UNLEN + 1];
    DWORD taille_nom = UNLEN + 1;
    char nom_utf8[4 * (UNLEN + 1)];
    MEMORYSTATUSEX memoire = {0};
    SYSTEMTIME date;
    double cpu;
    int longueur;

    if (!mesurer_cpu(&cpu)) return 0;
    memoire.dwLength = sizeof(memoire);
    if (!GetUserNameW(nom_windows, &taille_nom) ||
        !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, nom_windows, -1,
                             nom_utf8, sizeof(nom_utf8), NULL, NULL) ||
        !GlobalMemoryStatusEx(&memoire)) {
        fprintf(stderr, "[ERREUR] Lecture des informations systeme : %lu.\n", GetLastError());
        return 0;
    }
    GetLocalTime(&date);

    // Les quantites de RAM sont exprimees en Mio (1 Mio = 1024 * 1024 octets).
    longueur = snprintf(texte, capacite,
        "Utilisateur : %s\nIP : %s\nCPU : %.1f %% (sur 1 seconde)\n"
        "RAM : %llu Mio utilises / %llu Mio\n"
        "Date : %04u-%02u-%02u\nHeure : %02u:%02u:%02u\n",
        nom_utf8, ip_client, cpu,
        (memoire.ullTotalPhys - memoire.ullAvailPhys) / (1024ULL * 1024ULL),
        memoire.ullTotalPhys / (1024ULL * 1024ULL),
        (unsigned)date.wYear, (unsigned)date.wMonth, (unsigned)date.wDay,
        (unsigned)date.wHour, (unsigned)date.wMinute, (unsigned)date.wSecond);
    if (longueur < 0 || (size_t)longueur >= capacite) {
        fprintf(stderr, "[ERREUR] Informations trop longues pour le message.\n");
        return 0;
    }
    return 1;
}

static void envoyer_infos(HINTERNET connexion, char *texte)
{
    HINTERNET requete;
    DWORD code_http = 0;
    DWORD taille_code = sizeof(code_http);
    DWORD taille_texte = (DWORD)strlen(texte);
    DWORD options = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_AUTHENTICATION |
                    WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_KEEP_ALIVE;

    requete = WinHttpOpenRequest(connexion, L"POST", L"/infos", L"HTTP/1.1",
                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (requete == NULL) {
        fprintf(stderr, "[ERREUR] Creation de la requete HTTP : %lu.\n", GetLastError());
        return;
    }

    // Une seule destination, sans redirection ni authentification automatique.
    if (!WinHttpSetOption(requete, WINHTTP_OPTION_DISABLE_FEATURE, &options, sizeof(options)) ||
        !WinHttpSendRequest(requete, L"Content-Type: text/plain; charset=utf-8\r\n",
                            (DWORD)-1, texte, taille_texte, taille_texte, 0) ||
        !WinHttpReceiveResponse(requete, NULL) ||
        !WinHttpQueryHeaders(requete, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &code_http, &taille_code,
                             WINHTTP_NO_HEADER_INDEX)) {
        fprintf(stderr, "[ERREUR] Envoi HTTP impossible : %lu. Nouvel essai au prochain cycle.\n",
                GetLastError());
    } else if (code_http != 200) {
        fprintf(stderr, "[ERREUR] Le serveur a repondu HTTP %lu.\n", code_http);
    }

    // Seul le statut HTTP est lu : aucune commande n'est recue ou executee.
    WinHttpCloseHandle(requete);
}

int main(void)
{
    WSADATA winsock;
    WCHAR serveur[16];
    char ip_client[16];
    HINTERNET session = NULL;
    char texte[2048];
    int erreur = WSAStartup(MAKEWORD(2, 2), &winsock);

    if (erreur != 0) {
        fprintf(stderr, "[ERREUR] Initialisation reseau : %d.\n", erreur);
        return 1;
    }

    session = WinHttpOpen(L"Lab-Systeme/2.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == NULL || !WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000)) {
        fprintf(stderr, "[ERREUR] Initialisation HTTP : %lu.\n", GetLastError());
        if (session != NULL) WinHttpCloseHandle(session);
        WSACleanup();
        return 1;
    }
    printf("Collecte des informations systeme et envoi HTTP au serveur local "
           "toutes les 30 secondes. Ctrl+C pour arreter.\n");
    fflush(stdout);
    while (1) {
        ULONGLONG debut = GetTickCount64();
        ULONGLONG duree;

        if (trouver_serveur(serveur, sizeof(serveur) / sizeof(serveur[0]), ip_client)) {
            HINTERNET connexion = WinHttpConnect(session, serveur, PORT, 0);

            if (connexion == NULL) {
                fprintf(stderr, "[ERREUR] Preparation de la connexion : %lu.\n", GetLastError());
            } else {
                if (preparer_infos(texte, sizeof(texte), ip_client)) envoyer_infos(connexion, texte);
                WinHttpCloseHandle(connexion);
            }
        } else {
            fprintf(stderr, "[ATTENTE] Serveur introuvable. Nouvel essai dans 30 secondes.\n");
        }

        // Compter la collecte et l'envoi dans l'intervalle de trente secondes.
        duree = GetTickCount64() - debut;
        if (duree < INTERVALLE_MS) {
            Sleep((DWORD)(INTERVALLE_MS - duree));
        }
    }
}
