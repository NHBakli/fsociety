#ifndef NEW_COMMON_H
#define NEW_COMMON_H

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define BAD_SOCKET INVALID_SOCKET
#define close_socket closesocket
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define BAD_SOCKET (-1)
#define close_socket close
#endif
#include <openssl/ssl.h>

#define LAB_SIZE 512
#define AGENT_PORT 4444
#define CONTROLLER_PORT 5555

#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/x509v3.h>
#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#endif

static inline int allowed(const char *cmd)
{
    return !strcmp(cmd, "hostname") || !strcmp(cmd, "os") || !strcmp(cmd, "time");
}

static inline void audit(const char *format, ...)
{
    char stamp[32] = "heure inconnue";
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    if (utc != NULL) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S UTC", utc);
    printf("[%s] ", stamp);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    putchar('\n');
    fflush(stdout);
}

static inline int net_init(void)
{
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return signal(SIGPIPE, SIG_IGN) != SIG_ERR;
#endif
}

static inline int net_timeout(socket_t sock, int seconds)
{
#ifdef _WIN32
    DWORD delay = (DWORD)seconds * 1000;
#else
    struct timeval delay = { seconds, 0 };
#endif
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&delay, sizeof(delay)) == 0
        && setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&delay, sizeof(delay)) == 0;
}

static inline int blocking(socket_t sock, int enabled)
{
#ifdef _WIN32
    u_long mode = !enabled;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    return flags >= 0 && fcntl(sock, F_SETFL, enabled ? flags & ~O_NONBLOCK : flags | O_NONBLOCK) == 0;
#endif
}

// Adresse IPv4 explicite : 127.0.0.1 par defaut dans les trois programmes.
static inline socket_t open_socket(const char *ip, int port, int listener)
{
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) return BAD_SOCKET;
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == BAD_SOCKET) return BAD_SOCKET;
    if (listener) {
        int reuse = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) != 0
            || bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0
            || listen(sock, 4) != 0) goto failed;
    } else {
        if (!blocking(sock, 0)) goto failed;
        int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
        if (result != 0) {
            fd_set writable;
            FD_ZERO(&writable);
            FD_SET(sock, &writable);
            struct timeval delay = {5, 0};
            int error = 0;
#ifdef _WIN32
            int size = sizeof(error);
#else
            socklen_t size = sizeof(error);
#endif
            if (select((int)sock + 1, NULL, &writable, NULL, &delay) <= 0
                || getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &size) != 0
                || error != 0) goto failed;
        }
        if (!blocking(sock, 1) || !net_timeout(sock, 5)) goto failed;
    }
    return sock;
failed:
    close_socket(sock);
    return BAD_SOCKET;
}

static inline socket_t net_listen(const char *ip, int port) { return open_socket(ip, port, 1); }
static inline socket_t net_connect(const char *ip, int port) { return open_socket(ip, port, 0); }

static inline SSL_CTX *tls_context(const char *role, int server)
{
    char cert[128], key[128];
    snprintf(cert, sizeof(cert), "certs/%s.crt", role);
    snprintf(key, sizeof(key), "certs/%s.key", role);
    SSL_CTX *ctx = SSL_CTX_new(TLS_method());
    if (ctx == NULL) return NULL;
    // OpenSSL verifie la signature, la validite et l'identite du pair.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | (server ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0), NULL);
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)
        || SSL_CTX_load_verify_locations(ctx, "certs/ca.crt", NULL) != 1
        || SSL_CTX_use_certificate_chain_file(ctx, cert) != 1
        || SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1
        || SSL_CTX_check_private_key(ctx) != 1) {
        audit("ERROR certificats manquants ou invalides pour %s", role);
        SSL_CTX_free(ctx);
        return NULL;
    }
    return ctx;
}

static inline SSL *tls_session(SSL_CTX *ctx, socket_t sock, int server, const char *peer)
{
    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL) return NULL;
    SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_WILDCARDS);
    if (!net_timeout(sock, 5) || SSL_set_fd(ssl, (int)sock) != 1
        || SSL_set1_host(ssl, peer) != 1
        || (server ? SSL_accept(ssl) : SSL_connect(ssl)) != 1
        || SSL_get_verify_result(ssl) != X509_V_OK) {
        audit("REFUSED authentification TLS : pair attendu %s", peer);
        SSL_free(ssl);
        return NULL;
    }
    return ssl;
}

static inline void tls_close(SSL *ssl, socket_t sock)
{
    if (ssl != NULL) {
        net_timeout(sock, 5);
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if (sock != BAD_SOCKET) close_socket(sock);
}

// Une ligne complete, courte, sans octet nul ni caractere de controle.
static inline int read_line(SSL *ssl, char *buf, size_t size)
{
    for (size_t used = 0; used + 1 < size; used++) {
        char c;
        int n = SSL_read(ssl, &c, 1);
        if (n <= 0) return used == 0 && SSL_get_error(ssl, n) == SSL_ERROR_ZERO_RETURN ? 0 : -1;
        if (c == '\n') { buf[used] = '\0'; return 1; }
        if ((unsigned char)c < 32 || c == 127) return -1;
        buf[used] = c;
    }
    return -1;
}

static inline int write_line(SSL *ssl, const char *text)
{
    char line[LAB_SIZE];
    size_t length = strlen(text);
    if (length + 1 >= sizeof(line) || strpbrk(text, "\r\n") != NULL) return 0;
    memcpy(line, text, length);
    line[length++] = '\n';
    size_t sent = 0;
    while (sent < length) {
        int n = SSL_write(ssl, line + sent, (int)(length - sent));
        if (n <= 0) return 0;
        sent += (size_t)n;
    }
    return 1;
}

#endif
