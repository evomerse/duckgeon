/**
 * @file combat.c
 * @brief Implémentation du combat en équipe façon Honkai: Star Rail
 *        (mode bots et duel local).
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "combat.h"
#include "inventaire.h"

#define SP_DEPART          3
#define ENERGIE_BASIQUE   20
#define ENERGIE_COMPETENCE 30
#define ENERGIE_COUP_RECU 10
#define ENERGIE_KO        10
#define RESISTANCE_DUEL   60  /* barre de résistance des héros en duel */
#define TOURS_AVANT_FATIGUE 30
#define BIT(e)            (1u << (e))

/* ------------------------------------------------------------------ */
/*  Capacités des classes                                             */
/* ------------------------------------------------------------------ */

static const Capacite CAPACITES[NB_CLASSES][NB_ACTIONS] = {
    [GUERRIER] = {
        { "Taille", "Frappe un ennemi : 100 % ATK.", CIBLE_ENNEMI },
        { "Fendoir", "Frappe un ennemi (130 % ATK) et ses voisins (65 %).", CIBLE_ZONE },
        { "Fracas titanesque", "Écrase un ennemi : 380 % ATK. Gagne un bouclier.", CIBLE_ENNEMI },
    },
    [MAGE] = {
        { "Étincelle", "Projectile magique : 100 % ATK.", CIBLE_ENNEMI },
        { "Boule de feu", "Explose sur un ennemi (150 % ATK) et ses voisins (75 %).", CIBLE_ZONE },
        { "Météore", "Pluie de feu sur tous les ennemis : 200 % ATK.", CIBLE_TOUS_ENNEMIS },
    },
    [VOLEUR] = {
        { "Coup bas", "Frappe un ennemi : 100 % ATK.", CIBLE_ENNEMI },
        { "Lames empoisonnées", "170 % ATK et poison (50 % ATK, 2 tours).", CIBLE_ENNEMI },
        { "Danse des ombres", "6 coups de 70 % ATK sur des ennemis au hasard.", CIBLE_TOUS_ENNEMIS },
    },
    [PALADIN] = {
        { "Coup de bouclier", "100 % ATK + 50 % DEF sur un ennemi.", CIBLE_ENNEMI },
        { "Égide sacrée", "Bouclier sur toute l'équipe (12 + 3 x DEF).", CIBLE_TOUS_ALLIES },
        { "Jugement divin", "150 % ATK à tous les ennemis, soigne l'équipe de 15 % PV.", CIBLE_TOUS_ENNEMIS },
    },
    [ARCHER] = {
        { "Tir", "Flèche sur un ennemi : 100 % ATK.", CIBLE_ENNEMI },
        { "Tir perforant", "Flèche chargée : 240 % ATK sur un ennemi.", CIBLE_ENNEMI },
        { "Pluie de flèches", "170 % ATK à tous les ennemis, l'archer avance son tour de 50 %.", CIBLE_TOUS_ENNEMIS },
    },
    [PRETRE] = {
        { "Rayon sacré", "Frappe un ennemi : 100 % ATK.", CIBLE_ENNEMI },
        { "Soin", "Soigne un allié (15 % PV max + 100 % ATK) et le purifie.", CIBLE_ALLIE },
        { "Bénédiction", "Soigne toute l'équipe (20 % PV max + 80 % ATK), ranime les K.O.", CIBLE_TOUS_ALLIES },
    },
    [NECROMANCIEN] = {
        { "Trait d'ombre", "Frappe un ennemi : 100 % ATK.", CIBLE_ENNEMI },
        { "Drain de vie", "160 % ATK, rend la moitié des dégâts en PV.", CIBLE_ENNEMI },
        { "Malédiction", "130 % ATK à tous, DEF -25 % et dégâts sur la durée (3 tours).", CIBLE_TOUS_ENNEMIS },
    },
    [BARDE] = {
        { "Note aiguë", "Frappe un ennemi : 100 % ATK.", CIBLE_ENNEMI },
        { "Hymne héroïque", "Allié : ATK +30 % (2 tours) et il rejoue immédiatement.", CIBLE_ALLIE },
        { "Symphonie", "Équipe : ATK +25 % (3 tours) et avance son tour de 40 %.", CIBLE_TOUS_ALLIES },
    },
};

/* Énergie nécessaire pour l'ultime de chaque classe. */
static const int ENERGIE_ULTIME[NB_CLASSES] = {
    [GUERRIER] = 120, [MAGE] = 120, [VOLEUR] = 120, [PALADIN] = 100,
    [ARCHER] = 110, [PRETRE] = 100, [NECROMANCIEN] = 120, [BARDE] = 100,
};

/* Faiblesses d'un héros en duel, selon son propre élément (chaque élément
 * est la faiblesse de deux autres : aucun n'est avantagé). */
static const unsigned FAIBLESSES_HEROS[NB_ELEMENTS] = {
    [PHYSIQUE]   = BIT(FEU) | BIT(VENT),
    [FEU]        = BIT(GLACE) | BIT(QUANTIQUE),
    [GLACE]      = BIT(FEU) | BIT(FOUDRE),
    [FOUDRE]     = BIT(PHYSIQUE) | BIT(IMAGINAIRE),
    [VENT]       = BIT(FOUDRE) | BIT(GLACE),
    [QUANTIQUE]  = BIT(IMAGINAIRE) | BIT(PHYSIQUE),
    [IMAGINAIRE] = BIT(QUANTIQUE) | BIT(VENT),
};

const Capacite *capaciteClasse(enum Classe classe, enum TypeAction action)
{
    if (classe < 0 || classe >= NB_CLASSES || action < 0 || action >= NB_ACTIONS) {
        return &CAPACITES[GUERRIER][ACTION_BASIQUE];
    }
    return &CAPACITES[classe][action];
}

/* ------------------------------------------------------------------ */
/*  Monstres                                                          */
/* ------------------------------------------------------------------ */

/** Ce que fait l'attaque spéciale d'un monstre. */
enum Special {
    SPECIAL_ZONE, SPECIAL_FORT, SPECIAL_RAGE, SPECIAL_TOUS, SPECIAL_POISON,
    SPECIAL_INVOCATION, /* appelle un monstre en renfort (Roi des Gelées)  */
    SPECIAL_DRAIN,      /* frappe tout le monde et se soigne (Liche)        */
    SPECIAL_NEANT       /* frappe tout le monde et brise la DEF (Canard Noir) */
};

