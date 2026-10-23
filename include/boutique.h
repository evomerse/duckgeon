/**
 * @file boutique.h
 * @brief Foie gras (monnaie gagnée au combat) et cosmétiques à débloquer.
 *
 * Module de logique pure (aucun affichage) : le catalogue, les prix, les
 * achats et le calcul des récompenses. Le foie gras et les cosmétiques
 * achetés appartiennent au joueur (structure Progression), pas à un héros :
 * un cosmétique débloqué est utilisable par tous les héros.
 */
#ifndef BOUTIQUE_H
#define BOUTIQUE_H

#include "sauvegarde.h"

/** Partie de l'apparence que modifie un cosmétique. */
enum TypeCosmetique { COSMETIQUE_COIFFE, COSMETIQUE_PLUMAGE };

/** Un article de la boutique. */
typedef struct {
    const char *nom;
    const char *description;
    enum TypeCosmetique type;
    unsigned char valeur; /**< Indice de coiffe ou de plumage débloqué */
    int prix;             /**< En foie gras                           */
} Cosmetique;

#define NB_COSMETIQUES 8

/** Bonus de foie gras la première fois qu'un étage est terminé. */
#define FOIE_GRAS_PREMIER_PASSAGE 25

/** Article d'indice i (0..NB_COSMETIQUES-1), NULL si invalide. */
const Cosmetique *cosmetique(int i);

/** Retourne 1 si l'article i a été acheté. */
int cosmetiqueAchete(const Progression *prog, int i);

/**
 * Retourne 1 si cette option d'apparence peut être portée : options de
 * base toujours, options payantes seulement si achetées.
 */
int optionDebloquee(const Progression *prog, enum TypeCosmetique type, int valeur);

/**
 * Achète l'article i.
 * @return 1 si l'achat a réussi, 0 s'il est déjà acheté, invalide ou trop
 *         cher (le foie gras n'est alors pas dépensé).
 */
int acheterCosmetique(Progression *prog, int i);

/**
 * Foie gras gagné en terminant l'étage `etage` (le bonus de premier
 * passage est compté et l'étage est marqué comme terminé).
 */
int recompenseFoieGras(Progression *prog, int etage, int avecBoss);

#endif /* BOUTIQUE_H */
