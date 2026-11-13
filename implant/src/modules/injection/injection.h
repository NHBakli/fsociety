#ifndef INJECTION_H
#define INJECTION_H

/* Injecte un exécutable dans un processus cible (ex: "explorer.exe").
 * Retourne 0 si succès, -1 si échec. */
int inject_into_process(const char *target_process, const char *exe_path);

#endif