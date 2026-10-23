/**
 * @file gui_combat.c
 * @brief Écran de combat tour par tour (style Honkai: Star Rail).
 *
 * Deux modes : contre les bots (monstres) ou duel local, où les deux
 * joueurs utilisent les mêmes commandes chacun leur tour.
 *
 * Commandes :
 *   Q / clic  : attaque de base (2e appui = confirmer)
 *   E / clic  : compétence (coûte 1 point de compétence)
 *   1..4      : ultime du héros correspondant (jauge d'énergie pleine)
 *   ← → / clic sur une cible : changer de cible
 *   Espace / Entrée : confirmer        Échap : annuler l'ultime
 *
 * Le moteur (combat.c) applique chaque action immédiatement et renvoie la
 * liste de ses effets ; ce module se contente de les mettre en scène.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui_app.h"
#include "gui_sprite.h"
#include "boutique.h"

#define PI_F 3.14159265f
#define FX_MAX 160

/** Étapes de la mise en scène. */
enum Phase {
    PHASE_ATTENTE,        /**< le joueur choisit une action            */
    PHASE_CINEMATIQUE,    /**< bannière d'ultime                       */
    PHASE_ACTION,         /**< animation d'une action                  */
    PHASE_ENNEMI,         /**< petite pause avant l'action d'un monstre */
    PHASE_DEBUT_TOUR,     /**< effets de début de tour (poison, gel)    */
    PHASE_FIN_VAGUE,      /**< vague vaincue                           */
    PHASE_RESULTAT        /**< victoire ou défaite                     */
};

/** Effet visuel : texte flottant ou particule. */
typedef struct {
    int actif;
    int texte;            /**< 1 : texte, 0 : particule                */
    float x, y, vx, vy;
    float vie, vieMax;
    float taille;
    SDL_Color c;
    enum Police police;
    char s[32];
} Fx;

struct CombatGui {
    Combat cb;
    Personnage *equipe[COMBAT_MAX_ALLIES];
    int niveauAvant[COMBAT_MAX_ALLIES];
    int etage;

    enum Phase phase;
    float tPhase;

    /* sélection du joueur */
    enum TypeAction choix;
    int cible;
    int lanceur;          /**< héros qui agit (l'actif, ou celui de l'ultime) */

    /* action en cours d'animation */
    int acteur;
    enum TypeAction actionAnim;
    int estMonstre;
    Effet effets[EFFETS_MAX];
    int nbEffets;
    int nbCoups;
    int coupsJoues;

    /* état affiché (suit l'état réel avec un temps de retard) */
    float affPv[COMBAT_MAX];
    float affRes[COMBAT_MAX];
    float flash[COMBAT_MAX];
    float apparition[COMBAT_MAX];
    float secousse;
    float banniere;
    char texteBanniere[48];

    Fx fx[FX_MAX];
    int recompensesVues;
    int foieGrasGagne;      /* foie gras gagné à la victoire */
    int dernierCamp;      /**< duel : camp du dernier tour (annonce)   */
    int revanche;         /**< duel : relancer le même duel            */
};

/* Couleurs des deux joueurs en duel. */
#define COUL_J1 rgba(90, 160, 255, 255)
#define COUL_J2 rgba(255, 100, 110, 255)

/* Décalage de l'écran (secousse), appliqué à toute la scène. */
static float decalX = 0, decalY = 0;

/* ------------------------------------------------------------------ */
/*  Placement                                                         */
/* ------------------------------------------------------------------ */

/** Position (pieds) et échelle du combattant i sur la scène. */
static void position(const CombatGui *cg, int i, float *x, float *y, float *s)
{
    if (combatEstAllie(i)) {
        *x = 160 + i * 122;
        *y = 462 + (i % 2) * 44;
        *s = 1.0f;
    } else if (cg->cb.c[i].heros) {
        /* duel : l'équipe 2 se tient à droite, face à l'équipe 1 */
        int k = i - PREMIER_ENNEMI;
        *x = 1120 - k * 122;
        *y = 440 + (k % 2) * 44;
        *s = 1.0f;
    } else {
        int n = 0, rang = 0, boss = 0;
        for (int k = PREMIER_ENNEMI; k < COMBAT_MAX; k++) {
            if (cg->cb.c[k].present) {
                if (k == i) rang = n;
                if (cg->cb.c[k].boss) boss = 1;
                n++;
            }
        }
        float ecart = boss ? 205 : 170;
        *x = 890 + (rang - (n - 1) / 2.0f) * ecart;
        *y = 430 + (rang % 2) * 18;
        *s = cg->cb.c[i].boss ? 0.95f : 1.0f;
    }
    *x += decalX;
    *y += decalY;
}

/** Hauteur visuelle du combattant (pour placer barres et effets). */
static float hauteur(const CombatGui *cg, int i)
{
    float x, y, s;
    position(cg, i, &x, &y, &s);
    return cg->cb.c[i].heros ? 150 * s : hauteurMonstre(cg->cb.c[i].monstre) * s;
}

/** Centre du corps (point d'impact). */
static void centre(const CombatGui *cg, int i, float *cx, float *cy)
{
    float x, y, s;
    position(cg, i, &x, &y, &s);
    *cx = x;
    *cy = y - hauteur(cg, i) * 0.5f;
}

static int estMelee(enum Classe c)
{
    return c == GUERRIER || c == VOLEUR || c == PALADIN;
}

/* ------------------------------------------------------------------ */
/*  Effets visuels                                                    */
/* ------------------------------------------------------------------ */

static Fx *nouveauFx(CombatGui *cg)
{
    Fx *plusVieux = &cg->fx[0];
    for (int i = 0; i < FX_MAX; i++) {
        if (!cg->fx[i].actif) return &cg->fx[i];
        if (cg->fx[i].vie < plusVieux->vie) plusVieux = &cg->fx[i];
    }
    return plusVieux;
}

static void texteFlottant(CombatGui *cg, float x, float y, const char *s, SDL_Color c, enum Police p)
{
    Fx *f = nouveauFx(cg);
    memset(f, 0, sizeof *f);
    f->actif = 1;
    f->texte = 1;
    f->x = x + (rand() % 30 - 15);
    f->y = y;
    f->vy = -55;
    f->vie = f->vieMax = 1.3f;
    f->c = c;
    f->police = p;
    snprintf(f->s, sizeof f->s, "%s", s);
}

static void eclats(CombatGui *cg, float x, float y, SDL_Color c, int n, float vitesse)
{
    for (int k = 0; k < n; k++) {
        Fx *f = nouveauFx(cg);
        memset(f, 0, sizeof *f);
        f->actif = 1;
        float a = (rand() % 628) / 100.0f;
        float v = vitesse * (0.4f + (rand() % 100) / 100.0f);
        f->x = x;
        f->y = y;
        f->vx = cosf(a) * v;
        f->vy = sinf(a) * v - 60;
        f->vie = f->vieMax = 0.5f + (rand() % 50) / 100.0f;
        f->taille = 2 + rand() % 4;
        f->c = c;
    }
}

static void majFx(CombatGui *cg, float dt)
{
    for (int i = 0; i < FX_MAX; i++) {
        Fx *f = &cg->fx[i];
        if (!f->actif) continue;
        f->vie -= dt;
        if (f->vie <= 0) {
            f->actif = 0;
            continue;
        }
        f->x += f->vx * dt;
        f->y += f->vy * dt;
        if (f->texte) {
            f->vy *= 0.93f;
        } else {
            f->vy += 300 * dt;
        }
    }
}

static void dessinerFx(Gui *g, CombatGui *cg)
{
    for (int i = 0; i < FX_MAX; i++) {
        Fx *f = &cg->fx[i];
        if (!f->actif) continue;
        float r = f->vie / f->vieMax;
        int alpha = (int)(255 * (r > 0.3f ? 1 : r / 0.3f));
        if (f->texte) {
            texteOmbre(g, f->police, f->s, f->x, f->y, avecAlpha(f->c, alpha), ALIGN_CENTRE);
        } else {
            halo(g, f->x, f->y, f->taille * 3, avecAlpha(f->c, alpha / 3));
            cercle(g, f->x, f->y, f->taille * r + 0.5f, avecAlpha(f->c, alpha));
        }
    }
}

