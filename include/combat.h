/**
 * @file combat.h
 * @brief Moteur de combat tour par tour en équipe (inspiré de Honkai: Star
 *        Rail), utilisé par l'interface graphique.
 *
 * Principes :
 *  - Ordre des tours par « valeur d'action » : chaque combattant a un
 *    compteur AV = 10000 / vitesse ; celui dont le compteur est le plus bas
 *    joue, et ce temps est retiré à tous les autres.
 *  - Points de compétence partagés par l'équipe (max 5) : l'attaque de
 *    base en rapporte 1, la compétence en coûte 1.
 *  - Énergie : chaque action et chaque coup reçu en donne ; jauge pleine =
 *    ultime, utilisable à tout moment pendant le tour du joueur.
 *  - Éléments : un élément fort contre celui de la cible fait +25 % de
 *    dégâts (faible : -20 %) et entame sa résistance ; à 0 la cible est
 *    « brisée » (dégâts bonus, action retardée et effet selon l'élément).
 *    Chaque attaque d'un héros porte aussi l'effet de son élément
 *    (brûlure, givre...) : changer d'élément change ses attaques.
 *
 * Deux modes : contre les bots (vagues de monstres pilotés par une IA) ou
 * duel local (deux équipes de héros sur le même PC). Les cases 0..3 forment
 * le camp 0 (joueur 1), les cases 4..8 le camp 1 (monstres ou joueur 2).
 *
 * Ce module ne fait aucun affichage : il décrit chaque action par une
 * liste d'effets que l'interface anime.
 */

/* [COMMUN] Logique partagée par Donjon (terminal) et Duckgeon (SDL).
 * Les blocs qui ne servent qu'à une des deux versions sont marqués
 * [DONJON - terminal] ou [DUCKGEON - SDL]. */
#ifndef COMBAT_H
#define COMBAT_H

#include "personnage.h"

#define COMBAT_MAX_ALLIES   4
#define COMBAT_MAX_ENNEMIS  5
#define COMBAT_MAX          (COMBAT_MAX_ALLIES + COMBAT_MAX_ENNEMIS)
#define PREMIER_ENNEMI      COMBAT_MAX_ALLIES /* indice du 1er ennemi */
#define SP_MAX              5
#define AV_BASE             10000.0
#define ETAGE_MAX           10
#define EFFETS_MAX          48
#define JOURNAL_MAX         6
#define JOURNAL_LONGUEUR    96

/** Modes de jeu. */
enum ModeCombat { MODE_BOTS, MODE_DUEL };

/** Les trois actions d'un héros. */
enum TypeAction { ACTION_BASIQUE, ACTION_COMPETENCE, ACTION_ULTIME, NB_ACTIONS };

/** Qui une capacité peut viser. */
enum Ciblage {
    CIBLE_ENNEMI,       /**< un ennemi                            */
    CIBLE_ZONE,         /**< un ennemi + ses voisins (dégâts réduits) */
    CIBLE_TOUS_ENNEMIS, /**< tous les ennemis                     */
    CIBLE_ALLIE,        /**< un allié                             */
    CIBLE_TOUS_ALLIES,  /**< toute l'équipe                       */
};

/** Description d'une capacité (pour l'affichage). */
typedef struct {
    const char *nom;
    const char *description;
    enum Ciblage ciblage;
} Capacite;

/** Monstres du donjon (les quatre derniers sont des boss). */
enum Monstre {
    MONSTRE_GELEE, MONSTRE_SQUELETTE, MONSTRE_LOUP, MONSTRE_GOLEM,
    MONSTRE_SPECTRE, MONSTRE_DRAGON,
    MONSTRE_CHAMPIGNON, MONSTRE_DIABLOTIN, MONSTRE_GARGOUILLE,
    MONSTRE_ROI_GELEE, MONSTRE_LICHE, MONSTRE_CANARD_NOIR,
    NB_MONSTRES
};

#define ETAGE_POOL_MAX 6

