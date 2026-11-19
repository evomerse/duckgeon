/**
 * @file sdl_donjon.c
 * @brief Donjon SDL : les écrans du menu (voir sdl_donjon.h). Le combat
 *        est dans sdl_combat.c.
 *
 * Chaque écran est une fonction appelée à chaque image : elle dessine et
 * réagit aux clics (un bouton retourne 1 quand on clique dessus).
 */

/* [DONJON SDL - partie 3] */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "combat.h"
#include "donjon.h"
#include "inventaire.h"
#include "personnage.h"
#include "sauvegarde.h"
#include "sdl_combat.h"
#include "sdl_donjon.h"
#include "sdl_ui.h"

/* ------------------------------------------------------------------ */
/*  État de l'application                                             */
/* ------------------------------------------------------------------ */

enum Ecran {
    ECRAN_MENU, ECRAN_CREER, ECRAN_FICHE, ECRAN_PREPARATION, ECRAN_COMBAT,
    ECRAN_BESTIAIRE, ECRAN_ELEMENTS, ECRAN_CONFIRMER
};

enum Onglet { ONGLET_RESUME, ONGLET_INVENTAIRE, ONGLET_STATS, ONGLET_ELEMENT, NB_ONGLETS };

static const char *const NOMS_ONGLETS[NB_ONGLETS] = { "Résumé", "Inventaire", "Stats", "Élément" };


typedef struct {
    Ui ui;
    Donjon donjon;
    Progression prog;
    enum Ecran ecran;
    int quitter;

    char message[160];
    double messageTemps;

    /* création */
    char nomSaisi[NOM_MAX];
    int classeChoisie;
    int elementChoisi; /* -1 : élément de la classe */
    Personnage apercu; /* stats de la classe choisie */

    /* fiche */
    int heroSel;
    enum Onglet onglet;
    int defilement;

    /* préparation du combat */
    int etageChoisi;
    int equipe[COMBAT_MAX_ALLIES];
    int nbEquipe;
    int autoDefaut;

    /* bestiaire */
    int monstreSel;

    /* combat */
    CombatUi combat;
} App;

static void dire(App *a, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(a->message, sizeof a->message, format, args);
    va_end(args);
    a->messageTemps = 6.0;
}

static int compterVivants(const Donjon *d)
{
    int n = 0;
    for (int i = 0; i < d->taille; i++) {
        n += estVivant(&d->elements[i]);
    }
    return n;
}

/* ------------------------------------------------------------------ */
/*  Éléments d'interface communs                                      */
/* ------------------------------------------------------------------ */

/** Titre de l'écran. */
static void titre(App *a, const char *texte)
{
    uiTexte(&a->ui, a->ui.grande, texte, 20, 14, UI_OR);
}

/** Rappel des commandes de l'écran, en bas à droite. */
static void aide(App *a, const char *texte)
{
    uiTexteDroite(&a->ui, a->ui.petite, texte, UI_LARGEUR - 14, 618, UI_GRIS);
}

/** Bouton « Retour » (ou touche Échap). Retourne 1 pour revenir au menu. */
static int boutonRetour(App *a)
{
    if (a->ui.touche == SDLK_ESCAPE) {
        return 1;
    }
    return uiBouton(&a->ui, (SDL_Rect){ 860, 14, 120, 38 }, "Retour", 1);
}

/** Petite fiche d'un héros dans une liste. */
static void ligneHeros(App *a, SDL_Rect r, const Personnage *p, int choisi, const char *marque)
{
    Ui *ui = &a->ui;
    int vivant = estVivant(p);
    uiCadre(ui, r, choisi ? UI_PANNEAU2 : UI_PANNEAU, choisi ? UI_OR : UI_BORD);
    SDL_Color couleur = uiCouleurElement((int)p->element);
    uiCube(ui, (SDL_Rect){ r.x + 8, r.y + 6, 26, 26 }, vivant ? couleur : uiTeinte(couleur, 40), "", 1);
    char buf[96];
    snprintf(buf, sizeof buf, "%s%s", marque, p->nom);
    uiTexte(ui, ui->normale, buf, r.x + 44, r.y + 3, vivant ? UI_TEXTE : UI_GRIS);
    snprintf(buf, sizeof buf, "%s  niv. %d%s", nomClasse(p->classe), p->niveau, vivant ? "" : "  (K.O.)");
    uiTexte(ui, ui->petite, buf, r.x + 44, r.y + 24, UI_GRIS);
    uiBarre(ui, (SDL_Rect){ r.x + r.w - 96, r.y + r.h / 2 - 5, 84, 10 }, p->pv, p->pvMax,
            p->pv * 100 < p->pvMax * 30 ? UI_ROUGE : UI_VERT);
}

/**
 * Liste défilante de héros. Retourne l'indice cliqué (ou -1).
 * @param equipe  indices déjà choisis (marqués d'une croix), ou NULL
 */