/** Joue les effets du coup numéro `coup` (textes, éclats, flash). */
static void jouerCoup(CombatGui *cg, int coup)
{
    char s[32];
    int empiles[COMBAT_MAX] = { 0 }; /* textes déjà posés sur chaque cible */
    for (int k = 0; k < cg->nbEffets; k++) {
        const Effet *e = &cg->effets[k];
        if (e->coup != coup) continue;
        float x, y;
        centre(cg, e->cible, &x, &y);
        /* plusieurs nombres sur la même cible : on les empile */
        float haut = y - hauteur(cg, e->cible) * 0.5f - 10 - empiles[e->cible] * 30;
        if (e->type == EFFET_DEGATS || e->type == EFFET_SOIN || e->type == EFFET_BOUCLIER) {
            empiles[e->cible]++;
        }
        SDL_Color ce = couleurElement(e->element);
        switch (e->type) {
        case EFFET_DEGATS:
            snprintf(s, sizeof s, "%d", e->valeur);
            if (e->critique) {
                texteFlottant(cg, x, haut - 20, "CRITIQUE", COUL_OR_CLAIR, POLICE_PETITE);
                texteFlottant(cg, x, haut, s, COUL_OR_CLAIR, POLICE_TITRE);
                cg->secousse = 9;
            } else {
                texteFlottant(cg, x, haut, s,
                              cg->cb.c[e->cible].heros && cg->cb.mode == MODE_BOTS
                                  ? rgba(255, 110, 110, 255) : melanger(ce, rgba(255, 255, 255, 255), 0.4f),
                              POLICE_GRANDE);
                if (cg->secousse < 3) cg->secousse = 3;
            }
            eclats(cg, x, y, ce, 12, 160);
            cg->flash[e->cible] = 1;
            break;
        case EFFET_SOIN:
            snprintf(s, sizeof s, "+%d", e->valeur);
            texteFlottant(cg, x, haut, s, COUL_VIE, POLICE_GRANDE);
            eclats(cg, x, y + 30, COUL_VIE, 10, 90);
            break;
        case EFFET_BOUCLIER:
            snprintf(s, sizeof s, "Bouclier %d", e->valeur);
            texteFlottant(cg, x, haut, s, COUL_ACCENT, POLICE_MOYENNE);
            eclats(cg, x, y, COUL_ACCENT, 8, 80);
            break;
        case EFFET_BRISE:
            texteFlottant(cg, x, haut - 40, e->texte, ce, POLICE_TITRE);
            eclats(cg, x, y, ce, 40, 320);
            eclats(cg, x, y, rgba(255, 255, 255, 255), 16, 260);
            cg->secousse = 14;
            break;
        case EFFET_KO:
            texteFlottant(cg, x, haut - 64, "K.O.", rgba(200, 200, 210, 255), POLICE_MOYENNE);
            break;
        default: /* bonus, malus, texte */
            texteFlottant(cg, x, haut - 50, e->texte,
                          e->type == EFFET_MALUS ? ce : COUL_OR_CLAIR, POLICE_NORMALE);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Enchaînement des phases                                           */
/* ------------------------------------------------------------------ */

static void allerPhase(CombatGui *cg, enum Phase p)
{
    cg->phase = p;
    cg->tPhase = 0;
}

static void banniere(CombatGui *cg, const char *texte)
{
    snprintf(cg->texteBanniere, sizeof cg->texteBanniere, "%s", texte);
    cg->banniere = 1.8f;
}

/** Prépare la sélection par défaut pour le héros dont c'est le tour. */
static void preparerTourJoueur(CombatGui *cg)
{
    int camp = combatCamp(cg->cb.actif);
    if (cg->cb.mode == MODE_DUEL && camp != cg->dernierCamp) {
        banniere(cg, camp == 0 ? "Au tour du joueur 1" : "Au tour du joueur 2");
        cg->cible = -1; /* l'ancienne cible est dans le mauvais camp */
    }
    cg->dernierCamp = camp;
    cg->lanceur = cg->cb.actif;
    cg->choix = ACTION_BASIQUE;
    if (cg->cible < 0 || !combatCibleValide(&cg->cb, cg->lanceur, cg->choix, cg->cible)) {
        cg->cible = combatCibleParDefaut(&cg->cb, cg->lanceur, cg->choix);
    }
    allerPhase(cg, PHASE_ATTENTE);
}

/** Copie les effets produits par le moteur pour les animer. */
static void copierEffets(CombatGui *cg, int acteur, enum TypeAction action, int monstre)
{
    cg->acteur = acteur;
    cg->actionAnim = action;
    cg->estMonstre = monstre;
    cg->nbEffets = cg->cb.nbEffets;
    memcpy(cg->effets, cg->cb.effets, sizeof cg->effets[0] * (size_t)cg->nbEffets);
    cg->nbCoups = cg->cb.nbCoups > 0 ? cg->cb.nbCoups : 1;
    cg->coupsJoues = 0;
}

/** Après une animation : vague suivante, fin du combat ou tour suivant. */
static void suite(CombatGui *cg)
{
    Combat *cb = &cg->cb;
    switch (cb->etat) {
    case COMBAT_VICTOIRE:
    case COMBAT_DEFAITE:
        allerPhase(cg, PHASE_RESULTAT);
        return;
    case COMBAT_FIN_VAGUE:
        banniere(cg, "Vague terminée !");
        allerPhase(cg, PHASE_FIN_VAGUE);
        return;
    default:
        break;
    }

    if (cb->tourFini) {
        combatAvancer(cb);
        if (cb->nbEffets > 0) {
            /* poison, brûlure, gel... au début du tour */
            copierEffets(cg, cb->actif, ACTION_BASIQUE, 0);
            jouerCoup(cg, 0);
            allerPhase(cg, PHASE_DEBUT_TOUR);
            return;
        }
        if (cb->etat == COMBAT_VICTOIRE || cb->etat == COMBAT_DEFAITE || cb->etat == COMBAT_FIN_VAGUE) {
            suite(cg);
            return;
        }
    }
    if (cb->etat == COMBAT_TOUR_JOUEUR) {
        preparerTourJoueur(cg);
    } else {
        allerPhase(cg, PHASE_ENNEMI);
    }
}

/** Lance l'action choisie par le joueur. */
static void lancer(CombatGui *cg)
{
    int lanceur = cg->lanceur;
    enum TypeAction action = cg->choix;
    if (!combatJouer(&cg->cb, lanceur, action, cg->cible)) {
        return;
    }
    copierEffets(cg, lanceur, action, 0);
    allerPhase(cg, action == ACTION_ULTIME ? PHASE_CINEMATIQUE : PHASE_ACTION);
}

/** Choisit une action (un 2e choix identique la confirme). */
static void choisir(CombatGui *cg, int lanceur, enum TypeAction action)
{
    if (!combatPeutAgir(&cg->cb, lanceur, action)) {
        return;
    }
    if (cg->choix == action && cg->lanceur == lanceur) {
        lancer(cg);
        return;
    }
    cg->choix = action;
    cg->lanceur = lanceur;
    if (!combatCibleValide(&cg->cb, lanceur, action, cg->cible)) {
        cg->cible = combatCibleParDefaut(&cg->cb, lanceur, action);
    }
}

/** Cible suivante/précédente valide pour l'action choisie. */
static void changerCible(CombatGui *cg, int sens)
{
    for (int k = 1; k <= COMBAT_MAX; k++) {
        int i = (cg->cible + sens * k + COMBAT_MAX * 2) % COMBAT_MAX;
        if (combatCibleValide(&cg->cb, cg->lanceur, cg->choix, i)) {
            cg->cible = i;
            return;
        }
    }
}

/** Durée totale de l'animation d'action. */
static float dureeAction(const CombatGui *cg)
{
    return 0.85f + (cg->nbCoups - 1) * 0.16f;
}

/** Instant (s) du coup numéro k. */
static float instantCoup(int k)
{
    return 0.32f + k * 0.16f;
}

/* ------------------------------------------------------------------ */
/*  Création / destruction                                            */
/* ------------------------------------------------------------------ */

CombatGui *combatGuiCreer(App *app)
{
    CombatGui *cg = calloc(1, sizeof *cg);
    if (cg == NULL) {
        perror("calloc");
        return NULL;
    }
    const Preparation *prep = &app->prep;
    Personnage *equipe2[COMBAT_MAX_ALLIES];
    int n = 0, n2 = 0;
    for (int k = 0; k < prep->nb[0] && n < COMBAT_MAX_ALLIES; k++) {
        Personnage *p = &app->donjon.elements[prep->choisis[0][k]];
        cg->equipe[n] = p;
        cg->niveauAvant[n] = p->niveau;
        n++;
    }
    for (int k = 0; k < prep->nb[1] && n2 < COMBAT_MAX_ALLIES; k++) {
        equipe2[n2++] = &app->donjon.elements[prep->choisis[1][k]];
    }
    cg->etage = prep->etage;
    int ok = prep->mode == MODE_DUEL ? combatInitDuel(&cg->cb, cg->equipe, n, equipe2, n2)
                                     : combatInit(&cg->cb, cg->equipe, n, cg->etage);
    if (!ok) {
        free(cg);
        return NULL;
    }
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (cg->cb.c[i].present) {
            cg->affPv[i] = (float)cg->cb.c[i].p->pv;
            cg->affRes[i] = (float)cg->cb.c[i].resistance;
            cg->apparition[i] = cg->cb.c[i].heros ? 1 : 0;
        }
    }
    cg->cible = -1;
    char titre[48];
    if (cg->cb.mode == MODE_DUEL) {
        snprintf(titre, sizeof titre, "Duel - %d contre %d", n, n2);
        cg->dernierCamp = combatCamp(cg->cb.actif); /* pas de 2e bannière */
    } else {
        snprintf(titre, sizeof titre, "Étage %d - Vague 1/%d", cg->etage, cg->cb.nbVagues);
    }
    banniere(cg, titre);
    if (cg->cb.etat == COMBAT_TOUR_JOUEUR) {
        preparerTourJoueur(cg);
    } else {
        allerPhase(cg, PHASE_ENNEMI);
    }
    return cg;
}

void combatGuiDetruire(CombatGui *cg)
{
    free(cg); /* free(NULL) ne fait rien */
}

/* ------------------------------------------------------------------ */
/*  Dessin de la scène                                                */
/* ------------------------------------------------------------------ */

static void dessinerDecor(Gui *g, int etage)
{
    float t = g->temps / 1000.0f;
    static const SDL_Color CIELS[ETAGE_MAX][2] = {
        { { 30, 24, 70, 255 }, { 70, 50, 110, 255 } },   /* 1 caves         */
        { { 24, 40, 50, 255 }, { 60, 90, 90, 255 } },    /* 2 champignons   */
        { { 16, 40, 60, 255 }, { 40, 100, 130, 255 } },  /* 3 trône visqueux */
        { { 20, 20, 30, 255 }, { 60, 60, 90, 255 } },    /* 4 ossuaire      */
        { { 50, 20, 20, 255 }, { 130, 60, 40, 255 } },   /* 5 forge         */
        { { 20, 30, 30, 255 }, { 40, 80, 70, 255 } },    /* 6 crypte        */
        { { 40, 44, 60, 255 }, { 100, 104, 130, 255 } }, /* 7 remparts      */
        { { 60, 16, 16, 255 }, { 150, 60, 30, 255 } },   /* 8 nid du dragon */
        { { 10, 6, 24, 255 }, { 40, 20, 60, 255 } },     /* 9 abîme         */
        { { 4, 0, 10, 255 }, { 50, 10, 70, 255 } },      /* 10 néant        */
    };
    int e = (etage - 1) % ETAGE_MAX;
    SDL_Color haut = CIELS[e][0], bas = CIELS[e][1];
    degradeVertical(g, 0, 0, LARGEUR, 400, haut, bas);
    halo(g, 900 + decalX, 250, 420, avecAlpha(melanger(bas, rgba(255, 220, 180, 255), 0.4f), 70));

    if (etage >= 9) {
        /* le vide : des étoiles violettes à la place des piliers */
        for (int k = 0; k < 60; k++) {
            unsigned h = (unsigned)k * 2654435761u;
            float sx = (float)(h % LARGEUR) + decalX * 0.2f, sy = (float)((h / LARGEUR) % 370);
            float sc = 0.5f + 0.5f * sinf(t * 1.3f + k);
            cercle(g, sx, sy, 1 + (k % 3) * 0.6f, rgba(200, 150, 255, (int)(60 + 140 * sc)));
        }
        if (etage == ETAGE_MAX) {
            /* l'œil du Néant */
            float o = 0.6f + 0.4f * sinf(t * 0.8f);
            halo(g, 640 + decalX * 0.3f, 150, 200, rgba(160, 60, 240, (int)(60 * o)));
            ellipse(g, 640 + decalX * 0.3f, 150, 90, 30 * o, rgba(20, 0, 30, 220));
            cercle(g, 640 + decalX * 0.3f, 150, 18 * o, rgba(220, 120, 255, 230));
        }
    } else {
        /* piliers lointains (crénelés sur les remparts) */
        for (int k = 0; k < 9; k++) {
            float x = k * 160 - 40 + decalX * 0.3f;
            float h = 160 + (k * 37) % 90;
            remplirRect(g, x, 380 - h, 46, h, assombrir(haut, 0.7f));
            remplirRect(g, x - 8, 380 - h, 62, 12, assombrir(haut, 0.6f));
            if (etage == 7) {
                remplirRect(g, x - 8, 380 - h - 14, 14, 14, assombrir(haut, 0.6f));
                remplirRect(g, x + 40, 380 - h - 14, 14, 14, assombrir(haut, 0.6f));
            }
        }
    }
    /* sol en perspective */
    degradeVertical(g, 0, 380, LARGEUR, HAUTEUR - 380, assombrir(bas, 0.55f), assombrir(haut, 0.4f));
    for (int k = -10; k <= 10; k++) {
        ligne(g, LARGEUR / 2.0f + k * 40 + decalX, 380, LARGEUR / 2.0f + k * 190 + decalX, HAUTEUR, 1,
              rgba(255, 255, 255, 18));
    }
    for (int k = 0; k < 7; k++) {
        float y = 380 + powf(k / 7.0f, 1.6f) * (HAUTEUR - 380);
        ligne(g, 0, y + decalY, LARGEUR, y + decalY, 1, rgba(255, 255, 255, 14));
    }
    /* cercles runiques sous les deux camps */
    float pulse = 0.5f + 0.5f * sinf(t * 2);
    ellipse(g, 330 + decalX, 490 + decalY, 260, 50, rgba(120, 180, 255, 25));
    ellipse(g, 890 + decalX, 440 + decalY, 290, 52, rgba(255, 110, 120, (int)(20 + 15 * pulse)));

    /* détails propres à chaque étage, au premier plan */
    switch (etage) {
    case 2: /* champignons lumineux */
        for (int k = 0; k < 6; k++) {
            float cx = 60 + k * 190 + decalX, cy = 560 + (k % 2) * 26 + decalY;
            float lum = 0.6f + 0.4f * sinf(t * 1.5f + k);
            halo(g, cx, cy - 20, 50, rgba(120, 255, 200, (int)(40 * lum)));
            remplirRect(g, cx - 4, cy - 18, 8, 18, rgba(220, 230, 210, 200));
            ellipse(g, cx, cy - 20, 20, 10, rgba(80, 220, 180, (int)(160 + 60 * lum)));
        }
        break;
    case 3: /* flaques de gelée */
        for (int k = 0; k < 5; k++) {
            ellipse(g, 120 + k * 260 + decalX, 560 + (k % 2) * 24 + decalY, 70, 12,
                    rgba(110, 170, 250, (int)(60 + 30 * sinf(t * 2 + k))));
        }
        break;
    case 4: case 6: /* os et pierres tombales */
        for (int k = 0; k < 6; k++) {
            float cx = 80 + k * 230 + decalX, cy = 570 + (k % 2) * 20 + decalY;
            if (etage == 6 && k % 2 == 0) {
                rectArrondi(g, cx - 18, cy - 50, 36, 50, 14, rgba(80, 90, 90, 200));
                ligne(g, cx, cy - 42, cx, cy - 18, 3, rgba(40, 50, 50, 220));
                ligne(g, cx - 8, cy - 34, cx + 8, cy - 34, 3, rgba(40, 50, 50, 220));
            } else {
                ligne(g, cx - 20, cy - 4, cx + 20, cy + 4, 5, rgba(220, 214, 190, 170));
                cercle(g, cx - 20, cy - 4, 5, rgba(220, 214, 190, 170));
                cercle(g, cx + 20, cy + 4, 5, rgba(220, 214, 190, 170));
            }
        }
        if (etage == 6) { /* brume verdâtre */
            for (int k = 0; k < 4; k++) {
                halo(g, fmodf(k * 380 + t * 20, LARGEUR + 300) - 150, 540, 220, rgba(120, 255, 170, 18));
            }
        }
        break;
    case 5: case 8: /* fissures de lave et braises */
        for (int k = 0; k < 5; k++) {
            float cx = 100 + k * 270 + decalX, cy = 550 + (k % 2) * 30 + decalY;
            float lum = 0.6f + 0.4f * sinf(t * 2.2f + k * 1.3f);
            halo(g, cx, cy, 70, rgba(255, 120, 30, (int)(50 * lum)));
            ligne(g, cx - 50, cy, cx - 10, cy - 6, 3, rgba(255, 160, 60, (int)(220 * lum)));
            ligne(g, cx - 10, cy - 6, cx + 30, cy + 4, 3, rgba(255, 160, 60, (int)(220 * lum)));
            ligne(g, cx + 30, cy + 4, cx + 60, cy - 2, 2, rgba(255, 160, 60, (int)(220 * lum)));
        }
        for (int k = 0; k < 30; k++) {
            float ph = fmodf(t * 0.25f + k * 0.0333f, 1.0f);
            float bx = fmodf(k * 173.0f + sinf(t + k) * 20, LARGEUR);
            cercle(g, bx, HAUTEUR - ph * 500, 2, rgba(255, 170, 60, (int)(200 * (1 - ph))));
        }
        break;
    case 9: case 10: /* éclats de Néant qui flottent */
        for (int k = 0; k < 10; k++) {
            float cx = fmodf(k * 137.0f + t * (8 + k % 4 * 4), LARGEUR + 100) - 50;
            float cy = 400 + (k * 53) % 180 + sinf(t + k) * 10;
            float xy[6] = { cx, cy - 10, cx + 6, cy, cx - 6, cy };
            polygone(g, xy, 3, rgba(180, 120, 255, 120));
        }
        break;
    default:
        break;
    }
}

/** Chevron doré au-dessus d'une cible. */
static void chevron(Gui *g, float x, float y, float taille, SDL_Color c)
{
    float b = sinf(g->temps / 160.0f) * 4;
    float xy[6] = { x - taille, y - taille * 1.2f + b, x + taille, y - taille * 1.2f + b, x, y + b };
    polygone(g, xy, 3, c);
}

/** Barres et faiblesses au-dessus d'une cible (monstre, ou héros en duel). */
static void hudCible(Gui *g, CombatGui *cg, int i)
{
    Combattant *c = &cg->cb.c[i];
    float x, y, s;
    position(cg, i, &x, &y, &s);
    float top = y - hauteur(cg, i) - 54;
    float w = c->boss ? 170 : 116;
    int alpha = (int)(255 * cg->apparition[i]);

    char nom[48];
    snprintf(nom, sizeof nom, "Niv %d  %s", c->p->niveau, c->p->nom);
    texteOmbre(g, POLICE_PETITE, nom, x, top - 4, avecAlpha(c->boss ? COUL_OR_CLAIR : COUL_TEXTE, alpha), ALIGN_CENTRE);
    barre(g, x - w / 2, top + 16, w, 9, cg->affPv[i] / c->p->pvMax, avecAlpha(COUL_DANGER, alpha),
          rgba(10, 8, 20, alpha * 4 / 5));
    /* barre de résistance (blanche) */
    float rRatio = c->resistanceMax > 0 ? cg->affRes[i] / c->resistanceMax : 0;
    barre(g, x - w / 2, top + 28, w, 5, rRatio, c->brise ? rgba(120, 120, 130, alpha) : rgba(240, 240, 250, alpha),
          rgba(10, 8, 20, alpha * 4 / 5));
    if (c->brise) {
        texte(g, POLICE_PETITE, "BRISÉ", x + w / 2 + 6, top + 22, avecAlpha(COUL_OR_CLAIR, alpha), ALIGN_GAUCHE);
    }
    if (c->boss && c->phase == 2) {
        texteOmbre(g, POLICE_PETITE, "PHASE 2", x - w / 2 - 6, top + 14, avecAlpha(COUL_DANGER, alpha), ALIGN_DROITE);
    }
    /* faiblesses */
    int n = 0;
    for (int e = 0; e < NB_ELEMENTS; e++) {
        if (c->faiblesses & (1u << e)) n++;
    }
    float ix = x - (n - 1) * 11;
    for (int e = 0; e < NB_ELEMENTS; e++) {
        if (c->faiblesses & (1u << e)) {
            iconeElement(g, (enum Element)e, ix, top + 46, 8);
            ix += 22;
        }
    }
    /* états */
    char etat[64] = "";
    if (c->dot > 0) strcat(etat, "DoT ");
    if (c->gele) strcat(etat, "Gelé ");
    if (c->malusDef) strcat(etat, "DEF- ");
    if (c->buffAtk) strcat(etat, "ATK+ ");
    if (etat[0]) texte(g, POLICE_PETITE, etat, x, top + 58, avecAlpha(rgba(255, 170, 200, 255), alpha), ALIGN_CENTRE);
}

/** Décalage d'un combattant pendant son animation d'action. */
static void decalageAction(const CombatGui *cg, int i, float *dx, float *dy, float *atk)
{
    *dx = *dy = *atk = 0;
    if (cg->phase != PHASE_ACTION || i != cg->acteur) {
        return;
    }
    float t = cg->tPhase;
    float d = dureeAction(cg);
    float aller = t < 0.3f ? t / 0.3f : (t > d - 0.3f ? (d - t) / 0.3f : 1);
    if (aller < 0) aller = 0;
    float lisse = aller * aller * (3 - 2 * aller);
    *atk = lisse;

    int melee = cg->estMonstre || estMelee(cg->cb.c[i].p->classe);
    const Capacite *cap = cg->estMonstre ? NULL : capaciteClasse(cg->cb.c[i].p->classe, cg->actionAnim);
    int surAllie = cap != NULL && (cap->ciblage == CIBLE_ALLIE || cap->ciblage == CIBLE_TOUS_ALLIES);
    if (melee && !surAllie && cg->nbEffets > 0) {
        float ax, ay, s, cx, cy;
        position(cg, i, &ax, &ay, &s);
        position(cg, cg->effets[0].cible, &cx, &cy, &s);
        float sens = cx > ax ? -1 : 1;
        *dx = (cx + sens * 90 - ax) * lisse;
        *dy = (cy - ay) * lisse * 0.6f;
    } else {
        *dx = (combatEstAllie(i) ? 24 : -24) * lisse; /* petit pas en avant */
    }
}

static void dessinerCombattants(Gui *g, CombatGui *cg)
{
    float t = g->temps / 1000.0f;
    /* ordre de dessin : du fond (y petit) vers l'avant */
    int ordre[COMBAT_MAX], n = 0;
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (cg->cb.c[i].present) ordre[n++] = i;
    }
    for (int a = 0; a < n; a++) {
        for (int b = a + 1; b < n; b++) {
            float xa, ya, xb, yb, s;
            position(cg, ordre[a], &xa, &ya, &s);
            position(cg, ordre[b], &xb, &yb, &s);
            if (yb < ya || (cg->phase == PHASE_ACTION && ordre[a] == cg->acteur)) {
                int tmp = ordre[a];
                ordre[a] = ordre[b];
                ordre[b] = tmp;
            }
        }
    }

    for (int k = 0; k < n; k++) {
        int i = ordre[k];
        Combattant *c = &cg->cb.c[i];
        float x, y, s, dx, dy, atk;
        position(cg, i, &x, &y, &s);
        decalageAction(cg, i, &dx, &dy, &atk);

        Pose pose = poseDefaut(t + i * 0.37f);
        pose.flash = cg->flash[i];
        pose.attaque = atk;
        pose.alpha = cg->apparition[i];
        int vivant = cg->affPv[i] > 0.5f;

        if (c->heros) {
            pose.sens = combatEstAllie(i) ? 1 : -1;
            if (!vivant) pose.alpha *= 0.35f;
            if (i == cg->cb.actif && cg->phase == PHASE_ATTENTE) {
                halo(g, x, y - 5, 70, rgba(255, 220, 140, 60));
                anneauEllipse(g, x, y, 48, 13, 2, rgba(255, 220, 140, 150), 0, 0);
            }
            if (c->bouclier > 0 && vivant) {
                halo(g, x + dx, y - 85 + dy, 95, rgba(120, 200, 255, 40));
            }
            if (c->gele && vivant) {
                halo(g, x, y - hauteur(cg, i) / 2, 80, rgba(120, 210, 255, 90));
            }
            dessinerHeros(g, c->p, x + dx, y + dy, s, &pose);
            if (vivant && c->resistanceMax > 0) hudCible(g, cg, i);
        } else {
            pose.sens = -1;
            if (!vivant) {
                /* le monstre vaincu s'efface */
                cg->apparition[i] -= g->dt * 1.5f;
                if (cg->apparition[i] < 0) cg->apparition[i] = 0;
                pose.alpha = cg->apparition[i];
                pose.flash = 1;
            } else if (cg->apparition[i] < 1) {
                cg->apparition[i] += g->dt * 2;
                if (cg->apparition[i] > 1) cg->apparition[i] = 1;
            }
            if (pose.alpha <= 0.01f) continue;
            if (c->brise && vivant) {
                halo(g, x, y - hauteur(cg, i) / 2, 90, avecAlpha(rgba(255, 255, 255, 255), 30));
            }
            if (c->gele && vivant) {
                halo(g, x, y - hauteur(cg, i) / 2, 80, rgba(120, 210, 255, 90));
            }
            dessinerMonstre(g, c->monstre, x + dx, y + dy, s, &pose);
            if (vivant) hudCible(g, cg, i);
        }
    }

    /* projectiles des attaques à distance */
    if (cg->phase == PHASE_ACTION && !cg->estMonstre && cg->nbEffets > 0) {
        enum Classe cl = cg->cb.c[cg->acteur].p->classe;
        const Capacite *cap = capaciteClasse(cl, cg->actionAnim);
        if (!estMelee(cl) && cap->ciblage != CIBLE_ALLIE && cap->ciblage != CIBLE_TOUS_ALLIES) {
            float p = (cg->tPhase - 0.12f) / (instantCoup(0) - 0.12f);
            if (p > 0 && p < 1) {
                float ax, ay, cx, cy;
                centre(cg, cg->acteur, &ax, &ay);
                SDL_Color ce = couleurElement(cg->cb.c[cg->acteur].p->element);
                for (int k = 0; k < cg->nbEffets; k++) {
                    if (cg->effets[k].type != EFFET_DEGATS
                        || combatCamp(cg->effets[k].cible) == combatCamp(cg->acteur)) continue;
                    centre(cg, cg->effets[k].cible, &cx, &cy);
                    float px = ax + (cx - ax) * p, py = ay - 30 + (cy - ay + 30) * p - sinf(p * PI_F) * 60;
                    halo(g, px, py, 30, avecAlpha(ce, 160));
                    cercle(g, px, py, 7, rgba(255, 255, 255, 255));
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Interface (HUD)                                                   */
/* ------------------------------------------------------------------ */

/** Ligne de temps des prochains tours, à gauche. */
static void dessinerLigneTemps(Gui *g, CombatGui *cg)
{
    int ordre[8];
    double avs[8];
    int n = combatOrdre(&cg->cb, ordre, avs, 7);
    float t = g->temps / 1000.0f;
    float y = 96;
    for (int k = 0; k < n; k++) {
        int i = ordre[k];
        int allie = combatEstAllie(i);
        float h = k == 0 ? 58 : 46, w = k == 0 ? 92 : 80;
        SDL_Color teinte = allie ? rgba(70, 120, 220, 230) : rgba(190, 60, 80, 230);
        if (cg->cb.c[i].heros && cg->cb.mode == MODE_DUEL) {
            teinte = avecAlpha(allie ? COUL_J1 : COUL_J2, 230);
        }
        rectArrondi(g, 14, y, w, h, 10, assombrir(teinte, 0.45f));
        degradeHorizontal(g, 14, y, w * 0.6f, h, avecAlpha(teinte, 160), avecAlpha(teinte, 0));
        if (k == 0) contourArrondi(g, 14, y, w, h, 10, 2, COUL_OR_CLAIR);
        float r = h / 2 - 6;
        if (cg->cb.c[i].heros) {
            portraitHeros(g, cg->cb.c[i].p, 20 + r, y + h / 2, r, t);
        } else {
            portraitMonstre(g, cg->cb.c[i].monstre, 20 + r, y + h / 2, r, t);
        }
        char av[16];
        snprintf(av, sizeof av, "%d", (int)avs[k]);
        texte(g, POLICE_PETITE, k == 0 ? "TOUR" : av, 14 + w - 6, y + h / 2 - 8,
              k == 0 ? COUL_OR_CLAIR : COUL_TEXTE, ALIGN_DROITE);
        y += h + 6;
    }
}

/** Cartes des héros d'un camp (PV, énergie, ultime) : en bas à gauche pour
 *  le joueur 1, en haut à droite pour le joueur 2 en duel. */
static void dessinerEquipe(Gui *g, CombatGui *cg, int camp)
{
    float t = g->temps / 1000.0f;
    int debut = camp == 0 ? 0 : PREMIER_ENNEMI;
    for (int i = debut; i < debut + COMBAT_MAX_ALLIES; i++) {
        Combattant *c = &cg->cb.c[i];
        if (!c->present || !c->heros) continue;
        int k = i - debut;
        int actif = (i == cg->cb.actif && cg->cb.etat == COMBAT_TOUR_JOUEUR);
        float w = 158, h = 100;
        float x = camp == 0 ? 20 + k * 168 : LARGEUR - 20 - w - k * 168;
        float y = camp == 0 ? (actif ? 598 : 612) : (actif ? 70 : 56);
        int vivant = estVivant(c->p);

        rectArrondi(g, x, y, w, h, 12, rgba(14, 12, 30, 225));
        degradeVertical(g, x, y, w, h / 2, avecAlpha(couleurElement(c->p->element), 40), rgba(0, 0, 0, 0));
        SDL_Color bord = cg->cb.mode == MODE_DUEL ? avecAlpha(camp == 0 ? COUL_J1 : COUL_J2, 150) : COUL_BORDURE;
        contourArrondi(g, x, y, w, h, 12, actif ? 2.5f : 1.2f, actif ? COUL_OR_CLAIR : bord);

        portraitHeros(g, c->p, x + 32, y + 36, 24, t);
        if (!vivant) {
            rectArrondi(g, x + 8, y + 12, 48, 48, 7, rgba(0, 0, 0, 150));
        }
        texte(g, POLICE_PETITE, c->p->nom, x + 64, y + 10, vivant ? COUL_TEXTE : COUL_TEXTE_DOUX, ALIGN_GAUCHE);

        /* ultime : jauge circulaire */
        float ux = x + w - 26, uy = y + 46;
        float ratio = (float)c->energie / c->energieMax;
        int pret = vivant && c->energie >= c->energieMax;
        SDL_Color ce = couleurElement(c->p->element);
        cercle(g, ux, uy, 19, rgba(20, 18, 40, 255));
        arc(g, ux, uy, 19, 4, -PI_F / 2, -PI_F / 2 + 2 * PI_F * ratio, pret ? COUL_OR_CLAIR : ce);
        if (pret) {
            halo(g, ux, uy, 34 + sinf(t * 6) * 4, avecAlpha(COUL_OR_CLAIR, 90));
            cercle(g, ux, uy, 14, avecAlpha(ce, 230));
        }
        char num[12];
        snprintf(num, sizeof num, "%d", k + 1);
        texte(g, POLICE_NORMALE, num, ux, uy - 10, pret ? rgba(30, 20, 10, 255) : COUL_TEXTE_DOUX, ALIGN_CENTRE);
        if (pret && cg->phase == PHASE_ATTENTE && zoneClic(g, ux - 20, uy - 20, 40, 40)) {
            choisir(cg, i, ACTION_ULTIME);
        }

        /* PV et bouclier */
        float pr = cg->affPv[i] / c->p->pvMax;
        barre(g, x + 64, y + 32, w - 100, 8, pr, pr > 0.3f ? COUL_VIE : COUL_DANGER, rgba(10, 8, 20, 220));
        if (c->bouclier > 0) {
            float br = (float)c->bouclier / c->p->pvMax;
            if (br > 1) br = 1;
            barre(g, x + 64, y + 42, (w - 100) * br, 4, 1, COUL_ACCENT, rgba(0, 0, 0, 0));
        }
        char pv[24];
        snprintf(pv, sizeof pv, "%d/%d", c->p->pv, c->p->pvMax);
        texte(g, POLICE_PETITE, pv, x + 64, y + 48, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

        char etat[48] = "";
        if (c->buffAtk) {
            char b[16];
            snprintf(b, sizeof b, "ATK+%d%% ", c->buffAtk);
            strcat(etat, b);
        }
        if (c->dot > 0) strcat(etat, "DoT");
        if (!vivant) snprintf(etat, sizeof etat, "K.O.");
        texte(g, POLICE_PETITE, etat, x + 12, y + 74, vivant ? rgba(255, 200, 120, 255) : COUL_DANGER, ALIGN_GAUCHE);
    }
}

/** Bouton rond d'action (attaque de base / compétence). */
static int boutonAction(Gui *g, float x, float y, float r, const char *touche, int selectionne,
                        int possible, SDL_Color c)
{
    float t = g->temps / 1000.0f;
    int dessus = possible && survol(g, x - r, y - r, 2 * r, 2 * r);
    if (selectionne) {
        halo(g, x, y, r * 1.8f + sinf(t * 5) * 3, avecAlpha(c, 90));
    }
    cercle(g, x, y, r + 3, selectionne ? COUL_OR_CLAIR : rgba(120, 110, 170, 160));
    cercle(g, x, y, r, possible ? (dessus ? melanger(c, rgba(255, 255, 255, 255), 0.2f) : c) : rgba(60, 58, 74, 255));
    cercle(g, x - r * 0.25f, y - r * 0.3f, r * 0.45f, rgba(255, 255, 255, 30));
    texteOmbre(g, POLICE_GRANDE, touche, x, y - 18, possible ? COUL_TEXTE : COUL_TEXTE_DOUX, ALIGN_CENTRE);
    if (dessus) g->survolCliquable = 1;
    return dessus && g->clic;
}

static void dessinerActions(Gui *g, CombatGui *cg)
{
    Combat *cb = &cg->cb;
    if (cb->etat != COMBAT_TOUR_JOUEUR || cg->phase != PHASE_ATTENTE) {
        return;
    }
    int l = cg->lanceur;
    enum Classe cl = cb->c[l].p->classe;
    SDL_Color cc = couleurClasse(cl);

    /* points de compétence (une réserve par camp) */
    int camp = combatCamp(l);
    int sp = cb->sp[camp];
    if (cb->mode == MODE_DUEL) {
        texte(g, POLICE_PETITE, camp == 0 ? "Points de compétence - Joueur 1" : "Points de compétence - Joueur 2",
              1240, 520, camp == 0 ? COUL_J1 : COUL_J2, ALIGN_DROITE);
    } else {
        texte(g, POLICE_PETITE, "Points de compétence", 1240, 520, COUL_TEXTE_DOUX, ALIGN_DROITE);
    }
    for (int k = 0; k < SP_MAX; k++) {
        float x = 1236 - (SP_MAX - 1 - k) * 26, y = 552;
        losange(g, x, y, 10, 12, rgba(30, 26, 50, 255));
        if (k < sp) {
            halo(g, x, y, 16, rgba(255, 220, 140, 80));
            losange(g, x, y, 8, 10, COUL_OR_CLAIR);
        }
    }

    int ult = cg->choix == ACTION_ULTIME;
    if (!ult) {
        if (boutonAction(g, 1078, 650, 36, "Q", cg->choix == ACTION_BASIQUE,
                         combatPeutAgir(cb, l, ACTION_BASIQUE), assombrir(cc, 0.6f))) {
            choisir(cg, l, ACTION_BASIQUE);
        }
        if (boutonAction(g, 1186, 630, 46, "E", cg->choix == ACTION_COMPETENCE,
                         combatPeutAgir(cb, l, ACTION_COMPETENCE), assombrir(cc, 0.8f))) {
            choisir(cg, l, ACTION_COMPETENCE);
        }
        texte(g, POLICE_PETITE, "Basique", 1078, 690, COUL_TEXTE_DOUX, ALIGN_CENTRE);
        char libComp[32];
        if (cb->c[l].recharge > 0) {
            snprintf(libComp, sizeof libComp, "Recharge : %d", cb->c[l].recharge);
        } else {
            snprintf(libComp, sizeof libComp, sp > 0 ? "Compétence" : "0 point !");
        }
        texte(g, POLICE_PETITE, libComp, 1186, 682,
              sp > 0 && cb->c[l].recharge == 0 ? COUL_TEXTE_DOUX : COUL_DANGER, ALIGN_CENTRE);
    }

    /* description de l'action choisie (nom et effet selon l'élément) */
    const Personnage *pl = cb->c[l].p;
    char nomCap[64], descCap[256];
    nomCapacite(pl, cg->choix, nomCap, sizeof nomCap);
    descriptionCapacite(pl, cg->choix, descCap, sizeof descCap);
    static const char *const TYPES[NB_ACTIONS] = { "Attaque de base", "Compétence", "ULTIME" };
    float pw = ult ? 550 : 330, ph = 124;
    float px = ult ? 700 : 1030 - pw, py = 584;
    rectArrondi(g, px, py, pw, ph, 12, rgba(14, 12, 30, 220));
    contourArrondi(g, px, py, pw, ph, 12, 1.2f, ult ? COUL_OR_CLAIR : COUL_BORDURE);
    char titre[96];
    snprintf(titre, sizeof titre, "%s - %s", pl->nom, TYPES[cg->choix]);
    texte(g, POLICE_PETITE, titre, px + 14, py + 8, cc, ALIGN_GAUCHE);
    iconeElement(g, pl->element, px + pw - 20, py + 16, 8);
    texte(g, POLICE_MOYENNE, nomCap, px + 14, py + 24, COUL_TEXTE, ALIGN_GAUCHE);
    texteBloc(g, POLICE_PETITE, descCap, px + 14, py + 52, (int)pw - 28, COUL_TEXTE_DOUX);

    /* efficacité contre la cible visée */
    if (cg->cible >= 0 && capaciteOffensive(pl->classe, cg->choix) && combatVivant(cb, cg->cible)) {
        int m = multiplicateurElement(pl->element, cb->c[cg->cible].p->element);
        if (m != 100) {
            char eff[96];
            snprintf(eff, sizeof eff, "%s contre %s : %s (%+d %%)", nomElement(pl->element),
                     nomElement(cb->c[cg->cible].p->element), m > 100 ? "efficace" : "peu efficace", m - 100);
            texte(g, POLICE_PETITE, eff, px + pw - 14, py + ph - 20, m > 100 ? COUL_VIE : COUL_DANGER,
                  ALIGN_DROITE);
        }
    }
    if (ult) {
        texte(g, POLICE_PETITE, "Espace : lancer  ·  Échap : annuler", px + pw - 14, py + 10,
              COUL_OR_CLAIR, ALIGN_DROITE);
    }
}

/** Marqueurs de cible et clic sur les combattants. */
static void gererCibles(Gui *g, CombatGui *cg)
{
    if (cg->phase != PHASE_ATTENTE) {
        return;
    }
    Combat *cb = &cg->cb;
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (!cb->c[i].present) continue;
        float x, y, s;
        position(cg, i, &x, &y, &s);
        float h = hauteur(cg, i);
        int valide = combatCibleValide(cb, cg->lanceur, cg->choix, i);
        if (valide && zoneClic(g, x - 50, y - h, 100, h + 10)) {
            if (cg->cible == i) {
                lancer(cg);
                return;
            }
            cg->cible = i;
        }
        if (cg->cible >= 0 && combatEstTouche(cb, cg->lanceur, cg->choix, cg->cible, i)) {
            int principal = (i == cg->cible);
            int aUnHud = !cb->c[i].heros || cb->c[i].resistanceMax > 0;
            float top = aUnHud ? y - h - 66 : y - h - 18;
            chevron(g, x, top, principal ? 12 : 8, principal ? COUL_OR_CLAIR : avecAlpha(COUL_OR, 180));
            float rx = cb->c[i].heros ? 46 : 60 * s * (cb->c[i].boss ? 1.6f : 1);
            anneauEllipse(g, x, y, rx, rx * 0.25f, 2, principal ? COUL_OR_CLAIR : avecAlpha(COUL_OR, 140),
                          1, g->temps / 900.0f);
        }
    }
}

/** Bannière centrale (vague, cinématique d'ultime). */
static void dessinerBanniere(Gui *g, CombatGui *cg)
{
    if (cg->banniere <= 0) {
        return;
    }
    cg->banniere -= g->dt;
    float a = cg->banniere > 1.5f ? (1.8f - cg->banniere) / 0.3f : (cg->banniere < 0.4f ? cg->banniere / 0.4f : 1);
    int alpha = (int)(255 * a);
    degradeHorizontal(g, 0, 250, LARGEUR / 2.0f, 80, rgba(0, 0, 0, 0), rgba(10, 6, 24, alpha * 3 / 4));
    degradeHorizontal(g, LARGEUR / 2.0f, 250, LARGEUR / 2.0f, 80, rgba(10, 6, 24, alpha * 3 / 4), rgba(0, 0, 0, 0));
    ligne(g, 300, 252, 980, 252, 1.5f, avecAlpha(COUL_OR, alpha));
    ligne(g, 300, 328, 980, 328, 1.5f, avecAlpha(COUL_OR, alpha));
    texteOmbre(g, POLICE_TITRE, cg->texteBanniere, LARGEUR / 2.0f, 262, avecAlpha(COUL_OR_CLAIR, alpha), ALIGN_CENTRE);
}

/** Cinématique d'ultime : bande colorée, héros en grand, nom de l'ultime. */
static void dessinerCinematique(Gui *g, CombatGui *cg)
{
    float t = cg->tPhase;
    const float duree = 1.15f;
    Personnage *p = cg->cb.c[cg->acteur].p;
    SDL_Color cc = couleurClasse(p->classe);
    SDL_Color ce = couleurElement(p->element);
    float entree = t < 0.25f ? t / 0.25f : 1;
    float sortie = t > duree - 0.2f ? (duree - t) / 0.2f : 1;
    float a = entree < sortie ? entree : sortie;
    if (a < 0) a = 0;

    remplirRect(g, 0, 0, LARGEUR, HAUTEUR, rgba(0, 0, 0, (int)(150 * a)));
    float glisse = (1 - entree) * -LARGEUR;
    float bande[8] = { glisse, 200, glisse + LARGEUR + 200, 120, glisse + LARGEUR + 200, 480, glisse, 560 };
    polygone(g, bande, 4, avecAlpha(assombrir(cc, 0.55f), (int)(230 * a)));
    for (int k = 0; k < 12; k++) {
        float y = 160 + k * 30;
        float x = fmodf(t * 1800 + k * 237, LARGEUR + 400) - 200;
        ligne(g, x, y, x + 160, y - 18, 2, avecAlpha(ce, (int)(120 * a)));
    }
    halo(g, 380 + glisse * 0.5f, 360, 300, avecAlpha(ce, (int)(110 * a)));
    Pose pose = poseDefaut(g->temps / 1000.0f);
    pose.attaque = t > 0.5f ? 1 : t / 0.5f;
    pose.alpha = a;
    dessinerHeros(g, p, 380 + glisse * 0.5f + t * 40, 640, 2.9f, &pose);

    char nomUlt[64];
    nomCapacite(p, ACTION_ULTIME, nomUlt, sizeof nomUlt);
    float tx = 700 - (1 - entree) * 200;
    texte(g, POLICE_MOYENNE, p->nom, tx, 240, avecAlpha(COUL_TEXTE, (int)(255 * a)), ALIGN_GAUCHE);
    texteOmbre(g, POLICE_TITRE, nomUlt, tx, 272, avecAlpha(COUL_OR_CLAIR, (int)(255 * a)), ALIGN_GAUCHE);
    texte(g, POLICE_NORMALE, "ULTIME", tx, 334, avecAlpha(ce, (int)(255 * a)), ALIGN_GAUCHE);
}

/** Fin d'un duel : l'équipe gagnante fait la fête. */
static void dessinerResultatDuel(App *app, CombatGui *cg)
{
    Gui *g = &app->g;
    Combat *cb = &cg->cb;
    int gagnant = cb->etat == COMBAT_VICTOIRE ? 0 : 1;
    float a = cg->tPhase < 0.6f ? cg->tPhase / 0.6f : 1;
    float t = g->temps / 1000.0f;
    SDL_Color c = gagnant == 0 ? COUL_J1 : COUL_J2;

    remplirRect(g, 0, 0, LARGEUR, HAUTEUR, rgba(4, 2, 12, (int)(228 * a)));
    float y0 = 90 + (1 - a) * 30;
    halo(g, LARGEUR / 2.0f, y0 + 50, 340, avecAlpha(c, (int)(70 * a)));
    texteOmbre(g, POLICE_ENORME, gagnant == 0 ? "JOUEUR 1 GAGNE" : "JOUEUR 2 GAGNE", LARGEUR / 2.0f, y0,
               avecAlpha(c, (int)(255 * a)), ALIGN_CENTRE);
    char buf[96];
    snprintf(buf, sizeof buf, "Duel remporté en %d actions", cb->tour);
    texte(g, POLICE_MOYENNE, buf, LARGEUR / 2.0f, y0 + 110, avecAlpha(COUL_TEXTE, (int)(255 * a)), ALIGN_CENTRE);

    int debut = gagnant == 0 ? 0 : PREMIER_ENNEMI;
    int n = 0;
    for (int i = debut; i < debut + COMBAT_MAX_ALLIES; i++) if (cb->c[i].present) n++;
    for (int k = 0; k < n; k++) {
        Personnage *p = cb->c[debut + k].p;
        float x = LARGEUR / 2.0f + (k - (n - 1) / 2.0f) * 200, y = 450 - fabsf(sinf(t * 5 + k)) * 18;
        Pose pose = poseDefaut(t + k);
        pose.alpha = a * (estVivant(p) ? 1 : 0.4f);
        pose.attaque = 0.5f + 0.5f * sinf(t * 5 + k); /* ils brandissent leur arme */
        dessinerHeros(g, p, x, y, 1.1f, &pose);
        texte(g, POLICE_NORMALE, p->nom, x, 470, COUL_TEXTE, ALIGN_CENTRE);
    }

    if (cg->tPhase > 0.6f) {
        if (bouton(g, LARGEUR / 2.0f - 230, 620, 220, 50, "Changer les équipes", BOUTON_NORMAL, 1)) {
            changerEcran(app, ECRAN_EQUIPE);
        }
        if (bouton(g, LARGEUR / 2.0f + 10, 620, 220, 50, "Revanche !", BOUTON_PRINCIPAL, 1)
            || guiTouche(g, SDLK_RETURN)) {
            cg->revanche = 1;
        }
    }
}

/** Écran de fin (victoire / défaite). */
static void dessinerResultat(App *app, CombatGui *cg)
{
    Gui *g = &app->g;
    Combat *cb = &cg->cb;
    int victoire = cb->etat == COMBAT_VICTOIRE;
    float a = cg->tPhase < 0.6f ? cg->tPhase / 0.6f : 1;

    if (cb->mode == MODE_DUEL) {
        dessinerResultatDuel(app, cg);
        return;
    }

    if (!cg->recompensesVues) {
        cg->recompensesVues = 1;
        if (victoire) {
            cg->foieGrasGagne = recompenseFoieGras(&app->prog, cg->etage,
                                                   combatEtage(cg->etage)->boss != NB_MONSTRES);
        }
        if (victoire && cg->etage == app->prog.etageMax && app->prog.etageMax < ETAGE_MAX) {
            app->prog.etageMax++;
            app->prep.etage = app->prog.etageMax;
        }
    }

    remplirRect(g, 0, 0, LARGEUR, HAUTEUR, rgba(4, 2, 12, (int)(228 * a)));
    float y0 = 90 + (1 - a) * 30;
    SDL_Color c = victoire ? COUL_OR_CLAIR : COUL_DANGER;
    halo(g, LARGEUR / 2.0f, y0 + 50, 320, avecAlpha(c, (int)(60 * a)));
    texteOmbre(g, POLICE_ENORME, victoire ? "VICTOIRE" : "DÉFAITE", LARGEUR / 2.0f, y0, avecAlpha(c, (int)(255 * a)),
               ALIGN_CENTRE);

    char buf[128];
    if (victoire) {
        if (cg->etage == ETAGE_MAX) {
            snprintf(buf, sizeof buf, "Le Canard Noir est vaincu, le donjon est libéré !  ·  +%d XP", cb->xpGagnee);
        } else {
            snprintf(buf, sizeof buf, "%s nettoyé  ·  +%d XP pour chaque survivant",
                     combatEtage(cg->etage)->nom, cb->xpGagnee);
        }
    } else {
        snprintf(buf, sizeof buf, "L'équipe est tombée à la vague %d/%d. Reposez-vous au camp.", cb->vague, cb->nbVagues);
    }
    texte(g, POLICE_MOYENNE, buf, LARGEUR / 2.0f, y0 + 110, avecAlpha(COUL_TEXTE, (int)(255 * a)), ALIGN_CENTRE);

    /* héros et progression */
    float t = g->temps / 1000.0f;
    int n = 0;
    for (int i = 0; i < COMBAT_MAX_ALLIES; i++) if (cb->c[i].present) n++;
    for (int i = 0; i < n; i++) {
        Personnage *p = cb->c[i].p;
        float x = LARGEUR / 2.0f + (i - (n - 1) / 2.0f) * 200, y = 420;
        Pose pose = poseDefaut(t + i);
        pose.alpha = a * (estVivant(p) ? 1 : 0.4f);
        dessinerHeros(g, p, x, y, 1.0f, &pose);
        texte(g, POLICE_NORMALE, p->nom, x, y + 10, COUL_TEXTE, ALIGN_CENTRE);
        snprintf(buf, sizeof buf, "Niv %d", p->niveau);
        texte(g, POLICE_PETITE, buf, x, y + 32, COUL_TEXTE_DOUX, ALIGN_CENTRE);
        if (p->niveau > cg->niveauAvant[i]) {
            snprintf(buf, sizeof buf, "NIVEAU +%d !", p->niveau - cg->niveauAvant[i]);
            texteOmbre(g, POLICE_NORMALE, buf, x, y - 200 + sinf(t * 3) * 3, COUL_OR_CLAIR, ALIGN_CENTRE);
        }
        barre(g, x - 60, y + 52, 120, 6, (float)p->xp / xpPourNiveauSuivant(p), COUL_OR, rgba(10, 8, 20, 200));
    }

    /* points de stats et foie gras */
    if (victoire) {
        snprintf(buf, sizeof buf, "+%d point%s de stats par survivant", cb->pointsGagnes,
                 cb->pointsGagnes > 1 ? "s" : "");
        texte(g, POLICE_NORMALE, buf, LARGEUR / 2.0f - 20, 488, COUL_VIE, ALIGN_DROITE);
        snprintf(buf, sizeof buf, "+%d foie gras", cg->foieGrasGagne);
        iconeFoieGras(g, LARGEUR / 2.0f + 30, 499, 10);
        texte(g, POLICE_NORMALE, buf, LARGEUR / 2.0f + 46, 488, COUL_OR_CLAIR, ALIGN_GAUCHE);
    }

    /* butin */
    float by = 522;
    for (int k = 0; k < cb->nbButins; k++) {
        const Objet *o = &cb->butins[k];
        snprintf(buf, sizeof buf, "Butin : %s (%s%d) → %s", o->nom, o->type == SOIN ? "soin +" : "ATK +",
                 o->valeur, cb->receveurs[k]);
        texte(g, POLICE_NORMALE, buf, LARGEUR / 2.0f, by + k * 24, COUL_OR_CLAIR, ALIGN_CENTRE);
    }

    if (cg->tPhase > 0.6f) {
        if (bouton(g, LARGEUR / 2.0f - 230, 620, 220, 50, "Mes héros", BOUTON_NORMAL, 1)) {
            changerEcran(app, ECRAN_HEROS);
        }
        if (bouton(g, LARGEUR / 2.0f + 10, 620, 220, 50, victoire ? "Étage suivant" : "Réessayer",
                   BOUTON_PRINCIPAL, 1) || guiTouche(g, SDLK_RETURN)) {
            changerEcran(app, ECRAN_EQUIPE);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Image par image                                                   */
/* ------------------------------------------------------------------ */

/** Fait avancer les animations et la logique selon la phase. */
static void majPhase(App *app, CombatGui *cg)
{
    Gui *g = &app->g;
    Combat *cb = &cg->cb;
    cg->tPhase += g->dt;

    switch (cg->phase) {
    case PHASE_ATTENTE: {
        if (guiTouche(g, SDLK_q) || guiTouche(g, SDLK_a)) choisir(cg, cb->actif, ACTION_BASIQUE);
        if (guiTouche(g, SDLK_e)) choisir(cg, cb->actif, ACTION_COMPETENCE);
        /* 1..4 : ultimes de l'équipe qui joue */
        int base = combatCamp(cb->actif) == 0 ? 0 : PREMIER_ENNEMI;
        for (int k = 0; k < COMBAT_MAX_ALLIES; k++) {
            if (guiTouche(g, SDLK_1 + k) || guiTouche(g, SDLK_KP_1 + k)) choisir(cg, base + k, ACTION_ULTIME);
        }
        if (cg->phase != PHASE_ATTENTE) break; /* l'action a été lancée */
        if (guiTouche(g, SDLK_LEFT) || guiTouche(g, SDLK_UP)) changerCible(cg, -1);
        if (guiTouche(g, SDLK_RIGHT) || guiTouche(g, SDLK_DOWN) || guiTouche(g, SDLK_d)) changerCible(cg, 1);
        if (guiTouche(g, SDLK_ESCAPE) && cg->choix == ACTION_ULTIME) {
            cg->lanceur = cb->actif;
            choisir(cg, cb->actif, ACTION_BASIQUE);
        }
        if (guiTouche(g, SDLK_SPACE) || guiTouche(g, SDLK_RETURN)) lancer(cg);
        break;
    }
    case PHASE_CINEMATIQUE:
        if (cg->tPhase >= 1.15f) allerPhase(cg, PHASE_ACTION);
        break;
    case PHASE_ACTION:
        while (cg->coupsJoues < cg->nbCoups && cg->tPhase >= instantCoup(cg->coupsJoues)) {
            jouerCoup(cg, cg->coupsJoues);
            cg->coupsJoues++;
        }
        if (cg->tPhase >= dureeAction(cg)) suite(cg);
        break;
    case PHASE_ENNEMI:
        if (cg->tPhase >= 0.55f) {
            int acteur = cb->actif;
            combatTourEnnemi(cb);
            copierEffets(cg, acteur, ACTION_BASIQUE, 1);
            allerPhase(cg, PHASE_ACTION);
        }
        break;
    case PHASE_DEBUT_TOUR:
        if (cg->tPhase >= 0.8f) {
            cg->nbEffets = 0;
            if (cb->etat == COMBAT_TOUR_JOUEUR) preparerTourJoueur(cg);
            else if (cb->etat == COMBAT_TOUR_ENNEMI) allerPhase(cg, PHASE_ENNEMI);
            else suite(cg);
        }
        break;
    case PHASE_FIN_VAGUE:
        if (cg->tPhase >= 1.3f) {
            combatVagueSuivante(cb);
            for (int i = PREMIER_ENNEMI; i < COMBAT_MAX; i++) {
                if (cb->c[i].present) {
                    cg->affPv[i] = (float)cb->c[i].p->pv;
                    cg->affRes[i] = (float)cb->c[i].resistance;
                    cg->apparition[i] = 0;
                }
            }
            char titre[48];
            snprintf(titre, sizeof titre, "Vague %d/%d", cb->vague, cb->nbVagues);
            banniere(cg, titre);
            cg->nbEffets = 0;
            if (cb->nbEffets > 0) {
                copierEffets(cg, cb->actif, ACTION_BASIQUE, 0);
                jouerCoup(cg, 0);
                allerPhase(cg, PHASE_DEBUT_TOUR);
            } else if (cb->etat == COMBAT_TOUR_JOUEUR) {
                cg->cible = -1;
                preparerTourJoueur(cg);
            } else {
                suite(cg);
            }
        }
        break;
    case PHASE_RESULTAT:
        break;
    }

    /* l'affichage des barres rattrape l'état réel (après l'impact) */
    int figer = (cg->phase == PHASE_ACTION && cg->coupsJoues == 0) || cg->phase == PHASE_CINEMATIQUE;
    float k = 1 - expf(-g->dt * 7);
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (!cb->c[i].present) continue;
        if (!figer) {
            cg->affPv[i] += (cb->c[i].p->pv - cg->affPv[i]) * k;
            if (fabsf(cg->affPv[i] - cb->c[i].p->pv) < 0.5f) cg->affPv[i] = (float)cb->c[i].p->pv;
            cg->affRes[i] += (cb->c[i].resistance - cg->affRes[i]) * k;
        }
        cg->flash[i] -= g->dt * 4;
        if (cg->flash[i] < 0) cg->flash[i] = 0;
    }
    cg->secousse *= expf(-g->dt * 9);
    majFx(cg, g->dt);
}

void ecranCombat(App *app)
{
    Gui *g = &app->g;
    CombatGui *cg = app->combat;
    if (cg == NULL) {
        changerEcran(app, ECRAN_EQUIPE);
        return;
    }
    Combat *cb = &cg->cb;

    majPhase(app, cg);

    decalX = (cg->secousse > 0.2f) ? sinf(g->temps * 0.09f) * cg->secousse : 0;
    decalY = (cg->secousse > 0.2f) ? cosf(g->temps * 0.11f) * cg->secousse * 0.6f : 0;

    dessinerDecor(g, cg->etage);
    gererCibles(g, cg);
    dessinerCombattants(g, cg);
    dessinerFx(g, cg);
    decalX = decalY = 0;

    /* en-tête : étage ou duel, journal */
    int duel = cb->mode == MODE_DUEL;
    char titre[64];
    if (duel) {
        int j = combatCamp(cb->actif);
        texteOmbre(g, POLICE_MOYENNE, "DUEL", 20, 18, COUL_OR_CLAIR, ALIGN_GAUCHE);
        texteOmbre(g, POLICE_MOYENNE, j == 0 ? "Joueur 1" : "Joueur 2", 100, 18, j == 0 ? COUL_J1 : COUL_J2,
                   ALIGN_GAUCHE);
        if (combatFatigue(cb) > 0) {
            snprintf(titre, sizeof titre, "Tour %d  ·  Fatigue : dégâts +%d %%", cb->tour + 1, combatFatigue(cb));
        } else {
            snprintf(titre, sizeof titre, "Tour %d", cb->tour + 1);
        }
    } else {
        snprintf(titre, sizeof titre, "ÉTAGE %d", cg->etage);
        texteOmbre(g, POLICE_MOYENNE, titre, 20, 18, COUL_OR_CLAIR, ALIGN_GAUCHE);
        snprintf(titre, sizeof titre, "%s  ·  Vague %d/%d  ·  Tour %d", combatEtage(cg->etage)->nom,
                 cb->vague, cb->nbVagues, cb->tour + 1);
    }
    texte(g, POLICE_PETITE, titre, 20, 50, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    for (int k = 0; k < cb->nbJournal && k < (duel ? 2 : 4); k++) {
        int idx = cb->nbJournal - 1 - k;
        texteOmbre(g, POLICE_PETITE, cb->journal[idx], LARGEUR / 2.0f, 16 + k * 18,
                   avecAlpha(COUL_TEXTE, 255 - k * 55), ALIGN_CENTRE);
    }

    dessinerLigneTemps(g, cg);
    dessinerEquipe(g, cg, 0);
    if (duel) {
        dessinerEquipe(g, cg, 1);
    }
    dessinerActions(g, cg);

    if (cg->phase == PHASE_ATTENTE) {
        const char *aide = "Q basique · E compétence · 1-4 ultimes · ←/→ cible · Espace confirmer";
        if (duel) {
            texte(g, POLICE_PETITE, aide, 20, 72, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
        } else {
            texte(g, POLICE_PETITE, aide, LARGEUR - 20, 50, COUL_TEXTE_DOUX, ALIGN_DROITE);
        }
    }
    if (cg->phase != PHASE_RESULTAT
        && bouton(g, LARGEUR - 120, 12, 100, 32, duel ? "Abandon" : "Fuir", BOUTON_DISCRET, 1)) {
        changerEcran(app, ECRAN_EQUIPE);
    }

    if (cg->phase == PHASE_CINEMATIQUE) {
        dessinerCinematique(g, cg);
    }
    dessinerBanniere(g, cg);
    if (cg->phase == PHASE_RESULTAT) {
        dessinerResultat(app, cg);
        if (cg->revanche) {
            /* même duel, mêmes équipes : on repart de zéro */
            combatGuiDetruire(app->combat);
            app->combat = combatGuiCreer(app);
            if (app->combat == NULL) changerEcran(app, ECRAN_EQUIPE);
        }
    }
}
