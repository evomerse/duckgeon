/**
 * @file sdl_combat.c
 * @brief Écran de combat de Donjon SDL : même disposition que Duckgeon
 *        (scène avec héros à gauche et monstres à droite, file des tours,
 *        cartes des héros en bas, points de compétence, touches Q / E /
 *        1-4 / flèches / Espace) mais avec des cubes de couleur à la place
 *        des canards.
 *
 * Le combat lui-même est le moteur partagé (combat.c, comme dans le
 * terminal). Déroulement d'une image : on laisse l'animation se terminer
 * (attente), on avance d'une étape (même boucle que deroulerCombat dans
 * console_jeu.c), on traite les touches, puis on dessine.
 */

/* [DONJON SDL - partie 3] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boutique.h"
#include "sdl_combat.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SOL_Y        400   /* ligne de sol de la scène                  */
#define DUREE_ELAN   0.45  /* durée de l'élan d'un attaquant            */
#define DUREE_COUP   0.35  /* durée de la secousse d'un coup reçu       */

/* ------------------------------------------------------------------ */
/*  Disposition de la scène                                           */
/* ------------------------------------------------------------------ */

/** Rectangle du cube du combattant i, sans animation. */
static SDL_Rect cubeFixe(const Combat *cb, int i)
{
    if (combatEstAllie(i)) {
        int taille = 72;
        return (SDL_Rect){ 130 + i * 100, SOL_Y - taille, taille, taille };
    }
    int k = i - PREMIER_ENNEMI;
    int taille = cb->c[i].boss ? 104 : 68;
    int sol = SOL_Y - (k % 2) * 58; /* un monstre sur deux plus loin */
    return (SDL_Rect){ 560 + k * 88, sol - taille, taille, taille };
}

/** Rectangle du cube avec l'élan et la secousse du moment. */
static SDL_Rect cubeAnime(const CombatUi *c, int i)
{
    SDL_Rect r = cubeFixe(c->cb, i);
    if (i == c->acteur && c->lunge > 0) {
        double phase = sin(M_PI * (1.0 - c->lunge / DUREE_ELAN));
        r.x += (int)((combatEstAllie(i) ? 1 : -1) * 70 * phase);
    }
    if (c->secousse[i] > 0) {
        r.x += ((int)(c->secousse[i] * 90) % 2 ? 6 : -6);
    }
    return r;
}

/** Durée d'une pause, réduite de moitié en mode rapide. */
static double delai(const CombatUi *c, double base)
{
    return c->rapide ? base / 2 : base;
}

/** Couleur d'un cube : celle de l'élément (monstres un peu plus sombres). */
static SDL_Color couleurCube(const Combat *cb, int i)
{
    SDL_Color c = uiCouleurElement((int)cb->c[i].p->element);
    return combatEstAllie(i) ? c : uiTeinte(c, 80);
}

/**
 * Affinité d'élément d'une attaque : 125 (fort), 80 (faible) ou 100.
 * Retourne 100 pour les actions qui ne font pas de dégâts.
 */
static int affinite(const Combat *cb, int lanceur, enum TypeAction a, int cible)
{
    if (lanceur < 0 || cible < PREMIER_ENNEMI || cible >= COMBAT_MAX || !cb->c[lanceur].present || !cb->c[cible].present
        || !capaciteOffensive(cb->c[lanceur].p->classe, a)) {
        return 100;
    }
    return multiplicateurElement(cb->c[lanceur].p->element, cb->c[cible].p->element);
}

/* ------------------------------------------------------------------ */
/*  Nombres flottants                                                 */
/* ------------------------------------------------------------------ */

static void ajouterFlottant(CombatUi *c, int x, int y, const char *texte, SDL_Color couleur)
{
    int libre = 0;
    for (int i = 0; i < FLOTTANTS_MAX; i++) {
        if (c->flottants[i].age < 0) {
            libre = i;
            break;
        }
        if (c->flottants[i].age > c->flottants[libre].age) {
            libre = i; /* sinon on remplace le plus ancien */
        }
    }
    Flottant *f = &c->flottants[libre];
    snprintf(f->texte, sizeof f->texte, "%s", texte);
    f->couleur = couleur;
    f->x = x;
    f->y = y;
    f->age = 0;
}

/** Fait jaillir n éclats de couleur autour de (x, y). */
static void ajouterParticules(CombatUi *c, int x, int y, int n, SDL_Color couleur, double vitesse)
{
    for (int k = 0; k < n; k++) {
        int libre = -1;
        for (int i = 0; i < PARTICULES_MAX; i++) {
            if (c->particules[i].vie <= 0) {
                libre = i;
                break;
            }
        }
        if (libre < 0) {
            return;
        }
        Particule *p = &c->particules[libre];
        double ang = (rand() % 628) / 100.0;
        double v = vitesse * (0.4 + (rand() % 100) / 100.0);
        p->x = x;
        p->y = y;
        p->vx = cos(ang) * v;
        p->vy = sin(ang) * v - vitesse * 0.5;
        p->duree = p->vie = 0.45 + (rand() % 40) / 100.0;
        p->taille = 3 + rand() % 4;
        p->couleur = (k % 3 == 0) ? (SDL_Color){ 255, 244, 214, 255 } : couleur;
    }
}

/** Fait trembler l'écran pendant `duree` secondes (garde le plus long). */
static void trembler(CombatUi *c, double duree)
{
    if (duree > c->tremblement) {
        c->tremblement = c->tremblementMax = duree;
    }
}

/** Fait avancer et dessine les éclats (ils retombent avec la gravité). */
static void dessinerParticules(CombatUi *c, Ui *ui, double dt)
{
    for (int i = 0; i < PARTICULES_MAX; i++) {
        Particule *p = &c->particules[i];
        if (p->vie <= 0) {
            continue;
        }
        p->vie -= dt;
        p->vy += 520 * dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        SDL_Color col = p->couleur;
        col.a = (Uint8)(255 * (p->vie > 0 ? p->vie / p->duree : 0));
        uiRect(ui, (SDL_Rect){ (int)p->x - p->taille / 2, (int)p->y - p->taille / 2, p->taille, p->taille }, col);
    }
}

/** Transforme les effets de la dernière action en nombres flottants. */
static void montrerEffets(CombatUi *c)
{
    const Combat *cb = c->cb;
    for (int k = 0; k < cb->nbEffets; k++) {
        const Effet *e = &cb->effets[k];
        if (e->cible < 0 || e->cible >= COMBAT_MAX) {
            continue;
        }
        SDL_Rect r = cubeFixe(cb, e->cible);
        char texte[40];
        SDL_Color couleur = UI_TEXTE;
        switch (e->type) {
        case EFFET_DEGATS:
            snprintf(texte, sizeof texte, e->critique ? "-%d !" : "-%d", e->valeur);
            couleur = uiCouleurElement((int)e->element);
            c->secousse[e->cible] = DUREE_COUP;
            if (c->ultTemps <= 0) { /* l'ultime a son propre impact, plus tard */
                ajouterParticules(c, r.x + r.w / 2, r.y + r.h / 2, e->critique ? 22 : 10, couleur, e->critique ? 300 : 200);
                if (e->critique) {
                    trembler(c, 0.22);
                }
            }
            break;
        case EFFET_SOIN:
            snprintf(texte, sizeof texte, "+%d", e->valeur);
            couleur = UI_VERT;
            break;
        case EFFET_BOUCLIER:
            snprintf(texte, sizeof texte, "Bouclier %d", e->valeur);
            couleur = UI_BLEU;
            break;
        case EFFET_BRISE:
            snprintf(texte, sizeof texte, "BRISÉ");
            couleur = UI_OR;
            break;
        case EFFET_KO:
            snprintf(texte, sizeof texte, "K.O.");
            couleur = UI_ROUGE;
            break;
        default:
            snprintf(texte, sizeof texte, "%s", e->texte);
            couleur = e->type == EFFET_MALUS ? UI_ROUGE : UI_OR;
            break;
        }
        if (texte[0] != '\0') {
            ajouterFlottant(c, r.x + r.w / 2 - 20 + (k % 3) * 18, r.y - 10 - (k % 4) * 16, texte, couleur);
        }
    }
}