static int listeHeros(App *a, SDL_Rect zone, int selection, const int *equipe, int nbEquipe)
{
    Donjon *d = &a->donjon;
    Ui *ui = &a->ui;
    int lignes = zone.h / 44;
    int maxDef = d->taille - lignes;
    if (maxDef < 0) maxDef = 0;
    if (uiDans(zone, ui->sx, ui->sy)) {
        a->defilement -= ui->molette;
    }
    if (a->defilement > maxDef) a->defilement = maxDef;
    if (a->defilement < 0) a->defilement = 0;

    int clique = -1;
    for (int k = 0; k < lignes && a->defilement + k < d->taille; k++) {
        int i = a->defilement + k;
        SDL_Rect r = { zone.x, zone.y + k * 44, zone.w, 40 };
        const char *marque = "";
        for (int e = 0; equipe != NULL && e < nbEquipe; e++) {
            if (equipe[e] == i) marque = "[x] ";
        }
        ligneHeros(a, r, &d->elements[i], i == selection, marque);
        if (ui->clic && uiDans(r, ui->sx, ui->sy)) {
            ui->clic = 0;
            clique = i;
        }
    }
    if (maxDef > 0) {
        char buf[48];
        snprintf(buf, sizeof buf, "%d-%d sur %d (molette)", a->defilement + 1,
                 a->defilement + lignes > d->taille ? d->taille : a->defilement + lignes, d->taille);
        uiTexte(ui, ui->petite, buf, zone.x, zone.y + lignes * 44, UI_GRIS);
    }
    return clique;
}

/** Statistiques détaillées d'un héros sur deux colonnes. */
static void statsHeros(App *a, const Personnage *p, int x, int y)
{
    Ui *ui = &a->ui;
    char buf[96];
    snprintf(buf, sizeof buf, "PV  %d / %d", p->pv, p->pvMax);
    uiTexte(ui, ui->normale, buf, x, y, UI_TEXTE);
    snprintf(buf, sizeof buf, "ATK  %d (arme comprise)", attaqueTotale(p));
    uiTexte(ui, ui->normale, buf, x, y + 26, UI_TEXTE);
    snprintf(buf, sizeof buf, "DEF  %d", p->def);
    uiTexte(ui, ui->normale, buf, x + 300, y, UI_TEXTE);
    snprintf(buf, sizeof buf, "VIT  %d", p->vit);
    uiTexte(ui, ui->normale, buf, x + 300, y + 26, UI_TEXTE);
}

/* ------------------------------------------------------------------ */
/*  Menu principal                                                    */
/* ------------------------------------------------------------------ */

#define NB_ENTREES 12

static const char *const ENTREES[NB_ENTREES] = {
    "1. Créer un personnage", "2. Afficher les personnages", "3. Lancer un combat", "4. Sauvegarder la partie",
    "5. Charger une partie", "6. Inventaire", "7. Améliorer un personnage", "8. Changer l'élément",
    "9. Bestiaire", "10. Table des éléments", "11. Se reposer au camp", "0. Quitter"
};

static void ouvrirFiche(App *a, enum Onglet onglet)
{
    if (a->donjon.taille == 0) {
        dire(a, "Le donjon est vide : créez d'abord un personnage.");
        return;
    }
    if (a->heroSel >= a->donjon.taille) {
        a->heroSel = 0;
    }
    a->onglet = onglet;
    a->ecran = ECRAN_FICHE;
}

static void ouvrirCreation(App *a)
{
    a->nomSaisi[0] = '\0';
    a->classeChoisie = GUERRIER;
    a->elementChoisi = -1;
    memset(&a->apercu, 0, sizeof a->apercu);
    initialiserStats(&a->apercu, GUERRIER);
    a->ecran = ECRAN_CREER;
}

static void lancerPreparation(App *a)
{
    if (a->donjon.taille == 0) {
        dire(a, "Le donjon est vide : créez d'abord un personnage.");
        return;
    }
    if (compterVivants(&a->donjon) == 0) {
        dire(a, "Aucun personnage en état de combattre : reposez-vous d'abord.");
        return;
    }
    a->nbEquipe = 0;
    a->etageChoisi = a->prog.etageMax;
    a->defilement = 0;
    a->ecran = ECRAN_PREPARATION;
}

static void sauvegarderDonjon(App *a)
{
    if (donjonSauvegarder(&a->donjon, FICHIER_SAUVEGARDE)) {
        dire(a, "%d personnage(s) sauvegardé(s) dans %s.", a->donjon.taille, FICHIER_SAUVEGARDE);
    } else {
        dire(a, "Échec de la sauvegarde.");
    }
}

static void chargerDonjon(App *a)
{
    if (donjonCharger(&a->donjon, FICHIER_SAUVEGARDE)) {
        a->heroSel = 0;
        dire(a, "%d personnage(s) chargé(s).", a->donjon.taille);
    } else {
        dire(a, "Échec du chargement : le donjon n'a pas été modifié.");
    }
}

static void demanderCharger(App *a)
{
    if (!sauvegardeExiste(FICHIER_SAUVEGARDE)) {
        dire(a, "Aucune sauvegarde trouvée (%s).", FICHIER_SAUVEGARDE);
    } else if (a->donjon.taille > 0) {
        a->ecran = ECRAN_CONFIRMER;
    } else {
        chargerDonjon(a);
    }
}

static void repos(App *a)
{
    for (int i = 0; i < a->donjon.taille; i++) {
        a->donjon.elements[i].pv = a->donjon.elements[i].pvMax;
    }
    dire(a, "Toute l'équipe se repose au camp et récupère ses PV.");
}

static void choisirEntree(App *a, int n)
{
    switch (n) {
    case 0: ouvrirCreation(a); break;
    case 1: ouvrirFiche(a, ONGLET_RESUME); break;
    case 2: lancerPreparation(a); break;
    case 3: sauvegarderDonjon(a); break;
    case 4: demanderCharger(a); break;
    case 5: ouvrirFiche(a, ONGLET_INVENTAIRE); break;
    case 6: ouvrirFiche(a, ONGLET_STATS); break;
    case 7: ouvrirFiche(a, ONGLET_ELEMENT); break;
    case 8: a->ecran = ECRAN_BESTIAIRE; break;
    case 9: a->ecran = ECRAN_ELEMENTS; break;
    case 10: repos(a); break;
    default: a->quitter = 1; break;
    }
}