static const struct {
    const char *nom;
    int pv, atk, def, vit, resistance;
    unsigned faiblesses;
    enum Element element;
    const char *attaque;
    const char *special;
    enum Special typeSpecial;
    int chanceSpecial; /* en % */
    const char *etat;  /* libellé de l'altération infligée (poison...) */
    int boss;
} MONSTRES[NB_MONSTRES] = {
    [MONSTRE_GELEE]      = { "Gelée", 50, 10, 2, 85, 20, BIT(FEU) | BIT(FOUDRE), GLACE,
                             "Bulle visqueuse", "Éclaboussure acide", SPECIAL_ZONE, 30, NULL, 0 },
    [MONSTRE_SQUELETTE]  = { "Squelette", 62, 12, 5, 95, 30, BIT(PHYSIQUE) | BIT(IMAGINAIRE), PHYSIQUE,
                             "Coup d'os", "Charge brutale", SPECIAL_FORT, 30, NULL, 0 },
    [MONSTRE_LOUP]       = { "Loup sombre", 55, 13, 3, 120, 20, BIT(FEU) | BIT(VENT), PHYSIQUE,
                             "Morsure", "Hurlement", SPECIAL_RAGE, 25, NULL, 0 },
    [MONSTRE_GOLEM]      = { "Golem", 120, 15, 9, 75, 60, BIT(FOUDRE) | BIT(QUANTIQUE), PHYSIQUE,
                             "Poing de pierre", "Séisme", SPECIAL_TOUS, 35, NULL, 0 },
    [MONSTRE_SPECTRE]    = { "Spectre", 70, 15, 3, 105, 30, BIT(IMAGINAIRE) | BIT(GLACE), QUANTIQUE,
                             "Toucher froid", "Lamentation", SPECIAL_POISON, 35, "Hanté", 0 },
    [MONSTRE_DRAGON]     = { "Dragon ancien", 340, 20, 8, 100, 140,
                             BIT(GLACE) | BIT(VENT) | BIT(QUANTIQUE), FEU,
                             "Griffes", "Souffle ardent", SPECIAL_TOUS, 40, NULL, 1 },
    [MONSTRE_CHAMPIGNON] = { "Champignon", 58, 11, 4, 80, 30, BIT(FEU) | BIT(GLACE), VENT,
                             "Coup de chapeau", "Nuage de spores", SPECIAL_POISON, 35, "Empoisonné", 0 },
    [MONSTRE_DIABLOTIN]  = { "Diablotin", 48, 14, 3, 130, 20, BIT(GLACE) | BIT(QUANTIQUE), FEU,
                             "Coup de fourche", "Pluie de braises", SPECIAL_ZONE, 35, NULL, 0 },
    [MONSTRE_GARGOUILLE] = { "Gargouille", 90, 15, 10, 90, 50, BIT(VENT) | BIT(IMAGINAIRE), FOUDRE,
                             "Griffes de pierre", "Piqué foudroyant", SPECIAL_FORT, 30, NULL, 0 },
    [MONSTRE_ROI_GELEE]  = { "Roi des Gelées", 260, 16, 5, 90, 110,
                             BIT(FEU) | BIT(FOUDRE) | BIT(PHYSIQUE), GLACE,
                             "Écrasement royal", "Appel royal", SPECIAL_INVOCATION, 40, NULL, 1 },
    [MONSTRE_LICHE]      = { "Liche", 300, 19, 6, 105, 130,
                             BIT(PHYSIQUE) | BIT(FEU) | BIT(IMAGINAIRE), QUANTIQUE,
                             "Rayon nécrotique", "Siphon d'âmes", SPECIAL_DRAIN, 40, NULL, 1 },
    [MONSTRE_CANARD_NOIR] = { "Canard Noir", 420, 22, 10, 115, 160,
                              BIT(FOUDRE) | BIT(VENT) | BIT(QUANTIQUE), IMAGINAIRE,
                              "Bec du Néant", "Coin-coin du Néant", SPECIAL_NEANT, 40, NULL, 1 },
};

const char *nomMonstre(enum Monstre m)
{
    return (m >= 0 && m < NB_MONSTRES) ? MONSTRES[m].nom : "???";
}

int monstreEstBoss(enum Monstre m)
{
    return m >= 0 && m < NB_MONSTRES && MONSTRES[m].boss;
}

/* ------------------------------------------------------------------ */
/*  Étages                                                            */
/* ------------------------------------------------------------------ */

#define AUCUN_BOSS NB_MONSTRES

/* Chaque étage a son ambiance et ses monstres ; un boss garde les
 * étages 3, 6, 8 et 10. */
static const InfoEtage ETAGES[ETAGE_MAX] = {
    { "Les Caves humides", "Des gelées et des loups rôdent dans l'entrée du donjon.",
      2, 2, { MONSTRE_GELEE, MONSTRE_LOUP }, AUCUN_BOSS },
    { "La Champignonnière", "Des champignons lumineux... et pas très amicaux.",
      2, 3, { MONSTRE_GELEE, MONSTRE_LOUP, MONSTRE_CHAMPIGNON }, AUCUN_BOSS },
    { "Le Trône visqueux", "Le Roi des Gelées appelle ses sujets à la rescousse.",
      3, 3, { MONSTRE_GELEE, MONSTRE_CHAMPIGNON, MONSTRE_SQUELETTE }, MONSTRE_ROI_GELEE },
    { "L'Ossuaire", "Des os partout. Certains bougent encore.",
      3, 3, { MONSTRE_SQUELETTE, MONSTRE_LOUP, MONSTRE_SPECTRE }, AUCUN_BOSS },
    { "La Forge oubliée", "Les golems gardent encore les braises des diablotins.",
      3, 3, { MONSTRE_GOLEM, MONSTRE_DIABLOTIN, MONSTRE_SQUELETTE }, AUCUN_BOSS },
    { "La Crypte de la Liche", "La Liche se nourrit de l'âme des imprudents.",
      3, 3, { MONSTRE_SPECTRE, MONSTRE_SQUELETTE, MONSTRE_DIABLOTIN }, MONSTRE_LICHE },
    { "Les Remparts gris", "Les gargouilles s'éveillent au moindre bruit.",
      3, 3, { MONSTRE_GARGOUILLE, MONSTRE_GOLEM, MONSTRE_SPECTRE }, AUCUN_BOSS },
    { "Le Nid du Dragon", "Il fait chaud. Beaucoup trop chaud.",
      4, 3, { MONSTRE_DIABLOTIN, MONSTRE_GARGOUILLE, MONSTRE_LOUP }, MONSTRE_DRAGON },
    { "L'Abîme", "Tout ce que le donjon a de pire, réuni.",
      4, 6, { MONSTRE_GOLEM, MONSTRE_SPECTRE, MONSTRE_GARGOUILLE, MONSTRE_DIABLOTIN,
              MONSTRE_CHAMPIGNON, MONSTRE_SQUELETTE }, AUCUN_BOSS },
    { "Le Cœur du Néant", "Le Canard Noir vous attend. Il n'est pas venu pour du pain.",
      4, 4, { MONSTRE_SPECTRE, MONSTRE_GARGOUILLE, MONSTRE_GOLEM, MONSTRE_DIABLOTIN },
      MONSTRE_CANARD_NOIR },
};

const InfoEtage *combatEtage(int etage)
{
    if (etage < 1) etage = 1;
    if (etage > ETAGE_MAX) etage = ETAGE_MAX;
    return &ETAGES[etage - 1];
}

/* ------------------------------------------------------------------ */
/*  Outils internes                                                   */
/* ------------------------------------------------------------------ */

int combatFatigue(const Combat *cb)
{
    if (cb->mode != MODE_DUEL || cb->tour <= TOURS_AVANT_FATIGUE) {
        return 0;
    }
    return (cb->tour - TOURS_AVANT_FATIGUE) * 5;
}

int combatCamp(int i)
{
    return i < PREMIER_ENNEMI ? 0 : 1;
}

