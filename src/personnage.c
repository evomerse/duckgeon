/**
 * @file personnage.c
 * @brief Implémentation du module Personnage.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "personnage.h"
#include "inventaire.h"
#include "saisie.h"

/* Statistiques de base par classe, indexées par enum Classe.
 * Une table évite une série de if/switch et centralise l'équilibrage. */
static const struct {
    const char *nom;
    int pv;
    int atk;
    int def;
    int vit;
    enum Element element;
    int critique;            /* chance de coup critique en % */
    const char *description;
} STATS_CLASSE[] = {
    [GUERRIER]     = { "Guerrier",     120, 14,  8,  95, PHYSIQUE,    5,
                       "Combattant robuste : gros PV, grosse defense." },
    [MAGE]         = { "Mage",          80, 20,  3, 100, FEU,         5,
                       "Sa magie ignore la moitie de la defense adverse." },
    [VOLEUR]       = { "Voleur",        95, 16,  5, 115, VENT,       25,
                       "25 % de chance de coup critique (degats x2)." },
    [PALADIN]      = { "Paladin",      130, 12, 10,  90, PHYSIQUE,    5,
                       "Tank sacre : ajoute la moitie de sa DEF a ses coups." },
    [ARCHER]       = { "Archer",        85, 18,  4, 110, FOUDRE,     15,
                       "Tireur rapide : 15 % de critique, 20 % de tir double." },
    [PRETRE]       = { "Pretre",        90, 13,  5,  98, GLACE,       5,
                       "Soigneur : recupere 10 % de ses PV max a chaque attaque." },
    [NECROMANCIEN] = { "Necromancien",  85, 18,  4, 102, QUANTIQUE,   5,
                       "Vole la vie : se soigne de la moitie des degats infliges." },
    [BARDE]        = { "Barde",         90, 14,  5, 108, IMAGINAIRE,  5,
                       "Imprevisible : ses notes infligent de 50 % a 160 % des degats." },
};

static const char *const NOMS_ELEMENTS[NB_ELEMENTS] = {
    "Physique", "Feu", "Glace", "Foudre", "Vent", "Quantique", "Imaginaire",
};

#define CHANCE_TIR_DOUBLE   20 /* archer, en pourcentage         */
#define SOIN_PRETRE_POURCENT 10 /* pretre : % des PV max par coup  */

/* Progression : XP requise = XP_PAR_NIVEAU * niveau actuel. */
#define XP_PAR_NIVEAU   50
#define XP_VICTOIRE     30 /* XP de base pour un K.O.          */
#define XP_PAR_NIV_CIBLE 10 /* bonus par niveau de la cible    */
#define GAIN_PV_NIVEAU  10
#define GAIN_ATK_NIVEAU  2
#define GAIN_DEF_NIVEAU  1

/** Retourne 1 si la valeur correspond à une classe existante. */
static int classeValide(enum Classe classe)
{
    return classe >= GUERRIER && classe < NB_CLASSES;
}

const char *nomClasse(enum Classe classe)
{
    if (!classeValide(classe)) {
        return "Inconnue";
    }
    return STATS_CLASSE[classe].nom;
}

const char *descriptionClasse(enum Classe classe)
{
    if (!classeValide(classe)) {
        return "";
    }
    return STATS_CLASSE[classe].description;
}

const char *nomElement(enum Element element)
{
    if (element < PHYSIQUE || element >= NB_ELEMENTS) {
        return "Inconnu";
    }
    return NOMS_ELEMENTS[element];
}

enum Element elementClasse(enum Classe classe)
{
    return classeValide(classe) ? STATS_CLASSE[classe].element : PHYSIQUE;
}

int chanceCritique(const Personnage *p)
{
    return classeValide(p->classe) ? STATS_CLASSE[p->classe].critique : 0;
}

void apparenceAleatoire(Apparence *a)
{
    if (a == NULL) {
        return;
    }
    a->plumage = (unsigned char)(rand() % NB_PLUMAGES_BASE); /* sans les cosmétiques payants */
    a->coupe = (unsigned char)(rand() % NB_COUPES);
    a->couleurCheveux = (unsigned char)(rand() % NB_COULEURS);
    a->couleurYeux = (unsigned char)(rand() % NB_COULEURS);
    a->regard = (unsigned char)(rand() % NB_REGARDS);
    a->tenue = (unsigned char)(rand() % NB_COULEURS);
    a->tenue2 = (unsigned char)(rand() % NB_COULEURS);
    a->coiffe = (unsigned char)(rand() % NB_COIFFES_BASE);
    a->pilosite = (unsigned char)(rand() % NB_PILOSITES);
    a->marque = (unsigned char)(rand() % NB_MARQUES);
    a->cape = (unsigned char)(rand() % 2);
    a->aura = (unsigned char)(rand() % 3 == 0 ? 1 + rand() % NB_COULEURS : 0);
    a->corpulence = (unsigned char)(rand() % NB_CORPULENCES);
}