static void ecranMenu(App *a)
{
    Ui *ui = &a->ui;
    uiTexte(ui, ui->grande, "LE DONJON", 40, 14, UI_OR);
    uiTexte(ui, ui->petite, "Gestion RPG - version graphique", 40, 52, UI_GRIS);

    for (int k = 0; k < NB_ENTREES; k++) {
        SDL_Rect r = { 40, 84 + k * 42, 360, 38 };
        if (uiBouton(ui, r, ENTREES[k], 1)) {
            choisirEntree(a, k);
        }
    }
    /* Raccourcis clavier : 1..9 puis 0. */
    if (ui->touche >= SDLK_1 && ui->touche <= SDLK_9) {
        choisirEntree(a, ui->touche - SDLK_1);
    } else if (ui->touche == SDLK_0) {
        a->quitter = 1;
    }

    /* Podium de l'équipe (comme Duckgeon, mais en cubes). */
    char buf[96];
    snprintf(buf, sizeof buf, "Foie gras : %d", a->prog.foieGras);
    uiTexteDroite(ui, ui->normale, buf, UI_LARGEUR - 20, 16, UI_OR);

    int n = a->donjon.taille < 4 ? a->donjon.taille : 4;
    uiRect(ui, (SDL_Rect){ 450, 420, 510, 22 }, (SDL_Color){ 58, 52, 100, 255 });
    uiRect(ui, (SDL_Rect){ 450, 420, 510, 4 }, (SDL_Color){ 110, 100, 170, 255 });
    if (n == 0) {
        uiParagraphe(ui, ui->normale,
                     "Le donjon est vide. Créez un personnage (1) ou chargez une sauvegarde (5), "
                     "puis lancez un combat (3). Le mode AUTO fait jouer vos héros tout seuls.",
                     470, 200, 470, UI_GRIS);
    }
    for (int k = 0; k < n; k++) {
        const Personnage *p = &a->donjon.elements[k];
        SDL_Rect cube = { 480 + k * 120, 420 - 80, 80, 80 };
        SDL_Color couleur = uiCouleurElement((int)p->element);
        uiCube(ui, cube, estVivant(p) ? couleur : uiTeinte(couleur, 40), "", 1);
        uiTexteCentre(ui, ui->petite, p->nom, (SDL_Rect){ cube.x - 20, 448, cube.w + 40, 18 }, UI_TEXTE);
        uiTexteCentre(ui, ui->petite, nomClasse(p->classe), (SDL_Rect){ cube.x - 20, 466, cube.w + 40, 18 }, UI_GRIS);
        uiBarre(ui, (SDL_Rect){ cube.x, 490, cube.w, 7 }, p->pv, p->pvMax, UI_VERT);
    }
    snprintf(buf, sizeof buf, "%d héros dans le donjon  ·  étage max débloqué : %d", a->donjon.taille, a->prog.etageMax);
    uiTexte(ui, ui->petite, buf, 40, 618, UI_GRIS);
    aide(a, "Touches 1-9 : choisir une entrée · 0 : quitter · clic : valider");
}

/* ------------------------------------------------------------------ */
/*  Création d'un personnage                                          */
/* ------------------------------------------------------------------ */

/** Ajoute le texte tapé au nom et gère l'effacement. */
static void saisirNom(App *a)
{
    size_t n = strlen(a->nomSaisi);
    for (const char *s = a->ui.texte; *s != '\0'; s++) {
        if ((unsigned char)*s >= 32 && n + 1 < sizeof a->nomSaisi) {
            a->nomSaisi[n++] = *s;
        }
    }
    a->nomSaisi[n] = '\0';
    if (a->ui.touche == SDLK_BACKSPACE && n > 0) {
        n--;
        while (n > 0 && ((unsigned char)a->nomSaisi[n] & 0xC0) == 0x80) {
            n--; /* on retire un caractère UTF-8 entier */
        }
        a->nomSaisi[n] = '\0';
    }
}

static void creerHeros(App *a)
{
    Personnage *p = donjonNouvelleCase(&a->donjon);
    if (p == NULL) {
        dire(a, "Mémoire insuffisante, personnage non créé.");
        return;
    }
    memset(p, 0, sizeof *p);
    snprintf(p->nom, sizeof p->nom, "%s", a->nomSaisi);
    initialiserStats(p, (enum Classe)a->classeChoisie);
    if (a->elementChoisi >= 0) {
        p->element = (enum Element)a->elementChoisi;
    }
    a->donjon.taille++; /* la case n'est comptée qu'une fois remplie */
    a->heroSel = a->donjon.taille - 1;
    dire(a, "%s a rejoint le donjon (pensez à sauvegarder).", p->nom);
    a->ecran = ECRAN_MENU;
}

