/**
 * @file personnage.h
 * @brief Déclarations du module Personnage : structure, classes et fonctions
 *        de création, d'affichage, de combat et de progression (XP).
 */
#ifndef PERSONNAGE_H
#define PERSONNAGE_H

#include "objet.h"

#define NOM_MAX 30
#define INVENTAIRE_MAX 5

/** Classes jouables dans le donjon (NB_CLASSES sert de borne). */
enum Classe {
    GUERRIER, MAGE, VOLEUR,                       /* classes du sujet    */
    PALADIN, ARCHER, PRETRE, NECROMANCIEN, BARDE, /* classes ajoutées    */
    NB_CLASSES
};

/** Éléments (type de dégâts) utilisés par le combat de l'interface
 *  graphique : un ennemi faible à l'élément d'un héros voit sa barre de
 *  résistance diminuer, puis se « brise ». */
enum Element {
    PHYSIQUE, FEU, GLACE, FOUDRE, VENT, QUANTIQUE, IMAGINAIRE,
    NB_ELEMENTS
};

/* Nombre de variantes pour chaque option de l'apparence. */
/* Les options au-delà de *_BASE sont des cosmétiques à débloquer avec du
 * foie gras (boutique de l'interface graphique, voir boutique.h). */
#define NB_PLUMAGES_BASE 8
#define NB_PLUMAGES     11
#define NB_COUPES       10
#define NB_COULEURS     18 /* palette partagée : cheveux, yeux, tenue, aura */
#define NB_COIFFES_BASE 10
#define NB_COIFFES      15
#define NB_PILOSITES     4
#define NB_MARQUES       5
#define NB_CORPULENCES   3
#define NB_REGARDS       4

/**
 * Apparence d'un personnage. On ne stocke que des indices (pas de couleurs
 * RGB) : la logique métier ne dépend pas de l'affichage, c'est l'interface
 * graphique qui traduit chaque indice en couleur ou en forme.
 */
typedef struct {
    unsigned char plumage;        /**< Couleur des plumes (nos héros sont des canards) */
    unsigned char coupe;          /**< Houppette (plumes du crâne)           */
    unsigned char couleurCheveux; /**< Indice dans la palette                */
    unsigned char couleurYeux;    /**< Indice dans la palette                */
    unsigned char regard;         /**< Forme des yeux                        */
    unsigned char tenue;          /**< Couleur principale de la tenue        */
    unsigned char tenue2;         /**< Couleur secondaire (bordures, cape)   */
    unsigned char coiffe;         /**< Couvre-chef / accessoire de tête      */
    unsigned char pilosite;       /**< Barbe, moustache...                   */
    unsigned char marque;         /**< Cicatrice, peinture de guerre...      */
    unsigned char cape;           /**< 1 = porte une cape                    */
    unsigned char aura;           /**< 0 = aucune, sinon couleur + 1         */
    unsigned char corpulence;     /**< Mince / normal / massif               */
} Apparence;

/**
 * Un personnage du donjon.
 *
 * Les 5 premiers champs sont ceux imposés par le cahier des charges ; les
 * suivants ajoutent l'expérience, l'inventaire et la personnalisation
 * (exercices supplémentaires). La structure ne contient aucun pointeur :
 * c'est un bloc contigu.
 */
struct Personnage {
    char nom[NOM_MAX];   /**< Nom du personnage (chaîne terminée par '\0') */
    int pv;              /**< Points de vie actuels                         */
    int atk;             /**< Attaque de base (hors arme)                   */
    int def;             /**< Défense (réduit les dégâts reçus)             */
    enum Classe classe;  /**< Classe du personnage                          */

    int pvMax;                        /**< PV maximum (augmente par niveau) */
    int niveau;                       /**< Niveau actuel, commence à 1      */
    int xp;                           /**< XP accumulée dans le niveau      */
    Objet arme;                       /**< Arme équipée (nom "" = aucune)   */
    Objet inventaire[INVENTAIRE_MAX]; /**< Objets transportés               */
    int nbObjets;                     /**< Nombre de cases utilisées        */

