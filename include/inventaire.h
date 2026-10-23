/**
 * @file inventaire.h
 * @brief Gestion de l'inventaire d'un personnage.
 */

/* [COMMUN] Logique partagée par Donjon (terminal) et Duckgeon (SDL).
 * Les blocs qui ne servent qu'à une des deux versions sont marqués
 * [DONJON - terminal] ou [DUCKGEON - SDL]. */
#ifndef INVENTAIRE_H
#define INVENTAIRE_H

#include "personnage.h"

/**
 * Ajoute un objet à l'inventaire.
 * @return 1 si l'objet a été ajouté, 0 si l'inventaire est plein.
 */
int ajouterObjet(Personnage *p, Objet objet);

/**
 * Retire l'objet d'indice index (les suivants sont décalés).
 * @return 1 si l'objet a été retiré, 0 si l'indice est invalide.
 */
int retirerObjet(Personnage *p, int index, Objet *retire);

/**
 * Utilise l'objet d'indice index :
 *  - SOIN : rend des PV (sans dépasser le max, ranime un K.O.), consommé ;
 *  - ARME : équipée, l'ancienne arme retourne dans l'inventaire.
 * @return 1 si l'objet a été utilisé, 0 sinon.
 */
int utiliserObjet(Personnage *p, int index);

/* [DONJON - terminal] */
/** Affiche l'arme équipée et le contenu de l'inventaire. */
void afficherInventaire(const Personnage *p);

#endif /* INVENTAIRE_H */