static void ecranCreer(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Créer un personnage");
    aide(a, "Tapez le nom · clic : classe et élément · Entrée : créer · Échap : retour");
    saisirNom(a);

    uiTexte(ui, ui->normale, "Nom :", 20, 78, UI_GRIS);
    SDL_Rect champ = { 80, 72, 340, 34 };
    uiCadre(ui, champ, UI_PANNEAU, UI_OR);
    char saisie[NOM_MAX + 2];
    snprintf(saisie, sizeof saisie, "%s%s", a->nomSaisi, (SDL_GetTicks() / 500) % 2 ? "|" : "");
    uiTexte(ui, ui->normale, saisie, champ.x + 8, champ.y + 6, UI_TEXTE);

    uiTexte(ui, ui->normale, "Classe :", 20, 124, UI_GRIS);
    for (int c = 0; c < NB_CLASSES; c++) {
        SDL_Rect r = { 20 + (c % 2) * 205, 154 + (c / 2) * 46, 195, 40 };
        if (uiOption(ui, r, nomClasse((enum Classe)c), a->classeChoisie == c, 1)) {
            a->classeChoisie = c;
            memset(&a->apercu, 0, sizeof a->apercu);
            initialiserStats(&a->apercu, (enum Classe)c);
            a->elementChoisi = -1;
        }
    }

    /* Aperçu de la classe. */
    SDL_Rect ap = { 450, 72, 530, 330 };
    uiCadre(ui, ap, UI_PANNEAU, UI_BORD);
    uiCube(ui, (SDL_Rect){ ap.x + ap.w - 100, ap.y + 24, 70, 70 },
           uiCouleurElement(a->elementChoisi >= 0 ? a->elementChoisi : (int)elementClasse((enum Classe)a->classeChoisie)),
           "", 1);
    uiTexte(ui, ui->grande, nomClasse((enum Classe)a->classeChoisie), ap.x + 16, ap.y + 10, UI_TEXTE);
    uiParagraphe(ui, ui->normale, descriptionClasse((enum Classe)a->classeChoisie), ap.x + 16, ap.y + 54, ap.w - 130,
                 UI_GRIS);
    statsHeros(a, &a->apercu, ap.x + 16, ap.y + 130);
    for (int k = 0; k < NB_ACTIONS; k++) {
        const Capacite *cap = capaciteClasse((enum Classe)a->classeChoisie, (enum TypeAction)k);
        if (cap != NULL) {
            char buf[200];
            static const char *const TYPES[NB_ACTIONS] = { "Base", "Compétence", "Ultime" };
            snprintf(buf, sizeof buf, "%s : %s", TYPES[k], cap->nom);
            uiTexte(ui, ui->normale, buf, ap.x + 16, ap.y + 190 + k * 30, UI_TEXTE);
        }
    }

    /* Élément des attaques. */
    uiTexte(ui, ui->normale, "Élément des attaques :", 20, 420, UI_GRIS);
    enum Element defaut = elementClasse((enum Classe)a->classeChoisie);
    int courant = a->elementChoisi >= 0 ? a->elementChoisi : (int)defaut;
    for (int e = 0; e < NB_ELEMENTS; e++) {
        SDL_Rect r = { 20 + (e % 4) * 128, 452 + (e / 4) * 42, 120, 36 };
        char buf[40];
        snprintf(buf, sizeof buf, "%s%s", nomElement((enum Element)e), e == (int)defaut ? " *" : "");
        if (uiOption(ui, r, buf, courant == e, 1)) {
            a->elementChoisi = e;
        }
    }
    uiTexte(ui, ui->petite, "* élément habituel de la classe", 20, 540, UI_GRIS);

    int ok = a->nomSaisi[0] != '\0';
    if (uiBouton(ui, (SDL_Rect){ 700, 540, 280, 48 }, "Créer le personnage", ok) || (ok && ui->touche == SDLK_RETURN)) {
        creerHeros(a);
    }
    if (boutonRetour(a)) {
        a->ecran = ECRAN_MENU;
    }
}

/* ------------------------------------------------------------------ */
/*  Fiche d'un personnage : résumé, inventaire, stats, élément        */
/* ------------------------------------------------------------------ */

static void ongletResume(App *a, Personnage *p, int x, int y)
{
    Ui *ui = &a->ui;
    char buf[200];
    statsHeros(a, p, x, y);
    snprintf(buf, sizeof buf, "Victoires : %d      Points de stats à répartir : %d", p->victoires, p->pointsStats);
    uiTexte(ui, ui->normale, buf, x, y + 56, p->pointsStats > 0 ? UI_OR : UI_TEXTE);
    if (p->arme.nom[0] != '\0') {
        snprintf(buf, sizeof buf, "Arme : %s (+%d ATK)", p->arme.nom, p->arme.valeur);
    } else {
        snprintf(buf, sizeof buf, "Arme : aucune");
    }
    uiTexte(ui, ui->normale, buf, x, y + 84, UI_TEXTE);
    uiParagraphe(ui, ui->petite, descriptionClasse(p->classe), x, y + 118, 620, UI_GRIS);
    static const char *const TYPES[NB_ACTIONS] = { "Base", "Compétence", "Ultime" };
    for (int k = 0; k < NB_ACTIONS; k++) {
        char nom[64], desc[200];
        nomCapacite(p, (enum TypeAction)k, nom, sizeof nom);
        descriptionCapacite(p, (enum TypeAction)k, desc, sizeof desc);
        snprintf(buf, sizeof buf, "%s : %s", TYPES[k], nom);
        uiTexte(ui, ui->normale, buf, x, y + 160 + k * 66, UI_TEXTE);
        uiParagraphe(ui, ui->petite, desc, x, y + 184 + k * 66, 620, UI_GRIS);
    }
}