static void dessinerFlottants(CombatUi *c, Ui *ui, double dt)
{
    for (int i = 0; i < FLOTTANTS_MAX; i++) {
        Flottant *f = &c->flottants[i];
        if (f->age < 0) {
            continue;
        }
        f->age += dt;
        if (f->age > 1.3) {
            f->age = -1;
            continue;
        }
        SDL_Color col = f->couleur;
        col.a = (Uint8)(f->age < 0.9 ? 255 : 255 * (1.3 - f->age) / 0.4);
        int y = f->y - (int)(34 * (1 - exp(-9 * f->age)) + f->age * 14); /* jaillit puis flotte */
        uiTexte(ui, ui->normale, f->texte, f->x + 1, y + 1, (SDL_Color){ 0, 0, 0, col.a });
        uiTexte(ui, ui->normale, f->texte, f->x, y, col);
    }
}

/* ------------------------------------------------------------------ */
/*  Choix de la cible (manuel et automatique)                         */
/* ------------------------------------------------------------------ */

/** Meilleur ennemi à viser : sensible à l'élément du héros d'abord, puis
 *  le plus blessé. Retourne -1 si aucune cible valide. */
static int ennemiVise(const Combat *cb, int h, enum TypeAction a)
{
    int meilleur = -1;
    long meilleurScore = -1;
    for (int i = PREMIER_ENNEMI; i < COMBAT_MAX; i++) {
        if (!combatCibleValide(cb, h, a, i)) {
            continue;
        }
        const Combattant *e = &cb->c[i];
        long score = 100000 - e->p->pv;
        if (e->resistanceMax > 0 && !e->brise && (e->faiblesses & (1u << cb->c[h].p->element))) {
            score += 1000000;
        }
        if (score > meilleurScore) {
            meilleurScore = score;
            meilleur = i;
        }
    }
    return meilleur;
}

/** Allié le plus en danger (plus faible rapport PV / PV max). */
static int allieVise(const Combat *cb, int h, enum TypeAction a)
{
    int pire = -1, ratioPire = 1 << 30;
    for (int i = 0; i < COMBAT_MAX_ALLIES; i++) {
        if (!combatCibleValide(cb, h, a, i)) {
            continue;
        }
        const Personnage *p = cb->c[i].p;
        int ratio = p->pvMax > 0 ? p->pv * 100 / p->pvMax : 100;
        if (ratio < ratioPire) {
            ratioPire = ratio;
            pire = i;
        }
    }
    return pire;
}

/** Plus petit pourcentage de PV parmi les héros en vie. */
static int ratioMinAllies(const Combat *cb)
{
    int mini = 100;
    for (int i = 0; i < COMBAT_MAX_ALLIES; i++) {
        if (combatVivant(cb, i) && cb->c[i].p->pvMax > 0) {
            int r = cb->c[i].p->pv * 100 / cb->c[i].p->pvMax;
            if (r < mini) {
                mini = r;
            }
        }
    }
    return mini;
}

/** Cible adaptée à une action : ennemi ou allié selon ce qu'elle vise. */
static int cibleAuto(const Combat *cb, int h, enum TypeAction a)
{
    int cible;
    if (capaciteOffensive(cb->c[h].p->classe, a)) {
        cible = ennemiVise(cb, h, a);
    } else {
        cible = allieVise(cb, h, a);
    }
    return cible >= 0 ? cible : combatCibleParDefaut(cb, h, a);
}

