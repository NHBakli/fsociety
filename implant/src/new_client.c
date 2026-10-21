#include "new_common.h"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#else
#include <sys/utsname.h>
#endif
#include <stdio.h>
#include <string.h>
#include <time.h>

static void diagnostic(const char *cmd, char *output, size_t size)
{
    // Cette verification reste dans l'agent, meme si le serveur filtre aussi.
    if (!allowed(cmd)) {
        snprintf(output, size, "REFUSED diagnostic inconnu");
        return;
    }
    if (!strcmp(cmd, "time")) {
        time_t now = time(NULL);
        struct tm *utc = now == (time_t)-1 ? NULL : gmtime(&now);
        if (!utc || !strftime(output, size, "OK %Y-%m-%d %H:%M:%S UTC", utc))
            snprintf(output, size, "ERROR heure indisponible");
        return;
    }
#ifdef _WIN32
    if (!strcmp(cmd, "hostname")) {
        char name[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD length = sizeof(name);
        if (GetComputerNameA(name, &length))
            snprintf(output, size, "OK %s", name);
        else
            snprintf(output, size, "ERROR nom de machine indisponible");
    } else {
        char build[32];
        DWORD length = sizeof(build);
        if (RegGetValueA(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuildNumber",
                RRF_RT_REG_SZ, NULL, build, &length) == ERROR_SUCCESS)
            snprintf(output, size, "OK Windows build %s", build);
        else
            snprintf(output, size, "ERROR version Windows indisponible");
    }
#else
    struct utsname info;
    if (uname(&info) != 0)
        snprintf(output, size, "ERROR informations systeme indisponibles");
    else if (!strcmp(cmd, "hostname"))
        snprintf(output, size, "OK %s", info.nodename);
    else
        snprintf(output, size, "OK %s %s", info.sysname, info.release);
#endif
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [adresse_IPv4_du_serveur]\n", argv[0]);
        return 1;
    }
    if (!net_init()) return 1;
    SSL_CTX *ctx = tls_context("agent", 0);
    if (!ctx) return 1;
    socket_t sock = net_connect(argc == 2 ? argv[1] : "127.0.0.1", AGENT_PORT);
    SSL *ssl = NULL;
    int result = 1;
    char command[LAB_SIZE], output[LAB_SIZE];
    if (sock == BAD_SOCKET) {
        audit("Connexion au serveur impossible");
        goto cleanup;
    }
    if (!net_timeout(sock, 5)) goto cleanup;
    ssl = tls_session(ctx, sock, 0, "server.lab");
    if (!ssl) goto cleanup;

    // Le serveur confirme aussi qu'il a accepte le certificat de l'agent.
    if (read_line(ssl, command, sizeof(command)) != 1 || strcmp(command, "READY")) {
        audit("Authentification refusee ou confirmation absente");
        goto cleanup;
    }
    audit("Serveur et agent authentifies");
    // Attendre une demande sans interroger en boucle.
    if (!net_timeout(sock, 0)) goto cleanup;
    int received;
    while ((received = read_line(ssl, command, sizeof(command))) == 1) {
        diagnostic(command, output, sizeof(output));
        audit("Diagnostic : %s", allowed(command) ? command : "refuse");
        if (!net_timeout(sock, 5) || !write_line(ssl, output)) {
            audit("Envoi du resultat impossible");
            goto cleanup;
        }
        if (!net_timeout(sock, 0)) goto cleanup;
    }
    audit(received == 0 ? "Serveur deconnecte" : "Connexion interrompue ou message invalide");
    result = received == 0 ? 0 : 1;

cleanup:
    if (ssl) tls_close(ssl, sock);
    else if (sock != BAD_SOCKET) close_socket(sock);
    SSL_CTX_free(ctx);
    return result;
}
