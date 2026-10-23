/**
 * @file gui_heros.c
 * @brief Écrans « Mes héros » (liste, fiche, inventaire) et « Préparer
 *        l'expédition » (choix de l'équipe et de l'étage).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gui_app.h"
#include "gui_sprite.h"
#include "inventaire.h"

#define PI_F 3.14159265f

/* Petit message temporaire (« Potion utilisée »...). */
static char messageFlash[96];
static float dureeMessage = 0;

static void annoncer(const char *message)
{
    snprintf(messageFlash, sizeof messageFlash, "%s", message);
    dureeMessage = 2.5f;
}

static void dessinerMessage(Gui *g)
{
    if (dureeMessage <= 0) {
        return;
    }
    dureeMessage -= g->dt;
    int alpha = dureeMessage > 0.5f ? 255 : (int)(510 * dureeMessage);
    int w;
    tailleTexte(g, POLICE_NORMALE, messageFlash, &w, NULL);
    rectArrondi(g, LARGEUR / 2.0f - w / 2.0f - 20, 640, w + 40, 40, 20, rgba(20, 16, 40, alpha * 9 / 10));
    contourArrondi(g, LARGEUR / 2.0f - w / 2.0f - 20, 640, w + 40, 40, 20, 1.5f, avecAlpha(COUL_OR, alpha));
    texte(g, POLICE_NORMALE, messageFlash, LARGEUR / 2.0f, 650, avecAlpha(COUL_TEXTE, alpha), ALIGN_CENTRE);
}

/** Carte compacte d'un héros ; retourne 1 si elle est cliquée. */
static int carteHeros(Gui *g, const Personnage *p, float x, float y, float w, float h,
                      int choisie, int active)
{
    float t = g->temps / 1000.0f;
    int dessus = active && survol(g, x, y, w, h);
    SDL_Color cc = couleurClasse(p->classe);
    int ko = !estVivant(p);

    rectArrondi(g, x, y, w, h, 12, dessus ? rgba(46, 40, 86, 235) : rgba(28, 24, 54, 225));
    degradeHorizontal(g, x + 6, y + 6, w * 0.5f, h - 12, avecAlpha(cc, 40), avecAlpha(cc, 0));
    contourArrondi(g, x, y, w, h, 12, choisie ? 2.5f : 1.2f,
                   choisie ? COUL_OR_CLAIR : (dessus ? rgba(200, 190, 255, 160) : COUL_BORDURE));

    float r = h / 2 - 12;
    portraitHeros(g, p, x + 12 + r, y + h / 2, r, t);
    if (ko) {
        rectArrondi(g, x + 12, y + h / 2 - r, 2 * r, 2 * r, r * 0.3f, rgba(0, 0, 0, 140));
        texteOmbre(g, POLICE_PETITE, "K.O.", x + 12 + r, y + h / 2 - 8, COUL_DANGER, ALIGN_CENTRE);
    }

    float tx = x + 24 + 2 * r;
    texte(g, POLICE_MOYENNE, p->nom, tx, y + 10, ko ? COUL_TEXTE_DOUX : COUL_TEXTE, ALIGN_GAUCHE);
    char ligneInfo[64];
    snprintf(ligneInfo, sizeof ligneInfo, "Niv %d  ·  %s", p->niveau, libelleClasse(p->classe));
    texte(g, POLICE_PETITE, ligneInfo, tx, y + 36, cc, ALIGN_GAUCHE);
    iconeElement(g, p->element, x + w - 20, y + 20, 10);
    if (p->pointsStats > 0) { /* points de stats à répartir */
        char pts[16];
        snprintf(pts, sizeof pts, "+%d", p->pointsStats);
        float px = x + w - 48;
        cercle(g, px, y + 20, 11, COUL_VIE);
        texte(g, POLICE_PETITE, pts, px, y + 12, rgba(10, 30, 16, 255), ALIGN_CENTRE);
    }

    float ratio = (float)p->pv / p->pvMax;
    barre(g, tx, y + h - 24, w - (tx - x) - 14, 9, ratio,
          ratio > 0.3f ? COUL_VIE : COUL_DANGER, rgba(10, 8, 20, 200));

    if (dessus) {
        g->survolCliquable = 1;
    }
    return dessus && g->clic;
}