/** Passe à la cible valide suivante (sens = +1 vers la droite, -1 vers la gauche). */
static void changerCible(CombatUi *c, int sens)
{
    const Combat *cb = c->cb;
    /* Ordre de lecture : ennemis de gauche à droite, ou alliés. */
    int debut = capaciteOffensive(cb->c[c->lanceur].p->classe, c->action) ? PREMIER_ENNEMI : 0;
    int n = debut == 0 ? COMBAT_MAX_ALLIES : COMBAT_MAX - PREMIER_ENNEMI;
    int pos = c->cible - debut;
    for (int k = 1; k <= n; k++) {
        int j = ((pos + sens * k) % n + n) % n + debut;
        if (combatCibleValide(cb, c->lanceur, c->action, j)) {
            c->cible = j;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Actions                                                           */
/* ------------------------------------------------------------------ */

/** Remet la sélection sur le héros dont c'est le tour (attaque de base). */
static void reinitialiserChoix(CombatUi *c)
{
    c->lanceur = c->cb->actif;
    c->action = ACTION_BASIQUE;
    c->cible = -1;
}

/** Joue une action et lance l'animation. Retourne 1 si elle a eu lieu. */
static int jouer(CombatUi *c, int lanceur, enum TypeAction action, int cible)
{
    if (!combatJouer(c->cb, lanceur, action, cible)) {
        return 0;
    }
    c->acteur = lanceur;
    c->lunge = DUREE_ELAN;
    if (action == ACTION_ULTIME) {
        char nom[64];
        nomCapacite(c->cb->c[lanceur].p, ACTION_ULTIME, nom, sizeof nom);
        snprintf(c->bandeau, sizeof c->bandeau, "%s  -  %s", c->cb->c[lanceur].p->nom, nom);
        c->bandeauCouleur = uiCouleurElement((int)c->cb->c[lanceur].p->element);
        c->bandeauDuree = c->bandeauTemps = delai(c, 1.4);
        c->ultImpact = 0;
        c->ultDuree = c->ultTemps = delai(c, 1.3);
        c->ultLanceur = lanceur;
        c->ultOffensif = capaciteOffensive(c->cb->c[lanceur].p->classe, ACTION_ULTIME);
    }
    montrerEffets(c);
    reinitialiserChoix(c);
    c->attente = delai(c, action == ACTION_ULTIME ? 1.3 : 0.9);
    return 1;
}

/** Choisit l'action (Q / E / ultime) du héros `lanceur` si elle est possible. */
static void choisirAction(CombatUi *c, int lanceur, enum TypeAction action)
{
    if (combatPeutAgir(c->cb, lanceur, action)) {
        c->lanceur = lanceur;
        c->action = action;
        c->cible = combatCibleParDefaut(c->cb, lanceur, action);
    }
}

/**
 * Tour d'un héros en mode automatique : ultime dès qu'il est prêt (les
 * soins seulement si quelqu'un est blessé), compétence quand les points
 * de compétence abondent ou qu'elle exploite une faiblesse, sinon attaque
 * de base pour regagner des points.
 */
static void heroAuto(CombatUi *c)
{
    Combat *cb = c->cb;
    int h = cb->actif;
    const Personnage *p = cb->c[h].p;
    int blesse = ratioMinAllies(cb);

    static const enum TypeAction PRIORITE[] = { ACTION_ULTIME, ACTION_COMPETENCE };
    for (int k = 0; k < 2; k++) {
        enum TypeAction a = PRIORITE[k];
        if (!combatPeutAgir(cb, h, a)) {
            continue;
        }
        int cible = cibleAuto(cb, h, a);
        if (capaciteOffensive(p->classe, a)) {
            int faible = cible >= PREMIER_ENNEMI && cb->c[cible].resistanceMax > 0 && !cb->c[cible].brise
                         && (cb->c[cible].faiblesses & (1u << p->element));
            if (a == ACTION_COMPETENCE && cb->sp[0] < 3 && !faible) {
                continue;
            }
        } else if (blesse >= 75) {
            continue; /* soin ou bonus inutile : on garde l'énergie */
        }
        if (cible >= 0 && jouer(c, h, a, cible)) {
            return;
        }
    }
    if (jouer(c, h, ACTION_BASIQUE, cibleAuto(cb, h, ACTION_BASIQUE))) {
        return;
    }
    cb->tourFini = 1; /* sécurité : ne jamais rester bloqué */
}

/** Une étape du combat (appelée quand l'animation précédente est finie). */
static void etape(CombatUi *c)
{
    Combat *cb = c->cb;
    if (cb->tourFini && (cb->etat == COMBAT_TOUR_JOUEUR || cb->etat == COMBAT_TOUR_ENNEMI)) {
        combatAvancer(cb); /* dégâts sur la durée, gel, prochain combattant */
        montrerEffets(c);
        reinitialiserChoix(c);
        c->attente = delai(c, 0.35);
        return;
    }
    switch (cb->etat) {
    case COMBAT_FIN_VAGUE:
        combatVagueSuivante(cb);
        reinitialiserChoix(c);
        c->attente = delai(c, 0.8);
        break;
    case COMBAT_TOUR_ENNEMI:
        c->acteur = cb->actif;
        c->lunge = DUREE_ELAN;
        combatTourEnnemi(cb);
        montrerEffets(c);
        if (!cb->tourFini && cb->etat == COMBAT_TOUR_ENNEMI) {
            cb->tourFini = 1; /* sécurité */
        }
        c->attente = delai(c, 0.9);
        break;
    case COMBAT_TOUR_JOUEUR:
        if (c->automatique) {
            heroAuto(c);
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Dessin de la scène                                                */
/* ------------------------------------------------------------------ */

/** Écrit les états d'un combattant (Brisé, Gelé...) dans buf. */
static void ecrireEtats(const Combattant *c, char *buf, int taille)
{
    buf[0] = '\0';
    int n = 0;
    if (c->brise) n += snprintf(buf + n, (size_t)(taille - n), "Brisé ");
    if (c->gele > 0) n += snprintf(buf + n, (size_t)(taille - n), "Gelé ");
    if (c->toursDot > 0) n += snprintf(buf + n, (size_t)(taille - n), "Dégâts/tour ");
    if (c->toursBuffAtk > 0) n += snprintf(buf + n, (size_t)(taille - n), "ATK+ ");
    if (c->toursMalusDef > 0) n += snprintf(buf + n, (size_t)(taille - n), "DEF- ");
    if (c->bouclier > 0) snprintf(buf + n, (size_t)(taille - n), "Bouclier %d", c->bouclier);
}

/* phase 0 : le personnage ; phase 1 : son HUD (par-dessus tous les autres). */
static void dessinerCombattant(Ui *ui, const CombatUi *cu, int i, int actif, int phase, int manuel)
{
    const Combat *cb = cu->cb;
    const Combattant *c = &cb->c[i];
    const Personnage *p = c->p;
    int vivant = combatVivant(cb, i);
    SDL_Rect fixe = cubeFixe(cb, i);
    SDL_Rect r = cubeAnime(cu, i);
    SDL_Color couleur = couleurCube(cb, i);

    if (phase == 0) {
        if (actif && vivant) {
            /* disque lumineux sous le combattant dont c'est le tour */
            uiRectArrondi(ui, (SDL_Rect){ fixe.x - 14, fixe.y + fixe.h - 8, fixe.w + 28, 22 }, 6, (SDL_Color){ 244, 190, 62, 70 });
            uiRectArrondi(ui, (SDL_Rect){ fixe.x - 6, fixe.y + fixe.h - 4, fixe.w + 12, 14 }, 4, (SDL_Color){ 244, 190, 62, 120 });
        }
        if (combatEstAllie(i) && cu->ultTemps > 0 && i == cu->ultLanceur) {
            /* le lanceur est dessiné par l'animation d'ultime (il saute) : on laisse son ombre */
            uiRect(ui, (SDL_Rect){ fixe.x - 4, fixe.y + fixe.h - 2, fixe.w + 8, 8 }, (SDL_Color){ 0, 0, 0, 110 });
        } else if (combatEstAllie(i)) {
            uiHeros(ui, r, (int)p->classe, couleur, 1, vivant, i * 1.7);
        } else {
            uiMonstre(ui, r, couleur, c->boss, vivant);
        }
        if (vivant && cu->secousse[i] > 0) {
            uiRectArrondi(ui, r, 2, (SDL_Color){ 255, 255, 255, (Uint8)(150 * cu->secousse[i] / DUREE_COUP) });
        }
        return;
    }

    char buf[96];
    char etats[64];
    ecrireEtats(c, etats, sizeof etats);
    if (!combatEstAllie(i)) {
        /* HUD du monstre : élément, niveau, nom, vie, garde, faiblesses. */
        int w = 110;
        int x = fixe.x + fixe.w / 2 - w / 2;
        int y = fixe.y - 62;
        if (!vivant) {
            return;
        }
        uiRectArrondi(ui, (SDL_Rect){ x - 6, y - 4, w + 12, 62 }, 4, (SDL_Color){ 12, 12, 14, 190 });
        uiElementIcone(ui, x + 8, y + 8, 8, (int)p->element);
        snprintf(buf, sizeof buf, "%d %s", p->niveau, p->nom);
        uiTexte(ui, ui->petite, buf, x + 22, y, UI_TEXTE);
        uiBarre(ui, (SDL_Rect){ x, y + 20, w, 9 }, p->pv, p->pvMax, UI_ROUGE);
        if (c->resistanceMax > 0) {
            uiBarre(ui, (SDL_Rect){ x, y + 31, w, 6 }, c->resistance, c->resistanceMax, (SDL_Color){ 235, 235, 245, 255 });
            if (!c->brise) {
                int n = 0;
                for (int e = 0; e < NB_ELEMENTS; e++) {
                    if (c->faiblesses & (1u << e)) {
                        uiElementIcone(ui, x + 8 + n * 19, y + 46, 8, e);
                        n++;
                    }
                }
            } else {
                uiTexte(ui, ui->petite, "BRISÉ", x, y + 38, UI_OR);
            }
        }
        /* Avantage d'élément de l'attaque en préparation. */
        int m = manuel ? affinite(cb, cu->lanceur, cu->action, i) : 100;
        if (m != 100) {
            SDL_Color col = m > 100 ? UI_VERT : UI_ROUGE;
            snprintf(buf, sizeof buf, m > 100 ? "+%d %%" : "%d %%", m - 100);
            SDL_Rect pastille = { x + w - 62, y + 38, 62, 18 };
            uiRectArrondi(ui, pastille, 4, uiTeinte(col, 35));
            uiFleche(ui, pastille.x + 9, pastille.y + 9, 7, m > 100, col);
            uiTexte(ui, ui->petite, buf, pastille.x + 19, pastille.y, col);
        }
        uiTexteCentre(ui, ui->petite, etats, (SDL_Rect){ fixe.x - 30, fixe.y + fixe.h + 6, fixe.w + 60, 16 }, UI_ORANGE);
    } else {
        uiElementIcone(ui, fixe.x + fixe.w / 2 - uiLargeur(ui->petite, p->nom) / 2 - 12, fixe.y + fixe.h + 17, 7, (int)p->element);
        uiTexteCentre(ui, ui->petite, p->nom, (SDL_Rect){ fixe.x - 20, fixe.y + fixe.h + 9, fixe.w + 40, 16 },
                      vivant ? UI_TEXTE : UI_GRIS);
        uiTexteCentre(ui, ui->petite, etats, (SDL_Rect){ fixe.x - 30, fixe.y + fixe.h + 26, fixe.w + 60, 16 }, UI_ORANGE);
    }
}

/** Sol de la scène : une arène éclairée où les combattants se tiennent. */
static void dessinerDecor(Ui *ui)
{
    uiRect(ui, (SDL_Rect){ -12, 72, UI_LARGEUR + 24, SOL_Y - 72 + 14 }, (SDL_Color){ 28, 28, 32, 120 });
    /* colonnes lointaines */
    for (int k = 0; k < 6; k++) {
        uiRectArrondi(ui, (SDL_Rect){ 130 + k * 160, 150 + (k % 2) * 30, 44, 260 }, 2, (SDL_Color){ 16, 16, 18, 150 });
    }
    uiRectDegrade(ui, (SDL_Rect){ -12, SOL_Y + 10, UI_LARGEUR + 24, 40 }, 0, (SDL_Color){ 56, 56, 62, 255 }, (SDL_Color){ 30, 30, 34, 255 });
    uiRect(ui, (SDL_Rect){ -12, SOL_Y + 10, UI_LARGEUR + 24, 2 }, (SDL_Color){ 120, 120, 128, 255 });
    /* cercles au sol : camp des héros à gauche, camp des monstres à droite */
    uiRectArrondi(ui, (SDL_Rect){ 60, SOL_Y - 6, 450, 40 }, 8, (SDL_Color){ 110, 130, 170, 45 });
    uiRectArrondi(ui, (SDL_Rect){ 560, SOL_Y - 6, 420, 40 }, 8, (SDL_Color){ 170, 90, 90, 40 });
}

/** File des prochains tours (colonne de gauche). */
static void dessinerFile(Ui *ui, const Combat *cb)
{
    int ordre[8];
    double avs[8];
    int n = combatOrdre(cb, ordre, avs, 7);
    for (int k = 0; k < n; k++) {
        int i = ordre[k];
        if (i < 0 || i >= COMBAT_MAX || cb->c[i].p == NULL) {
            continue;
        }
        SDL_Rect r = { 10, 82 + k * 48, 86, 42 };
        uiCadre(ui, r, combatEstAllie(i) ? (SDL_Color){ 40, 50, 70, 255 } : (SDL_Color){ 80, 40, 44, 255 },
                k == 0 ? UI_OR : UI_BORD);
        uiDisque(ui, r.x + 22, r.y + 21, 14, uiTeinte(couleurCube(cb, i), 70));
        uiElementIcone(ui, r.x + 22, r.y + 21, 11, (int)cb->c[i].p->element);
        char buf[16];
        if (k == 0) {
            snprintf(buf, sizeof buf, "TOUR");
        } else {
            snprintf(buf, sizeof buf, "%d", (int)(avs[k] + 0.5));
        }
        uiTexteDroite(ui, ui->petite, buf, r.x + r.w - 8, r.y + 12, k == 0 ? UI_OR : UI_TEXTE);
    }
}

/** Nombre de héros dont l'ultime est prêt (énergie pleine). */
static int ultimesPrets(const Combat *cb)
{
    int n = 0;
    for (int i = 0; i < COMBAT_MAX_ALLIES; i++) {
        if (combatVivant(cb, i) && cb->c[i].energie >= cb->c[i].energieMax) {
            n++;
        }
    }
    return n;
}

/**
 * Cartes des héros en bas, numérotées 1 à 4. Chaque carte a une grosse
 * bande ULTIME : grisée avec la jauge d'énergie, puis dorée et qui
 * pulse quand l'ultime est prêt (touche 1-4 ou clic sur la bande).
 */
static void dessinerEquipe(CombatUi *c, Ui *ui, int manuel)
{
    const Combat *cb = c->cb;
    double pulse = 0.5 + 0.5 * sin(ui->temps / 160.0);
    for (int i = 0; i < COMBAT_MAX_ALLIES; i++) {
        if (!cb->c[i].present) {
            continue;
        }
        const Combattant *h = &cb->c[i];
        const Personnage *p = h->p;
        SDL_Rect r = { 16 + i * 158, 536, 150, 98 };
        int vivant = combatVivant(cb, i);
        int pret = vivant && h->energie >= h->energieMax;
        int ultime = manuel && combatPeutAgir(cb, i, ACTION_ULTIME);
        int actif = cb->etat == COMBAT_TOUR_JOUEUR && cb->actif == i;
        int selection = manuel && c->action == ACTION_ULTIME && c->lanceur == i;
        if (pret) {
            uiLueur(ui, r, UI_OR, 60 + (int)(100 * pulse));
        }
        uiCadre(ui, r, actif ? UI_PANNEAU2 : UI_PANNEAU, selection ? UI_VERT : (actif || pret ? UI_OR : UI_BORD));

        SDL_Color col = couleurCube(cb, i);
        uiHeros(ui, (SDL_Rect){ r.x + 14, r.y + 20, 24, 24 }, (int)p->classe, col, 1, vivant, i * 1.7);
        uiElementIcone(ui, r.x + 38, r.y + 36, 7, (int)p->element);
        char buf[64];
        uiTexte(ui, ui->normale, p->nom, r.x + 48, r.y + 5, vivant ? UI_TEXTE : UI_GRIS);
        snprintf(buf, sizeof buf, "%s - niv %d", nomClasse(p->classe), p->niveau);
        uiTexte(ui, ui->petite, buf, r.x + 48, r.y + 27, UI_GRIS);
        uiBarre(ui, (SDL_Rect){ r.x + 10, r.y + 48, r.w - 20, 10 }, p->pv, p->pvMax,
                p->pv * 100 < p->pvMax * 30 ? UI_ROUGE : UI_VERT);
        snprintf(buf, sizeof buf, "%d/%d", p->pv, p->pvMax);
        uiTexteCentre(ui, ui->petite, buf, (SDL_Rect){ r.x + 10, r.y + 47, r.w - 20, 12 }, UI_TEXTE);

        /* Bande ULTIME */
        SDL_Rect bande = { r.x + 8, r.y + 66, r.w - 16, 26 };
        if (pret) {
            uiRectDegrade(ui, bande, 5, uiTeinte(UI_OR, 100 + (int)(25 * pulse)), uiTeinte(UI_OR, 74));
            snprintf(buf, sizeof buf, "[%d] ULTIME PRÊT !", i + 1);
            uiTexteCentre(ui, ui->petite, buf, bande, (SDL_Color){ 50, 30, 6, 255 });
        } else {
            uiBarre(ui, bande, h->energie, h->energieMax, UI_BLEU);
            snprintf(buf, sizeof buf, "[%d] Ultime %d/%d", i + 1, h->energie, h->energieMax);
            uiTexteCentre(ui, ui->petite, buf, bande, vivant ? UI_TEXTE : UI_GRIS);
        }
        if (ultime && ui->clic && uiDans(r, ui->sx, ui->sy)) {
            ui->clic = 0;
            choisirAction(c, i, ACTION_ULTIME);
        }
    }
}

/** Texte d'affinité entre l'élément de l'attaquant et celui de la cible. */
static void texteAffinite(const Combat *cb, int lanceur, int cible, char *buf, int taille, SDL_Color *couleur)
{
    enum Element ea = cb->c[lanceur].p->element, ec = cb->c[cible].p->element;
    int m = multiplicateurElement(ea, ec);
    if (m > 100) {
        snprintf(buf, (size_t)taille, "%s contre %s : très efficace (+25 %%) et entame la garde", nomElement(ea), nomElement(ec));
        *couleur = UI_VERT;
    } else if (m < 100) {
        snprintf(buf, (size_t)taille, "%s contre %s : peu efficace (-20 %%)", nomElement(ea), nomElement(ec));
        *couleur = UI_ROUGE;
    } else {
        snprintf(buf, (size_t)taille, "%s contre %s : dégâts normaux", nomElement(ea), nomElement(ec));
        *couleur = UI_GRIS;
    }
}

/** Description de l'action sélectionnée, points de compétence, boutons Q / E. */
static void dessinerActions(CombatUi *c, Ui *ui, int manuel, int fini)
{
    const Combat *cb = c->cb;
    SDL_Rect boite = { 104, 432, 500, 98 };
    int ult = manuel && c->action == ACTION_ULTIME;
    uiCadre(ui, boite, UI_PANNEAU, ult ? UI_OR : UI_BORD);

    if (fini) {
        uiTexteCentre(ui, ui->normale, "Combat terminé", boite, UI_GRIS);
    } else if (cb->etat != COMBAT_TOUR_JOUEUR || cb->tourFini) {
        char buf[96];
        snprintf(buf, sizeof buf, "Tour de %s...", cb->c[cb->actif].p->nom);
        uiTexteCentre(ui, ui->normale, buf, boite, UI_GRIS);
    } else if (c->automatique) {
        uiTexteCentre(ui, ui->normale, "Mode AUTO : les héros jouent seuls", (SDL_Rect){ boite.x, boite.y + 18, boite.w, 28 },
                      UI_OR);
        uiTexteCentre(ui, ui->petite, "A ou clic sur AUTO pour reprendre la main (ultimes inclus)",
                      (SDL_Rect){ boite.x, boite.y + 52, boite.w, 20 }, UI_GRIS);
    } else {
        const Personnage *p = cb->c[c->lanceur].p;
        static const char *const TYPES[NB_ACTIONS] = { "Attaque de base", "Compétence", "ULTIME" };
        char buf[200], nom[64], desc[200];
        snprintf(buf, sizeof buf, "%s - %s", p->nom, TYPES[c->action]);
        uiTexte(ui, ui->petite, buf, boite.x + 14, boite.y + 4, ult ? UI_OR : UI_VERT);
        nomCapacite(p, c->action, nom, sizeof nom);
        uiTexte(ui, ui->normale, nom, boite.x + 14, boite.y + 18, UI_TEXTE);
        descriptionCapacite(p, c->action, desc, sizeof desc);
        uiParagraphe(ui, ui->petite, desc, boite.x + 14, boite.y + 40, boite.w - 28, UI_GRIS);
        if (c->cible >= PREMIER_ENNEMI && capaciteOffensive(p->classe, c->action)) {
            SDL_Color col;
            texteAffinite(cb, c->lanceur, c->cible, buf, sizeof buf, &col);
            uiTexte(ui, ui->petite, buf, boite.x + 14, boite.y + 78, col);
        }
    }

    /* Points de compétence : un losange par point. */
    uiTexte(ui, ui->petite, "Points de compétence", 622, 440, UI_GRIS);
    for (int k = 0; k < SP_MAX; k++) {
        uiLosange(ui, 634 + k * 26, 474, 10, k < cb->sp[0] ? UI_OR : UI_PANNEAU2);
    }
    if (manuel && c->cible >= 0 && c->cible < COMBAT_MAX && cb->c[c->cible].present) {
        char buf[96];
        snprintf(buf, sizeof buf, "Cible : %s", cb->c[c->cible].p->nom);
        uiTexte(ui, ui->petite, buf, 790, 440, UI_TEXTE);
        int wt = uiTouche(ui, "←/→", 790, 456);
        uiTexte(ui, ui->petite, "cible", 790 + wt + 4, 458, UI_GRIS);
        int wt2 = uiTouche(ui, "Espace", 790 + wt + 50, 456);
        uiTexte(ui, ui->petite, "jouer", 790 + wt + 50 + wt2 + 4, 458, UI_GRIS);
    }

    int base = manuel && !c->automatique;
    int hero = cb->actif;
    int peutQ = base && combatPeutAgir(cb, hero, ACTION_BASIQUE);
    int peutE = base && combatPeutAgir(cb, hero, ACTION_COMPETENCE);
    if (uiOption(ui, (SDL_Rect){ 616, 494, 180, 36 }, "[Q] Base  +1 PC", manuel && c->action == ACTION_BASIQUE, peutQ)) {
        choisirAction(c, hero, ACTION_BASIQUE);
    }
    if (uiOption(ui, (SDL_Rect){ 806, 494, 180, 36 }, "[E] Compétence  1 PC", manuel && c->action == ACTION_COMPETENCE, peutE)) {
        choisirAction(c, hero, ACTION_COMPETENCE);
    }
}

/** Cercle (contour épais) de centre (cx, cy). */
static void anneau(Ui *ui, int cx, int cy, int rayon, int epaisseur, SDL_Color col)
{
    SDL_SetRenderDrawColor(ui->rend, col.r, col.g, col.b, col.a);
    int pas = rayon > 0 ? 360 * 2 : 1;
    for (int k = 0; k < pas; k++) {
        double ang = k * M_PI * 2 / pas;
        for (int e = 0; e < epaisseur; e++) {
            SDL_RenderDrawPoint(ui->rend, cx + (int)((rayon - e) * cos(ang)), cy + (int)((rayon - e) * sin(ang)));
        }
    }
}

/** Décélération : 0 -> 1 vite au début, doucement à la fin. */
static double facile(double t)
{
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return 1 - (1 - t) * (1 - t);
}

/** Centre de la zone ennemie visée (ou de l'équipe pour un soin). */
static void centreCible(const CombatUi *c, int *x, int *y)
{
    *x = c->ultOffensif ? 800 : 300;
    *y = c->ultOffensif ? SOL_Y - 40 : SOL_Y - 60;
}

/** Une étincelle de la forme de l'élément : disque (feu), losange (glace), carré. */
static void etincelle(Ui *ui, int x, int y, int taille, SDL_Color col, int element)
{
    if (element == FEU) {
        uiDisque(ui, x, y, taille / 2 + 1, col);
    } else if (element == GLACE || element == IMAGINAIRE) {
        uiLosange(ui, x, y, taille, col);
    } else {
        uiRect(ui, (SDL_Rect){ x - taille / 2, y - taille / 2, taille, taille }, col);
    }
}

/**
 * Impact de l'ultime sur chaque cible touchée : une signature par élément
 * (foudre qui tombe, tranchants, piliers de lumière, glace qui éclate...).
 * `t2` va de 0 (impact) à 1 (fin).
 */
static void dessinerImpact(CombatUi *c, Ui *ui, SDL_Color col, int element, double t2)
{
    const Combat *cb = c->cb;
    double fondu = 1 - t2;
    for (int k = 0; k < cb->nbEffets; k++) {
        const Effet *e = &cb->effets[k];
        if (e->type != EFFET_DEGATS || e->cible < 0 || e->cible >= COMBAT_MAX) {
            continue;
        }
        SDL_Rect r = cubeFixe(cb, e->cible);
        int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        SDL_Color a = col;
        a.a = (Uint8)(255 * fondu);
        /* flash blanc sur la cible + onde de choc (toujours) */
        uiRect(ui, r, (SDL_Color){ 255, 255, 255, (Uint8)(200 * fondu * fondu) });
        anneau(ui, cx, cy, (int)(10 + 120 * facile(t2)), 3, a);
        switch (element) {
        case FOUDRE: /* éclair en zigzag du haut de l'écran jusqu'à la cible */
            for (int b = 0; b < 2; b++) {
                int x = cx + (b ? 14 : -10), y = 72;
                SDL_SetRenderDrawColor(ui->rend, 255, 250, 200, a.a);
                for (int seg = 0; seg < 9; seg++) {
                    int nx = cx + ((seg * 53 + b * 31 + (int)(t2 * 40)) % 41 - 20) * (9 - seg) / 9;
                    int ny = 72 + (cy - 72) * (seg + 1) / 9;
                    SDL_RenderDrawLine(ui->rend, x, y, nx, ny);
                    SDL_RenderDrawLine(ui->rend, x + 1, y, nx + 1, ny);
                    x = nx;
                    y = ny;
                }
            }
            break;
        case PHYSIQUE: /* trois tranchants en diagonale */
            SDL_SetRenderDrawColor(ui->rend, 255, 255, 255, a.a);
            for (int b = -1; b <= 1; b++) {
                int l = (int)(70 * facile(t2 * 3));
                SDL_RenderDrawLine(ui->rend, cx - l + b * 14, cy - l, cx + l + b * 14, cy + l);
                SDL_RenderDrawLine(ui->rend, cx - l + b * 14 + 1, cy - l, cx + l + b * 14 + 1, cy + l);
            }
            break;
        case IMAGINAIRE: /* pilier de lumière */
            uiRect(ui, (SDL_Rect){ cx - 14, 72, 28, cy - 72 }, (SDL_Color){ a.r, a.g, a.b, (Uint8)(110 * fondu) });
            uiRect(ui, (SDL_Rect){ cx - 5, 72, 10, cy - 72 }, (SDL_Color){ 255, 255, 240, (Uint8)(220 * fondu) });
            break;
        case GLACE: /* éclats de glace qui s'écartent */
            for (int b = 0; b < 8; b++) {
                double ang = b * M_PI / 4 + 0.3;
                int d = (int)(14 + 70 * facile(t2));
                uiLosange(ui, cx + (int)(cos(ang) * d), cy + (int)(sin(ang) * d), 7, a);
            }
            break;
        case FEU: /* flammes qui montent */
            for (int b = 0; b < 7; b++) {
                int x = cx - 24 + b * 8;
                int h = (int)((30 + (b * 17) % 30) * facile(t2 * 1.6));
                uiRect(ui, (SDL_Rect){ x, cy + r.h / 2 - h - (int)(t2 * 40), 7, h }, (SDL_Color){ 255, (Uint8)(120 + b * 15), 40, (Uint8)(220 * fondu) });
            }
            break;
        case VENT: /* rafales horizontales */
            SDL_SetRenderDrawColor(ui->rend, a.r, a.g, a.b, a.a);
            for (int b = -2; b <= 2; b++) {
                int l = (int)(110 * facile(t2 * 2));
                SDL_RenderDrawLine(ui->rend, cx - l, cy + b * 12, cx + l, cy + b * 12);
            }
            break;
        default: /* Quantique : anneaux qui se répondent */
            anneau(ui, cx, cy, (int)(24 + 90 * facile(t2 * 1.3)), 2, a);
            anneau(ui, cx, cy, (int)(8 + 50 * facile(t2)), 2, a);
            break;
        }
    }
}

/**
 * Animation d'ultime en petit film : bandes de cinéma, le héros saute et
 * charge (anneaux, rayons, aura), puis retombe pendant que les étincelles
 * foncent vers les ennemis ; l'impact fait trembler l'écran et chaque
 * élément a sa signature (voir dessinerImpact).
 */
static void dessinerUltime(CombatUi *c, Ui *ui)
{
    if (c->ultTemps <= 0 || c->ultDuree <= 0) {
        return;
    }
    const Combat *cb = c->cb;
    double t = 1.0 - c->ultTemps / c->ultDuree; /* 0 -> 1 */
    SDL_Color col = c->bandeauCouleur;
    int element = (int)cb->c[c->ultLanceur].p->element;
    SDL_Rect cube = cubeFixe(cb, c->ultLanceur);
    int cx = cube.x + cube.w / 2, cy = cube.y + cube.h / 2;
    const double T_IMPACT = 0.62;

    /* scène assombrie pendant l'ultime */
    int noir = (int)(170 * sin(M_PI * t));
    uiRect(ui, (SDL_Rect){ -12, 72, UI_LARGEUR + 24, UI_HAUTEUR - 72 }, (SDL_Color){ 4, 4, 6, (Uint8)noir });

    /* le héros saute (charge), puis s'abat au moment de l'impact */
    int saut = 0;
    if (t < 0.4) {
        saut = (int)(54 * facile(t / 0.4));
    } else if (t < T_IMPACT) {
        double d = (t - 0.4) / (T_IMPACT - 0.4);
        saut = (int)(54 * (1 - d * d));
    }
    SDL_Rect hero = cubeAnime(c, c->ultLanceur);
    hero.y -= saut;
    double charge = t < T_IMPACT ? sin(M_PI * t / T_IMPACT) : 0;
    uiLueur(ui, hero, col, (int)(255 * (charge > 0.2 ? charge : sin(M_PI * t))));
    uiHeros(ui, hero, (int)cb->c[c->ultLanceur].p->classe, uiTeinte(col, 130), 1, 1, 0);
    /* ombre qui rétrécit quand il saute */
    int ombre = cube.w + 8 - saut / 2;
    uiRect(ui, (SDL_Rect){ cx - ombre / 2, cube.y + cube.h + 1, ombre, 5 }, (SDL_Color){ 0, 0, 0, 120 });

    /* rayons qui tournent (pendant la charge) */
    SDL_SetRenderDrawColor(ui->rend, col.r, col.g, col.b, (Uint8)(200 * (1 - t)));
    for (int k = 0; k < 14; k++) {
        double ang = k * M_PI * 2 / 14 + t * 2.2;
        int l1 = (int)(30 + 260 * t), l0 = (int)(20 + 120 * t);
        SDL_RenderDrawLine(ui->rend, cx + (int)(l0 * cos(ang)), cy - saut + (int)(l0 * sin(ang)), cx + (int)(l1 * cos(ang)),
                           cy - saut + (int)(l1 * sin(ang)));
    }
    /* anneaux décalés autour du héros */
    for (int k = 0; k < 3; k++) {
        double tk = t * 1.4 - k * 0.2;
        if (tk > 0 && tk < 1) {
            SDL_Color a = col;
            a.a = (Uint8)(230 * (1 - tk));
            anneau(ui, cx, cy - saut, (int)(20 + 330 * tk), 4, a);
        }
    }
    /* énergie aspirée vers le héros pendant la charge */
    if (t < T_IMPACT) {
        for (int k = 0; k < 16; k++) {
            double ang = k * 2.4 + t * 3;
            double d = (1 - t / T_IMPACT) * (90 + (k % 4) * 30);
            SDL_Color a = col;
            a.a = (Uint8)(200 * charge);
            etincelle(ui, cx + (int)(cos(ang) * d), cy - saut + (int)(sin(ang) * d), 5, a, element);
        }
    }
    /* étincelles : du héros vers les ennemis, ou pluie sur l'équipe */
    int cibleX, cibleY;
    centreCible(c, &cibleX, &cibleY);
    for (int k = 0; k < 26; k++) {
        double tk = (t - 0.3) * 1.9 - (k % 13) * 0.04;
        if (tk <= 0 || tk >= 1) {
            continue;
        }
        double dec = ((k * 37) % 11 - 5) * 10.0;
        int x = (int)(cx + (cibleX - cx) * tk + (c->ultOffensif ? 0 : dec * 3));
        int y = (int)(cy + (cibleY - cy) * tk + sin(tk * M_PI) * dec * 2.0 - (c->ultOffensif ? 0 : 140 * (1 - tk)));
        int taille = 5 + (k % 3) * 2;
        SDL_Color p = k % 2 ? col : (SDL_Color){ 255, 240, 200, 255 };
        p.a = (Uint8)(255 * (1 - tk * 0.6));
        etincelle(ui, x, y, taille, p, element);
    }
    /* éclair blanc au lancement */
    if (t < 0.12) {
        uiRect(ui, (SDL_Rect){ -12, 0, UI_LARGEUR + 24, UI_HAUTEUR }, (SDL_Color){ 255, 255, 255, (Uint8)(210 * (1 - t / 0.12)) });
    }
    /* impact : une seule fois, secousse + éclats ; puis la signature de l'élément */
    if (t >= T_IMPACT) {
        if (!c->ultImpact) {
            c->ultImpact = 1;
            trembler(c, 0.5);
            if (c->ultOffensif) {
                for (int k = 0; k < cb->nbEffets; k++) {
                    const Effet *e = &cb->effets[k];
                    if (e->type == EFFET_DEGATS && e->cible >= 0 && e->cible < COMBAT_MAX) {
                        SDL_Rect r = cubeFixe(cb, e->cible);
                        ajouterParticules(c, r.x + r.w / 2, r.y + r.h / 2, 26, col, 340);
                    }
                }
            } else {
                ajouterParticules(c, 300, SOL_Y - 60, 30, col, 240);
            }
        }
        if (c->ultOffensif) {
            dessinerImpact(c, ui, col, element, (t - T_IMPACT) / (1 - T_IMPACT));
        }
        /* flash d'impact */
        if (t < T_IMPACT + 0.06) {
            uiRect(ui, (SDL_Rect){ -12, 72, UI_LARGEUR + 24, UI_HAUTEUR - 72 },
                   (SDL_Color){ 255, 255, 255, (Uint8)(150 * (1 - (t - T_IMPACT) / 0.06)) });
        }
    }
}

/** Bandes de cinéma qui se referment sur l'écran pendant un ultime. */
static void dessinerBandesCinema(const CombatUi *c, Ui *ui)
{
    if (c->ultTemps <= 0 || c->ultDuree <= 0) {
        return;
    }
    double t = 1.0 - c->ultTemps / c->ultDuree;
    double entree = t < 0.12 ? facile(t / 0.12) : (t > 0.88 ? facile((1 - t) / 0.12) : 1);
    int h = (int)(46 * entree);
    uiRect(ui, (SDL_Rect){ 0, 0, UI_LARGEUR, h }, (SDL_Color){ 0, 0, 0, 255 });
    uiRect(ui, (SDL_Rect){ 0, UI_HAUTEUR - h, UI_LARGEUR, h }, (SDL_Color){ 0, 0, 0, 255 });
}

/**
 * Annonce d'ultime : la bande glisse depuis les bords, le portrait du héros
 * bondit dedans et le nom arrive en deux temps. Sinon, rappel quand un
 * ultime est prêt.
 */
static void dessinerAnnonces(CombatUi *c, Ui *ui, int manuel)
{
    if (c->bandeauTemps > 0 && c->bandeauDuree > 0) {
        double ecoule = c->bandeauDuree - c->bandeauTemps;
        double entree = facile(ecoule / 0.22);
        int alpha = c->bandeauTemps > 0.3 ? 255 : (int)(255 * c->bandeauTemps / 0.3);
        SDL_Color col = c->bandeauCouleur;
        int demi = (int)(UI_LARGEUR / 2 * entree);
        SDL_Rect b = { UI_LARGEUR / 2 - demi, 196, demi * 2, 78 };
        uiRect(ui, (SDL_Rect){ 0, 0, UI_LARGEUR, UI_HAUTEUR }, (SDL_Color){ col.r, col.g, col.b, (Uint8)(alpha / 10) });
        uiRect(ui, b, (SDL_Color){ 8, 8, 10, (Uint8)(alpha * 215 / 255) });
        uiRect(ui, (SDL_Rect){ b.x, b.y, b.w, 3 }, (SDL_Color){ col.r, col.g, col.b, (Uint8)alpha });
        uiRect(ui, (SDL_Rect){ b.x, b.y + b.h - 3, b.w, 3 }, (SDL_Color){ col.r, col.g, col.b, (Uint8)alpha });
        /* lignes de vitesse qui filent derrière le texte */
        SDL_SetRenderDrawColor(ui->rend, col.r, col.g, col.b, (Uint8)(alpha / 3));
        for (int k = 0; k < 7; k++) {
            int y = b.y + 8 + k * 10;
            int x = (int)(ecoule * (900 + k * 140) + k * 97) % (UI_LARGEUR + 200) - 100;
            SDL_RenderDrawLine(ui->rend, x, y, x + 90 + k * 14, y);
        }
        if (entree >= 1) {
            /* portrait : bondit puis se pose */
            double saut = ecoule < 0.5 ? fabs(sin(ecoule * 14)) * 12 * (0.5 - ecoule) * 2 : 0;
            uiHeros(ui, (SDL_Rect){ 70, b.y + 12 - (int)saut, 52, 52 }, (int)c->cb->c[c->ultLanceur].p->classe,
                    uiCouleurElement((int)c->cb->c[c->ultLanceur].p->element), 1, 1, 0);
            double texte = facile((ecoule - 0.15) / 0.25);
            int dx = (int)(160 * (1 - texte));
            uiTexteCentre(ui, ui->petite, "ULTIME", (SDL_Rect){ 150 + dx, b.y + 6, UI_LARGEUR - 150, 18 },
                          (SDL_Color){ 244, 190, 62, (Uint8)(alpha * texte) });
            uiTexteCentre(ui, ui->grande, c->bandeau, (SDL_Rect){ 150 + dx, b.y + 26, UI_LARGEUR - 150, 44 },
                          (SDL_Color){ 255, 255, 255, (Uint8)(alpha * texte) });
        }
    } else if (manuel && ultimesPrets(c->cb) > 0) {
        double pulse = 0.5 + 0.5 * sin(ui->temps / 160.0);
        char buf[96];
        int n = ultimesPrets(c->cb);
        snprintf(buf, sizeof buf, n > 1 ? "%d ULTIMES PRÊTS - touches 1 à 4" : "ULTIME PRÊT - touche 1 à 4 ou clic sur la carte", n);
        SDL_Rect p = { UI_LARGEUR / 2 - 200, 80, 400, 28 };
        uiRectArrondi(ui, p, 6, (SDL_Color){ 244, 190, 62, (Uint8)(60 + 80 * pulse) });
        uiTexteCentre(ui, ui->normale, buf, p, UI_OR);
    }
}

/* ------------------------------------------------------------------ */
/*  Démarrage / fin                                                   */
/* ------------------------------------------------------------------ */

int combatUiDemarrer(CombatUi *c, Personnage *equipe[], int n, int etage, int automatique, int libre)
{
    if (c == NULL || equipe == NULL || n <= 0) {
        return 0;
    }
    memset(c, 0, sizeof *c);
    c->cb = malloc(sizeof *c->cb); /* grosse structure : sur le tas */
    if (c->cb == NULL) {
        perror("malloc");
        return 0;
    }
    if (!(libre ? combatInitLibre(c->cb, equipe, n, etage) : combatInit(c->cb, equipe, n, etage))) {
        free(c->cb);
        c->cb = NULL;
        return 0;
    }
    c->etage = etage;
    c->libre = libre;
    c->automatique = automatique;
    c->acteur = -1;
    c->attente = 0.6;
    reinitialiserChoix(c);
    for (int i = 0; i < FLOTTANTS_MAX; i++) {
        c->flottants[i].age = -1;
    }
    for (int i = 0; i < PARTICULES_MAX; i++) {
        c->particules[i].vie = 0;
    }
    return 1;
}

void combatUiLiberer(CombatUi *c)
{
    if (c == NULL) {
        return;
    }
    free(c->cb);
    c->cb = NULL;
}

/** Récompenses de victoire : foie gras, étage suivant, sauvegarde. */
static void recompenser(CombatUi *c, Progression *prog)
{
    int boss = combatEtage(c->etage)->boss != NB_MONSTRES;
    if (c->libre) {
        /* Entraînement : petit gain, ni bonus de premier passage ni déblocage. */
        c->foieGras = (4 + 3 * c->etage) / 2;
        prog->foieGras += c->foieGras;
    } else {
        c->foieGras = recompenseFoieGras(prog, c->etage, boss);
    }
    if (!c->libre && c->etage == prog->etageMax && prog->etageMax < ETAGE_MAX) {
        prog->etageMax++;
        c->etageDebloque = prog->etageMax;
    }
    if (!sauvegarderProgression(FICHIER_PROGRESSION, prog)) {
        fprintf(stderr, "La progression n'a pas pu etre enregistree.\n");
    }
    c->recompenseFaite = 1;
}

/** Fenêtre de fin de combat. Retourne 1 quand on clique sur Continuer. */
static int dessinerFin(CombatUi *c, Ui *ui)
{
    const Combat *cb = c->cb;
    int victoire = cb->etat == COMBAT_VICTOIRE;
    uiRect(ui, (SDL_Rect){ 0, 0, UI_LARGEUR, UI_HAUTEUR }, (SDL_Color){ 0, 0, 0, 170 });
    SDL_Rect r = { 220, 130, 560, 380 };
    uiCadre(ui, r, UI_PANNEAU, victoire ? UI_OR : UI_ROUGE);
    uiTexteCentre(ui, ui->grande, victoire ? "VICTOIRE !" : "DÉFAITE...", (SDL_Rect){ r.x, r.y + 14, r.w, 44 },
                  victoire ? UI_OR : UI_ROUGE);

    int y = r.y + 76;
    char buf[160];
    if (victoire) {
        snprintf(buf, sizeof buf, c->libre ? "Combat libre terminé (étage %d : %s)" : "Étage %d nettoyé : %s", c->etage,
                 combatEtage(c->etage)->nom);
        uiTexte(ui, ui->normale, buf, r.x + 30, y, UI_TEXTE);
        y += 32;
        snprintf(buf, sizeof buf, "Survivants : +%d XP et +%d point(s) de stats chacun", cb->xpGagnee, cb->pointsGagnes);
        uiTexte(ui, ui->normale, buf, r.x + 30, y, UI_TEXTE);
        y += 28;
        snprintf(buf, sizeof buf, "Foie gras : +%d", c->foieGras);
        uiTexte(ui, ui->normale, buf, r.x + 30, y, UI_OR);
        y += 28;
        for (int k = 0; k < cb->nbButins; k++) {
            snprintf(buf, sizeof buf, "Butin : %s pour %s", cb->butins[k].nom, cb->receveurs[k]);
            uiTexte(ui, ui->normale, buf, r.x + 30, y, UI_VERT);
            y += 26;
        }
        if (c->etageDebloque > 0) {
            snprintf(buf, sizeof buf, "Nouvel étage débloqué : %d - %s", c->etageDebloque,
                     combatEtage(c->etageDebloque)->nom);
            uiTexte(ui, ui->normale, buf, r.x + 30, y + 6, UI_BLEU);
        } else if (!c->libre && c->etage == ETAGE_MAX) {
            uiTexte(ui, ui->normale, "Le Canard Noir est vaincu : le donjon est libéré !", r.x + 30, y + 6, UI_BLEU);
        }
    } else {
        uiTexte(ui, ui->normale, "L'équipe est K.O.", r.x + 30, y, UI_TEXTE);
        uiParagraphe(ui, ui->normale,
                     "Reposez-vous au camp (menu 11) ou utilisez une potion, puis retentez. "
                     "Changer l'élément des héros ou monter leurs stats aide aussi.",
                     r.x + 30, y + 36, r.w - 60, UI_GRIS);
    }
    uiTexteCentre(ui, ui->petite, "Pensez à sauvegarder vos héros (menu 4).", (SDL_Rect){ r.x, r.y + r.h - 96, r.w, 20 },
                  UI_GRIS);
    int clic = uiBouton(ui, (SDL_Rect){ r.x + 180, r.y + r.h - 62, 200, 42 }, "Continuer  [Entrée]", 1);
    return clic || ui->touche == SDLK_RETURN || ui->touche == SDLK_ESCAPE;
}

/* ------------------------------------------------------------------ */
/*  Image                                                             */
/* ------------------------------------------------------------------ */

int combatUiImage(CombatUi *c, Ui *ui, Progression *prog, double dt)
{
    if (c == NULL || c->cb == NULL || ui == NULL || prog == NULL) {
        return 1;
    }
    Combat *cb = c->cb;
    int fini = cb->etat == COMBAT_VICTOIRE || cb->etat == COMBAT_DEFAITE;

    /* ---- Animations ---- */
    if (c->lunge > 0) c->lunge -= dt;
    if (c->bandeauTemps > 0) c->bandeauTemps -= dt;
    if (c->ultTemps > 0) c->ultTemps -= dt;
    if (c->tremblement > 0) c->tremblement -= dt;
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (c->secousse[i] > 0) c->secousse[i] -= dt;
    }

    /* ---- Avancement ---- */
    if (c->attente > 0) {
        c->attente -= dt;
    } else if (!fini) {
        etape(c);
    } else if (cb->etat == COMBAT_VICTOIRE && !c->recompenseFaite) {
        recompenser(c, prog);
    }
    fini = cb->etat == COMBAT_VICTOIRE || cb->etat == COMBAT_DEFAITE;

    int tourHeros = !fini && cb->etat == COMBAT_TOUR_JOUEUR && !cb->tourFini;
    int manuel = tourHeros && !c->automatique;

    /* ---- Touches ---- */
    if (!fini && ui->touche == SDLK_a) {
        c->automatique = !c->automatique;
        manuel = tourHeros && !c->automatique;
    }
    if (manuel) {
        if (c->lanceur < 0 || c->lanceur >= COMBAT_MAX || !cb->c[c->lanceur].present) {
            reinitialiserChoix(c);
        }
        if (ui->touche == SDLK_q) {
            choisirAction(c, cb->actif, ACTION_BASIQUE);
        } else if (ui->touche == SDLK_e) {
            choisirAction(c, cb->actif, ACTION_COMPETENCE);
        } else if (ui->touche >= SDLK_1 && ui->touche <= SDLK_4) {
            choisirAction(c, ui->touche - SDLK_1, ACTION_ULTIME);
        } else if (ui->touche == SDLK_LEFT) {
            changerCible(c, -1);
        } else if (ui->touche == SDLK_RIGHT) {
            changerCible(c, +1);
        }
        /* La cible doit rester valide (un monstre peut tomber, une vague changer). */
        if (!combatCibleValide(cb, c->lanceur, c->action, c->cible)) {
            c->cible = combatCibleParDefaut(cb, c->lanceur, c->action);
        }
        if ((ui->touche == SDLK_SPACE || ui->touche == SDLK_RETURN) && c->attente <= 0 && c->cible >= 0) {
            jouer(c, c->lanceur, c->action, c->cible);
            manuel = 0;
        }
    }

    /* ---- Haut de l'écran ---- */
    uiRectDegrade(ui, (SDL_Rect){ 0, 0, UI_LARGEUR, 72 }, 0, (SDL_Color){ 44, 44, 50, 255 }, (SDL_Color){ 30, 30, 34, 255 });
    uiRect(ui, (SDL_Rect){ 0, 71, UI_LARGEUR, 1 }, UI_BORD);
    char buf[200];
    snprintf(buf, sizeof buf, "ÉTAGE %d", c->etage);
    uiTexte(ui, ui->grande, buf, 20, 4, UI_OR);
    snprintf(buf, sizeof buf, "%s · Vague %d/%d · Tour %d", combatEtage(c->etage)->nom, cb->vague, cb->nbVagues,
             cb->tour + 1);
    uiTexte(ui, ui->petite, buf, 20, 44, UI_GRIS);
    if (cb->nbJournal > 0) {
        uiTexteCentre(ui, ui->normale, cb->journal[cb->nbJournal - 1], (SDL_Rect){ 110, 8, 620, 24 }, UI_TEXTE);
    }
    if (cb->nbJournal > 1) {
        uiTexteCentre(ui, ui->petite, cb->journal[cb->nbJournal - 2], (SDL_Rect){ 110, 29, 620, 18 }, UI_GRIS);
    }
    uiRaccourcis(ui, "Q|base;E|compétence;1-4|ultime;←/→|cible;Espace|jouer;A|auto;Échap|fuir", UI_LARGEUR - 14, 48);

    if (uiOption(ui, (SDL_Rect){ 740, 8, 96, 34 }, "AUTO [A]", c->automatique, !fini)) {
        c->automatique = !c->automatique;
    }
    if (uiOption(ui, (SDL_Rect){ 842, 8, 52, 34 }, "×2", c->rapide, 1)) {
        c->rapide = !c->rapide;
    }
    if (uiBouton(ui, (SDL_Rect){ 900, 8, 86, 34 }, fini ? "Quitter" : "Fuir", 1)) {
        return 1;
    }
    if (!fini && ui->touche == SDLK_ESCAPE) {
        return 1;
    }

    /* ---- Scène ---- */
    /* Le décor (sol, mur) et la file de tours restent fixes : seuls les
     * combattants et les effets tremblent, pour une base stable. */
    dessinerDecor(ui);
    dessinerFile(ui, cb);
    int tremble = c->tremblement > 0 && c->tremblementMax > 0;
    SDL_Rect vue = { 0, 0, UI_LARGEUR, UI_HAUTEUR };
    if (tremble) {
        double force = 5 * c->tremblement / c->tremblementMax;
        vue.x = (int)(sin(ui->temps / 17.0) * force);
        vue.y = (int)(cos(ui->temps / 23.0) * force * 0.6);
        SDL_RenderSetViewport(ui->rend, &vue);
    }

    /* Cibles touchées par l'action en préparation. */
    unsigned touches = 0;
    if (manuel && c->cible >= 0) {
        for (int i = 0; i < COMBAT_MAX; i++) {
            if (combatEstTouche(cb, c->lanceur, c->action, c->cible, i)) {
                touches |= 1u << i;
            }
        }
    }
    /* Les monstres du fond (colonnes impaires) d'abord, puis le devant, puis les héros. */
    static const int ORDRE[] = { 5, 7, 4, 6, 8, 0, 1, 2, 3 };
    for (int k = 0; k < (int)(sizeof ORDRE / sizeof ORDRE[0]); k++) {
        int i = ORDRE[k];
        if (!cb->c[i].present) {
            continue;
        }
        SDL_RenderSetViewport(ui->rend, (tremble && !combatEstAllie(i)) ? &vue : NULL); /* l'équipe ne tremble pas */
        dessinerCombattant(ui, c, i, !fini && i == cb->actif, 0, manuel);
        /* Un clic sur un cube valide le choisit et joue l'action. */
        if (manuel && ui->clic && c->attente <= 0 && combatCibleValide(cb, c->lanceur, c->action, i)
            && uiDans(cubeFixe(cb, i), ui->sx, ui->sy)) {
            ui->clic = 0;
            jouer(c, c->lanceur, c->action, i);
            manuel = 0;
        }
    }
    for (int k = 0; k < (int)(sizeof ORDRE / sizeof ORDRE[0]); k++) {
        int i = ORDRE[k];
        if (!cb->c[i].present) {
            continue;
        }
        SDL_RenderSetViewport(ui->rend, (tremble && !combatEstAllie(i)) ? &vue : NULL);
        dessinerCombattant(ui, c, i, !fini && i == cb->actif, 1, manuel);
        if (touches & (1u << i)) {
            SDL_Rect r = cubeFixe(cb, i);
            uiRepere(ui, r.x + r.w / 2, combatEstAllie(i) ? r.y - 22 : r.y - 78, 9, i == c->cible ? UI_OR : UI_TEXTE);
        }
    }
    SDL_RenderSetViewport(ui->rend, tremble ? &vue : NULL);
    dessinerUltime(c, ui);
    dessinerParticules(c, ui, dt);
    dessinerFlottants(c, ui, dt);
    SDL_RenderSetViewport(ui->rend, NULL);
    dessinerBandesCinema(c, ui);

    dessinerActions(c, ui, manuel, fini);
    dessinerEquipe(c, ui, manuel);
    dessinerAnnonces(c, ui, manuel);

    if (fini && c->attente <= 0 && (cb->etat == COMBAT_DEFAITE || c->recompenseFaite)) {
        return dessinerFin(c, ui);
    }
    return 0;
}
