/**
 * @file donjon.h
 * @brief Tableau dynamique de personnages (Donjon), partagé par la version
 *        console et la version graphique.
 */

/* [COMMUN] Logique partagée par Donjon (terminal) et Duckgeon (SDL).
 * Les blocs qui ne servent qu'à une des deux versions sont marqués
 * [DONJON - terminal] ou [DUCKGEON - SDL]. */
#ifndef DONJON_H
#define DONJON_H

#include "personnage.h"

#define CAPACITE_INITIALE 4

/** Tableau dynamique de personnages. */
typedef struct {
    Personnage *elements; /**< Bloc alloué sur le tas         */
    int taille;           /**< Nombre de personnages présents */
    int capacite;         /**< Nombre de cases allouées       */
} Donjon;

/** Alloue le tableau initial. Retourne 0 si l'allocation échoue. */
int donjonInit(Donjon *d);

/** Libère la mémoire du donjon et remet ses champs à zéro. */
void donjonLiberer(Donjon *d);

/**
 * Retourne un pointeur vers une case libre en fin de tableau, en doublant
 * la capacité si nécessaire. Retourne NULL si la mémoire manque (le
 * tableau existant reste alors intact). La case n'est comptée dans
 * taille qu'une fois remplie par l'appelant (d->taille++).
 */
Personnage *donjonNouvelleCase(Donjon *d);

/* [DUCKGEON - SDL] */
/**
 * Supprime le personnage d'indice index en décalant les suivants.
 * @return 1 si le personnage a été supprimé, 0 si l'indice est invalide.
 */
int donjonSupprimer(Donjon *d, int index);

/** Sauvegarde tout le donjon dans un fichier binaire. Retourne 1 si OK. */
int donjonSauvegarder(const Donjon *d, const char *nomFichier);

/**
 * Remplace le contenu du donjon par celui du fichier (le tableau est
 * réalloué à la bonne taille). Retourne 1 si OK ; en cas d'échec le
 * donjon est inchangé.
 */
int donjonCharger(Donjon *d, const char *nomFichier);

#endif /* DONJON_H */
