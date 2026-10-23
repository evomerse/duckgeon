/**
 * @file gui_bestiaire.c
 * @brief Écran « Bestiaire » : fiche de chaque monstre (statistiques à
 *        l'étage choisi, attaques, faiblesses) et table des éléments.
 *
 * Toutes les données viennent du moteur de combat (infoMonstre,
 * statsMonstre, elementsForts...) : l'écran ne fait que les afficher.
 */
#include <math.h>
#include <stdio.h>

#include "gui_app.h"
#include "gui_sprite.h"

#define PI_F 3.14159265f

/** Niveau moyen supposé de l'équipe à un étage (pour les stats affichées). */
static int niveauTypique(int etage)
{
    return 1 + (etage - 1) * 3 / 2;
}

/** Premier étage (>= depart) où le monstre apparaît, sinon le premier. */
static int etageSuivant(enum Monstre m, int depart, int sens)
{
    unsigned masque = etagesMonstre(m);
    for (int k = 1; k <= ETAGE_MAX; k++) {
        int e = ((depart - 1 + sens * k) % ETAGE_MAX + ETAGE_MAX) % ETAGE_MAX + 1;
        if (masque & (1u << (e - 1))) {
            return e;
        }
    }
    return 1;
}

/** Ligne de la liste des monstres ; retourne 1 si elle est cliquée. */
static int ligneMonstre(Gui *g, enum Monstre m, float x, float y, float w, float h, int choisi, float t)
{
    const InfoMonstre *info = infoMonstre(m);
    int dessus = survol(g, x, y, w, h);
    rectArrondi(g, x, y, w, h, 10, choisi ? rgba(70, 56, 120, 240) : (dessus ? rgba(46, 40, 86, 240) : rgba(28, 24, 54, 200)));
    if (choisi) {
        contourArrondi(g, x, y, w, h, 10, 2, COUL_OR_CLAIR);
    }
    portraitMonstre(g, m, x + 24, y + h / 2, 17, t);
    texte(g, POLICE_NORMALE, info->nom, x + 50, y + h / 2 - 11, COUL_TEXTE, ALIGN_GAUCHE);
    iconeElement(g, info->element, x + w - 18, y + h / 2, 8);
    if (info->boss) {
        texte(g, POLICE_PETITE, "BOSS", x + w - 34, y + h / 2 - 9, COUL_OR_CLAIR, ALIGN_DROITE);
    }
    if (dessus) {
        g->survolCliquable = 1;
    }
    return dessus && g->clic;
}