/* ------------------------------------------------------------------ */
/*  Mes héros                                                         */
/* ------------------------------------------------------------------ */

/** Ligne de statistique « Libellé  valeur ». */
static void stat(Gui *g, const char *libelle, const char *valeur, float x, float y)
{
    texte(g, POLICE_PETITE, libelle, x, y, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    texte(g, POLICE_NORMALE, valeur, x + 150, y - 2, COUL_TEXTE, ALIGN_DROITE);
}

/** Fiche détaillée du héros sélectionné. */
static void ficheHeros(App *app, Personnage *p)
{
    Gui *g = &app->g;
    float t = g->temps / 1000.0f;
    const float px = 680, py = 110, pw = 570, ph = 590;
    panneau(g, px, py, pw, ph);

    /* sprite sur un piédestal */
    float sx = px + 110, sy = py + 400;
    halo(g, sx, sy - 120, 170, avecAlpha(couleurElement(p->element), 60));
    ellipse(g, sx, sy + 4, 95, 20, rgba(40, 32, 70, 255));
    ellipse(g, sx, sy, 88, 16, rgba(80, 68, 130, 255));
    Pose pose = poseDefaut(t);
    if (!estVivant(p)) pose.alpha = 0.55f;
    dessinerHeros(g, p, sx, sy, 1.55f, &pose);

    if (bouton(g, px + 22, py + 438, 180, 40, "Personnaliser", BOUTON_NORMAL, 1)) {
        ouvrirEditeur(app, app->selection);
    }
    char libAmelio[40];
    snprintf(libAmelio, sizeof libAmelio, p->pointsStats > 0 ? "Améliorer (%d pts)" : "Améliorer", p->pointsStats);
    if (bouton(g, px + 22, py + 486, 180, 40, libAmelio, BOUTON_PRINCIPAL, p->pointsStats > 0)) {
        ouvrirAmelioration(app, app->selection);
    }
    const char *libSuppr = app->confirmerSuppression ? "Confirmer ?" : "Supprimer";
    if (bouton(g, px + 22, py + 534, 180, 40, libSuppr, BOUTON_DANGER, 1)) {
        if (app->confirmerSuppression) {
            char msg[96];
            snprintf(msg, sizeof msg, "%s quitte le donjon.", p->nom);
            donjonSupprimer(&app->donjon, app->selection);
            if (app->selection >= app->donjon.taille) app->selection = app->donjon.taille - 1;
            app->confirmerSuppression = 0;
            app->prep.nb[0] = app->prep.nb[1] = 0; /* indices des équipes périmés */
            annoncer(msg);
            return;
        }
        app->confirmerSuppression = 1;
    }

    /* identité */
    float ix = px + 235;
    texteOmbre(g, POLICE_GRANDE, p->nom, ix, py + 18, COUL_TEXTE, ALIGN_GAUCHE);
    char buf[128];
    snprintf(buf, sizeof buf, "%s  ·  %s", libelleClasse(p->classe), nomElement(p->element));
    iconeElement(g, p->element, ix + 9, py + 70, 9);
    texte(g, POLICE_NORMALE, buf, ix + 26, py + 61, couleurClasse(p->classe), ALIGN_GAUCHE);

    snprintf(buf, sizeof buf, "Niveau %d  ·  %d victoire%s", p->niveau, p->victoires, p->victoires > 1 ? "s" : "");
    texte(g, POLICE_NORMALE, buf, ix, py + 92, COUL_OR_CLAIR, ALIGN_GAUCHE);
    snprintf(buf, sizeof buf, "XP %d / %d", p->xp, xpPourNiveauSuivant(p));
    texte(g, POLICE_PETITE, buf, px + pw - 24, py + 95, COUL_TEXTE_DOUX, ALIGN_DROITE);
    barre(g, ix, py + 118, pw - 260, 7, (float)p->xp / xpPourNiveauSuivant(p), COUL_OR, rgba(10, 8, 20, 200));

    /* statistiques */
    float sy2 = py + 140;
    snprintf(buf, sizeof buf, "%d / %d", p->pv, p->pvMax);
    stat(g, "PV", buf, ix, sy2);
    snprintf(buf, sizeof buf, "%d (+%d)", attaqueTotale(p), p->arme.valeur);
    stat(g, "Attaque", buf, ix + 170, sy2);
    snprintf(buf, sizeof buf, "%d", p->def);
    stat(g, "Défense", buf, ix, sy2 + 26);
    snprintf(buf, sizeof buf, "%d", p->vit);
    stat(g, "Vitesse", buf, ix + 170, sy2 + 26);

    /* capacités */
    static const char *const TYPES[NB_ACTIONS] = { "BASIQUE", "COMPÉTENCE", "ULTIME" };
    float cy = py + 200;
    texte(g, POLICE_MOYENNE, "Capacités", ix, cy, COUL_OR, ALIGN_GAUCHE);
    cy += 30;
    for (int a = 0; a < NB_ACTIONS; a++) {
        const Capacite *cap = capaciteClasse(p->classe, (enum TypeAction)a);
        texte(g, POLICE_PETITE, TYPES[a], ix, cy, avecAlpha(couleurClasse(p->classe), 220), ALIGN_GAUCHE);
        texte(g, POLICE_NORMALE, cap->nom, ix + 112, cy - 2, COUL_TEXTE, ALIGN_GAUCHE);
        cy += 20;
        cy += texteBloc(g, POLICE_PETITE, cap->description, ix, cy, (int)(pw - 260), COUL_TEXTE_DOUX) + 8;
    }

    /* équipement et sac */
    cy = py + 430;
    texte(g, POLICE_MOYENNE, "Équipement", ix, cy, COUL_OR, ALIGN_GAUCHE);
    cy += 30;
    texte(g, POLICE_PETITE, "clic : utiliser · clic droit : jeter", px + pw - 24, cy - 24,
          COUL_TEXTE_DOUX, ALIGN_DROITE);
    if (p->arme.nom[0] != '\0') {
        snprintf(buf, sizeof buf, "Arme : %s (+%d ATK)", p->arme.nom, p->arme.valeur);
    } else {
        snprintf(buf, sizeof buf, "Arme : aucune");
    }
    texte(g, POLICE_NORMALE, buf, ix, cy, COUL_TEXTE, ALIGN_GAUCHE);
    cy += 26;

    float largeurChip = (pw - 260 - 8) / 2;
    for (int i = 0; i < INVENTAIRE_MAX; i++) {
        float cx = ix + (i % 2) * (largeurChip + 8);
        float cy2 = cy + (i / 2) * 32;
        if (i >= p->nbObjets) {
            contourArrondi(g, cx, cy2, largeurChip, 26, 13, 1, rgba(120, 110, 170, 50));
            continue;
        }
        const Objet *o = &p->inventaire[i];
        int dessus = survol(g, cx, cy2, largeurChip, 26);
        SDL_Color teinte = o->type == SOIN ? COUL_VIE : rgba(150, 170, 255, 255);
        rectArrondi(g, cx, cy2, largeurChip, 26, 13, dessus ? rgba(70, 60, 120, 255) : rgba(40, 34, 74, 255));
        cercle(g, cx + 13, cy2 + 13, 6, teinte);
        snprintf(buf, sizeof buf, "%s %s%d", o->nom, o->type == SOIN ? "+" : "ATK+", o->valeur);
        texte(g, POLICE_PETITE, buf, cx + 25, cy2 + 5, COUL_TEXTE, ALIGN_GAUCHE);
        if (dessus) {
            g->survolCliquable = 1;
            char msg[96];
            if (g->clic) {
                snprintf(msg, sizeof msg, "%s : %s %s", p->nom,
                         o->type == SOIN ? "utilise" : "équipe", o->nom);
                utiliserObjet(p, i); /* même fonction que la console */
                annoncer(msg);
                break;
            }
            if (g->clicDroit) {
                Objet jete;
                retirerObjet(p, i, &jete);
                snprintf(msg, sizeof msg, "%s jette %s.", p->nom, jete.nom);
                annoncer(msg);
                break;
            }
        }
    }
}

void ecranHeros(App *app)
{
    Gui *g = &app->g;
    Donjon *d = &app->donjon;
    dessinerFond(g, rgba(16, 12, 36, 255), rgba(30, 20, 50, 255));

    char sous[64];
    snprintf(sous, sizeof sous, "%d héros dans le donjon", d->taille);
    if (enTete(app, "Mes héros", sous)) {
        changerEcran(app, ECRAN_MENU);
    }
    if (bouton(g, 1040, 30, 210, 44, "+ Nouveau héros", BOUTON_PRINCIPAL, 1)) {
        ouvrirEditeur(app, -1);
    }
    if (bouton(g, 830, 30, 196, 44, "Se reposer au camp", BOUTON_NORMAL, d->taille > 0)) {
        for (int i = 0; i < d->taille; i++) {
            d->elements[i].pv = d->elements[i].pvMax;
        }
        annoncer("Toute l'équipe a récupéré ses PV.");
    }

    if (d->taille == 0) {
        texte(g, POLICE_MOYENNE, "Le donjon est vide : créez votre premier héros !",
              LARGEUR / 2.0f, 340, COUL_TEXTE_DOUX, ALIGN_CENTRE);
        dessinerMessage(g);
        return;
    }
    if (app->selection < 0 || app->selection >= d->taille) {
        app->selection = 0;
    }

    /* liste défilante (2 colonnes) */
    const float lx = 30, ly = 110, lw = 630, lh = 590;
    const float cw = 305, ch = 92, gap = 14;
    int lignes = (d->taille + 1) / 2;
    int maxDefil = (int)(lignes * (ch + gap) - lh);
    if (maxDefil < 0) maxDefil = 0;
    if (survol(g, lx, ly, lw, lh)) {
        app->defilement -= g->molette * 40;
    }
    if (app->defilement < 0) app->defilement = 0;
    if (app->defilement > maxDefil) app->defilement = maxDefil;

    SDL_Rect clip = { (int)lx - 4, (int)ly, (int)lw + 8, (int)lh };
    SDL_RenderSetClipRect(g->rendu, &clip);
    for (int i = 0; i < d->taille; i++) {
        float x = lx + (i % 2) * (cw + gap);
        float y = ly + (i / 2) * (ch + gap) - app->defilement;
        if (y + ch < ly || y > ly + lh) continue;
        int visible = g->sourisY >= ly && g->sourisY < ly + lh;
        if (carteHeros(g, &d->elements[i], x, y, cw, ch, i == app->selection, visible)) {
            app->selection = i;
            app->confirmerSuppression = 0;
        }
    }
    SDL_RenderSetClipRect(g->rendu, NULL);
    if (maxDefil > 0) {
        float hauteur = lh * lh / (lh + maxDefil);
        rectArrondi(g, lx + lw + 6, ly + (lh - hauteur) * app->defilement / maxDefil, 5, hauteur, 2.5f,
                    rgba(200, 190, 255, 90));
    }

    /* navigation clavier */
    if (guiTouche(g, SDLK_DOWN)) app->selection = (app->selection + 2) % d->taille;
    if (guiTouche(g, SDLK_UP)) app->selection = (app->selection - 2 + d->taille * 2) % d->taille;
    if (guiTouche(g, SDLK_RIGHT)) app->selection = (app->selection + 1) % d->taille;
    if (guiTouche(g, SDLK_LEFT)) app->selection = (app->selection - 1 + d->taille) % d->taille;

    ficheHeros(app, &d->elements[app->selection]);
    dessinerMessage(g);
}

/* ------------------------------------------------------------------ */
/*  Préparer le combat (mode, taille des équipes, héros)              */
/* ------------------------------------------------------------------ */

/* Couleurs des deux joueurs en duel. */
#define COUL_J1 rgba(90, 160, 255, 255)
#define COUL_J2 rgba(255, 100, 110, 255)

/** Position de i dans l'équipe, -1 s'il n'y est pas. */
static int positionDans(const Preparation *prep, int equipe, int i)
{
    for (int k = 0; k < prep->nb[equipe]; k++) {
        if (prep->choisis[equipe][k] == i) return k;
    }
    return -1;
}

int dansEquipePrep(const Preparation *prep, int equipe, int i)
{
    return positionDans(prep, equipe, i) >= 0;
}

static void retirerDeEquipe(Preparation *prep, int equipe, int position)
{
    for (int k = position; k < prep->nb[equipe] - 1; k++) {
        prep->choisis[equipe][k] = prep->choisis[equipe][k + 1];
    }
    prep->nb[equipe]--;
}

/** Complète une équipe avec les premiers héros disponibles. */
static void completerEquipe(App *app, int equipe)
{
    Preparation *prep = &app->prep;
    Donjon *d = &app->donjon;
    int duel = prep->mode == MODE_DUEL;
    /* en duel, l'équipe 2 commence après les héros de l'équipe 1 */
    int depart = (duel && equipe == 1) ? prep->taille : 0;
    for (int k = 0; k < d->taille && prep->nb[equipe] < prep->taille; k++) {
        int i = (depart + k) % d->taille;
        if (positionDans(prep, equipe, i) < 0 && (duel || estVivant(&d->elements[i]))) {
            prep->choisis[equipe][prep->nb[equipe]++] = i;
        }
    }
}

/** Un emplacement d'équipe (héros choisi ou case vide). Retourne 1 si cliqué. */
static int emplacement(App *app, int equipe, int k, float x, float y, float w, float h, SDL_Color c)
{
    Gui *g = &app->g;
    Preparation *prep = &app->prep;
    float t = g->temps / 1000.0f;
    panneau(g, x, y, w, h);
    char num[12];
    snprintf(num, sizeof num, "%d", k + 1);
    texte(g, POLICE_GRANDE, num, x + 12, y + 6, avecAlpha(c, 60), ALIGN_GAUCHE);
    if (k >= prep->nb[equipe]) {
        texte(g, POLICE_PETITE, "Libre", x + w / 2, y + h / 2 - 8, COUL_TEXTE_DOUX, ALIGN_CENTRE);
        return 0;
    }
    Personnage *p = &app->donjon.elements[prep->choisis[equipe][k]];
    float sol = y + h - 46;
    halo(g, x + w / 2, sol - 60, w * 0.55f, avecAlpha(couleurElement(p->element), 50));
    ellipse(g, x + w / 2, sol + 2, w * 0.32f, 9, rgba(80, 68, 130, 255));
    Pose pose = poseDefaut(t + k + equipe * 2);
    pose.sens = equipe == 0 ? 1 : -1;
    float echelle = fminf((h - 64) / 185.0f, w < 150 ? 0.72f : 0.85f); /* le chapeau tient dans la case */
    dessinerHeros(g, p, x + w / 2, sol, echelle, &pose);
    texte(g, POLICE_NORMALE, p->nom, x + w / 2, sol + 8, COUL_TEXTE, ALIGN_CENTRE);
    char info[48];
    snprintf(info, sizeof info, "Niv %d %s", p->niveau, libelleClasse(p->classe));
    texte(g, POLICE_PETITE, info, x + w / 2, sol + 27, couleurClasse(p->classe), ALIGN_CENTRE);
    return zoneClic(g, x, y, w, h);
}

/** Panneau de choix de l'étage (mode bots). */
static void panneauEtage(App *app, float ex, float ey, float ew, float eh)
{
    Gui *g = &app->g;
    Preparation *prep = &app->prep;
    float t = g->temps / 1000.0f;
    panneau(g, ex, ey, ew, eh);
    texte(g, POLICE_MOYENNE, "Étage", ex + 24, ey + 16, COUL_OR, ALIGN_GAUCHE);
    char etage[32];
    snprintf(etage, sizeof etage, "Étage %d / %d", prep->etage, ETAGE_MAX);
    prep->etage += selecteur(g, ex + 160, ey + 14, 240, etage);
    if (prep->etage < 1) prep->etage = 1;
    if (prep->etage > app->prog.etageMax) prep->etage = app->prog.etageMax;

    const InfoEtage *info = combatEtage(prep->etage);
    for (int k = 0; k < ETAGE_MAX; k++) {
        int bossEtage = combatEtage(k + 1)->boss != NB_MONSTRES;
        SDL_Color c = k < prep->etage ? COUL_OR : (k < app->prog.etageMax ? rgba(140, 120, 180, 255) : rgba(70, 60, 100, 255));
        losange(g, ex + 32 + k * 22, ey + 62, bossEtage ? 9 : 6, bossEtage ? 11 : 8, c);
    }
    texteOmbre(g, POLICE_NORMALE, info->nom, ex + 24, ey + 78, COUL_OR_CLAIR, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, info->description, ex + 24, ey + 102, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

    char ligneInfo[96];
    snprintf(ligneInfo, sizeof ligneInfo, "%d vagues de %d monstre%s  ·  niveau conseillé %d", info->nbVagues,
             prep->taille, prep->taille > 1 ? "s" : "", 1 + (prep->etage - 1) * 2);
    texte(g, POLICE_PETITE, ligneInfo, ex + 24, ey + 120, COUL_TEXTE, ALIGN_GAUCHE);
    if (info->boss != NB_MONSTRES) {
        snprintf(ligneInfo, sizeof ligneInfo, "BOSS : %s", nomMonstre(info->boss));
        texteOmbre(g, POLICE_PETITE, ligneInfo, ex + 24, ey + 138, COUL_DANGER, ALIGN_GAUCHE);
    } else if (app->prog.etageMax < ETAGE_MAX && prep->etage == app->prog.etageMax) {
        texte(g, POLICE_PETITE, "Terminez cet étage pour débloquer le suivant.", ex + 24, ey + 138,
              COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    }

    /* aperçu des monstres (boss en dernier, encadré) */
    int k = 0;
    for (; k < info->taillePool; k++) {
        portraitMonstre(g, info->pool[k], ex + 44 + k * 50, ey + eh - 32, 20, t);
    }
    if (info->boss != NB_MONSTRES) {
        float bx = ex + 44 + k * 50 + 14;
        halo(g, bx, ey + eh - 32, 40, rgba(255, 80, 80, 50));
        portraitMonstre(g, info->boss, bx, ey + eh - 32, 24, t);
    }
}

void ecranPreparation(App *app)
{
    Gui *g = &app->g;
    Donjon *d = &app->donjon;
    Preparation *prep = &app->prep;
    dessinerFond(g, rgba(14, 12, 34, 255), rgba(40, 18, 40, 255));
    int duel = prep->mode == MODE_DUEL;

    if (enTete(app, "Préparer le combat", duel ? "Duel local : deux joueurs sur le même PC, chacun son tour"
                                               : "Votre équipe contre des monstres pilotés par l'ordinateur")) {
        changerEcran(app, ECRAN_MENU);
    }

    /* mode et taille des équipes */
    if (bouton(g, 40, 100, 230, 40, "Contre les bots", duel ? BOUTON_DISCRET : BOUTON_PRINCIPAL, 1)) {
        prep->mode = MODE_BOTS;
    }
    if (bouton(g, 282, 100, 230, 40, "Duel local (J1 contre J2)", duel ? BOUTON_PRINCIPAL : BOUTON_DISCRET, 1)) {
        prep->mode = MODE_DUEL;
        prep->equipeEditee = 0;
    }
    texte(g, POLICE_NORMALE, "Taille des équipes", 560, 110, COUL_OR, ALIGN_GAUCHE);
    char taille[32];
    snprintf(taille, sizeof taille, "%d contre %d", prep->taille, prep->taille);
    int dt = selecteur(g, 730, 105, 200, taille);
    if (guiTouche(g, SDLK_UP)) dt = 1;
    if (guiTouche(g, SDLK_DOWN)) dt = -1;
    prep->taille += dt;
    if (prep->taille < 1) prep->taille = 1;
    if (prep->taille > COMBAT_MAX_ALLIES) prep->taille = COMBAT_MAX_ALLIES;
    duel = prep->mode == MODE_DUEL;

    /* les équipes ne gardent que des héros existants (vivants hors duel) */
    for (int e = 0; e < 2; e++) {
        for (int k = prep->nb[e] - 1; k >= 0; k--) {
            int i = prep->choisis[e][k];
            if (i >= d->taille || k >= prep->taille || (!duel && e == 0 && !estVivant(&d->elements[i]))) {
                retirerDeEquipe(prep, e, k);
            }
        }
    }
    static int dejaPropose = 0;
    if (!dejaPropose) {
        dejaPropose = 1;
        completerEquipe(app, 0);
    }
    if (duel && prep->nb[1] == 0) {
        completerEquipe(app, 1);
    }

    int pret;
    if (!duel) {
        /* équipe + étage */
        const float w = 175, gap = 15;
        for (int k = 0; k < prep->taille; k++) {
            if (emplacement(app, 0, k, 40 + k * (w + gap), 156, w, 220, COUL_OR)) {
                retirerDeEquipe(prep, 0, k);
            }
        }
        panneauEtage(app, 820, 156, 430, 220);
        pret = prep->nb[0] == prep->taille;
        if (bouton(g, 820, 388, 430, 46, pret ? "Lancer le combat !" : "Équipe incomplète",
                   BOUTON_PRINCIPAL, pret) || (pret && guiTouche(g, SDLK_RETURN))) {
            combatGuiDetruire(app->combat);
            app->combat = combatGuiCreer(app);
            if (app->combat != NULL) changerEcran(app, ECRAN_COMBAT);
        }
    } else {
        /* deux équipes face à face */
        for (int e = 0; e < 2; e++) {
            float bx = e == 0 ? 40 : 660, bw = 580;
            SDL_Color c = e == 0 ? COUL_J1 : COUL_J2;
            int editee = prep->equipeEditee == e;
            char titre[48];
            snprintf(titre, sizeof titre, "Joueur %d  (%d/%d)", e + 1, prep->nb[e], prep->taille);
            if (zoneClic(g, bx, 150, bw, 30)) prep->equipeEditee = e;
            rectArrondi(g, bx, 150, bw, 30, 15, avecAlpha(c, editee ? 90 : 30));
            if (editee) contourArrondi(g, bx, 150, bw, 30, 15, 2, c);
            texte(g, POLICE_NORMALE, titre, bx + 16, 155, COUL_TEXTE, ALIGN_GAUCHE);
            texte(g, POLICE_PETITE, editee ? "les clics ajoutent ici" : "cliquer pour éditer", bx + bw - 16, 158,
                  COUL_TEXTE_DOUX, ALIGN_DROITE);
            float gap = 10, w = (bw - gap * (prep->taille - 1)) / prep->taille;
            if (w > 175) w = 175;
            float x0 = bx + (bw - (w * prep->taille + gap * (prep->taille - 1))) / 2;
            for (int k = 0; k < prep->taille; k++) {
                if (emplacement(app, e, k, x0 + k * (w + gap), 188, w, 190, c)) {
                    retirerDeEquipe(prep, e, k);
                    prep->equipeEditee = e;
                }
            }
        }
        texteOmbre(g, POLICE_GRANDE, "VS", LARGEUR / 2.0f, 262, COUL_OR_CLAIR, ALIGN_CENTRE);
        pret = prep->nb[0] == prep->taille && prep->nb[1] == prep->taille;
        texte(g, POLICE_PETITE, "Duel amical : PV complets, sans XP ni K.O. durable. Un même héros peut jouer des deux côtés.",
              LARGEUR / 2.0f, 386, COUL_TEXTE_DOUX, ALIGN_CENTRE);
        if (bouton(g, LARGEUR / 2.0f - 200, 404, 400, 40, pret ? "Lancer le duel !" : "Équipes incomplètes",
                   BOUTON_PRINCIPAL, pret) || (pret && guiTouche(g, SDLK_RETURN))) {
            combatGuiDetruire(app->combat);
            app->combat = combatGuiCreer(app);
            if (app->combat != NULL) changerEcran(app, ECRAN_COMBAT);
        }
    }

    /* héros disponibles */
    texte(g, POLICE_MOYENNE, "Héros disponibles", 40, 452, COUL_OR, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, duel ? "Cliquez pour ajouter à l'équipe éditée (ou retirer)."
                                 : "Les héros K.O. doivent se reposer (Mes héros) ou boire une potion.",
          260, 458, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    /* liste défilante (4 colonnes) : tous les héros sont accessibles */
    const float cw = 290, ch = 84, gap = 12;
    const float lx = 40, ly = 486, lw = 4 * cw + 3 * 14, lh = HAUTEUR - ly - 8;
    int lignes = (d->taille + 3) / 4;
    int maxDefil = (int)(lignes * (ch + gap) - gap - lh);
    if (maxDefil < 0) maxDefil = 0;
    if (survol(g, lx, ly, lw, lh)) {
        prep->defilement -= g->molette * 40;
    }
    if (prep->defilement < 0) prep->defilement = 0;
    if (prep->defilement > maxDefil) prep->defilement = maxDefil;
    int sourisDansListe = g->sourisY >= ly && g->sourisY < ly + lh;

    SDL_Rect clip = { (int)lx - 4, (int)ly - 4, (int)lw + 8, (int)lh + 8 };
    SDL_RenderSetClipRect(g->rendu, &clip);
    for (int i = 0; i < d->taille; i++) {
        float x = lx + (i % 4) * (cw + 14), y = ly + (i / 4) * (ch + gap) - prep->defilement;
        if (y + ch < ly || y > ly + lh) continue; /* hors de la zone visible */
        Personnage *p = &d->elements[i];
        int e = duel ? prep->equipeEditee : 0;
        int pos = positionDans(prep, e, i);
        int choisi = pos >= 0 || (duel && dansEquipePrep(prep, 1 - e, i));
        if (carteHeros(g, p, x, y, cw, ch, choisi, sourisDansListe && (duel || estVivant(p)))) {
            if (pos >= 0) {
                retirerDeEquipe(prep, e, pos);
            } else if (prep->nb[e] < prep->taille) {
                prep->choisis[e][prep->nb[e]++] = i;
                if (duel && prep->nb[e] == prep->taille && prep->nb[1 - e] < prep->taille) {
                    prep->equipeEditee = 1 - e; /* équipe pleine : on passe à l'autre */
                }
            }
        }
        /* pastilles J1 / J2 */
        if (duel) {
            for (int k = 0; k < 2; k++) {
                if (dansEquipePrep(prep, k, i)) {
                    float px = x + cw - 70 + k * 22;
                    cercle(g, px, y + 20, 9, k == 0 ? COUL_J1 : COUL_J2);
                    texte(g, POLICE_PETITE, k == 0 ? "1" : "2", px, y + 12, COUL_TEXTE, ALIGN_CENTRE);
                }
            }
        }
    }
    SDL_RenderSetClipRect(g->rendu, NULL);
    if (maxDefil > 0) {
        float hauteur = lh * lh / (lh + maxDefil);
        rectArrondi(g, lx + lw + 8, ly + (lh - hauteur) * prep->defilement / maxDefil, 5, hauteur, 2.5f,
                    rgba(200, 190, 255, 90));
    }
    if (maxDefil > 0) {
        texte(g, POLICE_PETITE, "(molette pour voir tous les héros)", 1240, 458, COUL_TEXTE_DOUX, ALIGN_DROITE);
    }
}
