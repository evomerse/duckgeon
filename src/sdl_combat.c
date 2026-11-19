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
    int sol = SOL_Y - (k % 2) * 50; /* un monstre sur deux plus loin */
    return (SDL_Rect){ 590 + k * 80, sol - taille, taille, taille };
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
        int y = f->y - (int)(f->age * 40);
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
    montrerEffets(c);
    reinitialiserChoix(c);
    c->attente = delai(c, 0.9);
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

/** Un combattant : son cube, son HUD (monstres) et son nom (héros). */
/* phase 0 : le cube ; phase 1 : son HUD (par-dessus tous les cubes). */
static void dessinerCombattant(Ui *ui, const CombatUi *cu, int i, int actif, int phase)
{
    const Combat *cb = cu->cb;
    const Combattant *c = &cb->c[i];
    const Personnage *p = c->p;
    int vivant = combatVivant(cb, i);
    SDL_Rect fixe = cubeFixe(cb, i);
    SDL_Rect r = cubeAnime(cu, i);
    SDL_Color couleur = couleurCube(cb, i);
    char lettre[2] = { p->nom[0], '\0' };

    if (phase == 0 && !vivant) {
        /* Un cube K.O. est écrasé au sol. */
        SDL_Rect plat = { r.x, r.y + r.h - 14, r.w, 14 };
        uiRect(ui, plat, uiTeinte(couleur, 40));
        uiTexteCentre(ui, ui->petite, "K.O.", (SDL_Rect){ r.x, r.y + r.h - 40, r.w, 20 }, UI_ROUGE);
    } else if (phase == 0) {
        if (actif) {
            SDL_Rect anneau = { r.x - 6, r.y - 6, r.w + 12, r.h + 18 };
            SDL_SetRenderDrawColor(ui->rend, UI_OR.r, UI_OR.g, UI_OR.b, 255);
            SDL_RenderDrawRect(ui->rend, &anneau);
        }
        uiCube(ui, r, couleur, lettre, combatEstAllie(i));
        if (cu->secousse[i] > 0) {
            uiRect(ui, r, (SDL_Color){ 255, 255, 255, (Uint8)(160 * cu->secousse[i] / DUREE_COUP) });
        }
    }

    if (phase == 0) {
        return;
    }
    char buf[96];
    if (!combatEstAllie(i)) {
        /* HUD du monstre : niveau, nom, vie, garde et faiblesses. */
        int w = 112;
        int x = fixe.x + fixe.w / 2 - w / 2;
        int y = fixe.y - 58;
        uiRect(ui, (SDL_Rect){ x - 4, y - 2, w + 8, 52 }, (SDL_Color){ 10, 12, 20, 170 });
        snprintf(buf, sizeof buf, "Niv %d  %s", p->niveau, p->nom);
        uiTexteCentre(ui, ui->petite, buf, (SDL_Rect){ x, y, w, 16 }, vivant ? UI_TEXTE : UI_GRIS);
        uiBarre(ui, (SDL_Rect){ x, y + 18, w, 8 }, p->pv, p->pvMax, UI_ROUGE);
        if (c->resistanceMax > 0) {
            uiBarre(ui, (SDL_Rect){ x, y + 28, w, 5 }, c->resistance, c->resistanceMax, (SDL_Color){ 235, 235, 245, 255 });
            if (vivant && !c->brise) {
                int n = 0;
                for (int e = 0; e < NB_ELEMENTS; e++) {
                    if (c->faiblesses & (1u << e)) {
                        SDL_Rect ic = { x + n * 14, y + 36, 11, 11 };
                        uiCadre(ui, ic, uiCouleurElement(e), UI_FOND);
                        n++;
                    }
                }
            } else if (vivant) {
                uiTexte(ui, ui->petite, "BRISÉ", x, y + 34, UI_OR);
            }
        }
        char etats[64];
        ecrireEtats(c, etats, sizeof etats);
        uiTexteCentre(ui, ui->petite, etats, (SDL_Rect){ fixe.x - 20, fixe.y + fixe.h + 6, fixe.w + 40, 16 }, UI_ORANGE);
    } else {
        uiTexteCentre(ui, ui->petite, p->nom, (SDL_Rect){ fixe.x - 20, fixe.y + fixe.h + 8, fixe.w + 40, 16 },
                      vivant ? UI_TEXTE : UI_GRIS);
        char etats[64];
        ecrireEtats(c, etats, sizeof etats);
        uiTexteCentre(ui, ui->petite, etats, (SDL_Rect){ fixe.x - 20, fixe.y + fixe.h + 26, fixe.w + 40, 16 }, UI_ORANGE);
    }
}