/** Ce que fait l'attaque spéciale d'un monstre. */
enum SpecialMonstre {
    SPECIAL_ZONE, SPECIAL_FORT, SPECIAL_RAGE, SPECIAL_TOUS, SPECIAL_POISON,
    SPECIAL_INVOCATION, /**< appelle un monstre en renfort (Roi des Gelées) */
    SPECIAL_DRAIN,      /**< frappe tout le monde et se soigne (Liche)       */
    SPECIAL_NEANT       /**< frappe tout le monde, DEF -30 % (Canard Noir)   */
};

/** Fiche d'un monstre (bestiaire) : statistiques de base, avant
 *  l'adaptation à l'étage et à l'équipe (voir statsMonstre). */
typedef struct {
    const char *nom;
    const char *description;   /**< Une phrase d'ambiance                   */
    int pv, atk, def, vit, resistance;
    unsigned faiblessesBonus;  /**< Faiblesses en plus de celles de l'élément */
    enum Element element;
    const char *attaque;       /**< Attaque normale                         */
    const char *special;       /**< Attaque spéciale                        */
    const char *effetSpecial;  /**< Ce que fait l'attaque spéciale          */
    enum SpecialMonstre typeSpecial;
    int chanceSpecial;         /**< En %                                    */
    const char *etat;          /**< Altération infligée (poison...) ou NULL */
    int boss;
} InfoMonstre;

/** Description d'un étage du donjon. */
typedef struct {
    const char *nom;          /**< « Les Caves humides »...               */
    const char *description;  /**< Une phrase d'ambiance                  */
    int nbVagues;
    int taillePool;
    enum Monstre pool[ETAGE_POOL_MAX]; /**< Monstres qui peuvent apparaître */
    enum Monstre boss;        /**< Boss de la dernière vague, ou NB_MONSTRES */
} InfoEtage;

/** Un participant au combat (héros ou monstre). */
typedef struct {
    int present;           /**< 1 si la case est utilisée                  */
    int heros;             /**< 1 : héros (joué par un humain), 0 : monstre */
    Personnage *p;         /**< Mode bots : pointe dans le donjon ; monstre
                                ou duel : pointe sur `stock`               */
    Personnage stock;      /**< Copie locale (monstre, héros en duel)      */
    enum Monstre monstre;  /**< Type de monstre (ennemis seulement)        */
    int boss;              /**< 1 pour le boss de fin d'étage              */
    int phase;             /**< Phase du boss (1, puis 2 sous 50 % PV)     */

    double av;             /**< Valeur d'action restante avant de jouer    */
    int energie;           /**< Énergie actuelle (héros)                   */
    int energieMax;        /**< Coût de l'ultime                           */

    int resistance;        /**< Barre de résistance (monstres)             */
    int resistanceMax;
    unsigned faiblesses;   /**< Masque de bits : 1u << enum Element        */
                           /*   (resistanceMax = 0 : pas de barre)         */
    int brise;             /**< 1 tant que la résistance est brisée        */

    int recharge;          /**< Tours avant de pouvoir relancer la compétence */
    int bouclier;          /**< PV absorbés avant les vrais PV             */
    int buffAtk;           /**< Bonus d'ATK en %                           */
    int toursBuffAtk;
    int malusDef;          /**< Réduction de DEF en %                      */
    int toursMalusDef;
    int dot;               /**< Dégâts subis au début de chaque tour       */
    int toursDot;
    enum Element elementDot;
    int gele;              /**< Nombre de tours à passer                   */
} Combattant;

/** Types d'effets produits par une action (animés par l'interface). */
enum TypeEffet {
    EFFET_DEGATS, EFFET_SOIN, EFFET_BOUCLIER, EFFET_BRISE,
    EFFET_BONUS, EFFET_MALUS, EFFET_KO, EFFET_TEXTE
};

/** Un effet visible sur un combattant. */
typedef struct {
    int cible;             /**< Indice du combattant touché                */
    enum TypeEffet type;
    int valeur;            /**< Dégâts, soin, bouclier...                  */
    int critique;
    enum Element element;
    int coup;              /**< Numéro du coup (actions à plusieurs coups) */
    char texte[32];        /**< Libellé éventuel ("Gelé", "ATK +30 %")     */
} Effet;

