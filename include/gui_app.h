/**
 * @file gui_app.h
 * @brief État de l'application graphique et fonctions des différents
 *        écrans (menu, héros, éditeur, préparation, combat).
 */

/* [DUCKGEON - SDL] Fichier de la version graphique (./duckgeon) uniquement. */
#ifndef GUI_APP_H
#define GUI_APP_H

#include "gui.h"
#include "donjon.h"
#include "combat.h"
#include "sauvegarde.h"
#include "reseau.h"

/** Écrans de l'application. */
enum Ecran { ECRAN_MENU, ECRAN_HEROS, ECRAN_EDITEUR, ECRAN_EQUIPE, ECRAN_COMBAT, ECRAN_BOUTIQUE,
             ECRAN_BESTIAIRE, ECRAN_EN_LIGNE };

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
    int niveauDuel;                 /**< Duel : 0 = niveaux actuels, sinon niveau imposé */
    int etage;
    int defilement;                 /**< Défilement des héros disponibles   */
} Preparation;

/** Duel en ligne : connexion et héros des deux joueurs. */
typedef struct {
    Reseau net;
    int choisis[COMBAT_MAX_ALLIES]; /**< Nos héros (indices dans le donjon) */
    int nbChoisis;
    int defilement;         /**< Défilement de la liste des héros         */
    FormatDuel format;      /**< Taille des équipes, niveau, graine (hôte) */
    char adresse[64];       /**< IP tapée pour rejoindre                  */
    int saisieAdresse;      /**< 1 si le champ de l'IP a le focus         */
    char ips[160];          /**< Nos adresses à donner à l'autre joueur   */
    int bonjourEnvoye;      /**< Notre équipe est partie                  */
    int bonjourRecu;        /**< L'équipe de l'adversaire est arrivée     */
    int manche;             /**< Numéro du duel (revanches)               */
    Personnage moi[COMBAT_MAX_ALLIES];        /**< Copies envoyées        */
    int nbMoi;
    Personnage adversaire[COMBAT_MAX_ALLIES]; /**< Copies reçues          */
    int nbAdversaire;
} EnLigne;

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
    int ongletBestiaire;    /**< 0 : monstres, 1 : table des éléments     */
    int monstreBestiaire;   /**< Monstre affiché dans le bestiaire        */
    int etageBestiaire;     /**< Étage pour lequel ses stats sont calculées */
    int elementBestiaire;   /**< Élément mis en avant dans la table       */
    EnLigne enLigne;        /**< Duel en ligne                            */
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

/** Carte compacte d'un héros (portrait, nom, classe) ; 1 si cliquée. */
int carteHeros(Gui *g, const Personnage *p, float x, float y, float w, float h, int choisie, int active);

/** Libellé du niveau d'un duel : « Niveaux actuels » ou « Niveau 5 ». */
void libelleNiveauDuel(int niveau, char *buf, int taille);

/** Pour le duel : 1 si `i` est dans l'équipe `equipe` (0 ou 1). */
int dansEquipePrep(const Preparation *prep, int equipe, int i);
void ecranCombat(App *app);
void ecranBoutique(App *app);
void ecranBestiaire(App *app);

/** Ouvre l'éditeur sur l'onglet Statistiques pour répartir les points. */
void ouvrirAmelioration(App *app, int index);

/** Écran du duel en ligne : héberger ou rejoindre par adresse IP. */
void ecranEnLigne(App *app);

/** Ouvre l'écran du duel en ligne. */
void ouvrirEnLigne(App *app);

/** Ouvre l'éditeur sur un nouveau héros (index -1) ou un héros existant. */
void ouvrirEditeur(App *app, int index);

/** Crée l'état graphique d'un combat (NULL si impossible). */
CombatGui *combatGuiCreer(App *app);
/** Crée le duel en ligne entre les équipes app->enLigne.moi et
 *  .adversaire (l'hôte est le joueur 1), pour la manche app->enLigne.manche. */
CombatGui *combatGuiCreerEnLigne(App *app);
/** Libère l'état graphique du combat. */
void combatGuiDetruire(CombatGui *cg);

#endif /* GUI_APP_H */
