/**
 * @file donjon.c
 * @brief Implémentation du tableau dynamique de personnages.
 */
#include <stdio.h>
#include <stdlib.h>

#include "donjon.h"
#include "sauvegarde.h"

int donjonInit(Donjon *d)
{
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
    free(d->elements);
    d->elements = NULL; /* évite un pointeur pendant (dangling) */
    d->taille = 0;
    d->capacite = 0;
}

Personnage *donjonNouvelleCase(Donjon *d)
{
    if (d->taille == d->capacite) {
        int nouvelleCapacite = d->capacite * 2;
        /* On passe par un pointeur temporaire : si realloc échoue, l'ancien
         * bloc est conservé et on ne le perd pas (pas de fuite). */
        Personnage *tmp = realloc(d->elements, nouvelleCapacite * sizeof *tmp);
        if (tmp == NULL) {
            perror("realloc");
            return NULL;
        }
        d->elements = tmp;
        d->capacite = nouvelleCapacite;
    }
    return &d->elements[d->taille];
}

int donjonSupprimer(Donjon *d, int index)
{
    if (index < 0 || index >= d->taille) {
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
    return sauvegarderFichier(nomFichier, d->elements, d->taille);
}

int donjonCharger(Donjon *d, const char *nomFichier)
{
    if (!chargerFichier(nomFichier, &d->elements, &d->taille)) {
        return 0;
    }
    /* charger() alloue exactement max(n, 1) cases : la capacité suit. */
    d->capacite = d->taille > 0 ? d->taille : 1;
    return 1;
}