/** État global du combat. */
enum EtatCombat {
    COMBAT_TOUR_JOUEUR,    /**< Un héros (de l'un ou l'autre camp) attend  */
    COMBAT_TOUR_ENNEMI,    /**< Un monstre va jouer (combatTourEnnemi)     */
    COMBAT_FIN_VAGUE,      /**< Vague vaincue, la suivante arrive          */
    COMBAT_VICTOIRE,       /**< Le camp 0 a gagné                          */
    COMBAT_DEFAITE         /**< Le camp 1 a gagné                          */
};

/** Un combat complet. Le camp 0 occupe les cases 0..3, le camp 1 les
 *  cases PREMIER_ENNEMI.. ; seules les cases `present` sont utilisées. */
typedef struct {
    enum ModeCombat mode;
    Combattant c[COMBAT_MAX];
    int sp[2];                  /**< Points de compétence de chaque camp  */
    int actif;                  /**< Combattant dont c'est le tour        */
    enum EtatCombat etat;
    int etage;                  /**< Difficulté (1..ETAGE_MAX)            */
    int vague, nbVagues;        /**< Vague en cours (1..nbVagues)          */
    int tour;                   /**< Nombre d'actions jouées              */
    int tourFini;               /**< 1 : l'actif a fini, appeler combatAvancer */

    Effet effets[EFFETS_MAX];   /**< Effets de la dernière action         */
    int nbEffets;
    int nbCoups;                /**< Nombre de coups de la dernière action */

    char journal[JOURNAL_MAX][JOURNAL_LONGUEUR]; /**< Dernières actions   */
    int nbJournal;
    int nbJournalTotal;         /**< Lignes écrites depuis le début (console) */
    unsigned touches;           /**< Cibles déjà marquées par l'élément pendant
                                     l'action en cours (bit = case)        */
    unsigned graine;            /**< État du générateur aléatoire du combat */

    int xpGagnee;               /**< Récompenses (fin de combat)          */
    int pointsGagnes;           /**< Points de stats par survivant        */
    int nbButins;
    Objet butins[4];
    char receveurs[4][NOM_MAX];
} Combat;

/** Retourne la capacité `action` d'une classe (nom de base, ciblage). */
const Capacite *capaciteClasse(enum Classe classe, enum TypeAction action);

/** Retourne 1 si l'action frappe des ennemis (et porte donc l'élément). */
int capaciteOffensive(enum Classe classe, enum TypeAction action);

/** Nom de la capacité pour ce héros : les attaques prennent le nom de son
 *  élément (« Fendoir de feu », « Météore de givre »...). */
void nomCapacite(const Personnage *p, enum TypeAction action, char *buf, int taille);

/** Description de la capacité pour ce héros, effet de son élément compris. */
void descriptionCapacite(const Personnage *p, enum TypeAction action, char *buf, int taille);

/** Nom de l'effet qu'un élément ajoute aux attaques (« Brûlure »...). */
const char *nomEffetElement(enum Element e);

/** Description courte de cet effet (« 20 % ATK pendant 2 tours »...). */
const char *effetElement(enum Element e);

/** Tours d'attente après la compétence d'une classe (0 : aucune attente). */
int rechargeCompetence(enum Classe classe);

/** Nom d'un type de monstre. */
const char *nomMonstre(enum Monstre m);

/** Retourne 1 si le type de monstre est un boss. */
int monstreEstBoss(enum Monstre m);

/** Fiche du monstre pour le bestiaire (NULL si invalide). */
const InfoMonstre *infoMonstre(enum Monstre m);

/** Faiblesses d'un monstre : celles de son élément + ses faiblesses bonus. */
unsigned faiblessesMonstre(enum Monstre m);

/** Étages où le monstre peut apparaître (bit e-1 pour l'étage e). */
unsigned etagesMonstre(enum Monstre m);

/**
 * Statistiques réelles d'un monstre à cet étage, face à une équipe de
 * `taille` héros de niveau moyen `niveau` (PV max, ATK, DEF, VIT dans
 * `out`, résistance dans *resistance).
 */
