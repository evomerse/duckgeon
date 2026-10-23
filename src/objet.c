/**
 * @file objet.c
 * @brief Table de butin et affichage des objets.
 */
#include <stdio.h>
#include <stdlib.h>

#include "objet.h"

/* Butin possible après une victoire. */
static const Objet TABLE_BUTIN[] = {
    { "Potion mineure",  SOIN, 30 },
    { "Grande potion",   SOIN, 60 },
    { "Dague rouillee",  ARME,  2 },
    { "Epee longue",     ARME,  5 },
    { "Baton runique",   ARME,  7 },
    { "Hache de guerre", ARME,  9 },
};
#define NB_BUTIN ((int)(sizeof TABLE_BUTIN / sizeof TABLE_BUTIN[0]))

Objet objetAleatoire(void)
{
    return TABLE_BUTIN[rand() % NB_BUTIN];
}

Objet objetDepart(void)
{
    return TABLE_BUTIN[0]; /* une potion mineure */
}

void afficherObjet(const Objet *o)
{
    if (o == NULL) {
        return;
    }
    if (o->type == SOIN) {
        printf("%-16s (soin +%d PV)", o->nom, o->valeur);
    } else {
        printf("%-16s (arme +%d ATK)", o->nom, o->valeur);
    }
}