static void ongletInventaire(App *a, Personnage *p, int x, int y)
{
    Ui *ui = &a->ui;
    char buf[160];
    if (p->arme.nom[0] != '\0') {
        snprintf(buf, sizeof buf, "Arme équipée : %s (+%d ATK)", p->arme.nom, p->arme.valeur);
    } else {
        snprintf(buf, sizeof buf, "Aucune arme équipée");
    }
    uiTexte(ui, ui->normale, buf, x, y, UI_TEXTE);
    snprintf(buf, sizeof buf, "Sac : %d / %d", p->nbObjets, INVENTAIRE_MAX);
    uiTexte(ui, ui->petite, buf, x, y + 28, UI_GRIS);
    if (p->nbObjets == 0) {
        uiTexte(ui, ui->normale, "Le sac est vide.", x, y + 60, UI_GRIS);
    }
    for (int i = 0; i < p->nbObjets; i++) {
        const Objet *o = &p->inventaire[i];
        SDL_Rect r = { x, y + 52 + i * 48, 620, 42 };
        uiCadre(ui, r, UI_PANNEAU, UI_BORD);
        snprintf(buf, sizeof buf, "%s  (%s +%d)", o->nom, o->type == SOIN ? "soin" : "arme", o->valeur);
        uiTexte(ui, ui->normale, buf, r.x + 12, r.y + 10, UI_TEXTE);
        if (uiBouton(ui, (SDL_Rect){ r.x + r.w - 232, r.y + 4, 120, 34 }, o->type == SOIN ? "Utiliser" : "Équiper", 1)) {
            char nom[NOM_OBJET_MAX];
            snprintf(nom, sizeof nom, "%s", o->nom);
            if (utiliserObjet(p, i)) {
                dire(a, "%s : %s utilisé.", p->nom, nom);
            }
            return; /* le sac a changé : on redessine à l'image suivante */
        }
        if (uiBouton(ui, (SDL_Rect){ r.x + r.w - 104, r.y + 4, 92, 34 }, "Jeter", 1)) {
            Objet jete;
            if (retirerObjet(p, i, &jete)) {
                dire(a, "%s jette %s.", p->nom, jete.nom);
            }
            return;
        }
    }
}

static void ongletStats(App *a, Personnage *p, int x, int y)
{
    Ui *ui = &a->ui;
    char buf[96];
    snprintf(buf, sizeof buf, "%d point(s) de stats à répartir", p->pointsStats);
    uiTexte(ui, ui->normale, buf, x, y, p->pointsStats > 0 ? UI_OR : UI_GRIS);
    if (p->pointsStats == 0) {
        uiTexte(ui, ui->petite, "Gagnez des combats pour en obtenir.", x, y + 28, UI_GRIS);
    }
    for (int s = 0; s < NB_STATS; s++) {
        SDL_Rect r = { x, y + 60 + s * 52, 620, 44 };
        uiCadre(ui, r, UI_PANNEAU, UI_BORD);
        int valeur = s == STAT_PV ? p->pvMax : (s == STAT_ATK ? p->atk : (s == STAT_DEF ? p->def : p->vit));
        snprintf(buf, sizeof buf, "%s : %d", nomStat((enum Stat)s), valeur);
        uiTexte(ui, ui->normale, buf, r.x + 14, r.y + 11, UI_TEXTE);
        snprintf(buf, sizeof buf, "+%d", gainStat((enum Stat)s));
        if (uiBouton(ui, (SDL_Rect){ r.x + r.w - 110, r.y + 5, 98, 34 }, buf, p->pointsStats > 0)) {
            ameliorerStat(p, (enum Stat)s);
        }
    }
}

static void ongletElement(App *a, Personnage *p, int x, int y)
{
    Ui *ui = &a->ui;
    uiTexte(ui, ui->normale, "Élément des attaques :", x, y, UI_GRIS);
    for (int e = 0; e < NB_ELEMENTS; e++) {
        SDL_Rect r = { x + (e % 4) * 156, y + 34 + (e / 4) * 46, 148, 40 };
        if (uiOption(ui, r, nomElement((enum Element)e), (int)p->element == e, 1)) {
            p->element = (enum Element)e;
            dire(a, "%s attaquera avec l'élément %s.", p->nom, nomElement(p->element));
        }
    }
    char forts[64], faibles[64], buf[200];
    ecrireElements(elementsForts(p->element), forts, sizeof forts);
    ecrireElements(elementsFaibles(p->element), faibles, sizeof faibles);
    snprintf(buf, sizeof buf, "Fort contre : %s", forts);
    uiTexte(ui, ui->normale, buf, x, y + 150, UI_VERT);
    snprintf(buf, sizeof buf, "Faible contre : %s", faibles);
    uiTexte(ui, ui->normale, buf, x, y + 180, UI_ROUGE);
    snprintf(buf, sizeof buf, "Effet des attaques - %s : %s", nomEffetElement(p->element), effetElement(p->element));
    uiParagraphe(ui, ui->normale, buf, x, y + 220, 620, UI_TEXTE);
}