void statsMonstre(enum Monstre m, int etage, double niveau, int taille, Personnage *out,
                  int *resistance);

/** Description de l'étage (1..ETAGE_MAX ; borné si hors limites). */
const InfoEtage *combatEtage(int etage);

/**
 * Mode bots : prépare un combat entre `n` héros (pointeurs vers le donjon :
 * leurs PV réels sont modifiés) et des vagues de monstres de l'étage donné,
 * aussi nombreux que les héros. Le premier tour est déjà déterminé.
 * @return 1 si le combat a pu commencer, 0 si aucun héros n'est vivant.
 */
int combatInit(Combat *cb, Personnage *equipe[], int n, int etage);

/**
 * Duel local : équipe 1 (camp 0) contre équipe 2 (camp 1). Les héros sont
 * copiés avec tous leurs PV : le duel n'a aucune conséquence sur le donjon,
 * et un même héros peut être dans les deux équipes.
 * @return 1 si le duel a pu commencer.
 */
int combatInitDuel(Combat *cb, Personnage *equipe1[], int n1, Personnage *equipe2[], int n2);

/* [DUCKGEON - SDL] */
/** Fixe la graine du hasard du combat (critiques, dégâts...). Deux combats
 *  initialisés pareil et de même graine se déroulent à l'identique si on
 *  leur fait jouer les mêmes actions : c'est ce qu'utilise le duel en ligne. */
void combatGraine(Combat *cb, unsigned graine);

/** Retourne 1 si le combattant i est présent et en vie. */
int combatVivant(const Combat *cb, int i);

/** Bonus de dégâts (%) dû à la fatigue en duel (0 au début). */
int combatFatigue(const Combat *cb);

/** Camp d'une case : 0 (cases 0..3) ou 1 (cases PREMIER_ENNEMI..). */
int combatCamp(int i);

/** Retourne 1 si i est dans le camp 0. */
int combatEstAllie(int i);

/** Retourne 1 si le héros `lanceur` peut utiliser cette action maintenant
 *  (SP pour la compétence, énergie pour l'ultime, son tour sinon). Un
 *  ultime n'est possible que pendant le tour d'un héros de son camp. */
int combatPeutAgir(const Combat *cb, int lanceur, enum TypeAction action);

/** Retourne 1 si `cible` est une cible valide pour cette action. */
int combatCibleValide(const Combat *cb, int lanceur, enum TypeAction action, int cible);

/** Première cible valide (ennemi ou allié selon l'action), -1 sinon. */
int combatCibleParDefaut(const Combat *cb, int lanceur, enum TypeAction action);

/* [DUCKGEON - SDL] */
/** Retourne 1 si `i` est touché par l'action visant `cible` (zone). */
int combatEstTouche(const Combat *cb, int lanceur, enum TypeAction action,
                    int cible, int i);

/**
 * Exécute une action d'un héros. L'attaque de base et la compétence
 * terminent son tour ; l'ultime s'intercale sans consommer le tour.
 * Les effets produits sont dans cb->effets.
 * @return 1 si l'action a été jouée, 0 si elle était impossible.
 */
int combatJouer(Combat *cb, int lanceur, enum TypeAction action, int cible);

/** Fait jouer le monstre actif (intelligence simple). */
void combatTourEnnemi(Combat *cb);

/**
 * Passe au combattant suivant sur la ligne de temps et applique les effets
 * de début de tour (dégâts sur la durée, gel, fin de rupture). Ne fait rien
 * si le combat est terminé ou en attente de la vague suivante.
 */
void combatAvancer(Combat *cb);

/** Fait entrer la vague suivante (après COMBAT_FIN_VAGUE). */
void combatVagueSuivante(Combat *cb);

/**
 * Prédit l'ordre des prochains tours (le combattant actif en premier).
 * @return le nombre d'indices écrits dans ordre (au plus max).
 */
int combatOrdre(const Combat *cb, int ordre[], double avs[], int max);

#endif /* COMBAT_H */