/** Sol de la scène : une bande plus claire où les cubes se tiennent. */
static void dessinerDecor(Ui *ui)
{
    uiRect(ui, (SDL_Rect){ 0, 74, UI_LARGEUR, SOL_Y - 74 + 10 }, (SDL_Color){ 22, 24, 38, 255 });
    uiRect(ui, (SDL_Rect){ 0, SOL_Y + 10, UI_LARGEUR, 36 }, (SDL_Color){ 34, 38, 58, 255 });
    SDL_SetRenderDrawColor(ui->rend, UI_BORD.r, UI_BORD.g, UI_BORD.b, 255);
    SDL_RenderDrawLine(ui->rend, 0, SOL_Y + 10, UI_LARGEUR, SOL_Y + 10);
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
        SDL_Rect r = { 10, 84 + k * 48, 84, 42 };
        uiCadre(ui, r, combatEstAllie(i) ? (SDL_Color){ 30, 44, 74, 255 } : (SDL_Color){ 70, 30, 40, 255 },
                k == 0 ? UI_OR : UI_BORD);
        uiCube(ui, (SDL_Rect){ r.x + 8, r.y + 8, 24, 24 }, couleurCube(cb, i), "", combatEstAllie(i));
        char buf[16];
        if (k == 0) {
            snprintf(buf, sizeof buf, "TOUR");
        } else {
            snprintf(buf, sizeof buf, "%d", (int)(avs[k] + 0.5));
        }
        uiTexteDroite(ui, ui->petite, buf, r.x + r.w - 6, r.y + 12, k == 0 ? UI_OR : UI_TEXTE);
    }
}

/** Cartes des héros en bas, numérotées 1 à 4 (touche = ultime). */
static void dessinerEquipe(CombatUi *c, Ui *ui, int manuel)
{
    const Combat *cb = c->cb;
    for (int i = 0; i < COMBAT_MAX_ALLIES; i++) {
        if (!cb->c[i].present) {
            continue;
        }
        const Combattant *h = &cb->c[i];
        const Personnage *p = h->p;
        SDL_Rect r = { 16 + i * 158, 540, 150, 92 };
        int vivant = combatVivant(cb, i);
        int ultime = manuel && combatPeutAgir(cb, i, ACTION_ULTIME);
        int actif = cb->etat == COMBAT_TOUR_JOUEUR && cb->actif == i;
        uiCadre(ui, r, UI_PANNEAU, actif ? UI_OR : (ultime ? UI_VERT : UI_BORD));

        uiCube(ui, (SDL_Rect){ r.x + 8, r.y + 8, 30, 30 }, couleurCube(cb, i), "", 1);
        char buf[64];
        snprintf(buf, sizeof buf, "%s", p->nom);
        uiTexte(ui, ui->normale, buf, r.x + 46, r.y + 6, vivant ? UI_TEXTE : UI_GRIS);
        snprintf(buf, sizeof buf, "%s", nomClasse(p->classe));
        uiTexte(ui, ui->petite, buf, r.x + 46, r.y + 28, UI_GRIS);
        uiBarre(ui, (SDL_Rect){ r.x + 8, r.y + 46, r.w - 16, 9 }, p->pv, p->pvMax,
                p->pv * 100 < p->pvMax * 30 ? UI_ROUGE : UI_VERT);
        snprintf(buf, sizeof buf, "%d/%d", p->pv, p->pvMax);
        uiTexte(ui, ui->petite, buf, r.x + 8, r.y + 56, vivant ? UI_TEXTE : UI_GRIS);
        uiBarre(ui, (SDL_Rect){ r.x + 8, r.y + 76, r.w - 16, 6 }, h->energie, h->energieMax,
                h->energie >= h->energieMax ? UI_OR : UI_BLEU);
        if (h->energie >= h->energieMax) {
            snprintf(buf, sizeof buf, "[%d] ULTIME", i + 1);
            uiTexteDroite(ui, ui->petite, buf, r.x + r.w - 8, r.y + 56, UI_OR);
        } else {
            snprintf(buf, sizeof buf, "[%d] %d/%d", i + 1, h->energie, h->energieMax);
            uiTexteDroite(ui, ui->petite, buf, r.x + r.w - 8, r.y + 56, UI_GRIS);
        }
        if (ultime && ui->clic && uiDans(r, ui->sx, ui->sy)) {
            ui->clic = 0;
            choisirAction(c, i, ACTION_ULTIME);
        }
    }
}

