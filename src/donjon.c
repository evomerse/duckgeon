/**
 * @file donjon.c
 * @brief Implémentation du tableau dynamique de personnages.
 */

/* [COMMUN] Logique partagée par Donjon (terminal) et Duckgeon (SDL).
 * Les blocs qui ne servent qu'à une des deux versions sont marqués
 * [DONJON - terminal] ou [DUCKGEON - SDL]. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#include "donjon.h"
#include "sauvegarde.h"

int donjonInit(Donjon *d)
{
    if (d == NULL) {
        return 0;
    }
    d->elements = malloc(CAPACITE_INITIALE * sizeof *d->elements);
    if (d->elements == NULL) {
        perror("malloc");
        return 0;
    }
    d->taille = 0;
    d->capacite = CAPACITE_INITIALE;
    return 1;
}

void donjonLiberer(Donjon *d)
{
    if (d == NULL) {
        return;
    }
    free(d->elements);
    d->elements = NULL; /* évite un pointeur pendant (dangling) */
    d->taille = 0;
    d->capacite = 0;
}

Personnage *donjonNouvelleCase(Donjon *d)
{
    if (d == NULL || d->taille < 0 || d->capacite < 0 || d->taille > d->capacite
        || (d->elements == NULL && d->capacite > 0)) {
        return NULL; /* donjon pas initialisé ou incohérent */
    }
    if (d->taille == d->capacite) {
        /* Capacité 0 (donjon libéré ou jamais initialisé) : 0 * 2 = 0, on
         * écrirait hors du tableau. On repart donc de la capacité initiale. */
        if (d->capacite > INT_MAX / 2
            || (size_t)d->capacite * 2 > SIZE_MAX / sizeof *d->elements) {
            return NULL; /* la multiplication déborderait */
        }
        int nouvelleCapacite = d->capacite > 0 ? d->capacite * 2 : CAPACITE_INITIALE;
        /* On passe par un pointeur temporaire : si realloc échoue, l'ancien
         * bloc est conservé et on ne le perd pas (pas de fuite). */
        Personnage *tmp = realloc(d->elements, (size_t)nouvelleCapacite * sizeof *tmp);
        if (tmp == NULL) {
            perror("realloc");
            return NULL;
        }
        d->elements = tmp;
        d->capacite = nouvelleCapacite;
    }
    return &d->elements[d->taille];
}

/* [DUCKGEON - SDL] */
int donjonSupprimer(Donjon *d, int index)
{
    if (d == NULL || d->elements == NULL || index < 0 || index >= d->taille) {
        return 0;
    }
    for (int i = index; i < d->taille - 1; i++) {
        d->elements[i] = d->elements[i + 1];
    }
    d->taille--;
    return 1;
}

int donjonSauvegarder(const Donjon *d, const char *nomFichier)
{
    if (d == NULL) {
        return 0;
    }
    return sauvegarderFichier(nomFichier, d->elements, d->taille);
}

int donjonCharger(Donjon *d, const char *nomFichier)
{
    if (d == NULL || !chargerFichier(nomFichier, &d->elements, &d->taille)) {
        return 0;
    }
    /* charger() alloue exactement max(n, 1) cases : la capacité suit. */
    d->capacite = d->taille > 0 ? d->taille : 1;
    return 1;
}
