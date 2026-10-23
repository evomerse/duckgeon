/**
 * @file inventaire.c
 * @brief Implémentation de la gestion de l'inventaire.
 */
#include <stdio.h>

#include "inventaire.h"

int ajouterObjet(Personnage *p, Objet objet)
{
    if (p == NULL || p->nbObjets >= INVENTAIRE_MAX) {
        return 0;
    }
    p->inventaire[p->nbObjets++] = objet;
    return 1;
}

int retirerObjet(Personnage *p, int index, Objet *retire)
{
    if (p == NULL || index < 0 || index >= p->nbObjets) {
        return 0;
    }
    if (retire != NULL) {
        *retire = p->inventaire[index];
    }
    /* Décale les objets suivants pour garder un tableau sans trou. */
    for (int i = index; i < p->nbObjets - 1; i++) {
        p->inventaire[i] = p->inventaire[i + 1];
    }
    p->nbObjets--;
    return 1;
}

int utiliserObjet(Personnage *p, int index)
{
    Objet objet;
    if (!retirerObjet(p, index, &objet)) {
        return 0;
    }

    if (objet.type == SOIN) {
        int avant = p->pv;
        p->pv += objet.valeur;
        if (p->pv > p->pvMax) {
            p->pv = p->pvMax;
        }
        printf("%s utilise %s : +%d PV (%d/%d).%s\n", p->nom, objet.nom,
               p->pv - avant, p->pv, p->pvMax,
               avant == 0 ? " Il revient au combat !" : "");
    } else {
        /* Échange : l'arme actuelle (s'il y en a une) prend la place libérée
         * par la nouvelle, donc l'inventaire ne peut pas déborder. */
        if (p->arme.nom[0] != '\0') {
            ajouterObjet(p, p->arme);
            printf("%s range %s dans son sac.\n", p->nom, p->arme.nom);
        }
        p->arme = objet;
        printf("%s equipe %s : ATK %d.\n", p->nom, objet.nom, attaqueTotale(p));
    }
    return 1;
}

void afficherInventaire(const Personnage *p)
{
    if (p == NULL) {
        return;
    }
    printf("Arme equipee : ");
    if (p->arme.nom[0] != '\0') {
        afficherObjet(&p->arme);
        printf("\n");
    } else {
        printf("aucune\n");
    }

    printf("Sac (%d/%d) :\n", p->nbObjets, INVENTAIRE_MAX);
    if (p->nbObjets == 0) {
        printf("  (vide)\n");
    }
    for (int i = 0; i < p->nbObjets; i++) {
        printf("  %d. ", i + 1);
        afficherObjet(&p->inventaire[i]);
        printf("\n");
    }
}