int estVivant(const Personnage *p)
{
    return p != NULL && p->pv > 0;
}

int attaqueTotale(const Personnage *p)
{
    return p->atk + p->arme.valeur; /* arme vide : valeur 0 */
}

int xpPourNiveauSuivant(const Personnage *p)
{
    return XP_PAR_NIVEAU * p->niveau;
}

void initialiserStats(Personnage *p, enum Classe classe)
{
    if (p == NULL) {
        return;
    }
    p->classe = classe;
    p->pvMax = STATS_CLASSE[classe].pv;
    p->pv = p->pvMax;
    p->atk = STATS_CLASSE[classe].atk;
    p->def = STATS_CLASSE[classe].def;
    p->vit = STATS_CLASSE[classe].vit;
    p->element = STATS_CLASSE[classe].element;
    p->niveau = 1;
    p->xp = 0;
    p->pointsStats = 0;
    p->victoires = 0;
    apparenceAleatoire(&p->apparence);

    /* Aucune arme, inventaire vide avec une potion de départ. La remise à
     * zéro évite de laisser des octets non initialisés dans la structure. */
    memset(&p->arme, 0, sizeof p->arme);
    memset(p->inventaire, 0, sizeof p->inventaire);
    p->nbObjets = 0;
    ajouterObjet(p, objetDepart());
}

void gagnerXp(Personnage *p, int xp)
{
    if (p == NULL || xp <= 0) {
        return;
    }
    p->xp += xp;
    printf("%s gagne %d XP.\n", p->nom, xp);

    /* while et non if : un gros gain peut faire monter de plusieurs niveaux. */
    while (p->xp >= xpPourNiveauSuivant(p)) {
        p->xp -= xpPourNiveauSuivant(p);
        p->niveau++;
        p->pvMax += GAIN_PV_NIVEAU;
        p->atk += GAIN_ATK_NIVEAU;
        p->def += GAIN_DEF_NIVEAU;
        p->pv = p->pvMax; /* la montée de niveau soigne entièrement */
        p->pointsStats += POINTS_PAR_NIVEAU;
        printf("*** %s passe niveau %d ! (PV %d, ATK %d, DEF %d, +%d points de stats) ***\n",
               p->nom, p->niveau, p->pvMax, p->atk, p->def, POINTS_PAR_NIVEAU);
    }
}

const char *nomStat(enum Stat s)
{
    static const char *const NOMS[NB_STATS] = { "Points de vie", "Attaque", "Defense", "Vitesse" };
    return (s >= 0 && s < NB_STATS) ? NOMS[s] : "?";
}

int gainStat(enum Stat s)
{
    static const int GAINS[NB_STATS] = { BONUS_PV_POINT, BONUS_ATK_POINT, BONUS_DEF_POINT, BONUS_VIT_POINT };
    return (s >= 0 && s < NB_STATS) ? GAINS[s] : 0;
}

int ameliorerStat(Personnage *p, enum Stat s)
{
    if (p == NULL || p->pointsStats <= 0 || s < 0 || s >= NB_STATS) {
        return 0;
    }
    switch (s) {
    case STAT_PV:
        p->pvMax += BONUS_PV_POINT;
        p->pv += BONUS_PV_POINT;
        break;
    case STAT_ATK: p->atk += BONUS_ATK_POINT; break;
    case STAT_DEF: p->def += BONUS_DEF_POINT; break;
    default: p->vit += BONUS_VIT_POINT; break;
    }
    p->pointsStats--;
    return 1;
}

void creerPersonnage(Personnage *p)
{
    if (p == NULL) {
        return;
    }

    /* Nom : on redemande tant qu'il est vide. */
    do {
        printf("Nom du personnage (%d caracteres max) : ", NOM_MAX - 1);
        fflush(stdout);
        if (!lireLigne(p->nom, sizeof p->nom)) {
            strcpy(p->nom, "Anonyme");
            break;
        }
    } while (p->nom[0] == '\0');

    printf("Classe :\n");
    for (int c = GUERRIER; c < NB_CLASSES; c++) {
        printf("  %d. %-12s (PV %3d | ATK %2d | DEF %2d | VIT %3d) %s\n", c + 1,
               STATS_CLASSE[c].nom, STATS_CLASSE[c].pv, STATS_CLASSE[c].atk,
               STATS_CLASSE[c].def, STATS_CLASSE[c].vit,
               STATS_CLASSE[c].description);
    }
    int choix = lireEntier("Votre choix : ", 1, NB_CLASSES);
    if (choix == -1) {
        choix = 1; /* EOF : classe par défaut */
    }

    initialiserStats(p, (enum Classe)(choix - 1));
}

