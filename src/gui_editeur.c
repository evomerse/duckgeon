/**
 * @file gui_editeur.c
 * @brief Éditeur de héros : identité (nom, classe, élément), apparence
 *        (13 options) et répartition des points de statistiques.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gui_app.h"
#include "gui_sprite.h"
#include "boutique.h"

/* Aperçu : sens du regard et animation d'attaque. */
static int sensApercu = 1;
static float attaqueApercu = 0;

void ouvrirEditeur(App *app, int index)
{
    Editeur *ed = &app->editeur;
    memset(ed, 0, sizeof *ed);
    ed->index = index;
    if (index < 0) {
        initialiserStats(&ed->brouillon, (enum Classe)(SDL_GetTicks() % NB_CLASSES));
        ed->onglet = ONGLET_IDENTITE;
        ed->saisieNom = 1;
        SDL_StartTextInput();
    } else {
        ed->brouillon = app->donjon.elements[index];
        ed->onglet = ONGLET_APPARENCE;
    }
    changerEcran(app, ECRAN_EDITEUR);
}

void ouvrirAmelioration(App *app, int index)
{
    ouvrirEditeur(app, index);
    app->editeur.onglet = ONGLET_STATS;
}

/** Statistique finale du brouillon en tenant compte des points bonus. */
static int statFinale(const Editeur *ed, int quelle)
{
    const Personnage *p = &ed->brouillon;
    switch (quelle) {
    case 0: return p->pvMax + ed->points[0] * BONUS_PV_POINT;
    case 1: return p->atk + ed->points[1] * BONUS_ATK_POINT;
    case 2: return p->def + ed->points[2] * BONUS_DEF_POINT;
    default: return p->vit + ed->points[3] * BONUS_VIT_POINT;
    }
}

/** Points disponibles : les points bonus de création pour un nouveau
 *  héros, ses points gagnés au combat (pointsStats) sinon. */
static int pointsTotal(const Editeur *ed)
{
    return ed->index < 0 ? POINTS_BONUS : ed->brouillon.pointsStats;
}

static int pointsRestants(const Editeur *ed)
{
    return pointsTotal(ed) - ed->points[0] - ed->points[1] - ed->points[2] - ed->points[3];
}

/** Change la classe du brouillon en gardant nom et apparence. */
static void changerClasse(Editeur *ed, enum Classe c)
{
    Apparence a = ed->brouillon.apparence;
    initialiserStats(&ed->brouillon, c);
    ed->brouillon.apparence = a;
}

/** Enregistre le héros dans le donjon et revient à la liste. */
static void valider(App *app)
{
    Editeur *ed = &app->editeur;
    Personnage *b = &ed->brouillon;
    if (b->nom[0] == '\0') {
        snprintf(b->nom, NOM_MAX, "Héros sans nom");
    }
    if (ed->index < 0) {
        b->pvMax = statFinale(ed, 0);
        b->atk = statFinale(ed, 1);
        b->def = statFinale(ed, 2);
        b->vit = statFinale(ed, 3);
        b->pv = b->pvMax;
        Personnage *p = donjonNouvelleCase(&app->donjon);
        if (p == NULL) {
            return; /* mémoire insuffisante : on reste dans l'éditeur */
        }
        *p = *b;
        app->selection = app->donjon.taille;
        app->donjon.taille++;
    } else {
        /* points gagnés au combat : dépensés avec la même fonction que la
         * console (ameliorerStat) */
        for (int i = 0; i < NB_STATS; i++) {
            for (int k = 0; k < ed->points[i]; k++) {
                ameliorerStat(b, (enum Stat)i);
            }
        }
        app->donjon.elements[ed->index] = *b;
        app->selection = ed->index;
    }
    SDL_StopTextInput();
    changerEcran(app, ECRAN_HEROS);
}

/* ------------------------------------------------------------------ */
/*  Onglet Identité                                                   */
/* ------------------------------------------------------------------ */

