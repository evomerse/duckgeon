/**
 * @file gui_app.h
 * @brief État de l'application graphique et fonctions des différents
 *        écrans (menu, héros, éditeur, préparation, combat).
 */
#ifndef GUI_APP_H
#define GUI_APP_H

#include "gui.h"
#include "donjon.h"
#include "combat.h"
#include "sauvegarde.h"

/** Écrans de l'application. */
enum Ecran { ECRAN_MENU, ECRAN_HEROS, ECRAN_EDITEUR, ECRAN_EQUIPE, ECRAN_COMBAT, ECRAN_BOUTIQUE };

/** Onglets de l'éditeur de personnage. */
enum OngletEditeur { ONGLET_IDENTITE, ONGLET_APPARENCE, ONGLET_STATS, NB_ONGLETS };

/** État de l'éditeur (création ou personnalisation). */
typedef struct {
    Personnage brouillon;   /**< Copie modifiée, validée à la fin         */
    int index;              /**< -1 : nouveau héros, sinon indice édité   */
    enum OngletEditeur onglet;
    int points[4];          /**< Points bonus : PV, ATK, DEF, VIT          */
    int saisieNom;          /**< 1 si le champ du nom a le focus          */
} Editeur;

/** État de l'écran de préparation du combat. */
typedef struct {
    enum ModeCombat mode;           /**< Contre les bots ou duel local      */
    int taille;                     /**< Taille des équipes (1 à 4)         */
    int choisis[2][COMBAT_MAX_ALLIES]; /**< Indices des héros de chaque équipe */
    int nb[2];
    int equipeEditee;               /**< Duel : équipe qui reçoit le héros  */
    int etage;
    int defilement;                 /**< Défilement des héros disponibles   */
} Preparation;

typedef struct CombatGui CombatGui; /* défini dans gui_combat.c */

/** Toute l'application graphique. */
typedef struct {
    Gui g;
    Donjon donjon;          /**< Même tableau dynamique que la console    */
    enum Ecran ecran;
    enum Ecran prochain;    /**< Écran demandé (transition en fondu)      */
    float fondu;            /**< 1 = noir, 0 = transparent                */
    int selection;          /**< Héros sélectionné dans la liste          */
    int confirmerSuppression;
    int defilement;         /**< Défilement de la liste des héros         */
    Editeur editeur;
    Preparation prep;
    CombatGui *combat;      /**< Alloué au lancement d'un combat          */
    Progression prog;       /**< Étage débloqué, foie gras, cosmétiques   */
    int articleBoutique;    /**< Article sélectionné dans la boutique     */
} App;

/** Demande un changement d'écran (avec fondu). */
void changerEcran(App *app, enum Ecran e);

/** Fond animé commun (dégradé, étoiles, brume). */
void dessinerFond(Gui *g, SDL_Color haut, SDL_Color bas);

/** Titre d'écran avec bouton retour ; retourne 1 si « Retour » cliqué. */
int enTete(App *app, const char *titre, const char *sousTitre);

/* Écrans : chacun dessine une image et traite les entrées. */
void ecranMenu(App *app);
void ecranHeros(App *app);
void ecranEditeur(App *app);
void ecranPreparation(App *app);

/** Pour le duel : 1 si `i` est dans l'équipe `equipe` (0 ou 1). */
int dansEquipePrep(const Preparation *prep, int equipe, int i);
void ecranCombat(App *app);
void ecranBoutique(App *app);

/** Ouvre l'éditeur sur l'onglet Statistiques pour répartir les points. */
void ouvrirAmelioration(App *app, int index);

/** Ouvre l'éditeur sur un nouveau héros (index -1) ou un héros existant. */
void ouvrirEditeur(App *app, int index);

/** Crée l'état graphique d'un combat (NULL si impossible). */
CombatGui *combatGuiCreer(App *app);
/** Libère l'état graphique du combat. */
void combatGuiDetruire(CombatGui *cg);

#endif /* GUI_APP_H */
