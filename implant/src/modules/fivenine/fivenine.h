#ifndef FIVENINE_H
#define FIVENINE_H

/* Installe la persistance (choix du mécanisme). Retourne 0 si OK. */
int fivenine_install(void);

/* Retire toutes les entrées de persistance créées. Retourne 0 si OK. */
int fivenine_uninstall(void);

/* Vérifie si la persistance est déjà installée. */
int fivenine_is_installed(void);

#endif