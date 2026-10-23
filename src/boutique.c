/**
 * @file boutique.c
 * @brief Catalogue des cosmétiques, achats et récompenses en foie gras.
 */

/* [COMMUN] Logique partagée par Donjon (terminal) et Duckgeon (SDL).
 * Les blocs qui ne servent qu'à une des deux versions sont marqués
 * [DONJON - terminal] ou [DUCKGEON - SDL]. */
#include <stddef.h>

#include "boutique.h"

/* Les indices de coiffe/plumage suivent ceux de base (NB_*_BASE) : ce sont
 * les options supplémentaires dessinées par l'interface graphique. */
static const Cosmetique CATALOGUE[NB_COSMETIQUES] = {
    { "Toque de chef", "Le chef, c'est vous.", COSMETIQUE_COIFFE, NB_COIFFES_BASE + 0, 30 },
    { "Haut-de-forme", "Un canard de bonne famille.", COSMETIQUE_COIFFE, NB_COIFFES_BASE + 1, 50 },
    { "Bicorne de pirate", "À l'abordage de la mare !", COSMETIQUE_COIFFE, NB_COIFFES_BASE + 2, 70 },
    { "Casque viking", "Des cornes et du courage.", COSMETIQUE_COIFFE, NB_COIFFES_BASE + 3, 90 },
    { "Couronne du Néant", "Volée au Canard Noir.", COSMETIQUE_COIFFE, NB_COIFFES_BASE + 4, 160 },
    { "Plumage laqué", "Brillant comme un canard laqué.", COSMETIQUE_PLUMAGE, NB_PLUMAGES_BASE + 0, 40 },
    { "Plumage doré", "Le canard aux plumes d'or.", COSMETIQUE_PLUMAGE, NB_PLUMAGES_BASE + 1, 100 },
    { "Plumage du Néant", "Noir violacé, plein d'étoiles.", COSMETIQUE_PLUMAGE, NB_PLUMAGES_BASE + 2, 150 },
};

/* [DUCKGEON - SDL] Boutique de cosmétiques : uniquement dans Duckgeon
 * (dans le terminal on gagne le foie gras mais on le dépense ici). */
const Cosmetique *cosmetique(int i)
{
    return (i >= 0 && i < NB_COSMETIQUES) ? &CATALOGUE[i] : NULL;
}

int cosmetiqueAchete(const Progression *prog, int i)
{
    return prog != NULL && i >= 0 && i < NB_COSMETIQUES && (prog->cosmetiques & (1u << i)) != 0;
}

int optionDebloquee(const Progression *prog, enum TypeCosmetique type, int valeur)
{
    int base = (type == COSMETIQUE_COIFFE) ? NB_COIFFES_BASE : NB_PLUMAGES_BASE;
    if (valeur < base) {
        return 1;
    }
    for (int i = 0; i < NB_COSMETIQUES; i++) {
        if (CATALOGUE[i].type == type && CATALOGUE[i].valeur == valeur) {
            return cosmetiqueAchete(prog, i);
        }
    }
    return 0; /* option inconnue */
}

int acheterCosmetique(Progression *prog, int i)
{
    if (prog == NULL || i < 0 || i >= NB_COSMETIQUES || cosmetiqueAchete(prog, i)
        || prog->foieGras < CATALOGUE[i].prix) {
        return 0;
    }
    prog->foieGras -= CATALOGUE[i].prix;
    prog->cosmetiques |= 1u << i;
    return 1;
}

/* [COMMUN] Les deux versions donnent du foie gras après un étage. */
int recompenseFoieGras(Progression *prog, int etage, int avecBoss)
{
    /* Étage 0 : 1u << -1, décalage interdit en C. On borne comme le moteur. */
    if (prog == NULL) {
        return 0;
    }
    if (etage < 1) etage = 1;
    if (etage > 32) etage = 32;
    int gain = 4 + 3 * etage;
    if (avecBoss) {
        gain *= 2;
    }
    unsigned bit = 1u << (etage - 1);
    if (!(prog->etagesNettoyes & bit)) {
        gain += FOIE_GRAS_PREMIER_PASSAGE;
        prog->etagesNettoyes |= bit;
    }
    prog->foieGras += gain;
    return gain;
}