/** Description de l'action sélectionnée, points de compétence, boutons Q / E. */
static void dessinerActions(CombatUi *c, Ui *ui, int manuel, int fini)
{
    const Combat *cb = c->cb;
    SDL_Rect boite = { 104, 448, 500, 84 };
    uiCadre(ui, boite, UI_PANNEAU, UI_BORD);

    if (fini) {
        uiTexteCentre(ui, ui->normale, "Combat terminé", boite, UI_GRIS);
    } else if (cb->etat != COMBAT_TOUR_JOUEUR || cb->tourFini) {
        char buf[96];
        snprintf(buf, sizeof buf, "Tour de %s...", cb->c[cb->actif].p->nom);
        uiTexteCentre(ui, ui->normale, buf, boite, UI_GRIS);
    } else if (c->automatique) {
        uiTexteCentre(ui, ui->normale, "Mode AUTO : les héros jouent seuls", (SDL_Rect){ boite.x, boite.y + 14, boite.w, 28 },
                      UI_OR);
        uiTexteCentre(ui, ui->petite, "A ou clic sur AUTO pour reprendre la main", (SDL_Rect){ boite.x, boite.y + 46, boite.w, 20 },
                      UI_GRIS);
    } else {
        const Personnage *p = cb->c[c->lanceur].p;
        static const char *const TYPES[NB_ACTIONS] = { "Attaque de base", "Compétence", "Ultime" };
        char buf[200], nom[64], desc[200];
        snprintf(buf, sizeof buf, "%s - %s", p->nom, TYPES[c->action]);
        uiTexte(ui, ui->petite, buf, boite.x + 12, boite.y + 6, UI_VERT);
        nomCapacite(p, c->action, nom, sizeof nom);
        uiTexte(ui, ui->normale, nom, boite.x + 12, boite.y + 22, UI_TEXTE);
        descriptionCapacite(p, c->action, desc, sizeof desc);
        uiParagraphe(ui, ui->petite, desc, boite.x + 12, boite.y + 48, boite.w - 24, UI_GRIS);
    }

    /* Points de compétence : un losange par point. */
    uiTexte(ui, ui->petite, "Points de compétence", 620, 448, UI_GRIS);
    for (int k = 0; k < SP_MAX; k++) {
        uiLosange(ui, 632 + k * 26, 484, 10, k < cb->sp[0] ? UI_OR : UI_PANNEAU2);
    }
    if (manuel && c->cible >= 0 && c->cible < COMBAT_MAX && cb->c[c->cible].present) {
        char buf[96];
        snprintf(buf, sizeof buf, "Cible : %s", cb->c[c->cible].p->nom);
        uiTexte(ui, ui->petite, buf, 790, 448, UI_TEXTE);
        uiTexte(ui, ui->petite, "←/→ changer · Espace jouer", 790, 466, UI_GRIS);
    }

    int base = manuel && !c->automatique;
    int hero = cb->actif;
    int peutQ = base && combatPeutAgir(cb, hero, ACTION_BASIQUE);
    int peutE = base && combatPeutAgir(cb, hero, ACTION_COMPETENCE);
    if (uiOption(ui, (SDL_Rect){ 610, 500, 170, 34 }, "[Q] Base  +1 PC", manuel && c->action == ACTION_BASIQUE, peutQ)) {
        choisirAction(c, hero, ACTION_BASIQUE);
    }
    if (uiOption(ui, (SDL_Rect){ 786, 500, 200, 34 }, "[E] Compétence  1 PC", manuel && c->action == ACTION_COMPETENCE, peutE)) {
        choisirAction(c, hero, ACTION_COMPETENCE);
    }
}

/* ------------------------------------------------------------------ */
/*  Démarrage / fin                                                   */
/* ------------------------------------------------------------------ */

int combatUiDemarrer(CombatUi *c, Personnage *equipe[], int n, int etage, int automatique)
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
    if (!combatInit(c->cb, equipe, n, etage)) {
        free(c->cb);
        c->cb = NULL;
        return 0;
    }
    c->etage = etage;
    c->automatique = automatique;
    c->acteur = -1;
    c->attente = 0.6;
    reinitialiserChoix(c);
    for (int i = 0; i < FLOTTANTS_MAX; i++) {
        c->flottants[i].age = -1;
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
    c->foieGras = recompenseFoieGras(prog, c->etage, boss);
    if (c->etage == prog->etageMax && prog->etageMax < ETAGE_MAX) {
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
        snprintf(buf, sizeof buf, "Étage %d nettoyé : %s", c->etage, combatEtage(c->etage)->nom);
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
        } else if (c->etage == ETAGE_MAX) {
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
    uiRect(ui, (SDL_Rect){ 0, 0, UI_LARGEUR, 72 }, UI_PANNEAU);
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
        uiTexteCentre(ui, ui->petite, cb->journal[cb->nbJournal - 2], (SDL_Rect){ 110, 32, 620, 18 }, UI_GRIS);
    }
    uiTexteDroite(ui, ui->petite, "Q base · E compétence · 1-4 ultime · ←/→ cible · Espace jouer · A auto · Échap fuir",
                  UI_LARGEUR - 14, 52, UI_GRIS);

    if (uiOption(ui, (SDL_Rect){ 740, 8, 96, 34 }, c->automatique ? "AUTO [A] ON" : "AUTO [A]", c->automatique, !fini)) {
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
    dessinerDecor(ui);
    dessinerFile(ui, cb);

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
        dessinerCombattant(ui, c, i, !fini && i == cb->actif, 0);
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
        dessinerCombattant(ui, c, i, !fini && i == cb->actif, 1);
        if (touches & (1u << i)) {
            SDL_Rect r = cubeFixe(cb, i);
            uiRepere(ui, r.x + r.w / 2, combatEstAllie(i) ? r.y - 22 : r.y - 78, 9, i == c->cible ? UI_OR : UI_TEXTE);
        }
    }
    dessinerFlottants(c, ui, dt);

    dessinerActions(c, ui, manuel, fini);
    dessinerEquipe(c, ui, manuel);

    if (fini && c->attente <= 0 && (cb->etat == COMBAT_DEFAITE || c->recompenseFaite)) {
        return dessinerFin(c, ui);
    }
    return 0;
}