int combatEstAllie(int i)
{
    return i >= 0 && i < PREMIER_ENNEMI;
}

/** Première case et case de fin (exclue) d'un camp. */
static int debutCamp(int camp) { return camp == 0 ? 0 : PREMIER_ENNEMI; }
static int finCamp(int camp) { return camp == 0 ? PREMIER_ENNEMI : COMBAT_MAX; }

int combatVivant(const Combat *cb, int i)
{
    return i >= 0 && i < COMBAT_MAX && cb->c[i].present && estVivant(cb->c[i].p);
}

/** Durée entre deux tours d'un combattant (plus il est rapide, plus elle
 *  est courte). */
static double avDeBase(const Combattant *c)
{
    int vit = c->p->vit > 0 ? c->p->vit : 1;
    return AV_BASE / vit;
}

/** Ajoute une ligne au journal (les plus anciennes sont décalées). */
static void journaliser(Combat *cb, const char *format, ...)
{
    if (cb->nbJournal == JOURNAL_MAX) {
        memmove(cb->journal[0], cb->journal[1], (JOURNAL_MAX - 1) * sizeof cb->journal[0]);
        cb->nbJournal--;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(cb->journal[cb->nbJournal], JOURNAL_LONGUEUR, format, args);
    va_end(args);
    cb->nbJournal++;
}

/** Enregistre un effet pour l'animation de l'interface. */
static void ajouterEffet(Combat *cb, int cible, enum TypeEffet type, int valeur,
                         int critique, enum Element element, const char *texte)
{
    if (cb->nbEffets >= EFFETS_MAX) {
        return;
    }
    Effet *e = &cb->effets[cb->nbEffets++];
    e->cible = cible;
    e->type = type;
    e->valeur = valeur;
    e->critique = critique;
    e->element = element;
    e->coup = cb->nbCoups - 1;
    snprintf(e->texte, sizeof e->texte, "%s", texte != NULL ? texte : "");
}

/** Liste les combattants vivants d'un camp, dans l'ordre des cases.
 *  Retourne leur nombre. */
static int vivantsCamp(const Combat *cb, int camp, int liste[])
{
    int n = 0;
    for (int i = debutCamp(camp); i < finCamp(camp); i++) {
        if (combatVivant(cb, i)) {
            liste[n++] = i;
        }
    }
    return n;
}

/** Vivants du camp de `i` (ses alliés, lui compris). */
static int alliesDe(const Combat *cb, int i, int liste[])
{
    return vivantsCamp(cb, combatCamp(i), liste);
}

/** Vivants du camp adverse de `i`. */
static int adversairesDe(const Combat *cb, int i, int liste[])
{
    return vivantsCamp(cb, 1 - combatCamp(i), liste);
}

/** Ajoute de l'énergie à un héros sans dépasser le maximum. */
static void gagnerEnergie(Combattant *c, int quantite)
{
    c->energie += quantite;
    if (c->energie > c->energieMax) {
        c->energie = c->energieMax;
    }
}

/** Avance (pourcent > 0) ou retarde (pourcent < 0) le prochain tour. */
static void decalerAction(Combattant *c, int pourcent)
{
    c->av -= avDeBase(c) * pourcent / 100.0;
    if (c->av < 0) {
        c->av = 0;
    }
}

/** Nombre aléatoire réel entre a et b. */
static double aleatoire(double a, double b)
{
    return a + (b - a) * rand() / (double)RAND_MAX;
}

/* ------------------------------------------------------------------ */
/*  Fin de combat                                                     */
/* ------------------------------------------------------------------ */

/** Mode bots : distribue l'XP et le butin aux héros survivants. */
static void distribuerRecompenses(Combat *cb)
{
    int allies[COMBAT_MAX_ALLIES];
    int n = vivantsCamp(cb, 0, allies);
    /* Points de stats : 1 par victoire, 2 de plus si l'étage avait un boss
     * (en plus de ceux gagnés à chaque montée de niveau). */
    cb->pointsGagnes = POINTS_PAR_VICTOIRE + (combatEtage(cb->etage)->boss != AUCUN_BOSS ? 2 : 0);
    for (int k = 0; k < n; k++) {
        Personnage *p = cb->c[allies[k]].p;
        gagnerXp(p, cb->xpGagnee);
        p->pointsStats += cb->pointsGagnes;
        p->victoires++;
    }

    /* Un objet par vague remportée, donné à un survivant au hasard. */
    cb->nbButins = 0;
    for (int v = 0; v < cb->nbVagues && n > 0; v++) {
        Personnage *receveur = cb->c[allies[rand() % n]].p;
        Objet butin = objetAleatoire();
        if (ajouterObjet(receveur, butin)) {
            cb->butins[cb->nbButins] = butin;
            snprintf(cb->receveurs[cb->nbButins], NOM_MAX, "%s", receveur->nom);
            cb->nbButins++;
        }
    }
}

/** Met à jour l'état si un camp est entièrement K.O.
 *  @return 1 si la vague ou le combat est terminé. */
static int verifierFin(Combat *cb)
{
    int liste[COMBAT_MAX];
    if (vivantsCamp(cb, 0, liste) == 0) {
        cb->etat = COMBAT_DEFAITE;
        journaliser(cb, cb->mode == MODE_DUEL ? "Le joueur 2 remporte le duel !"
                                              : "L'équipe est vaincue...");
        return 1;
    }
    if (vivantsCamp(cb, 1, liste) == 0) {
        if (cb->mode == MODE_DUEL) {
            cb->etat = COMBAT_VICTOIRE;
            journaliser(cb, "Le joueur 1 remporte le duel !");
        } else if (cb->vague < cb->nbVagues) {
            cb->etat = COMBAT_FIN_VAGUE;
            journaliser(cb, "Vague %d remportée !", cb->vague);
        } else {
            cb->etat = COMBAT_VICTOIRE;
            journaliser(cb, cb->etage == ETAGE_MAX ? "Le Canard Noir est vaincu : le donjon est libéré !"
                                                   : "Victoire ! L'étage %d est nettoyé.", cb->etage);
            distribuerRecompenses(cb);
        }
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Dégâts, soins, rupture                                            */
/* ------------------------------------------------------------------ */

/** Retire des PV (le bouclier absorbe d'abord). Gère le K.O. */
static void subirDegats(Combat *cb, int source, int cible, int degats,
                        int critique, enum Element element)
{
    Combattant *c = &cb->c[cible];
    int restant = degats;
    if (c->bouclier > 0) {
        int absorbe = restant < c->bouclier ? restant : c->bouclier;
        c->bouclier -= absorbe;
        restant -= absorbe;
    }
    c->p->pv -= restant;
    if (c->p->pv < 0) {
        c->p->pv = 0;
    }
    ajouterEffet(cb, cible, EFFET_DEGATS, degats, critique, element, NULL);

    if (c->heros) {
        gagnerEnergie(c, ENERGIE_COUP_RECU);
    }
    if (!estVivant(c->p)) {
        ajouterEffet(cb, cible, EFFET_KO, 0, 0, element, "K.O.");
        journaliser(cb, "%s est K.O. !", c->p->nom);
        c->bouclier = 0;
        c->dot = c->toursDot = 0;
        if (!c->heros) {
            cb->xpGagnee += (10 + 6 * cb->etage) * (c->boss ? 4 : 1);
        }
        if (source >= 0 && cb->c[source].heros) {
            gagnerEnergie(&cb->c[source], ENERGIE_KO);
        }
    }
}

/** Calcule les dégâts d'un coup à `pourcent` % de l'ATK de l'attaquant. */
static int calculerDegats(const Combat *cb, int a, int c, int pourcent, int *critique)
{
    const Combattant *ca = &cb->c[a];
    const Combattant *cc = &cb->c[c];

    double atk = attaqueTotale(ca->p) * (100 + ca->buffAtk) / 100.0;
    if (ca->heros && ca->p->classe == PALADIN) {
        atk += ca->p->def * 0.5; /* l'armure du paladin frappe aussi */
    }
    double def = cc->p->def * (100 - cc->malusDef) / 100.0;
    if (ca->heros && ca->p->classe == MAGE) {
        def /= 2; /* la magie ignore la moitié de l'armure */
    }

    double degats = atk * pourcent / 100.0 * 100.0 / (100.0 + 6.0 * def);
    if (cc->resistanceMax > 0) {
        if (!(cc->faiblesses & BIT(ca->p->element))) {
            degats *= 0.9; /* résistance hors faiblesse */
        }
        if (cc->brise) {
            degats *= 1.25; /* cible brisée : vulnérable */
        }
    }

    /* Duel : au-delà de 30 actions, la fatigue augmente les dégâts pour
     * qu'un duel entre soigneurs finisse toujours. */
    if (cb->mode == MODE_DUEL && cb->tour > TOURS_AVANT_FATIGUE) {
        degats *= 1.0 + combatFatigue(cb) / 100.0;
    }

    *critique = (rand() % 100) < chanceCritique(ca->p);
    if (*critique) {
        degats *= (ca->p->classe == VOLEUR) ? 2.0 : 1.5;
    }
    degats *= aleatoire(0.92, 1.08);
    return degats < 1 ? 1 : (int)(degats + 0.5);
}

/** Rupture : la cible perd son tour en partie et subit un effet élémentaire. */
static void briser(Combat *cb, int a, int c)
{
    Combattant *ca = &cb->c[a];
    Combattant *cc = &cb->c[c];
    enum Element e = ca->p->element;
    int atk = attaqueTotale(ca->p);

    cc->brise = 1;
    cc->resistance = 0;
    ajouterEffet(cb, c, EFFET_BRISE, 0, 0, e, "BRISÉ !");
    journaliser(cb, "La garde de %s est brisée (%s) !", cc->p->nom, nomElement(e));

    decalerAction(cc, -25);
    int degatsRupture = (int)(atk * (cc->boss ? 0.8 : 1.2));
    subirDegats(cb, a, c, degatsRupture, 0, e);
    if (!estVivant(cc->p)) {
        return;
    }

    switch (e) {
    case GLACE:
        cc->gele = 1;
        ajouterEffet(cb, c, EFFET_MALUS, 0, 0, e, "Gelé");
        break;
    case QUANTIQUE:
        decalerAction(cc, -20);
        ajouterEffet(cb, c, EFFET_MALUS, 0, 0, e, "Enchevêtré");
        break;
    case IMAGINAIRE:
        decalerAction(cc, -30);
        ajouterEffet(cb, c, EFFET_MALUS, 0, 0, e, "Entravé");
        break;
    default: {
        /* Physique, feu, foudre, vent : dégâts sur la durée. */
        static const char *const NOMS_DOT[NB_ELEMENTS] = {
            [PHYSIQUE] = "Saignement", [FEU] = "Brûlure",
            [FOUDRE] = "Électrocuté", [VENT] = "Lacéré",
        };
        cc->dot = (int)(atk * 0.7);
        cc->toursDot = (e == VENT) ? 3 : 2;
        cc->elementDot = e;
        ajouterEffet(cb, c, EFFET_MALUS, 0, 0, e, NOMS_DOT[e]);
        break;
    }
    }
}

/**
 * Un combattant en frappe un autre : calcul, application, énergie,
 * réduction de résistance si l'élément correspond à une faiblesse.
 * @return les dégâts infligés.
 */
static int frapper(Combat *cb, int a, int c, int pourcent, int resistance)
{
    if (!combatVivant(cb, c)) {
        return 0;
    }
    int critique;
    int degats = calculerDegats(cb, a, c, pourcent, &critique);
    subirDegats(cb, a, c, degats, critique, cb->c[a].p->element);

    Combattant *cc = &cb->c[c];
    if (cc->resistanceMax > 0 && estVivant(cc->p) && !cc->brise
        && (cc->faiblesses & BIT(cb->c[a].p->element))) {
        cc->resistance -= resistance;
        if (cc->resistance <= 0) {
            briser(cb, a, c);
        }
    }
    return degats;
}

/** Soigne un combattant (ranime s'il est K.O. et que `ranimer` vaut 1). */
static void soignerCombattant(Combat *cb, int c, int soin, int ranimer)
{
    Personnage *p = cb->c[c].p;
    if (!estVivant(p) && !ranimer) {
        return;
    }
    if (!estVivant(p)) {
        cb->c[c].av = avDeBase(&cb->c[c]); /* revient sur la ligne de temps */
        journaliser(cb, "%s revient au combat !", p->nom);
    }
    int avant = p->pv;
    p->pv += soin;
    if (p->pv > p->pvMax) {
        p->pv = p->pvMax;
    }
    ajouterEffet(cb, c, EFFET_SOIN, p->pv - avant, 0, PHYSIQUE, NULL);
}

static void appliquerDot(Combattant *c, int degats, int tours, enum Element e)
{
    if (degats > c->dot) {
        c->dot = degats;
    }
    if (tours > c->toursDot) {
        c->toursDot = tours;
    }
    c->elementDot = e;
}

/** Termine le tour d'un combattant : durées des effets, nouvelle AV. */
static void finTour(Combat *cb, int i)
{
    Combattant *c = &cb->c[i];
    if (c->toursBuffAtk > 0 && --c->toursBuffAtk == 0) {
        c->buffAtk = 0;
    }
    if (c->toursMalusDef > 0 && --c->toursMalusDef == 0) {
        c->malusDef = 0;
    }
    c->av = avDeBase(c);
    cb->tourFini = 1;
    cb->tour++;
}

/* ------------------------------------------------------------------ */
/*  Vagues (mode bots)                                                */
/* ------------------------------------------------------------------ */

/** Crée un monstre dans la case `i`, adapté à l'étage et à l'équipe. */
static void creerMonstre(Combat *cb, int i, enum Monstre m, double niveauEquipe,
                         int tailleEquipe, char suffixe)
{
    Combattant *c = &cb->c[i];
    memset(c, 0, sizeof *c);
    c->present = 1;
    c->p = &c->stock;
    c->monstre = m;
    c->boss = MONSTRES[m].boss;
    c->phase = 1;

    /* Une petite équipe affronte des monstres un peu moins robustes. */
    double fTaille = 0.4 + 0.15 * tailleEquipe;
    if (c->boss) {
        fTaille *= fTaille; /* le boss compte double face à une petite équipe */
    }
    double fPv = 1.5 * fTaille * (1.0 + 0.35 * (cb->etage - 1)) * (1.0 + 0.12 * (niveauEquipe - 1));
    double fAtk = (1.0 + 0.18 * (cb->etage - 1)) * (1.0 + 0.08 * (niveauEquipe - 1));

    Personnage *p = &c->stock;
    if (suffixe != '\0' && !c->boss) {
        snprintf(p->nom, NOM_MAX, "%s %c", MONSTRES[m].nom, suffixe);
    } else {
        snprintf(p->nom, NOM_MAX, "%s", MONSTRES[m].nom);
    }
    p->pvMax = (int)(MONSTRES[m].pv * fPv);
    p->pv = p->pvMax;
    p->atk = (int)(MONSTRES[m].atk * fAtk);
    p->def = MONSTRES[m].def + 2 * (cb->etage - 1);
    p->vit = MONSTRES[m].vit + 2 * (cb->etage - 1);
    p->classe = GUERRIER;
    p->element = MONSTRES[m].element;
    p->niveau = 2 * cb->etage - 2 + (int)niveauEquipe;

    c->resistanceMax = MONSTRES[m].resistance + 10 * (cb->etage - 1);
    c->resistance = c->resistanceMax;
    c->faiblesses = MONSTRES[m].faiblesses;
    c->av = avDeBase(c);
}

/** Niveau moyen et nombre des héros présents (pour adapter les monstres). */
static double niveauMoyenEquipe(const Combat *cb, int *nbHeros)
{
    double total = 0;
    int n = 0;
    for (int i = 0; i < PREMIER_ENNEMI; i++) {
        if (cb->c[i].present) {
            total += cb->c[i].p->niveau;
            n++;
        }
    }
    *nbHeros = n;
    return n > 0 ? total / n : 1;
}

/** Remplit les cases ennemies avec la vague `cb->vague` : équipe contre
 *  équipe, il y a autant de monstres que de héros. Le boss de l'étage
 *  (s'il y en a un) arrive avec la dernière vague. */
static void genererVague(Combat *cb)
{
    int n;
    double niveau = niveauMoyenEquipe(cb, &n);
    const InfoEtage *info = combatEtage(cb->etage);

    for (int i = PREMIER_ENNEMI; i < COMBAT_MAX; i++) {
        cb->c[i].present = 0;
    }

    int boss = (cb->vague == cb->nbVagues && info->boss != AUCUN_BOSS);
    int nb = n < 1 ? 1 : n;
    int caseBoss = nb >= 3 ? 1 : 0; /* le boss au centre */
    for (int k = 0; k < nb; k++) {
        enum Monstre m = info->pool[rand() % info->taillePool];
        if (boss && k == caseBoss) {
            m = info->boss;
        }
        creerMonstre(cb, PREMIER_ENNEMI + k, m, niveau, n, (char)('A' + k));
    }
}

/**
 * Fait apparaître un monstre en renfort : dans la case d'un monstre vaincu,
 * sinon dans une case libre (au plus 4 ennemis, pour que la scène reste
 * lisible). Retourne la case utilisée, ou -1 s'il n'y a pas de place.
 */
static int invoquer(Combat *cb, enum Monstre m)
{
    int presents = 0, caseLibre = -1;
    for (int i = PREMIER_ENNEMI; i < COMBAT_MAX; i++) {
        if (cb->c[i].present) {
            presents++;
            if (!estVivant(cb->c[i].p) && caseLibre < 0) {
                caseLibre = i;
            }
        }
    }
    if (caseLibre < 0 && presents < 4) {
        for (int i = PREMIER_ENNEMI; i < COMBAT_MAX && caseLibre < 0; i++) {
            if (!cb->c[i].present) {
                caseLibre = i;
            }
        }
    }
    if (caseLibre < 0) {
        return -1;
    }
    int n;
    double niveau = niveauMoyenEquipe(cb, &n);
    creerMonstre(cb, caseLibre, m, niveau, n, (char)('A' + caseLibre - PREMIER_ENNEMI));
    ajouterEffet(cb, caseLibre, EFFET_TEXTE, 0, 0, MONSTRES[m].element, "Renfort !");
    return caseLibre;
}

/* ------------------------------------------------------------------ */
/*  Initialisation                                                    */
/* ------------------------------------------------------------------ */

/** Place un héros dans la case i. `copie` : le héros est recopié dans la
 *  case (duel) au lieu d'être modifié directement dans le donjon. */
static void placerHeros(Combat *cb, int i, Personnage *p, int copie)
{
    Combattant *c = &cb->c[i];
    memset(c, 0, sizeof *c);
    c->present = 1;
    c->heros = 1;
    if (copie) {
        c->stock = *p;
        c->stock.pv = c->stock.pvMax;
        c->p = &c->stock;
        c->resistanceMax = c->resistance = RESISTANCE_DUEL;
        c->faiblesses = FAIBLESSES_HEROS[p->element];
    } else {
        c->p = p;
    }
    c->energieMax = ENERGIE_ULTIME[c->p->classe];
    c->energie = c->energieMax / 2; /* on commence à moitié chargé */
    c->av = avDeBase(c);
}

int combatInit(Combat *cb, Personnage *equipe[], int n, int etage)
{
    memset(cb, 0, sizeof *cb);
    cb->mode = MODE_BOTS;
    cb->etage = etage < 1 ? 1 : (etage > ETAGE_MAX ? ETAGE_MAX : etage);
    cb->nbVagues = combatEtage(cb->etage)->nbVagues;
    cb->vague = 1;
    cb->sp[0] = cb->sp[1] = SP_DEPART;

    int nbAllies = 0;
    for (int k = 0; k < n && nbAllies < COMBAT_MAX_ALLIES; k++) {
        if (equipe[k] != NULL && estVivant(equipe[k])) {
            placerHeros(cb, nbAllies++, equipe[k], 0);
        }
    }
    if (nbAllies == 0) {
        return 0;
    }

    genererVague(cb);
    journaliser(cb, "Étage %d : %s — vague 1/%d", cb->etage, combatEtage(cb->etage)->nom, cb->nbVagues);
    cb->tourFini = 1;
    combatAvancer(cb);
    return 1;
}

int combatInitDuel(Combat *cb, Personnage *equipe1[], int n1, Personnage *equipe2[], int n2)
{
    memset(cb, 0, sizeof *cb);
    cb->mode = MODE_DUEL;
    cb->etage = 1;
    cb->nbVagues = cb->vague = 1;
    cb->sp[0] = cb->sp[1] = SP_DEPART;

    int a = 0, b = 0;
    for (int k = 0; k < n1 && a < COMBAT_MAX_ALLIES; k++) {
        if (equipe1[k] != NULL) placerHeros(cb, a++, equipe1[k], 1);
    }
    for (int k = 0; k < n2 && b < COMBAT_MAX_ALLIES; k++) {
        if (equipe2[k] != NULL) placerHeros(cb, PREMIER_ENNEMI + b++, equipe2[k], 1);
    }
    if (a == 0 || b == 0) {
        return 0;
    }
    journaliser(cb, "Duel : %d contre %d !", a, b);
    cb->tourFini = 1;
    combatAvancer(cb);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Règles d'action                                                   */
/* ------------------------------------------------------------------ */

int combatPeutAgir(const Combat *cb, int lanceur, enum TypeAction action)
{
    if (cb->etat != COMBAT_TOUR_JOUEUR || lanceur < 0 || lanceur >= COMBAT_MAX
        || !cb->c[lanceur].heros || !combatVivant(cb, lanceur)
        || combatCamp(lanceur) != combatCamp(cb->actif)) {
        return 0;
    }
    const Combattant *c = &cb->c[lanceur];
    switch (action) {
    case ACTION_ULTIME:
        return c->energie >= c->energieMax;
    case ACTION_COMPETENCE:
        return lanceur == cb->actif && !cb->tourFini && cb->sp[combatCamp(lanceur)] > 0;
    case ACTION_BASIQUE:
        return lanceur == cb->actif && !cb->tourFini;
    default:
        return 0;
    }
}

int combatCibleValide(const Combat *cb, int lanceur, enum TypeAction action, int cible)
{
    if (cible < 0 || cible >= COMBAT_MAX || lanceur < 0 || lanceur >= COMBAT_MAX) {
        return 0;
    }
    int memeCamp = combatCamp(cible) == combatCamp(lanceur);
    const Capacite *cap = capaciteClasse(cb->c[lanceur].p->classe, action);
    switch (cap->ciblage) {
    case CIBLE_ENNEMI:
    case CIBLE_ZONE:
    case CIBLE_TOUS_ENNEMIS:
        return !memeCamp && combatVivant(cb, cible);
    case CIBLE_ALLIE:
        return memeCamp && combatVivant(cb, cible);
    case CIBLE_TOUS_ALLIES:
        return memeCamp && cb->c[cible].present;
    }
    return 0;
}

int combatCibleParDefaut(const Combat *cb, int lanceur, enum TypeAction action)
{
    /* Pour un soin, on vise l'allié le plus blessé. */
    if (cb->c[lanceur].p->classe == PRETRE
        && capaciteClasse(PRETRE, action)->ciblage == CIBLE_ALLIE) {
        int meilleur = -1;
        for (int i = debutCamp(combatCamp(lanceur)); i < finCamp(combatCamp(lanceur)); i++) {
            if (combatVivant(cb, i)
                && (meilleur < 0 || cb->c[i].p->pv * 100 / cb->c[i].p->pvMax
                                    < cb->c[meilleur].p->pv * 100 / cb->c[meilleur].p->pvMax)) {
                meilleur = i;
            }
        }
        return meilleur;
    }
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (combatCibleValide(cb, lanceur, action, i)) {
            return i;
        }
    }
    return -1;
}

int combatEstTouche(const Combat *cb, int lanceur, enum TypeAction action, int cible, int i)
{
    const Capacite *cap = capaciteClasse(cb->c[lanceur].p->classe, action);
    int memeCamp = combatCamp(i) == combatCamp(lanceur);
    switch (cap->ciblage) {
    case CIBLE_ENNEMI:
    case CIBLE_ALLIE:
        return i == cible;
    case CIBLE_TOUS_ENNEMIS:
        return !memeCamp && combatVivant(cb, i);
    case CIBLE_TOUS_ALLIES:
        return memeCamp && cb->c[i].present
               && (combatVivant(cb, i) || (cb->c[lanceur].p->classe == PRETRE
                                           && action == ACTION_ULTIME));
    case CIBLE_ZONE: {
        int liste[COMBAT_MAX];
        int n = adversairesDe(cb, lanceur, liste);
        for (int k = 0; k < n; k++) {
            if (liste[k] == cible) {
                return i == cible || (k > 0 && liste[k - 1] == i)
                       || (k + 1 < n && liste[k + 1] == i);
            }
        }
        return 0;
    }
    }
    return 0;
}

/** Capacité « zone » : la cible prend `principal` %, ses voisins `voisins` %. */
static void frapperZone(Combat *cb, int a, int cible, int principal, int voisins, int resistance)
{
    int liste[COMBAT_MAX];
    int n = adversairesDe(cb, a, liste);
    int gauche = -1, droite = -1;
    for (int k = 0; k < n; k++) {
        if (liste[k] == cible) {
            gauche = k > 0 ? liste[k - 1] : -1;
            droite = k + 1 < n ? liste[k + 1] : -1;
        }
    }
    frapper(cb, a, cible, principal, resistance);
    if (gauche >= 0) frapper(cb, a, gauche, voisins, resistance / 2);
    if (droite >= 0) frapper(cb, a, droite, voisins, resistance / 2);
}

/** Frappe tous les adversaires vivants de `a`. */
static void frapperTous(Combat *cb, int a, int pourcent, int resistance)
{
    int liste[COMBAT_MAX];
    int n = adversairesDe(cb, a, liste);
    for (int k = 0; k < n; k++) {
        frapper(cb, a, liste[k], pourcent, resistance);
    }
}

/** Applique un bonus d'ATK. */
static void donnerBuffAtk(Combat *cb, int c, int pourcent, int tours)
{
    Combattant *cc = &cb->c[c];
    if (pourcent >= cc->buffAtk) {
        cc->buffAtk = pourcent;
    }
    if (tours > cc->toursBuffAtk) {
        cc->toursBuffAtk = tours;
    }
    char texte[32];
    snprintf(texte, sizeof texte, "ATK +%d %%", pourcent);
    ajouterEffet(cb, c, EFFET_BONUS, pourcent, 0, IMAGINAIRE, texte);
}

/** Exécute la capacité d'un héros (sans gérer SP, énergie ni fin de tour). */
static void executerCapacite(Combat *cb, int a, enum TypeAction action, int cible)
{
    Combattant *ca = &cb->c[a];
    Personnage *p = ca->p;
    int allies[COMBAT_MAX];
    int nbAllies;

    if (action == ACTION_BASIQUE) {
        frapper(cb, a, cible, 100, 10);
        return;
    }

    int ult = (action == ACTION_ULTIME);
    switch (p->classe) {
    case GUERRIER:
        if (!ult) {
            frapperZone(cb, a, cible, 130, 65, 20);
        } else {
            frapper(cb, a, cible, 380, 30);
            ca->bouclier += 10 + 2 * p->def;
            ajouterEffet(cb, a, EFFET_BOUCLIER, ca->bouclier, 0, PHYSIQUE, NULL);
        }
        break;

    case MAGE:
        if (!ult) {
            frapperZone(cb, a, cible, 150, 75, 20);
        } else {
            frapperTous(cb, a, 200, 20);
        }
        break;

    case VOLEUR:
        if (!ult) {
            frapper(cb, a, cible, 170, 20);
            if (combatVivant(cb, cible)) {
                appliquerDot(&cb->c[cible], attaqueTotale(p) / 2, 2, p->element);
                ajouterEffet(cb, cible, EFFET_MALUS, 0, 0, p->element, "Empoisonné");
            }
        } else {
            for (int coup = 0; coup < 6; coup++) {
                int liste[COMBAT_MAX];
                int n = adversairesDe(cb, a, liste);
                if (n == 0) {
                    break;
                }
                cb->nbCoups = coup + 1;
                frapper(cb, a, liste[rand() % n], 70, 10);
            }
        }
        break;

    case PALADIN:
        nbAllies = alliesDe(cb, a, allies);
        if (!ult) {
            for (int k = 0; k < nbAllies; k++) {
                Combattant *c = &cb->c[allies[k]];
                int bouclier = 12 + 3 * p->def;
                if (bouclier > c->bouclier) {
                    c->bouclier = bouclier;
                }
                ajouterEffet(cb, allies[k], EFFET_BOUCLIER, bouclier, 0, p->element, NULL);
            }
        } else {
            frapperTous(cb, a, 150, 20);
            for (int k = 0; k < nbAllies; k++) {
                soignerCombattant(cb, allies[k], cb->c[allies[k]].p->pvMax * 15 / 100, 0);
            }
        }
        break;

    case ARCHER:
        if (!ult) {
            frapper(cb, a, cible, 240, 20);
        } else {
            frapperTous(cb, a, 170, 20);
            /* L'ultime ne termine pas le tour : on agit sur le prochain. */
            decalerAction(ca, 50);
            ajouterEffet(cb, a, EFFET_BONUS, 50, 0, p->element, "Avance 50 %");
        }
        break;

    case PRETRE: {
        int bonus = attaqueTotale(p);
        if (!ult) {
            Combattant *c = &cb->c[cible];
            soignerCombattant(cb, cible, c->p->pvMax * 15 / 100 + bonus, 0);
            if (c->dot > 0) {
                c->dot = c->toursDot = 0;
                ajouterEffet(cb, cible, EFFET_BONUS, 0, 0, p->element, "Purifié");
            }
        } else {
            int camp = combatCamp(a);
            for (int i = debutCamp(camp); i < finCamp(camp); i++) {
                if (cb->c[i].present) {
                    int base = estVivant(cb->c[i].p) ? cb->c[i].p->pvMax * 20 / 100
                                                     : cb->c[i].p->pvMax * 25 / 100;
                    soignerCombattant(cb, i, base + bonus * 8 / 10, 1);
                }
            }
        }
        break;
    }

    case NECROMANCIEN:
        if (!ult) {
            int degats = frapper(cb, a, cible, 160, 20);
            soignerCombattant(cb, a, degats / 2, 0);
        } else {
            frapperTous(cb, a, 130, 20);
            int liste[COMBAT_MAX];
            int n = adversairesDe(cb, a, liste);
            for (int k = 0; k < n; k++) {
                Combattant *c = &cb->c[liste[k]];
                appliquerDot(c, attaqueTotale(p) * 6 / 10, 3, p->element);
                c->malusDef = 25;
                c->toursMalusDef = 3;
                ajouterEffet(cb, liste[k], EFFET_MALUS, 25, 0, p->element, "DEF -25 %");
            }
        }
        break;

    case BARDE:
        if (!ult) {
            donnerBuffAtk(cb, cible, 30, 2);
            if (cible != a) {
                cb->c[cible].av = 0; /* rejoue tout de suite */
                ajouterEffet(cb, cible, EFFET_BONUS, 100, 0, p->element, "Rejoue !");
            }
        } else {
            nbAllies = alliesDe(cb, a, allies);
            for (int k = 0; k < nbAllies; k++) {
                donnerBuffAtk(cb, allies[k], 25, 3);
                decalerAction(&cb->c[allies[k]], 40);
            }
        }
        break;

    default:
        frapper(cb, a, cible, 100, 10);
        break;
    }
}

int combatJouer(Combat *cb, int lanceur, enum TypeAction action, int cible)
{
    if (!combatPeutAgir(cb, lanceur, action)
        || !combatCibleValide(cb, lanceur, action, cible)) {
        return 0;
    }

    Combattant *c = &cb->c[lanceur];
    int *sp = &cb->sp[combatCamp(lanceur)];
    cb->nbEffets = 0;
    cb->nbCoups = 1;

    const Capacite *cap = capaciteClasse(c->p->classe, action);
    journaliser(cb, "%s : %s", c->p->nom, cap->nom);

    switch (action) {
    case ACTION_BASIQUE:
        if (*sp < SP_MAX) {
            (*sp)++;
        }
        gagnerEnergie(c, ENERGIE_BASIQUE);
        break;
    case ACTION_COMPETENCE:
        (*sp)--;
        gagnerEnergie(c, ENERGIE_COMPETENCE);
        break;
    case ACTION_ULTIME:
        c->energie = 5;
        break;
    default:
        break;
    }

    executerCapacite(cb, lanceur, action, cible);

    if (action != ACTION_ULTIME) {
        finTour(cb, lanceur);
    }
    verifierFin(cb);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Intelligence des monstres                                         */
/* ------------------------------------------------------------------ */

/** Choisit le héros visé par un monstre : les guerriers et paladins
 *  attirent davantage les coups. */
static int choisirCibleMonstre(const Combat *cb)
{
    int liste[COMBAT_MAX_ALLIES];
    int n = vivantsCamp(cb, 0, liste);
    int poids[COMBAT_MAX_ALLIES];
    int total = 0;
    for (int k = 0; k < n; k++) {
        enum Classe cl = cb->c[liste[k]].p->classe;
        poids[k] = (cl == PALADIN || cl == GUERRIER) ? 3 : 2;
        total += poids[k];
    }
    int tirage = rand() % total;
    for (int k = 0; k < n; k++) {
        if (tirage < poids[k]) {
            return liste[k];
        }
        tirage -= poids[k];
    }
    return liste[0];
}

void combatTourEnnemi(Combat *cb)
{
    int a = cb->actif;
    if (cb->etat != COMBAT_TOUR_ENNEMI || !combatVivant(cb, a) || cb->c[a].heros) {
        return;
    }
    Combattant *c = &cb->c[a];
    enum Monstre m = c->monstre;
    cb->nbEffets = 0;
    cb->nbCoups = 1;

    int allies[COMBAT_MAX_ALLIES];
    int n = vivantsCamp(cb, 0, allies);
    int cible = choisirCibleMonstre(cb);

    /* Boss sous la moitié de ses PV : il passe en phase 2 (une seule fois).
     * Garde restaurée, ATK +30 % jusqu'à la fin, attaques spéciales plus
     * fréquentes. La transformation lui coûte son tour. */
    if (c->boss && c->phase == 1 && c->p->pv * 2 < c->p->pvMax) {
        c->phase = 2;
        c->brise = 0;
        c->resistance = c->resistanceMax;
        c->dot = c->toursDot = 0;
        c->gele = 0;
        c->buffAtk = 30;
        c->toursBuffAtk = 999;
        journaliser(cb, "%s entre dans une rage folle : PHASE 2 !", c->p->nom);
        ajouterEffet(cb, a, EFFET_BONUS, 30, 0, c->p->element, "PHASE 2");
        finTour(cb, a);
        c->av = avDeBase(c) / 2; /* et rejoue vite */
        return;
    }

    int chance = MONSTRES[m].chanceSpecial + (c->phase == 2 ? 20 : 0);
    if ((rand() % 100) < chance) {
        journaliser(cb, "%s : %s", c->p->nom, MONSTRES[m].special);
        switch (MONSTRES[m].typeSpecial) {
        case SPECIAL_ZONE:
            for (int k = 0; k < n; k++) {
                int d = allies[k] - cible;
                if (d >= -1 && d <= 1) {
                    frapper(cb, a, allies[k], allies[k] == cible ? 110 : 60, 0);
                }
            }
            break;
        case SPECIAL_FORT:
            frapper(cb, a, cible, 190, 0);
            break;
        case SPECIAL_RAGE:
            donnerBuffAtk(cb, a, 40, 3);
            frapper(cb, a, cible, 80, 0);
            break;
        case SPECIAL_TOUS:
            for (int k = 0; k < n; k++) {
                frapper(cb, a, allies[k], c->boss ? 90 : 70, 0);
                if (c->boss && combatVivant(cb, allies[k]) && rand() % 2 == 0) {
                    appliquerDot(&cb->c[allies[k]], c->p->atk / 2, 2, FEU);
                    ajouterEffet(cb, allies[k], EFFET_MALUS, 0, 0, FEU, "Brûlure");
                }
            }
            break;
        case SPECIAL_POISON:
            frapper(cb, a, cible, 100, 0);
            if (combatVivant(cb, cible)) {
                appliquerDot(&cb->c[cible], c->p->atk / 2, 2, c->p->element);
                ajouterEffet(cb, cible, EFFET_MALUS, 0, 0, c->p->element,
                             MONSTRES[m].etat != NULL ? MONSTRES[m].etat : "Poison");
            }
            break;
        case SPECIAL_INVOCATION: {
            /* une gelée en renfort (deux en phase 2) ; sans place, il
             * écrase la ligne de front */
            int invoques = 0;
            for (int k = 0; k < c->phase; k++) {
                if (invoquer(cb, MONSTRE_GELEE) >= 0) invoques++;
            }
            if (invoques == 0) {
                for (int k = 0; k < n; k++) {
                    int d = allies[k] - cible;
                    if (d >= -1 && d <= 1) frapper(cb, a, allies[k], allies[k] == cible ? 120 : 70, 0);
                }
            } else {
                journaliser(cb, "%d gelée%s répond%s à l'appel !", invoques, invoques > 1 ? "s" : "",
                            invoques > 1 ? "ent" : "");
            }
            break;
        }
        case SPECIAL_DRAIN: {
            int total = 0;
            for (int k = 0; k < n; k++) {
                total += frapper(cb, a, allies[k], 75, 0);
            }
            soignerCombattant(cb, a, total / 2, 0);
            break;
        }
        case SPECIAL_NEANT:
            for (int k = 0; k < n; k++) {
                frapper(cb, a, allies[k], 85, 0);
                if (combatVivant(cb, allies[k])) {
                    Combattant *h = &cb->c[allies[k]];
                    h->malusDef = 30;
                    h->toursMalusDef = 2;
                    h->energie -= h->energie / 4; /* le Néant aspire l'énergie */
                    ajouterEffet(cb, allies[k], EFFET_MALUS, 30, 0, c->p->element, "DEF -30 %");
                }
            }
            break;
        }
    } else {
        journaliser(cb, "%s : %s", c->p->nom, MONSTRES[m].attaque);
        frapper(cb, a, cible, c->boss ? 130 : 100, 0);
    }

    finTour(cb, a);
    verifierFin(cb);
}

/* ------------------------------------------------------------------ */
/*  Ligne de temps                                                    */
/* ------------------------------------------------------------------ */

void combatAvancer(Combat *cb)
{
    if (cb->etat == COMBAT_VICTOIRE || cb->etat == COMBAT_DEFAITE
        || cb->etat == COMBAT_FIN_VAGUE || !cb->tourFini) {
        return;
    }
    cb->nbEffets = 0;
    cb->nbCoups = 1;

    /* Boucle : un combattant gelé ou tué par un poison passe son tour. */
    for (int garde = 0; garde < 100; garde++) {
        int suivant = -1;
        for (int i = 0; i < COMBAT_MAX; i++) {
            if (combatVivant(cb, i) && (suivant < 0 || cb->c[i].av < cb->c[suivant].av)) {
                suivant = i;
            }
        }
        if (suivant < 0) {
            return;
        }

        /* Le temps s'écoule pour tout le monde. */
        double ecoule = cb->c[suivant].av;
        for (int i = 0; i < COMBAT_MAX; i++) {
            if (combatVivant(cb, i)) {
                cb->c[i].av -= ecoule;
            }
        }
        cb->actif = suivant;
        cb->tourFini = 0;
        Combattant *c = &cb->c[suivant];

        /* Une cible brisée récupère sa garde au début de son tour. */
        if (c->brise) {
            c->brise = 0;
            c->resistance = c->resistanceMax;
        }

        if (c->toursDot > 0) {
            c->toursDot--;
            subirDegats(cb, -1, suivant, c->dot, 0, c->elementDot);
            if (c->toursDot == 0) {
                c->dot = 0;
            }
            if (verifierFin(cb)) {
                return;
            }
            if (!estVivant(c->p)) {
                continue;
            }
        }

        if (c->gele > 0) {
            c->gele--;
            ajouterEffet(cb, suivant, EFFET_TEXTE, 0, 0, GLACE, "Gelé : passe son tour");
            journaliser(cb, "%s est gelé et passe son tour.", c->p->nom);
            finTour(cb, suivant);
            continue;
        }

        cb->etat = c->heros ? COMBAT_TOUR_JOUEUR : COMBAT_TOUR_ENNEMI;
        return;
    }
}

void combatVagueSuivante(Combat *cb)
{
    if (cb->etat != COMBAT_FIN_VAGUE) {
        return;
    }
    cb->vague++;
    genererVague(cb);
    cb->nbEffets = 0;
    journaliser(cb, "Vague %d/%d !", cb->vague, cb->nbVagues);

    /* Si un héros a fini la vague avec son ultime, son tour continue. */
    cb->etat = COMBAT_TOUR_JOUEUR;
    if (cb->tourFini || !cb->c[cb->actif].heros || !combatVivant(cb, cb->actif)) {
        cb->tourFini = 1;
        combatAvancer(cb);
    }
}

int combatOrdre(const Combat *cb, int ordre[], double avs[], int max)
{
    double temps[COMBAT_MAX];
    for (int i = 0; i < COMBAT_MAX; i++) {
        temps[i] = combatVivant(cb, i) ? cb->c[i].av : -1;
    }

    int n = 0;
    if (!cb->tourFini && combatVivant(cb, cb->actif) && n < max) {
        /* Le combattant actif d'abord ; il rejouera après sa durée de base. */
        ordre[n] = cb->actif;
        avs[n] = 0;
        n++;
        temps[cb->actif] = avDeBase(&cb->c[cb->actif]);
    }
    while (n < max) {
        int suivant = -1;
        for (int i = 0; i < COMBAT_MAX; i++) {
            if (temps[i] >= 0 && (suivant < 0 || temps[i] < temps[suivant])) {
                suivant = i;
            }
        }
        if (suivant < 0) {
            break;
        }
        ordre[n] = suivant;
        avs[n] = temps[suivant];
        n++;
        temps[suivant] += avDeBase(&cb->c[suivant]);
    }
    return n;
}
