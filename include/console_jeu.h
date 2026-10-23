/**
 * @file console_jeu.h
 * @brief Le jeu complet dans le terminal, sans aucune interface graphique :
 *        exploration des étages en équipe, duel en équipe, bestiaire,
 *        table des éléments, changement d'élément et repos.
 *
 * Ce module utilise le même moteur de combat (combat.h) et les mêmes
 * fichiers de sauvegarde que Duckgeon : on peut commencer une partie dans
 * le terminal et la continuer dans l'interface graphique, et inversement.
 * Il n'inclut rien de SDL.
 */

/* [DONJON - terminal] Fichier de la version terminal (./donjon) uniquement. */
#ifndef CONSOLE_JEU_H
#define CONSOLE_JEU_H

#include "donjon.h"
#include "sauvegarde.h"

/**
 * Explorer le donjon : choix de l'étage (jusqu'au plus haut débloqué) et
 * d'une équipe de 1 à 4 héros, puis combat tour par tour contre les
 * vagues de monstres. Une victoire rapporte XP, objets, points de stats
 * et foie gras, et débloque l'étage suivant (progression sauvegardée).
 */
void menuExplorer(Donjon *d, Progression *prog);

/** Duel en équipe : deux joueurs sur le même clavier (sans conséquence
 *  sur les héros du donjon). */
void menuDuelEquipe(Donjon *d);

/** Bestiaire : fiche de chaque monstre (stats par étage, attaques,
 *  éléments). */
void menuBestiaire(void);

/** Table des éléments : effet sur les attaques, forces et faiblesses. */
void afficherTableElements(void);

/** Change l'élément des attaques d'un personnage. */
void menuElement(Donjon *d);

/** Repos au camp : tous les personnages récupèrent leurs PV. */
void menuRepos(Donjon *d);

#endif /* CONSOLE_JEU_H */
