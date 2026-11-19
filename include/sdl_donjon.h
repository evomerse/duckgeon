/**
 * @file sdl_donjon.h
 * @brief Donjon SDL (partie 3) : la version graphique SIMPLE du terminal.
 *
 * Reprend le menu de ./donjon (créer, afficher, combat, sauvegarde,
 * inventaire, stats, élément, bestiaire, éléments, repos) dans une
 * fenêtre SDL2. C'est un programme à part (./donjon_sdl) : il partage avec
 * le terminal et avec Duckgeon uniquement la logique de jeu (personnage,
 * combat, sauvegarde) et les mêmes fichiers donjon.sav / progression.sav,
 * mais aucun fichier d'interface de Duckgeon (gui_*.c).
 */

/* [DONJON SDL - partie 3] */
#ifndef SDL_DONJON_H
#define SDL_DONJON_H

/**
 * Ouvre la fenêtre et fait tourner le jeu jusqu'à la fermeture.
 * @return 0 si tout s'est bien passé, 1 si la fenêtre n'a pas pu s'ouvrir.
 */
int donjonSdl(void);

#endif /* SDL_DONJON_H */