/** Supprime le dernier caractère UTF-8 d'une chaîne. */
static void effacerCaractere(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && ((unsigned char)s[n - 1] & 0xC0) == 0x80) {
        n--; /* octets de continuation */
    }
    if (n > 0) {
        n--;
    }
    s[n] = '\0';
}

static void ongletIdentite(App *app, float x, float y)
{
    Gui *g = &app->g;
    Editeur *ed = &app->editeur;
    Personnage *b = &ed->brouillon;

    /* champ du nom */
    texte(g, POLICE_NORMALE, "Nom du héros", x, y, COUL_OR, ALIGN_GAUCHE);
    float fx = x, fy = y + 26, fw = 420, fh = 42;
    if (g->clic) {
        int avant = ed->saisieNom;
        ed->saisieNom = survol(g, fx, fy, fw, fh);
        if (ed->saisieNom && !avant) SDL_StartTextInput();
        if (!ed->saisieNom && avant) SDL_StopTextInput();
    }
    if (survol(g, fx, fy, fw, fh)) g->survolCliquable = 1;
    rectArrondi(g, fx, fy, fw, fh, 10, rgba(12, 10, 28, 240));
    contourArrondi(g, fx, fy, fw, fh, 10, ed->saisieNom ? 2 : 1.2f,
                   ed->saisieNom ? COUL_ACCENT : COUL_BORDURE);
    if (ed->saisieNom) {
        if (g->saisie[0] != '\0' && strlen(b->nom) + strlen(g->saisie) < NOM_MAX) {
            strcat(b->nom, g->saisie);
        }
        if (guiTouche(g, SDLK_BACKSPACE)) {
            effacerCaractere(b->nom);
        }
    }
    int largeurNom = 0;
    if (b->nom[0] != '\0') {
        largeurNom = texte(g, POLICE_MOYENNE, b->nom, fx + 14, fy + 9, COUL_TEXTE, ALIGN_GAUCHE);
    } else if (!ed->saisieNom) {
        texte(g, POLICE_NORMALE, "Cliquez pour saisir un nom", fx + 14, fy + 12, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    }
    if (ed->saisieNom && (g->temps / 500) % 2 == 0) {
        remplirRect(g, fx + 16 + largeurNom, fy + 9, 2, 24, COUL_ACCENT);
    }
    char compteur[16];
    snprintf(compteur, sizeof compteur, "%d/%d", (int)strlen(b->nom), NOM_MAX - 1);
    texte(g, POLICE_PETITE, compteur, fx + fw + 10, fy + 14, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

    /* classes */
    float cy = y + 90;
    texte(g, POLICE_NORMALE, "Classe", x, cy, COUL_OR, ALIGN_GAUCHE);
    int creation = ed->index < 0;
    if (!creation) {
        texte(g, POLICE_PETITE, "(fixée après la création)", x + 70, cy + 3, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    }
    for (int c = 0; c < NB_CLASSES; c++) {
        float bx = x + (c % 4) * 186, by = cy + 26 + (c / 4) * 64, bw = 176, bh = 56;
        int choisie = (b->classe == (enum Classe)c);
        int dessus = creation && survol(g, bx, by, bw, bh);
        SDL_Color cc = couleurClasse((enum Classe)c);
        rectArrondi(g, bx, by, bw, bh, 10, choisie ? avecAlpha(cc, 70) : (dessus ? rgba(50, 44, 90, 240) : rgba(28, 24, 54, 230)));
        contourArrondi(g, bx, by, bw, bh, 10, choisie ? 2.2f : 1, choisie ? cc : COUL_BORDURE);
        iconeElement(g, elementClasse((enum Classe)c), bx + 22, by + bh / 2, 11);
        texte(g, POLICE_NORMALE, libelleClasse((enum Classe)c), bx + 42, by + 8,
              creation || choisie ? COUL_TEXTE : COUL_TEXTE_DOUX, ALIGN_GAUCHE);
        static const char *const ROLES[NB_CLASSES] = {
            "Dégâts de zone", "Magie de zone", "Critiques, poison", "Tank, boucliers",
            "Rapide, perforant", "Soigneur", "Vol de vie, malus", "Soutien, vitesse",
        };
        texte(g, POLICE_PETITE, ROLES[c], bx + 42, by + 31, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
        if (dessus) {
            g->survolCliquable = 1;
            if (g->clic && !choisie) changerClasse(ed, (enum Classe)c);
        }
    }

    /* élément */
    float ey = cy + 166;
    texte(g, POLICE_NORMALE, "Élément", x, ey, COUL_OR, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, "Type de dégâts : vise les faiblesses ennemies pour briser leur garde.",
          x + 80, ey + 3, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    for (int e = 0; e < NB_ELEMENTS; e++) {
        float bx = x + e * 105, by = ey + 26, bw = 102, bh = 36;
        int choisi = (b->element == (enum Element)e);
        int dessus = survol(g, bx, by, bw, bh);
        SDL_Color ce = couleurElement((enum Element)e);
        rectArrondi(g, bx, by, bw, bh, bh / 2, choisi ? avecAlpha(ce, 80) : (dessus ? rgba(50, 44, 90, 240) : rgba(28, 24, 54, 230)));
        contourArrondi(g, bx, by, bw, bh, bh / 2, choisi ? 2 : 1, choisi ? ce : COUL_BORDURE);
        iconeElement(g, (enum Element)e, bx + 15, by + bh / 2, 9);
        texte(g, POLICE_PETITE, nomElement((enum Element)e), bx + 28, by + 10, COUL_TEXTE, ALIGN_GAUCHE);
        if (dessus) {
            g->survolCliquable = 1;
            if (g->clic) b->element = (enum Element)e;
        }
    }

    /* capacités de la classe */
    float ky = ey + 76;
    texte(g, POLICE_NORMALE, "Capacités", x, ky, COUL_OR, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, descriptionClasse(b->classe), x + 100, ky + 3, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    static const char *const TYPES[NB_ACTIONS] = { "Basique", "Compétence", "Ultime" };
    for (int a = 0; a < NB_ACTIONS; a++) {
        const Capacite *cap = capaciteClasse(b->classe, (enum TypeAction)a);
        float ly = ky + 24 + a * 36;
        texte(g, POLICE_PETITE, TYPES[a], x, ly + 2, couleurClasse(b->classe), ALIGN_GAUCHE);
        texte(g, POLICE_NORMALE, cap->nom, x + 92, ly, COUL_TEXTE, ALIGN_GAUCHE);
        texte(g, POLICE_PETITE, cap->description, x + 92, ly + 19, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    }
}

/* ------------------------------------------------------------------ */
/*  Onglet Apparence                                                  */
/* ------------------------------------------------------------------ */

/** Ligne « libellé + sélecteur » pour une option à n valeurs. */
static void optionListe(Gui *g, const char *libelle, unsigned char *valeur, int n,
                        const char *const *noms, float x, float y)
{
    texte(g, POLICE_PETITE, libelle, x, y, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    int d = selecteur(g, x, y + 20, 330, noms[*valeur % n]);
    *valeur = (unsigned char)((*valeur + n + d) % n);
}

/** Ligne « libellé + nuancier ». */
static void optionCouleur(Gui *g, const char *libelle, unsigned char *valeur,
                          const SDL_Color *couleurs, int n, float taille, float x, float y)
{
    texte(g, POLICE_PETITE, libelle, x, y, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    int c = nuancier(g, x + 4, y + 22, couleurs, n, *valeur, taille);
    if (c >= 0) *valeur = (unsigned char)c;
}

static void ongletApparence(App *app, float x, float y)
{
    Gui *g = &app->g;
    Apparence *a = &app->editeur.brouillon.apparence;
    const float pas = 66;
    float x2 = x + 380;

    /* plumage : les couleurs de la boutique sont verrouillées tant
     * qu'elles ne sont pas achetées */
    texte(g, POLICE_PETITE, "Plumage", x, y, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    const float taille = 24;
    int c0 = nuancier(g, x + 4, y + 22, PLUMAGES, NB_PLUMAGES, a->plumage, taille);
    for (int i = NB_PLUMAGES_BASE; i < NB_PLUMAGES; i++) {
        if (!optionDebloquee(&app->prog, COSMETIQUE_PLUMAGE, i)) {
            float cx0 = x + 4 + i * (taille + 5) + taille / 2, cy0 = y + 22 + taille / 2;
            cercle(g, cx0, cy0, taille / 2, rgba(10, 8, 20, 170));
            iconeCadenas(g, cx0, cy0, 6);
        }
    }
    if (c0 >= 0 && optionDebloquee(&app->prog, COSMETIQUE_PLUMAGE, c0)) a->plumage = (unsigned char)c0;
    optionListe(g, "Houppette", &a->coupe, NB_COUPES, NOMS_COUPES, x, y + pas);
    optionCouleur(g, "Couleur de la houppette", &a->couleurCheveux, PALETTE, NB_COULEURS, 14, x, y + 2 * pas);
    optionListe(g, "Forme des yeux", &a->regard, NB_REGARDS, NOMS_REGARDS, x, y + 3 * pas);
    optionCouleur(g, "Couleur des yeux", &a->couleurYeux, PALETTE, NB_COULEURS, 14, x, y + 4 * pas);
    optionListe(g, "Pilosité", &a->pilosite, NB_PILOSITES, NOMS_PILOSITES, x, y + 5 * pas);
    optionListe(g, "Corpulence", &a->corpulence, NB_CORPULENCES, NOMS_CORPULENCES, x, y + 6 * pas);

    optionCouleur(g, "Tenue principale", &a->tenue, PALETTE, NB_COULEURS, 14, x2, y);
    optionCouleur(g, "Tenue secondaire (bordures, cape)", &a->tenue2, PALETTE, NB_COULEURS, 14, x2, y + pas);
    /* coiffe : le sélecteur saute les coiffes pas encore achetées */
    texte(g, POLICE_PETITE, "Coiffe  (plus de choix à la boutique)", x2, y + 2 * pas, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    int d = selecteur(g, x2, y + 2 * pas + 20, 330, NOMS_COIFFES[a->coiffe % NB_COIFFES]);
    if (d != 0) {
        int v = a->coiffe;
        do {
            v = (v + NB_COIFFES + d) % NB_COIFFES;
        } while (!optionDebloquee(&app->prog, COSMETIQUE_COIFFE, v));
        a->coiffe = (unsigned char)v;
    }
    optionListe(g, "Marque", &a->marque, NB_MARQUES, NOMS_MARQUES, x2, y + 3 * pas);
    static const char *const OUI_NON[2] = { "Sans cape", "Avec cape" };
    optionListe(g, "Cape", &a->cape, 2, OUI_NON, x2, y + 4 * pas);

    /* aura : « aucune » puis la palette */
    texte(g, POLICE_PETITE, "Aura", x2, y + 5 * pas, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    SDL_Color auras[NB_COULEURS + 1];
    auras[0] = rgba(30, 28, 44, 255);
    for (int i = 0; i < NB_COULEURS; i++) auras[i + 1] = PALETTE[i];
    int c = nuancier(g, x2 + 4, y + 5 * pas + 22, auras, NB_COULEURS + 1, a->aura, 13);
    if (c >= 0) a->aura = (unsigned char)c;
    ligne(g, x2 + 6, y + 5 * pas + 33, x2 + 15, y + 5 * pas + 24, 1.5f, COUL_DANGER);

    texte(g, POLICE_PETITE, "Astuce : « Aléatoire » (à gauche) tire tout au hasard.",
          x2, y + 6 * pas + 24, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
}

/* ------------------------------------------------------------------ */
/*  Onglet Statistiques                                               */
/* ------------------------------------------------------------------ */

static void ongletStats(App *app, float x, float y)
{
    Gui *g = &app->g;
    Editeur *ed = &app->editeur;
    static const int MAX_AFFICHE[NB_STATS] = { 260, 40, 25, 160 };
    int creation = ed->index < 0;
    char buf[64];

    snprintf(buf, sizeof buf, "Points à répartir : %d / %d", pointsRestants(ed), pointsTotal(ed));
    texte(g, POLICE_MOYENNE, buf, x, y, pointsRestants(ed) > 0 ? COUL_OR_CLAIR : COUL_TEXTE, ALIGN_GAUCHE);
    if (creation) {
        texte(g, POLICE_PETITE, "Chaque point s'ajoute aux statistiques de base de la classe.",
              x, y + 30, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    } else if (pointsTotal(ed) == 0) {
        texte(g, POLICE_PETITE, "Gagnez des points en remportant des combats et en montant de niveau.",
              x, y + 30, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    } else {
        texte(g, POLICE_PETITE, "Points gagnés au combat. Ils sont dépensés en cliquant sur « Enregistrer ».",
              x, y + 30, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    }

    for (int i = 0; i < NB_STATS; i++) {
        float ly = y + 80 + i * 80;
        int valeur = statFinale(ed, i);
        texte(g, POLICE_NORMALE, i == STAT_DEF ? "Défense" : nomStat((enum Stat)i), x, ly, COUL_TEXTE, ALIGN_GAUCHE);
        snprintf(buf, sizeof buf, "%d", valeur);
        texte(g, POLICE_MOYENNE, buf, x + 330, ly - 4, ed->points[i] > 0 ? COUL_VIE : COUL_TEXTE, ALIGN_DROITE);
        barre(g, x, ly + 28, 330, 10, (float)valeur / MAX_AFFICHE[i], couleurClasse(ed->brouillon.classe),
              rgba(10, 8, 20, 200));
        if (pointsTotal(ed) > 0) {
            if (bouton(g, x + 360, ly + 4, 44, 36, "-", BOUTON_NORMAL, ed->points[i] > 0)) ed->points[i]--;
            if (bouton(g, x + 412, ly + 4, 44, 36, "+", BOUTON_NORMAL, pointsRestants(ed) > 0)) ed->points[i]++;
            snprintf(buf, sizeof buf, "%d pt%s  (+%d par point)", ed->points[i], ed->points[i] > 1 ? "s" : "",
                     gainStat((enum Stat)i));
            texte(g, POLICE_PETITE, buf, x + 476, ly + 14, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
        }
    }
    texte(g, POLICE_PETITE, "La vitesse fixe l'ordre des tours ; la défense réduit les dégâts reçus.",
          x, y + 410, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
}

/* ------------------------------------------------------------------ */
/*  Écran                                                             */
/* ------------------------------------------------------------------ */

void ecranEditeur(App *app)
{
    Gui *g = &app->g;
    Editeur *ed = &app->editeur;
    Personnage *b = &ed->brouillon;
    float t = g->temps / 1000.0f;
    dessinerFond(g, rgba(18, 12, 40, 255), rgba(24, 30, 60, 255));

    int creation = ed->index < 0;
    int retour = enTete(app, creation ? "Créer un héros" : "Personnaliser",
                        creation ? "Donnez vie à votre héros, détail par détail"
                                 : "Modifiez l'apparence, le nom ou l'élément");
    if (retour) {
        SDL_StopTextInput();
        changerEcran(app, creation ? ECRAN_MENU : ECRAN_HEROS);
    }

    /* aperçu */
    panneau(g, 30, 100, 420, 600);
    SDL_Color ce = couleurElement(b->element);
    halo(g, 240, 380, 260, avecAlpha(ce, 55));
    ellipse(g, 240, 566, 150, 30, rgba(30, 24, 56, 255));
    ellipse(g, 240, 560, 140, 24, rgba(76, 64, 124, 255));
    anneauEllipse(g, 240, 560, 140, 24, 1.5f, avecAlpha(ce, 160), 0, 0);
    if (attaqueApercu > 0) attaqueApercu -= g->dt * 1.6f;
    Pose pose = poseDefaut(t);
    pose.sens = sensApercu;
    pose.attaque = attaqueApercu > 0 ? sinf(attaqueApercu * 3.14159f) : 0;
    dessinerHeros(g, b, 240, 560, 2.15f, &pose);

    texteOmbre(g, POLICE_GRANDE, b->nom[0] ? b->nom : "???", 240, 114, COUL_TEXTE, ALIGN_CENTRE);
    char ligneInfo[64];
    snprintf(ligneInfo, sizeof ligneInfo, "%s  ·  %s", libelleClasse(b->classe), nomElement(b->element));
    texte(g, POLICE_NORMALE, ligneInfo, 240, 152, couleurClasse(b->classe), ALIGN_CENTRE);

    if (bouton(g, 50, 600, 120, 36, "Tourner", BOUTON_DISCRET, 1)) sensApercu = -sensApercu;
    if (bouton(g, 180, 600, 120, 36, "Attaquer", BOUTON_DISCRET, 1)) attaqueApercu = 1;
    if (bouton(g, 310, 600, 120, 36, "Aléatoire", BOUTON_DISCRET, 1)) apparenceAleatoire(&b->apparence);

    char st[96];
    snprintf(st, sizeof st, "PV %d   ATK %d   DEF %d   VIT %d",
             statFinale(ed, 0), statFinale(ed, 1), statFinale(ed, 2), statFinale(ed, 3));
    texte(g, POLICE_PETITE, st, 240, 660, COUL_TEXTE_DOUX, ALIGN_CENTRE);

    /* onglets */
    static const char *const ONGLETS[NB_ONGLETS] = { "Identité", "Apparence", "Statistiques" };
    for (int o = 0; o < NB_ONGLETS; o++) {
        float ox = 480 + o * 200, oy = 100;
        int actif = (ed->onglet == (enum OngletEditeur)o);
        int dessus = survol(g, ox, oy, 190, 42);
        rectArrondi(g, ox, oy, 190, 42, 12, actif ? rgba(70, 58, 130, 255) : (dessus ? rgba(46, 40, 86, 240) : rgba(26, 22, 50, 220)));
        if (actif) remplirRect(g, ox + 20, oy + 38, 150, 3, COUL_OR);
        texte(g, POLICE_NORMALE, ONGLETS[o], ox + 95, oy + 11, actif ? COUL_OR_CLAIR : COUL_TEXTE, ALIGN_CENTRE);
        if (dessus) {
            g->survolCliquable = 1;
            if (g->clic) ed->onglet = (enum OngletEditeur)o;
        }
    }
    if (guiTouche(g, SDLK_TAB)) ed->onglet = (enum OngletEditeur)((ed->onglet + 1) % NB_ONGLETS);

    panneau(g, 470, 150, 780, 490);
    float cx = 500, cy = 172;
    switch (ed->onglet) {
    case ONGLET_IDENTITE: ongletIdentite(app, cx, cy); break;
    case ONGLET_APPARENCE: ongletApparence(app, cx, cy); break;
    default: ongletStats(app, cx, cy); break;
    }
    if (ed->onglet != ONGLET_IDENTITE && ed->saisieNom) {
        ed->saisieNom = 0;
        SDL_StopTextInput();
    }

    if (bouton(g, 860, 654, 180, 46, "Annuler", BOUTON_DISCRET, 1)) {
        SDL_StopTextInput();
        changerEcran(app, creation ? ECRAN_MENU : ECRAN_HEROS);
    }
    if (bouton(g, 1060, 654, 190, 46, creation ? "Créer le héros" : "Enregistrer", BOUTON_PRINCIPAL, 1)
        || guiTouche(g, SDLK_RETURN)) {
        valider(app);
    }
}