    int vit;                 /**< Vitesse : fixe l'ordre des tours       */
    enum Element element;    /**< Élément des attaques                   */
    Apparence apparence;     /**< Personnalisation visuelle              */

    /* Ajoutés en version 2 de la sauvegarde (toujours en fin de structure :
     * un ancien fichier se relit en complétant ces champs par des 0). */
    int pointsStats;         /**< Points gagnés, à répartir (ameliorerStat) */
    int victoires;           /**< Combats ou duels remportés             */
};
typedef struct Personnage Personnage;

/* Points de personnalisation à répartir à la création (interface
 * graphique) et gain apporté par chaque point. */
#define POINTS_BONUS    10
#define BONUS_PV_POINT   8
#define BONUS_ATK_POINT  1
#define BONUS_DEF_POINT  1
#define BONUS_VIT_POINT  3

/* Points de statistiques gagnés en jouant. */
#define POINTS_PAR_NIVEAU    2 /* à chaque montée de niveau        */
#define POINTS_PAR_VICTOIRE  1 /* à chaque victoire (K.O. console,  */
                               /* étage nettoyé en graphique)       */

/** Statistiques améliorables avec un point. */
enum Stat { STAT_PV, STAT_ATK, STAT_DEF, STAT_VIT, NB_STATS };

/**
 * Remplit le personnage pointé par p en demandant nom et classe à
 * l'utilisateur. Les statistiques sont initialisées selon la classe.
 */
void creerPersonnage(Personnage *p);

/** Affiche une ligne décrivant le personnage (lecture seule : const). */
void afficherPersonnage(const Personnage *p);

/**
 * L'attaquant frappe la cible : les PV de la cible sont modifiés
 * directement grâce au pointeur. Les PV ne descendent jamais sous 0.
 * Si la cible tombe K.O., l'attaquant gagne de l'XP et un objet.
 */
void attaquer(Personnage *attaquant, Personnage *cible);

/* ---- Fonctions utilitaires ---- */

/**
 * Initialise un personnage niveau 1 de la classe donnée (nom inchangé) :
 * statistiques, élément de la classe, apparence aléatoire, potion.
 */
void initialiserStats(Personnage *p, enum Classe classe);

/** Retourne le libellé texte d'une classe ("Guerrier", "Mage", ...). */
const char *nomClasse(enum Classe classe);

/** Retourne une phrase décrivant le rôle et la particularité de la classe. */
const char *descriptionClasse(enum Classe classe);

/** Retourne le libellé d'un élément ("Feu", "Glace", ...). */
const char *nomElement(enum Element element);

/** Élément par défaut d'une classe. */
enum Element elementClasse(enum Classe classe);

/** Chance de coup critique de la classe, en pourcentage. */
int chanceCritique(const Personnage *p);

/** Tire une apparence au hasard (toutes les options sont valides). */
void apparenceAleatoire(Apparence *a);

/** Retourne 1 si le personnage est encore en vie, 0 sinon. */
int estVivant(const Personnage *p);

/** Attaque totale : attaque de base + bonus de l'arme équipée. */
int attaqueTotale(const Personnage *p);

/** XP nécessaire pour passer au niveau suivant. */
int xpPourNiveauSuivant(const Personnage *p);

/**
 * Ajoute de l'XP au personnage et applique les montées de niveau
 * (plusieurs d'un coup si besoin) : +PV max, +ATK, +DEF, soin complet.
 */
void gagnerXp(Personnage *p, int xp);

/** Libellé d'une statistique ("Points de vie", "Attaque"...). */
const char *nomStat(enum Stat s);

/** Gain apporté par un point dans cette statistique (8 PV, 1 ATK...). */
int gainStat(enum Stat s);

/**
 * Dépense un point de statistiques pour améliorer `s`. Les PV gagnés sont
 * aussi rendus (pas seulement le maximum).
 * @return 1 si le point a été dépensé, 0 s'il n'en reste aucun.
 */
int ameliorerStat(Personnage *p, enum Stat s);

#endif /* PERSONNAGE_H */