void afficherPersonnage(const Personnage *p)
{
    if (p == NULL) {
        return;
    }

    /* Petite barre de vie texte : 20 cases proportionnelles aux PV. */
    enum { LARGEUR_BARRE = 20 };
    int remplies = p->pv * LARGEUR_BARRE / p->pvMax;
    char barre[LARGEUR_BARRE + 1];
    for (int i = 0; i < LARGEUR_BARRE; i++) {
        barre[i] = (i < remplies) ? '#' : '.';
    }
    barre[LARGEUR_BARRE] = '\0';

    printf("%-20s %-12s Niv %2d [%s] %3d/%-3d ATK %2d DEF %2d XP %3d/%-3d%s%s\n",
           p->nom, nomClasse(p->classe), p->niveau, barre, p->pv, p->pvMax,
           attaqueTotale(p), p->def, p->xp, xpPourNiveauSuivant(p),
           estVivant(p) ? "" : " (K.O.)", p->pointsStats > 0 ? " [points a repartir]" : "");
}

/** Récompense l'attaquant qui vient de mettre la cible K.O. */
static void recompenserVictoire(Personnage *vainqueur, const Personnage *vaincu)
{
    gagnerXp(vainqueur, XP_VICTOIRE + XP_PAR_NIV_CIBLE * vaincu->niveau);
    vainqueur->victoires++;
    vainqueur->pointsStats += POINTS_PAR_VICTOIRE;
    printf("%s gagne %d point de stats (%d a repartir).\n", vainqueur->nom, POINTS_PAR_VICTOIRE,
           vainqueur->pointsStats);

    Objet butin = objetAleatoire();
    if (ajouterObjet(vainqueur, butin)) {
        printf("%s trouve : ", vainqueur->nom);
        afficherObjet(&butin);
        printf("\n");
    } else {
        printf("%s trouve %s mais son sac est plein.\n", vainqueur->nom, butin.nom);
    }
}

/** Applique des dégâts à la cible (PV jamais sous 0) et affiche le coup. */
static void infligerCoup(Personnage *attaquant, Personnage *cible, int degats,
                         int critique)
{
    /* Modification directe de la cible via le pointeur. */
    cible->pv -= degats;
    if (cible->pv < 0) {
        cible->pv = 0;
    }
    printf("%s attaque %s : %d degats%s. (%s : %d PV restants)\n",
           attaquant->nom, cible->nom, degats,
           critique ? " (COUP CRITIQUE !)" : "", cible->nom, cible->pv);
}

/** Soigne p de soin PV sans dépasser son maximum ; retourne le gain réel. */
static int soigner(Personnage *p, int soin)
{
    int avant = p->pv;
    p->pv += soin;
    if (p->pv > p->pvMax) {
        p->pv = p->pvMax;
    }
    return p->pv - avant;
}

void attaquer(Personnage *attaquant, Personnage *cible)
{
    if (attaquant == NULL || cible == NULL) {
        return;
    }
    if (!estVivant(attaquant) || !estVivant(cible)) {
        printf("Combat impossible : un des personnages est K.O.\n");
        return;
    }

    /* Formule de base : attaque - défense, avec au moins 1 dégât pour
     * qu'un combat finisse toujours par se terminer. Chaque classe modifie
     * ensuite le calcul selon sa particularité. */
    int attaque = attaqueTotale(attaquant);
    int defense = cible->def;

    switch (attaquant->classe) {
    case MAGE:
        defense /= 2; /* la magie ignore la moitié de l'armure */
        break;
    case PALADIN:
        attaque += attaquant->def / 2; /* châtiment : l'armure frappe aussi */
        break;
    case GUERRIER:
    default:
        break;
    }

    int degats = attaque - defense;
    if (degats < 1) {
        degats = 1;
    }
    if (attaquant->classe == BARDE) {
        /* Note juste ou fausse : entre 50 % et 160 % des dégâts. */
        degats = degats * (50 + rand() % 111) / 100;
        if (degats < 1) {
            degats = 1;
        }
    }

    /* Coup critique (voleur 25 %, archer 15 %, autres 5 %) : dégâts x2. */
    int critique = (rand() % 100) < chanceCritique(attaquant);
    if (critique) {
        degats *= 2;
    }

    infligerCoup(attaquant, cible, degats, critique);

    /* Effets après le coup. */
    if (attaquant->classe == ARCHER && estVivant(cible)
        && (rand() % 100) < CHANCE_TIR_DOUBLE) {
        printf("%s decoche une seconde fleche !\n", attaquant->nom);
        infligerCoup(attaquant, cible, degats, 0);
    } else if (attaquant->classe == NECROMANCIEN) {
        int gain = soigner(attaquant, degats / 2);
        if (gain > 0) {
            printf("%s draine %d PV (%d/%d).\n", attaquant->nom, gain,
                   attaquant->pv, attaquant->pvMax);
        }
    } else if (attaquant->classe == PRETRE) {
        int gain = soigner(attaquant, attaquant->pvMax * SOIN_PRETRE_POURCENT / 100);
        if (gain > 0) {
            printf("%s se soigne de %d PV (%d/%d).\n", attaquant->nom, gain,
                   attaquant->pv, attaquant->pvMax);
        }
    }

    if (!estVivant(cible)) {
        printf("%s est K.O. !\n", cible->nom);
        recompenserVictoire(attaquant, cible);
    }
}
