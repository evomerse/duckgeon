/**
 * @file saisie.h
 * @brief Fonctions de saisie clavier sécurisées (pas de scanf brut).
 */
#ifndef SAISIE_H
#define SAISIE_H

#include <stddef.h>

/**
 * Lit une ligne sur stdin dans buffer (taille octets max, '\n' retiré).
 * Vide le reste de la ligne si elle est trop longue.
 * @return 1 si une ligne a été lue, 0 en cas de fin de fichier (EOF).
 */
int lireLigne(char *buffer, size_t taille);

/**
 * Demande un entier compris entre min et max (inclus) jusqu'à obtenir
 * une saisie valide.
 * @return l'entier saisi, ou -1 si l'entrée standard est fermée (EOF).
 */
int lireEntier(const char *invite, int min, int max);

#endif /* SAISIE_H */
