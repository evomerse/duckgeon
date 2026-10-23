/**
 * @file main.h
 * @brief Déclarations du programme principal : fonctions du menu
 *        interactif (le tableau dynamique est dans donjon.h).
 */
#ifndef MAIN_H
#define MAIN_H

#include "donjon.h"

/* ---- Actions du menu ---- */

/** Affiche tous les personnages du donjon. */
void afficherTous(const Donjon *d);

/** Menu 1 : crée un personnage et l'ajoute au donjon. */
void menuCreer(Donjon *d);

/**
 * Demande à l'utilisateur de choisir un personnage vivant.
 * @param exclu indice à ne pas accepter (-1 pour aucun)
 * @return l'indice choisi, ou -1 en cas d'EOF.
 */
int choisirCombattant(const Donjon *d, const char *invite, int exclu);

/** Menu 3 : attaque simple ou combat jusqu'au K.O. entre deux personnages. */
void menuCombat(Donjon *d);

/** Menu 4 : sauvegarde le donjon dans FICHIER_SAUVEGARDE. */
void menuSauvegarder(const Donjon *d);

/** Menu 5 : remplace le donjon par le contenu de FICHIER_SAUVEGARDE
 *  (après confirmation si des personnages sont déjà présents). */
void menuCharger(Donjon *d);

/** Menu 6 : sous-menu de l'inventaire d'un personnage (K.O. compris : une
 *  potion permet de le ranimer). */
void menuInventaire(Donjon *d);

/** Menu 7 : dépense les points de stats d'un personnage. */
void menuAmeliorer(Donjon *d);

#endif /* MAIN_H */