static void ecranFiche(App *a)
{
    Ui *ui = &a->ui;
    Donjon *d = &a->donjon;
    titre(a, "Personnages");
    aide(a, "Clic : choisir un héros / un onglet · molette : défiler · Échap : retour");
    if (d->taille == 0) {
        a->ecran = ECRAN_MENU;
        return;
    }
    int clic = listeHeros(a, (SDL_Rect){ 20, 76, 300, 528 }, a->heroSel, NULL, 0);
    if (clic >= 0) {
        a->heroSel = clic;
    }
    if (a->heroSel >= d->taille) {
        a->heroSel = d->taille - 1;
    }
    Personnage *p = &d->elements[a->heroSel];

    SDL_Rect pan = { 340, 76, 640, 528 };
    uiCadre(ui, pan, UI_PANNEAU, UI_BORD);
    uiCube(ui, (SDL_Rect){ pan.x + 14, pan.y + 12, 54, 54 }, estVivant(p) ? uiCouleurElement((int)p->element)
                                                                       : uiTeinte(uiCouleurElement((int)p->element), 40), "", 1);
    uiTexte(ui, ui->grande, p->nom, pan.x + 82, pan.y + 8, UI_TEXTE);
    char buf[120];
    snprintf(buf, sizeof buf, "%s  niv. %d   %s", nomClasse(p->classe), p->niveau, nomElement(p->element));
    uiTexte(ui, ui->normale, buf, pan.x + 82, pan.y + 48, uiCouleurElement((int)p->element));
    uiBarre(ui, (SDL_Rect){ pan.x + 330, pan.y + 16, 290, 12 }, p->pv, p->pvMax, UI_VERT);
    snprintf(buf, sizeof buf, "PV %d/%d", p->pv, p->pvMax);
    uiTexteDroite(ui, ui->petite, buf, pan.x + 620, pan.y + 30, UI_GRIS);
    uiBarre(ui, (SDL_Rect){ pan.x + 330, pan.y + 56, 290, 8 }, p->xp, xpPourNiveauSuivant(p), UI_BLEU);
    snprintf(buf, sizeof buf, "XP %d/%d", p->xp, xpPourNiveauSuivant(p));
    uiTexteDroite(ui, ui->petite, buf, pan.x + 620, pan.y + 66, UI_GRIS);

    for (int o = 0; o < NB_ONGLETS; o++) {
        if (uiOption(ui, (SDL_Rect){ pan.x + 14 + o * 154, pan.y + 96, 148, 36 }, NOMS_ONGLETS[o], (int)a->onglet == o, 1)) {
            a->onglet = (enum Onglet)o;
        }
    }
    int cx = pan.x + 14, cy = pan.y + 150;
    switch (a->onglet) {
    case ONGLET_RESUME: ongletResume(a, p, cx, cy); break;
    case ONGLET_INVENTAIRE: ongletInventaire(a, p, cx, cy); break;
    case ONGLET_STATS: ongletStats(a, p, cx, cy); break;
    default: ongletElement(a, p, cx, cy); break;
    }
    if (boutonRetour(a)) {
        a->ecran = ECRAN_MENU;
    }
}

/* ------------------------------------------------------------------ */
/*  Préparation du combat : étage et équipe                           */
/* ------------------------------------------------------------------ */

static int dansEquipe(const App *a, int i)
{
    for (int k = 0; k < a->nbEquipe; k++) {
        if (a->equipe[k] == i) {
            return k;
        }
    }
    return -1;
}

static void lancerCombat(App *a)
{
    Personnage *equipe[COMBAT_MAX_ALLIES];
    for (int k = 0; k < a->nbEquipe; k++) {
        equipe[k] = &a->donjon.elements[a->equipe[k]];
    }
    if (combatUiDemarrer(&a->combat, equipe, a->nbEquipe, a->etageChoisi, a->autoDefaut)) {
        a->ecran = ECRAN_COMBAT;
    } else {
        dire(a, "Le combat n'a pas pu commencer.");
    }
}

static void ecranPreparation(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Lancer un combat");
    aide(a, "Clic : étage et héros (1 à 4) · Entrée : lancer · A : mode auto · Échap : retour");
    if (a->ui.touche == SDLK_a) {
        a->autoDefaut = !a->autoDefaut;
    }

    uiTexte(ui, ui->normale, "Étage", 20, 66, UI_GRIS);
    for (int e = 1; e <= ETAGE_MAX; e++) {
        const InfoEtage *info = combatEtage(e);
        SDL_Rect r = { 20, 94 + (e - 1) * 50, 450, 46 };
        int libre = e <= a->prog.etageMax;
        char buf[160];
        if (libre) {
            snprintf(buf, sizeof buf, "%d. %s", e, info->nom);
        } else {
            snprintf(buf, sizeof buf, "%d. ?????  (verrouillé)", e);
        }
        if (uiOption(ui, r, "", a->etageChoisi == e, libre)) {
            a->etageChoisi = e;
        }
        uiTexte(ui, ui->normale, buf, r.x + 12, r.y + 4, libre ? UI_TEXTE : UI_GRIS);
        if (libre) {
            snprintf(buf, sizeof buf, "%d vagues%s%s%s", info->nbVagues, info->boss != NB_MONSTRES ? "  -  BOSS : " : "",
                     info->boss != NB_MONSTRES ? nomMonstre(info->boss) : "",
                     (a->prog.etagesNettoyes & (1u << (e - 1))) ? "  -  déjà nettoyé" : "");
            uiTexte(ui, ui->petite, buf, r.x + 12, r.y + 26, UI_GRIS);
        }
    }

    uiTexte(ui, ui->normale, "Équipe : 1 à 4 héros en vie (cliquez pour choisir)", 500, 66, UI_GRIS);
    int clic = listeHeros(a, (SDL_Rect){ 500, 94, 480, 360 }, -1, a->equipe, a->nbEquipe);
    if (clic >= 0) {
        int k = dansEquipe(a, clic);
        if (k >= 0) {
            for (int j = k; j + 1 < a->nbEquipe; j++) {
                a->equipe[j] = a->equipe[j + 1];
            }
            a->nbEquipe--;
        } else if (!estVivant(&a->donjon.elements[clic])) {
            dire(a, "%s est K.O. : reposez-vous d'abord.", a->donjon.elements[clic].nom);
        } else if (a->nbEquipe < COMBAT_MAX_ALLIES) {
            a->equipe[a->nbEquipe++] = clic;
        } else {
            dire(a, "Une équipe compte 4 héros au maximum.");
        }
    }

    char buf[96];
    snprintf(buf, sizeof buf, "%d héros choisi(s) : les monstres seront aussi nombreux", a->nbEquipe);
    uiTexte(ui, ui->petite, buf, 500, 482, UI_GRIS);
    if (uiOption(ui, (SDL_Rect){ 500, 510, 230, 42 }, a->autoDefaut ? "Mode AUTO : oui" : "Mode AUTO : non", a->autoDefaut, 1)) {
        a->autoDefaut = !a->autoDefaut;
    }
    uiTexte(ui, ui->petite, "(modifiable pendant le combat)", 500, 556, UI_GRIS);
    if (uiBouton(ui, (SDL_Rect){ 750, 510, 230, 42 }, "Lancer le combat", a->nbEquipe > 0)
        || (a->nbEquipe > 0 && ui->touche == SDLK_RETURN)) {
        lancerCombat(a);
    }
    if (boutonRetour(a)) {
        a->ecran = ECRAN_MENU;
    }
}

