/**
 * @file sdl_combat.h
 * @brief Écran de combat de Donjon SDL (partie 3) : le moteur de combat
 *        en équipe (combat.h, le même que le terminal) affiché avec des
 *        cartes, plus un mode de combat automatique.
 */

/* [DONJON SDL - partie 3] */
#ifndef SDL_COMBAT_H
#define SDL_COMBAT_H

#include "combat.h"
#include "sauvegarde.h"
#include "sdl_ui.h"

#define FLOTTANTS_MAX 24

/** Petit texte qui monte au-dessus d'une carte (dégâts, soins...). */
typedef struct {
    char texte[32];
    SDL_Color couleur;
    int x, y;
    double age; /**< Secondes écoulées ; -1 = case libre */
} Flottant;

/** Tout ce qu'il faut pour afficher et piloter un combat. */
typedef struct {
    Combat *cb;            /**< Alloué sur le tas (grosse structure)      */
    int etage;
    int automatique;       /**< 1 : les héros jouent seuls                */
    int rapide;            /**< 1 : animations deux fois plus courtes     */
    enum TypeAction action;/**< Action choisie (tour manuel)              */
    int lanceur;           /**< Héros qui lancera l'action (ultime : n'importe lequel) */
    int cible;             /**< Cible sélectionnée                        */
    int acteur;            /**< Dernier combattant à avoir agi (animation) */
    double lunge;          /**< Temps restant de son élan vers l'avant    */
    double secousse[COMBAT_MAX]; /**< Temps restant de la secousse d'un coup reçu */
    double attente;        /**< Pause avant la prochaine étape (secondes) */
    int recompenseFaite;   /**< 1 : la victoire a déjà été récompensée    */
    int foieGras;          /**< Foie gras gagné                           */
    int etageDebloque;     /**< Étage débloqué par la victoire (0 sinon)  */
    Flottant flottants[FLOTTANTS_MAX];
} CombatUi;

/**
 * Prépare un combat de l'équipe (pointeurs vers le donjon) à l'étage donné.
 * @return 1 si le combat commence, 0 sinon (mémoire ou équipe K.O.).
 */
int combatUiDemarrer(CombatUi *c, Personnage *equipe[], int n, int etage, int automatique);

/** Libère le combat. */
void combatUiLiberer(CombatUi *c);

/**
 * Fait avancer le combat de dt secondes, traite les touches (Q, E, 1-4,
 * flèches, Espace, A, Échap) et le dessine.
 * @return 1 quand le joueur quitte l'écran (fuite ou retour après la fin).
 */
int combatUiImage(CombatUi *c, Ui *ui, Progression *prog, double dt);

#endif /* SDL_COMBAT_H */