/** Libellé + valeur d'une statistique. */
static void stat(Gui *g, const char *libelle, int valeur, float x, float y)
{
    char buf[16];
    snprintf(buf, sizeof buf, "%d", valeur);
    texte(g, POLICE_PETITE, libelle, x, y + 3, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    texte(g, POLICE_NORMALE, buf, x + 64, y, COUL_TEXTE, ALIGN_GAUCHE);
}

static void ongletMonstres(App *app)
{
    Gui *g = &app->g;
    float t = g->temps / 1000.0f;
    enum Monstre m = (enum Monstre)app->monstreBestiaire;
    const InfoMonstre *info = infoMonstre(m);

    /* liste */
    panneau(g, 30, 100, 300, 600);
    for (int i = 0; i < NB_MONSTRES; i++) {
        if (ligneMonstre(g, (enum Monstre)i, 42, 112 + i * 48, 276, 42, i == app->monstreBestiaire, t + i)) {
            app->monstreBestiaire = i;
            app->etageBestiaire = 0;
        }
    }
    if (guiTouche(g, SDLK_DOWN)) {
        app->monstreBestiaire = (app->monstreBestiaire + 1) % NB_MONSTRES;
        app->etageBestiaire = 0;
    }
    if (guiTouche(g, SDLK_UP)) {
        app->monstreBestiaire = (app->monstreBestiaire + NB_MONSTRES - 1) % NB_MONSTRES;
        app->etageBestiaire = 0;
    }
    m = (enum Monstre)app->monstreBestiaire;
    info = infoMonstre(m);
    unsigned etages = etagesMonstre(m);
    if (app->etageBestiaire < 1 || !(etages & (1u << (app->etageBestiaire - 1)))) {
        app->etageBestiaire = etageSuivant(m, ETAGE_MAX, 1); /* premier étage */
    }

    /* scène */
    panneau(g, 350, 100, 900, 600);
    SDL_Color ce = couleurElement(info->element);
    float sx = 520, sy = 520;
    halo(g, sx, sy - 120, 220, avecAlpha(ce, 55));
    ellipse(g, sx, sy + 6, 150, 26, rgba(30, 24, 56, 255));
    ellipse(g, sx, sy, 140, 20, rgba(76, 64, 124, 255));
    float s = 250.0f / hauteurMonstre(m);
    if (s > 1.7f) s = 1.7f;
    Pose pose = poseDefaut(t);
    pose.sens = -1;
    dessinerMonstre(g, m, sx, sy, s, &pose);

    /* étage choisi */
    char buf[192];
    snprintf(buf, sizeof buf, "Étage %d", app->etageBestiaire);
    int d = selecteur(g, 400, 580, 240, buf);
    if (d != 0) {
        app->etageBestiaire = etageSuivant(m, app->etageBestiaire, d);
    }
    texte(g, POLICE_PETITE, combatEtage(app->etageBestiaire)->nom, 520, 628, COUL_TEXTE_DOUX, ALIGN_CENTRE);
    snprintf(buf, sizeof buf, "Stats pour 4 héros de niveau %d", niveauTypique(app->etageBestiaire));
    texte(g, POLICE_PETITE, buf, 520, 650, COUL_TEXTE_DOUX, ALIGN_CENTRE);

    /* fiche */
    float x = 700, y = 116, w = 530;
    texteOmbre(g, POLICE_GRANDE, info->nom, x, y, info->boss ? COUL_OR_CLAIR : COUL_TEXTE, ALIGN_GAUCHE);
    iconeElement(g, info->element, x + 9, y + 60, 9);
    snprintf(buf, sizeof buf, "%s%s", nomElement(info->element), info->boss ? "  ·  Boss" : "");
    texte(g, POLICE_NORMALE, buf, x + 26, y + 51, ce, ALIGN_GAUCHE);
    y += 82;
    y += texteBloc(g, POLICE_PETITE, info->description, x, y, (int)w, COUL_TEXTE_DOUX) + 12;

    Personnage stats;
    int garde;
    statsMonstre(m, app->etageBestiaire, niveauTypique(app->etageBestiaire), 4, &stats, &garde);
    stat(g, "PV", stats.pvMax, x, y);
    stat(g, "Attaque", stats.atk, x + 140, y);
    stat(g, "Défense", stats.def, x + 280, y);
    stat(g, "Vitesse", stats.vit, x, y + 26);
    stat(g, "Garde", garde, x + 140, y + 26);
    y += 64;

    texte(g, POLICE_MOYENNE, "Éléments", x, y, COUL_OR, ALIGN_GAUCHE);
    y += 28;
    afficherAffinites(g, info->element, x, y, 0);
    y += 24;
    texte(g, POLICE_PETITE, "Garde brisée par", x, y, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    float fx = x + 118;
    unsigned f = faiblessesMonstre(m);
    for (int e = 0; e < NB_ELEMENTS; e++) {
        if (f & (1u << e)) {
            iconeElement(g, (enum Element)e, fx + 8, y + 9, 7);
            fx += 28 + texte(g, POLICE_PETITE, nomElement((enum Element)e), fx + 19, y, COUL_TEXTE, ALIGN_GAUCHE);
        }
    }
    y += 34;

    texte(g, POLICE_MOYENNE, "Attaques", x, y, COUL_OR, ALIGN_GAUCHE);
    y += 28;
    snprintf(buf, sizeof buf, "%s : %d %% ATK sur un héros.", info->attaque, info->boss ? 130 : 100);
    y += texteBloc(g, POLICE_PETITE, buf, x, y, (int)w, COUL_TEXTE) + 4;
    snprintf(buf, sizeof buf, "%s (%d %% des tours) : %s", info->special, info->chanceSpecial, info->effetSpecial);
    y += texteBloc(g, POLICE_PETITE, buf, x, y, (int)w, COUL_TEXTE) + 4;
    if (info->boss) {
        y += texteBloc(g, POLICE_PETITE,
                       "Phase 2 sous 50 % PV : ATK +30 %, garde restaurée, attaque spéciale plus fréquente (+20 %).",
                       x, y, (int)w, COUL_DANGER) + 4;
    }
    y += 10;

    /* étages d'apparition */
    int pos = snprintf(buf, sizeof buf, info->boss ? "Garde l'étage" : "Apparaît aux étages");
    for (int e = 1; e <= ETAGE_MAX && pos < (int)sizeof buf; e++) {
        if (etages & (1u << (e - 1))) {
            pos += snprintf(buf + pos, sizeof buf - (size_t)pos, " %d", e);
        }
    }
    texte(g, POLICE_PETITE, buf, x, y, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
}

/** Flèche de (x1, y1) vers (x2, y2), pointe à l'arrivée. */
static void fleche(Gui *g, float x1, float y1, float x2, float y2, SDL_Color c)
{
    float dx = x2 - x1, dy = y2 - y1;
    float l = sqrtf(dx * dx + dy * dy);
    if (l < 1) return;
    dx /= l;
    dy /= l;
    ligne(g, x1, y1, x2 - dx * 8, y2 - dy * 8, 2, c);
    float xy[6] = { x2, y2, x2 - dx * 12 - dy * 6, y2 - dy * 12 + dx * 6, x2 - dx * 12 + dy * 6, y2 - dy * 12 - dx * 6 };
    polygone(g, xy, 3, c);
}

static void ongletElements(App *app)
{
    Gui *g = &app->g;

    /* table */
    panneau(g, 30, 100, 760, 600);
    texte(g, POLICE_PETITE, "Fort contre : +25 % de dégâts et la garde de la cible baisse.", 50, 114, COUL_VIE, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, "Faible contre : -20 % de dégâts.", 50, 134, COUL_DANGER, ALIGN_GAUCHE);
    for (int e = 0; e < NB_ELEMENTS; e++) {
        float y = 166 + e * 76;
        int choisi = app->elementBestiaire == e;
        if (zoneClic(g, 42, y, 736, 70)) {
            app->elementBestiaire = e;
        }
        rectArrondi(g, 42, y, 736, 70, 10, choisi ? rgba(60, 50, 110, 230) : rgba(28, 24, 54, 200));
        if (choisi) contourArrondi(g, 42, y, 736, 70, 10, 2, couleurElement((enum Element)e));
        iconeElement(g, (enum Element)e, 70, y + 24, 12);
        texte(g, POLICE_MOYENNE, nomElement((enum Element)e), 92, y + 10, couleurElement((enum Element)e), ALIGN_GAUCHE);
        char buf[128];
        snprintf(buf, sizeof buf, "%s : %s", nomEffetElement((enum Element)e), effetElement((enum Element)e));
        texte(g, POLICE_PETITE, buf, 60, y + 44, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
        afficherAffinites(g, (enum Element)e, 250, y + 14, 0);
    }

    /* cycle : chaque élément bat les deux suivants */
    panneau(g, 810, 100, 440, 600);
    texte(g, POLICE_MOYENNE, "Le cycle des éléments", 1030, 116, COUL_OR, ALIGN_CENTRE);
    texte(g, POLICE_PETITE, "Une flèche va de l'élément fort vers l'élément faible.", 1030, 148,
          COUL_TEXTE_DOUX, ALIGN_CENTRE);
    float cx = 1030, cy = 420, r = 170;
    float px[NB_ELEMENTS], py[NB_ELEMENTS];
    /* chaque élément sur le cercle, dans l'ordre du cycle */
    enum Element ordre[NB_ELEMENTS];
    for (int k = 0; k < NB_ELEMENTS; k++) {
        enum Element e = elementCycle(k);
        float a = -PI_F / 2 + k * 2 * PI_F / NB_ELEMENTS;
        ordre[k] = e;
        px[e] = cx + cosf(a) * r;
        py[e] = cy + sinf(a) * r;
    }
    for (int k = 0; k < NB_ELEMENTS; k++) {
        enum Element e = ordre[k];
        int actif = app->elementBestiaire == (int)e;
        for (int j = 0; j < NB_ELEMENTS; j++) {
            if (elementsForts(e) & (1u << j)) {
                float dx = px[j] - px[e], dy = py[j] - py[e];
                float l = sqrtf(dx * dx + dy * dy);
                SDL_Color c = actif ? COUL_VIE : (app->elementBestiaire == j ? COUL_DANGER : rgba(150, 140, 200, 70));
                fleche(g, px[e] + dx / l * 30, py[e] + dy / l * 30, px[j] - dx / l * 30, py[j] - dy / l * 30, c);
            }
        }
    }
    for (int e = 0; e < NB_ELEMENTS; e++) {
        int actif = app->elementBestiaire == e;
        if (actif) halo(g, px[e], py[e], 50, avecAlpha(couleurElement((enum Element)e), 90));
        iconeElement(g, (enum Element)e, px[e], py[e], actif ? 22 : 18);
        texteOmbre(g, POLICE_PETITE, nomElement((enum Element)e), px[e], py[e] + 26, COUL_TEXTE, ALIGN_CENTRE);
        if (zoneClic(g, px[e] - 24, py[e] - 24, 48, 48)) {
            app->elementBestiaire = e;
        }
    }
    texte(g, POLICE_PETITE, "Vert : ce que l'élément choisi bat. Rouge : ce qui le bat.", 1030, 662,
          COUL_TEXTE_DOUX, ALIGN_CENTRE);
}

void ecranBestiaire(App *app)
{
    Gui *g = &app->g;
    dessinerFond(g, rgba(14, 18, 34, 255), rgba(30, 26, 50, 255));
    if (enTete(app, "Bestiaire", "Les monstres du donjon et la table des éléments")) {
        changerEcran(app, ECRAN_MENU);
    }
    if (bouton(g, 860, 30, 180, 44, "Monstres", app->ongletBestiaire == 0 ? BOUTON_PRINCIPAL : BOUTON_NORMAL, 1)) {
        app->ongletBestiaire = 0;
    }
    if (bouton(g, 1050, 30, 200, 44, "Table des éléments",
               app->ongletBestiaire == 1 ? BOUTON_PRINCIPAL : BOUTON_NORMAL, 1)) {
        app->ongletBestiaire = 1;
    }
    if (guiTouche(g, SDLK_TAB)) {
        app->ongletBestiaire = !app->ongletBestiaire;
    }
    if (app->ongletBestiaire == 0) {
        ongletMonstres(app);
    } else {
        ongletElements(app);
    }
}