/* ------------------------------------------------------------------ */
/*  Bestiaire et table des éléments                                   */
/* ------------------------------------------------------------------ */

/** Niveau moyen supposé de l'équipe à un étage (comme le terminal). */
static int niveauTypique(int etage)
{
    return 1 + (etage - 1) * 3 / 2;
}

static void ecranBestiaire(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Bestiaire");
    aide(a, "Clic : choisir un monstre · Échap : retour");
    for (int m = 0; m < NB_MONSTRES; m++) {
        SDL_Rect r = { 20, 70 + m * 42, 240, 38 };
        char buf[64];
        snprintf(buf, sizeof buf, "%s%s", nomMonstre((enum Monstre)m), monstreEstBoss((enum Monstre)m) ? " (BOSS)" : "");
        if (uiOption(ui, r, buf, a->monstreSel == m, 1)) {
            a->monstreSel = m;
        }
    }
    enum Monstre m = (enum Monstre)a->monstreSel;
    const InfoMonstre *info = infoMonstre(m);
    if (info == NULL) {
        return;
    }
    SDL_Rect pan = { 280, 70, 700, 540 };
    uiCadre(ui, pan, UI_PANNEAU, UI_BORD);
    char buf[260], liste[96];
    uiTexte(ui, ui->grande, info->nom, pan.x + 14, pan.y + 8, UI_TEXTE);
    snprintf(buf, sizeof buf, "Élément %s%s", nomElement(info->element), info->boss ? "  -  BOSS" : "");
    uiTexte(ui, ui->normale, buf, pan.x + 14, pan.y + 46, uiCouleurElement((int)info->element));
    int y = pan.y + 76 + uiParagraphe(ui, ui->petite, info->description, pan.x + 14, pan.y + 76, pan.w - 28, UI_GRIS);
    y += 10;
    ecrireElements(faiblessesMonstre(m), liste, sizeof liste);
    snprintf(buf, sizeof buf, "Garde brisée par : %s", liste);
    uiTexte(ui, ui->normale, buf, pan.x + 14, y, UI_VERT);
    snprintf(buf, sizeof buf, "Attaque : %s", info->attaque);
    uiTexte(ui, ui->normale, buf, pan.x + 14, y + 28, UI_TEXTE);
    snprintf(buf, sizeof buf, "Spéciale : %s (%d %% des tours) - %s", info->special, info->chanceSpecial, info->effetSpecial);
    int h = uiParagraphe(ui, ui->normale, buf, pan.x + 14, y + 56, pan.w - 28, UI_TEXTE);
    y += 56 + h + 12;

    uiTexte(ui, ui->petite, "Étage   Niv. équipe      PV    ATK   DEF   VIT   Garde   (équipe de 4)", pan.x + 14, y, UI_GRIS);
    y += 22;
    unsigned etages = etagesMonstre(m);
    for (int e = 1; e <= ETAGE_MAX && y < pan.y + pan.h - 20; e++) {
        if (etages & (1u << (e - 1))) {
            Personnage s;
            int garde;
            statsMonstre(m, e, niveauTypique(e), 4, &s, &garde);
            snprintf(buf, sizeof buf, "%3d       %5d          %5d   %4d  %4d  %4d   %5d", e, niveauTypique(e), s.pvMax,
                     s.atk, s.def, s.vit, garde);
            uiTexte(ui, ui->petite, buf, pan.x + 14, y, UI_TEXTE);
            y += 20;
        }
    }
    if (boutonRetour(a)) {
        a->ecran = ECRAN_MENU;
    }
}

static void ecranElements(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Table des éléments");
    aide(a, "Échap : retour");
    uiTexte(ui, ui->normale, "Fort contre : +25 % de dégâts et la garde de la cible baisse. Faible contre : -20 %.", 20, 66,
            UI_GRIS);
    for (int e = 0; e < NB_ELEMENTS; e++) {
        SDL_Rect r = { 20, 100 + e * 64, 960, 58 };
        uiCadre(ui, r, UI_PANNEAU, UI_BORD);
        uiRect(ui, (SDL_Rect){ r.x + 1, r.y + 1, 8, r.h - 2 }, uiCouleurElement(e));
        char forts[64], faibles[64], buf[256];
        ecrireElements(elementsForts((enum Element)e), forts, sizeof forts);
        ecrireElements(elementsFaibles((enum Element)e), faibles, sizeof faibles);
        uiTexte(ui, ui->normale, nomElement((enum Element)e), r.x + 22, r.y + 8, uiCouleurElement(e));
        snprintf(buf, sizeof buf, "fort contre %s   -   faible contre %s", forts, faibles);
        uiTexte(ui, ui->normale, buf, r.x + 150, r.y + 8, UI_TEXTE);
        snprintf(buf, sizeof buf, "Effet des attaques - %s : %s", nomEffetElement((enum Element)e),
                 effetElement((enum Element)e));
        uiTexte(ui, ui->petite, buf, r.x + 150, r.y + 34, UI_GRIS);
    }
    char buf[256];
    int n = snprintf(buf, sizeof buf, "Cycle : ");
    for (int k = 0; k < NB_ELEMENTS && n < (int)sizeof buf - 24; k++) {
        n += snprintf(buf + n, sizeof buf - (size_t)n, "%s > ", nomElement(elementCycle(k)));
    }
    snprintf(buf + n, sizeof buf - (size_t)n, "%s", nomElement(elementCycle(0)));
    uiTexte(ui, ui->petite, buf, 20, 560, UI_GRIS);
    uiTexte(ui, ui->petite, "Chaque élément bat les deux suivants.", 20, 582, UI_GRIS);
    if (boutonRetour(a)) {
        a->ecran = ECRAN_MENU;
    }
}

/* ------------------------------------------------------------------ */
/*  Confirmation du chargement                                        */
/* ------------------------------------------------------------------ */

static void ecranConfirmer(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Charger une partie");
    aide(a, "Entrée : confirmer · Échap : annuler");
    char buf[160];
    snprintf(buf, sizeof buf, "Les %d personnage(s) actuel(s) seront remplacés par la sauvegarde.", a->donjon.taille);
    uiTexteCentre(ui, ui->normale, buf, (SDL_Rect){ 0, 200, UI_LARGEUR, 40 }, UI_TEXTE);
    if (uiBouton(ui, (SDL_Rect){ 300, 280, 190, 48 }, "Confirmer", 1) || ui->touche == SDLK_RETURN) {
        chargerDonjon(a);
        a->ecran = ECRAN_MENU;
    }
    if (uiBouton(ui, (SDL_Rect){ 510, 280, 190, 48 }, "Annuler", 1) || ui->touche == SDLK_ESCAPE) {
        a->ecran = ECRAN_MENU;
    }
}

/* ------------------------------------------------------------------ */
/*  Boucle principale                                                 */
/* ------------------------------------------------------------------ */

static void dessinerMessage(App *a, double dt)
{
    if (a->messageTemps <= 0) {
        return;
    }
    a->messageTemps -= dt;
    SDL_Color c = UI_OR;
    c.a = (Uint8)(a->messageTemps > 1 ? 255 : 255 * a->messageTemps);
    uiTexte(&a->ui, a->ui.normale, a->message, 20, 612, c);
}

int donjonSdl(void)
{
    App *a = calloc(1, sizeof *a); /* grosse structure : sur le tas */
    if (a == NULL) {
        perror("calloc");
        return 1;
    }
    if (!donjonInit(&a->donjon)) {
        free(a);
        return 1;
    }
    if (!uiInit(&a->ui, "Le Donjon - SDL")) {
        donjonLiberer(&a->donjon);
        free(a);
        return 1;
    }
    /* Même progression que le terminal et Duckgeon (étages débloqués...). */
    a->prog = (Progression){ 1, 0, 0, 0 };
    chargerProgression(FICHIER_PROGRESSION, &a->prog);
    if (a->prog.etageMax < 1) a->prog.etageMax = 1;
    if (a->prog.etageMax > ETAGE_MAX) a->prog.etageMax = ETAGE_MAX;
    a->ecran = ECRAN_MENU;

    Uint32 precedent = SDL_GetTicks();
    while (!a->ui.quitter && !a->quitter) {
        uiEvenements(&a->ui);
        Uint32 maintenant = SDL_GetTicks();
        double dt = (maintenant - precedent) / 1000.0;
        precedent = maintenant;
        if (dt > 0.1) dt = 0.1; /* évite un saut après une pause */

        uiDebut(&a->ui);
        switch (a->ecran) {
        case ECRAN_MENU: ecranMenu(a); break;
        case ECRAN_CREER: ecranCreer(a); break;
        case ECRAN_FICHE: ecranFiche(a); break;
        case ECRAN_PREPARATION: ecranPreparation(a); break;
        case ECRAN_COMBAT:
            if (combatUiImage(&a->combat, &a->ui, &a->prog, dt)) {
                int fuite = a->combat.cb->etat != COMBAT_VICTOIRE && a->combat.cb->etat != COMBAT_DEFAITE;
                combatUiLiberer(&a->combat);
                if (fuite) {
                    dire(a, "L'équipe prend la fuite !");
                }
                a->ecran = ECRAN_MENU;
            }
            break;
        case ECRAN_BESTIAIRE: ecranBestiaire(a); break;
        case ECRAN_ELEMENTS: ecranElements(a); break;
        case ECRAN_CONFIRMER: ecranConfirmer(a); break;
        }
        dessinerMessage(a, dt);
        uiFin(&a->ui);
        SDL_Delay(4);
    }

    combatUiLiberer(&a->combat);
    uiFermer(&a->ui);
    donjonLiberer(&a->donjon);
    free(a);
    return 0;
}
