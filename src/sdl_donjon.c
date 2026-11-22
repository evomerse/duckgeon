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

#include "boutique.h"
#include "combat.h"
#include "donjon.h"
#include "inventaire.h"
#include <sys/stat.h>
#include <time.h>

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
    ECRAN_BESTIAIRE, ECRAN_ELEMENTS, ECRAN_CONFIRMER, ECRAN_BOUTIQUE, ECRAN_SLOTS
};

/** Ce qu'on sait d'un emplacement de sauvegarde (lu à l'ouverture de l'écran). */
typedef struct {
    int existe;
    char nom[SLOT_NOM_MAX];
    int nbHeros;
    int niveauMax;
    char date[24];
} InfoSlot;

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
    int modeLibre;    /**< 1 : combat libre (entraînement) */

    /* emplacements de sauvegarde */
    int slotCharger;  /**< 1 : on charge, 0 : on sauvegarde */
    int slotSel;      /**< emplacement choisi, -1 : aucun    */
    char nomPartie[SLOT_NOM_MAX];
    InfoSlot slots[SLOTS_MAX];

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
    uiTexte(&a->ui, a->ui.grande, texte, 22, 17, (SDL_Color){ 10, 10, 12, 255 });
    uiTexte(&a->ui, a->ui.grande, texte, 20, 14, UI_OR);
}

/** Petit foie gras dessiné (pastille rosée avec reflet). */
static void iconeFoie(Ui *ui, int x, int y)
{
    uiRectArrondi(ui, (SDL_Rect){ x, y + 2, 26, 16 }, 8, (SDL_Color){ 150, 90, 90, 255 });
    uiRectDegrade(ui, (SDL_Rect){ x, y, 26, 16 }, 8, (SDL_Color){ 250, 206, 190, 255 }, (SDL_Color){ 226, 150, 150, 255 });
    uiRectArrondi(ui, (SDL_Rect){ x + 5, y + 3, 10, 3 }, 1, (SDL_Color){ 255, 255, 255, 140 });
}

/** Compteur de foie gras en haut à droite. */
static void compteurFoie(App *a, int xDroite, int y)
{
    char buf[48];
    snprintf(buf, sizeof buf, "%d foie gras", a->prog.foieGras);
    int w = uiLargeur(a->ui.normale, buf);
    SDL_Rect r = { xDroite - w - 56, y, w + 56, 36 };
    uiRectArrondi(&a->ui, r, 6, (SDL_Color){ 10, 10, 12, 150 });
    iconeFoie(&a->ui, r.x + 14, r.y + 10);
    uiTexte(&a->ui, a->ui.normale, buf, r.x + 48, r.y + 7, UI_OR);
}

/** Rappel des commandes de l'écran, en bas à droite. */
static void aide(App *a, const char *texte)
{
    uiRaccourcis(&a->ui, texte, UI_LARGEUR - 14, 612);
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
    uiHeros(ui, (SDL_Rect){ r.x + 12, r.y + 12, 22, 22 }, (int)p->classe, couleur, 1, vivant, 0);
    uiElementIcone(ui, r.x + 36, r.y + 34, 6, (int)p->element);
    char buf[96];
    snprintf(buf, sizeof buf, "%s%s", marque, p->nom);
    uiTexte(ui, ui->normale, buf, r.x + 44, r.y + 3, vivant ? UI_TEXTE : UI_GRIS);
    snprintf(buf, sizeof buf, "%s  niv. %d%s", nomClasse(p->classe), p->niveau, vivant ? "" : "  (K.O.)");
    uiTexte(ui, ui->petite, buf, r.x + 44, r.y + 24, UI_GRIS);
    uiBarre(ui, (SDL_Rect){ r.x + r.w - 96, r.y + r.h / 2 - 5, 84, 10 }, p->pv, p->pvMax,
            p->pv * 100 < p->pvMax * 30 ? UI_ROUGE : UI_VERT);
    if (p->pointsStats > 0) {
        snprintf(buf, sizeof buf, "+%d pts", p->pointsStats);
        uiTexteDroite(ui, ui->petite, buf, r.x + r.w - 12, r.y + 2, UI_OR);
    }
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

/** Relit les emplacements : nom, nombre de héros, niveau max et date. */
static void lireSlots(App *a)
{
    for (int k = 0; k < SLOTS_MAX; k++) {
        InfoSlot *s = &a->slots[k];
        char chemin[64];
        memset(s, 0, sizeof *s);
        cheminSlot(k, chemin, sizeof chemin);
        if (!sauvegardeExiste(chemin)) {
            continue;
        }
        s->existe = 1;
        if (!lireNomSlot(k, s->nom, sizeof s->nom)) {
            snprintf(s->nom, sizeof s->nom, "Partie %d", k + 1);
        }
        Personnage *liste = malloc(sizeof *liste);
        int n = 0;
        if (liste != NULL && chargerFichier(chemin, &liste, &n)) {
            s->nbHeros = n;
            for (int i = 0; i < n; i++) {
                if (liste[i].niveau > s->niveauMax) {
                    s->niveauMax = liste[i].niveau;
                }
            }
        }
        free(liste);
        struct stat st;
        if (stat(chemin, &st) == 0) {
            time_t t = st.st_mtime;
            strftime(s->date, sizeof s->date, "%d/%m/%Y %H:%M", localtime(&t));
        }
    }
}

static void ouvrirSlots(App *a, int charger)
{
    lireSlots(a);
    a->slotCharger = charger;
    a->slotSel = -1;
    a->nomPartie[0] = '\0';
    a->ecran = ECRAN_SLOTS;
}

static void sauvegarderDonjon(App *a, int slot)
{
    char chemin[64];
    cheminSlot(slot, chemin, sizeof chemin);
    if (donjonSauvegarder(&a->donjon, chemin) && ecrireNomSlot(slot, a->nomPartie)) {
        dire(a, "« %s » sauvegardée (emplacement %d, %d personnage(s)).", a->nomPartie, slot + 1, a->donjon.taille);
    } else {
        dire(a, "Échec de la sauvegarde : l'emplacement %d n'a pas été modifié.", slot + 1);
    }
}

static void chargerDonjon(App *a, int slot)
{
    char chemin[64];
    cheminSlot(slot, chemin, sizeof chemin);
    if (donjonCharger(&a->donjon, chemin)) {
        a->heroSel = 0;
        dire(a, "« %s » chargée : %d personnage(s).", a->slots[slot].nom, a->donjon.taille);
    } else {
        dire(a, "Échec du chargement : le donjon n'a pas été modifié.");
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
    case 3: ouvrirSlots(a, 0); break;
    case 4: ouvrirSlots(a, 1); break;
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

    /* Bandeau du haut : titre à gauche, foie gras et boutique à droite. */
    uiRectDegrade(ui, (SDL_Rect){ 0, 0, UI_LARGEUR, 70 }, 0, (SDL_Color){ 46, 46, 52, 255 }, (SDL_Color){ 30, 30, 34, 255 });
    uiRect(ui, (SDL_Rect){ 0, 69, UI_LARGEUR, 2 }, UI_OR);
    uiTexte(ui, ui->enorme, "LE DONJON", 24, 10, (SDL_Color){ 10, 10, 12, 255 });
    uiTexte(ui, ui->enorme, "LE DONJON", 22, 7, (SDL_Color){ 255, 226, 150, 255 });
    compteurFoie(a, UI_LARGEUR - 160, 17);
    if (uiBouton(ui, (SDL_Rect){ UI_LARGEUR - 148, 14, 132, 42 }, "Boutique [B]", 1)) {
        a->defilement = 0;
        a->ecran = ECRAN_BOUTIQUE;
    }

    /* Salle de l'équipe : les héros alignés, avec leur fiche en dessous. */
    uiRectDegrade(ui, (SDL_Rect){ 0, 71, UI_LARGEUR, 262 }, 0, (SDL_Color){ 30, 30, 34, 255 }, (SDL_Color){ 22, 22, 26, 255 });
    for (int k = 0; k < 5; k++) {
        /* torches au mur */
        int tx = 90 + k * 205;
        uiRect(ui, (SDL_Rect){ tx, 110, 6, 22 }, (SDL_Color){ 110, 78, 50, 255 });
        uiRect(ui, (SDL_Rect){ tx - 3, 98, 12, 14 }, (SDL_Color){ 244, 150, 60, 255 });
        uiRect(ui, (SDL_Rect){ tx, 92, 6, 10 }, (SDL_Color){ 255, 220, 110, 255 });
    }
    uiRectDegrade(ui, (SDL_Rect){ 0, 280, UI_LARGEUR, 53 }, 0, (SDL_Color){ 62, 62, 68, 255 }, (SDL_Color){ 34, 34, 38, 255 });
    uiRect(ui, (SDL_Rect){ 0, 280, UI_LARGEUR, 2 }, (SDL_Color){ 120, 120, 128, 255 });
    int n = a->donjon.taille < 4 ? a->donjon.taille : 4;
    if (n == 0) {
        uiTexteCentre(ui, ui->normale,
                      "Le donjon est vide : créez un personnage (1) ou chargez une sauvegarde (5), puis lancez un combat (3).",
                      (SDL_Rect){ 0, 160, UI_LARGEUR, 30 }, UI_TEXTE);
        uiTexteCentre(ui, ui->petite, "Le mode AUTO fait jouer vos héros tout seuls.", (SDL_Rect){ 0, 192, UI_LARGEUR, 20 }, UI_GRIS);
    }
    for (int k = 0; k < n; k++) {
        const Personnage *p = &a->donjon.elements[k];
        int x = 70 + k * 222;
        SDL_Rect cube = { x + 62, 182, 84, 84 };
        uiHeros(ui, cube, (int)p->classe, uiCouleurElement((int)p->element), 1, estVivant(p), k * 1.3);
        char buf[96];
        uiTexte(ui, ui->normale, p->nom, x + 8, 292, UI_TEXTE);
        uiElementIcone(ui, x + 196, 302, 8, (int)p->element);
        snprintf(buf, sizeof buf, "%s niv. %d", nomClasse(p->classe), p->niveau);
        uiTexte(ui, ui->petite, buf, x + 8, 313, UI_GRIS);
        uiBarre(ui, (SDL_Rect){ x + 120, 311, 80, 8 }, p->pv, p->pvMax, UI_VERT);
    }

    /* Entrées du menu : une grille de 4 colonnes x 3 lignes. */
    for (int k = 0; k < NB_ENTREES; k++) {
        SDL_Rect r = { 20 + (k % 4) * 243, 360 + (k / 4) * 70, 233, 58 };
        char etiquette[64];
        int points = 0;
        for (int h = 0; k == 6 && h < a->donjon.taille; h++) {
            points += a->donjon.elements[h].pointsStats;
        }
        if (points > 0) {
            snprintf(etiquette, sizeof etiquette, "7. Améliorer (+%d pts)", points);
        } else {
            snprintf(etiquette, sizeof etiquette, "%s", ENTREES[k]);
        }
        if (uiOption(ui, r, etiquette, k == 2 || points > 0, 1)) {
            choisirEntree(a, k);
        }
    }
    /* Raccourcis clavier : 1..9 puis 0. */
    if (ui->touche >= SDLK_1 && ui->touche <= SDLK_9) {
        choisirEntree(a, ui->touche - SDLK_1);
    } else if (ui->touche == SDLK_0) {
        a->quitter = 1;
    } else if (ui->touche == SDLK_b) {
        a->defilement = 0;
        a->ecran = ECRAN_BOUTIQUE;
    }

    char buf[96];
    snprintf(buf, sizeof buf, "%d héros dans le donjon  ·  étage max débloqué : %d", a->donjon.taille, a->prog.etageMax);
    uiTexte(ui, ui->petite, buf, 20, 618, UI_GRIS);
    aide(a, "1-9|choisir;0|quitter;B|boutique;Clic|valider");
}

/* ------------------------------------------------------------------ */
/*  Boutique : on dépense le foie gras en potions et en armes         */
/* ------------------------------------------------------------------ */

typedef struct {
    Objet objet;
    int prix;
    const char *description;
} Article;

static const Article ARTICLES[] = {
    { { "Potion mineure",  SOIN, 30 }, 12, "Rend 30 PV (ranime un héros K.O.)." },
    { { "Grande potion",   SOIN, 60 }, 25, "Rend 60 PV (ranime un héros K.O.)." },
    { { "Dague rouillee",  ARME,  2 }, 20, "Arme : +2 ATK. Mieux que les poings." },
    { { "Epee longue",     ARME,  5 }, 45, "Arme : +5 ATK. Un classique fiable." },
    { { "Baton runique",   ARME,  7 }, 65, "Arme : +7 ATK. Gravé de runes." },
    { { "Hache de guerre", ARME,  9 }, 90, "Arme : +9 ATK. Lourde et dévastatrice." },
    { { "Parchemin d'oubli", PARCHEMIN, 0 }, 15, "Rend tous les points de stats dépensés (à utiliser dans le sac)." },
};
#define NB_ARTICLES ((int)(sizeof ARTICLES / sizeof ARTICLES[0]))

static void acheter(App *a, int article)
{
    const Article *art = &ARTICLES[article];
    Personnage *p = &a->donjon.elements[a->heroSel];
    if (a->prog.foieGras < art->prix) {
        dire(a, "Pas assez de foie gras (%d requis).", art->prix);
    } else if (!ajouterObjet(p, art->objet)) {
        dire(a, "Le sac de %s est plein : libérez de la place.", p->nom);
    } else {
        a->prog.foieGras -= art->prix;
        if (!sauvegarderProgression(FICHIER_PROGRESSION, &a->prog)) {
            fprintf(stderr, "La progression n'a pas pu etre enregistree.\n");
        }
        dire(a, "%s achète : %s (dans son sac).", p->nom, art->objet.nom);
    }
}

static void ecranBoutique(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Boutique");
    aide(a, "Clic|héros et article;Échap|retour");
    compteurFoie(a, 840, 14);
    uiTexte(ui, ui->petite, "Les achats vont dans le sac du héros choisi (équipez ou utilisez-les dans « Inventaire »).", 20, 56, UI_GRIS);

    if (a->donjon.taille == 0) {
        uiTexteCentre(ui, ui->normale, "Créez d'abord un personnage pour faire des achats.", (SDL_Rect){ 0, 260, UI_LARGEUR, 40 }, UI_TEXTE);
    } else {
        if (a->heroSel >= a->donjon.taille) {
            a->heroSel = 0;
        }
        int clic = listeHeros(a, (SDL_Rect){ 20, 82, 300, 400 }, a->heroSel, NULL, 0);
        if (clic >= 0) {
            a->heroSel = clic;
        }
        const Personnage *p = &a->donjon.elements[a->heroSel];
        SDL_Rect fiche = { 20, 492, 300, 112 };
        uiCadre(ui, fiche, UI_PANNEAU, UI_BORD);
        uiHeros(ui, (SDL_Rect){ fiche.x + 18, fiche.y + 44, 40, 40 }, (int)p->classe, uiCouleurElement((int)p->element), 1,
                estVivant(p), 0);
        char buf[96];
        uiTexte(ui, ui->normale, p->nom, fiche.x + 92, fiche.y + 12, UI_TEXTE);
        snprintf(buf, sizeof buf, "Sac : %d / %d", p->nbObjets, INVENTAIRE_MAX);
        uiTexte(ui, ui->petite, buf, fiche.x + 92, fiche.y + 40, p->nbObjets >= INVENTAIRE_MAX ? UI_ROUGE : UI_GRIS);
        snprintf(buf, sizeof buf, p->arme.nom[0] ? "Arme : %s (+%d)" : "Arme : aucune", p->arme.nom, p->arme.valeur);
        uiTexte(ui, ui->petite, buf, fiche.x + 92, fiche.y + 60, UI_GRIS);
        snprintf(buf, sizeof buf, "PV %d/%d", p->pv, p->pvMax);
        uiTexte(ui, ui->petite, buf, fiche.x + 92, fiche.y + 80, UI_VERT);

        for (int k = 0; k < NB_ARTICLES; k++) {
            const Article *art = &ARTICLES[k];
            SDL_Rect r = { 340 + (k % 2) * 322, 82 + (k / 2) * 131, 314, 125 };
            int abordable = a->prog.foieGras >= art->prix;
            int soin = art->objet.type == SOIN;
            int parchemin = art->objet.type == PARCHEMIN;
            uiCadre(ui, r, UI_PANNEAU, abordable ? UI_BORD : (SDL_Color){ 60, 60, 66, 255 });
            /* vignette : fiole ou lame */
            SDL_Rect vign = { r.x + 12, r.y + 12, 56, 56 };
            uiRectDegrade(ui, vign, 6, (SDL_Color){ 64, 64, 70, 255 }, (SDL_Color){ 38, 38, 42, 255 });
            if (parchemin) {
                uiRectArrondi(ui, (SDL_Rect){ vign.x + 12, vign.y + 8, 32, 40 }, 3, (SDL_Color){ 236, 222, 178, 255 });
                uiRect(ui, (SDL_Rect){ vign.x + 8, vign.y + 6, 40, 5 }, (SDL_Color){ 170, 130, 80, 255 });
                uiRect(ui, (SDL_Rect){ vign.x + 8, vign.y + 45, 40, 5 }, (SDL_Color){ 170, 130, 80, 255 });
                for (int l = 0; l < 3; l++) {
                    uiRect(ui, (SDL_Rect){ vign.x + 17, vign.y + 16 + l * 8, 22, 2 }, (SDL_Color){ 120, 90, 60, 255 });
                }
            } else if (soin) {
                SDL_Color liq = art->objet.valeur >= 60 ? (SDL_Color){ 240, 90, 120, 255 } : (SDL_Color){ 100, 210, 140, 255 };
                uiRectArrondi(ui, (SDL_Rect){ vign.x + 22, vign.y + 8, 20, 14 }, 4, (SDL_Color){ 210, 210, 230, 255 });
                uiRectDegrade(ui, (SDL_Rect){ vign.x + 12, vign.y + 20, 40, 38 }, 8, uiTeinte(liq, 125), uiTeinte(liq, 80));
            } else {
                int taille = 20 + art->objet.valeur * 3;
                uiRectArrondi(ui, (SDL_Rect){ vign.x + 29, vign.y + 56 - taille, 6, taille }, 3, (SDL_Color){ 210, 216, 236, 255 });
                uiRectArrondi(ui, (SDL_Rect){ vign.x + 20, vign.y + 52, 24, 5 }, 2, UI_OR);
                uiRectArrondi(ui, (SDL_Rect){ vign.x + 29, vign.y + 56, 6, 8 }, 2, (SDL_Color){ 140, 90, 60, 255 });
            }
            uiTexte(ui, ui->normale, art->objet.nom, r.x + 86, r.y + 10, UI_TEXTE);
                        uiParagraphe(ui, ui->petite, art->description, r.x + 86, r.y + 38, r.w - 100, UI_GRIS);
            char prix[24];
            snprintf(prix, sizeof prix, "%d", art->prix);
            iconeFoie(ui, r.x + 14, r.y + r.h - 32);
            uiTexte(ui, ui->normale, prix, r.x + 46, r.y + r.h - 34, abordable ? UI_OR : UI_ROUGE);
            if (uiBouton(ui, (SDL_Rect){ r.x + r.w - 124, r.y + r.h - 40, 110, 32 }, "Acheter", abordable)) {
                acheter(a, k);
            }
        }
    }
    if (boutonRetour(a)) {
        a->ecran = ECRAN_MENU;
    }
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
    aide(a, "A-Z|nom;Clic|classe, élément;Entrée|créer;Échap|retour");
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
    uiHeros(ui, (SDL_Rect){ ap.x + ap.w - 100, ap.y + 40, 64, 64 }, a->classeChoisie,
            uiCouleurElement(a->elementChoisi >= 0 ? a->elementChoisi : (int)elementClasse((enum Classe)a->classeChoisie)),
            0, 1, 0);
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
        if (o->type == PARCHEMIN) {
            snprintf(buf, sizeof buf, "%s  (remet les stats à zéro)", o->nom);
        } else {
            snprintf(buf, sizeof buf, "%s  (%s +%d)", o->nom, o->type == SOIN ? "soin" : "arme", o->valeur);
        }
        uiTexte(ui, ui->normale, buf, r.x + 12, r.y + 10, UI_TEXTE);
        if (uiBouton(ui, (SDL_Rect){ r.x + r.w - 232, r.y + 4, 120, 34 }, o->type == ARME ? "Équiper" : "Utiliser", 1)) {
            char nom[NOM_OBJET_MAX];
            snprintf(nom, sizeof nom, "%s", o->nom);
            int parchemin = o->type == PARCHEMIN;
            int avant = p->pointsStats;
            if (utiliserObjet(p, i)) {
                if (parchemin) {
                    dire(a, "%s : %d point(s) de stats à réattribuer (onglet Stats).", p->nom, p->pointsStats - avant);
                    a->onglet = ONGLET_STATS;
                } else {
                    dire(a, "%s : %s utilisé.", p->nom, nom);
                }
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
    char buf[160];
    snprintf(buf, sizeof buf, "Niveau %d  ·  XP %d / %d  ·  victoires : %d", p->niveau, p->xp, xpPourNiveauSuivant(p), p->victoires);
    uiTexte(ui, ui->normale, buf, x, y, UI_TEXTE);
    snprintf(buf, sizeof buf, "%d point(s) de stats à répartir", p->pointsStats);
    uiTexte(ui, ui->normale, buf, x, y + 28, p->pointsStats > 0 ? UI_OR : UI_GRIS);
    uiTexteDroite(ui, ui->petite, "+2 points par niveau, +1 par étage nettoyé", x + 620, y + 32, UI_GRIS);
    for (int s = 0; s < NB_STATS; s++) {
        SDL_Rect r = { x, y + 62 + s * 52, 620, 44 };
        uiCadre(ui, r, UI_PANNEAU, UI_BORD);
        int valeur = s == STAT_PV ? p->pvMax : (s == STAT_ATK ? p->atk : (s == STAT_DEF ? p->def : p->vit));
        snprintf(buf, sizeof buf, "%s : %d", nomStat((enum Stat)s), valeur);
        uiTexte(ui, ui->normale, buf, r.x + 14, r.y + 11, UI_TEXTE);
        if (s == STAT_ATK && p->arme.nom[0] != '\0') {
            snprintf(buf, sizeof buf, "(%d avec %s)", attaqueTotale(p), p->arme.nom);
            uiTexte(ui, ui->petite, buf, r.x + 230, r.y + 14, UI_GRIS);
        }
        snprintf(buf, sizeof buf, "+%d", gainStat((enum Stat)s));
        if (uiBouton(ui, (SDL_Rect){ r.x + r.w - 110, r.y + 5, 98, 34 }, buf, p->pointsStats > 0)) {
            ameliorerStat(p, (enum Stat)s);
        }
    }
    /* Parchemin d'oubli : réattribuer les points depuis le lobby. */
    int parchemin = -1;
    for (int i = 0; i < p->nbObjets; i++) {
        if (p->inventaire[i].type == PARCHEMIN) {
            parchemin = i;
            break;
        }
    }
    SDL_Rect bouton = { x, y + 276, 300, 40 };
    if (uiBouton(ui, bouton, "Utiliser un parchemin d'oubli", parchemin >= 0)) {
        int rendus = 0;
        if (utiliserObjet(p, parchemin)) {
            rendus = p->pointsStats;
        }
        dire(a, "%s : stats remises à zéro, %d point(s) à réattribuer.", p->nom, rendus);
    }
    uiParagraphe(ui, ui->petite, parchemin >= 0 ? "Remet PV, ATK, DEF et VIT à la valeur de la classe et du niveau."
                                                : "Aucun parchemin dans le sac : on en achète à la boutique (touche B).",
                 x + 312, y + 278, 300, UI_GRIS);
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
    aide(a, "Clic|héros, onglet;Molette|défiler;Échap|retour");
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
    uiHeros(ui, (SDL_Rect){ pan.x + 20, pan.y + 20, 46, 46 }, (int)p->classe, uiCouleurElement((int)p->element), 1,
            estVivant(p), 0);
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
        char nomOnglet[40];
        if (o == ONGLET_STATS && p->pointsStats > 0) {
            snprintf(nomOnglet, sizeof nomOnglet, "Stats (+%d)", p->pointsStats);
        } else {
            snprintf(nomOnglet, sizeof nomOnglet, "%s", NOMS_ONGLETS[o]);
        }
        if (uiOption(ui, (SDL_Rect){ pan.x + 14 + o * 154, pan.y + 96, 148, 36 }, nomOnglet, (int)a->onglet == o, 1)) {
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

/** Ajoute ou retire un héros de l'équipe du combat. */
static void basculerEquipe(App *a, int clic)
{
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

/** Carte d'un héros (grand cube, nom, classe, PV) ; rang = place dans l'équipe (0 : non choisi). */
static void carteHeros(App *a, SDL_Rect r, const Personnage *p, int rang)
{
    Ui *ui = &a->ui;
    int vivant = estVivant(p);
    int survol = uiDans(r, ui->sx, ui->sy);
    SDL_Color element = uiCouleurElement((int)p->element);
    if (rang > 0) {
        uiLueur(ui, r, UI_OR, 70);
    }
    uiCadre(ui, r, rang > 0 ? UI_PANNEAU2 : UI_PANNEAU, rang > 0 ? UI_OR : (survol && vivant ? UI_GRIS : UI_BORD));
    uiRect(ui, (SDL_Rect){ r.x + 4, r.y + 6, 4, r.h - 12 }, vivant ? element : UI_BORD);
    uiHeros(ui, (SDL_Rect){ r.x + 20, r.y + 24, 38, 38 }, (int)p->classe, element, 1, vivant, 0);
    char buf[96];
    uiTexte(ui, ui->normale, p->nom, r.x + 72, r.y + 8, vivant ? UI_TEXTE : UI_GRIS);
    snprintf(buf, sizeof buf, "%s · niv. %d", nomClasse(p->classe), p->niveau);
    uiTexte(ui, ui->petite, buf, r.x + 72, r.y + 32, UI_GRIS);
    uiElementIcone(ui, r.x + 80, r.y + 64, 7, (int)p->element);
    uiBarre(ui, (SDL_Rect){ r.x + 96, r.y + 60, r.w - 112, 8 }, p->pv, p->pvMax,
            p->pv * 100 < p->pvMax * 30 ? UI_ROUGE : UI_VERT);
    if (rang > 0) {
        snprintf(buf, sizeof buf, "%d", rang);
        uiDisque(ui, r.x + r.w - 16, r.y + 16, 11, UI_OR);
        uiTexteCentre(ui, ui->normale, buf, (SDL_Rect){ r.x + r.w - 27, r.y + 4, 22, 24 }, (SDL_Color){ 40, 26, 8, 255 });
    } else if (!vivant) {
        uiTexteDroite(ui, ui->petite, "K.O.", r.x + r.w - 10, r.y + 8, UI_ROUGE);
    }
}

/** Grille de cartes (2 colonnes) défilante. Retourne l'indice cliqué ou -1. */
static int grilleHeros(App *a, SDL_Rect zone)
{
    Donjon *d = &a->donjon;
    Ui *ui = &a->ui;
    const int hc = 86, ecart = 8;
    int lignes = (zone.h + ecart) / (hc + ecart);
    int total = (d->taille + 1) / 2;
    int maxDef = total - lignes;
    if (maxDef < 0) maxDef = 0;
    if (uiDans(zone, ui->sx, ui->sy)) {
        a->defilement -= ui->molette;
    }
    if (a->defilement > maxDef) a->defilement = maxDef;
    if (a->defilement < 0) a->defilement = 0;

    if (d->taille == 0) {
        uiCadre(ui, zone, UI_PANNEAU, UI_BORD);
        uiTexteCentre(ui, ui->normale, "Aucun héros : créez un personnage (1) au menu.", zone, UI_GRIS);
        return -1;
    }
    int clique = -1;
    for (int l = 0; l < lignes; l++) {
        for (int c = 0; c < 2; c++) {
            int i = (a->defilement + l) * 2 + c;
            if (i >= d->taille) break;
            SDL_Rect r = { zone.x + c * (zone.w / 2 + 3), zone.y + l * (hc + ecart), zone.w / 2 - 3, hc };
            carteHeros(a, r, &d->elements[i], dansEquipe(a, i) + 1);
            if (ui->clic && uiDans(r, ui->sx, ui->sy)) {
                ui->clic = 0;
                clique = i;
            }
        }
    }
    if (maxDef > 0) {
        char buf[48];
        snprintf(buf, sizeof buf, "Molette : %d/%d", a->defilement + 1, maxDef + 1);
        uiTexteDroite(ui, ui->petite, buf, zone.x + zone.w, zone.y + zone.h + 2, UI_GRIS);
    }
    return clique;
}

static void lancerCombat(App *a)
{
    Personnage *equipe[COMBAT_MAX_ALLIES];
    for (int k = 0; k < a->nbEquipe; k++) {
        equipe[k] = &a->donjon.elements[a->equipe[k]];
    }
    if (combatUiDemarrer(&a->combat, equipe, a->nbEquipe, a->etageChoisi, a->autoDefaut, a->modeLibre)) {
        a->ecran = ECRAN_COMBAT;
    } else {
        dire(a, "Le combat n'a pas pu commencer.");
    }
}

static void ecranPreparation(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Lancer un combat");
    aide(a, "Clic|étage, héros;Entrée|lancer;A|auto;Échap|retour");
    if (a->ui.touche == SDLK_a) {
        a->autoDefaut = !a->autoDefaut;
    }

    /* Niveau moyen de l'équipe choisie (0 si personne). */
    int niveauEquipe = 0;
    for (int k = 0; k < a->nbEquipe; k++) {
        niveauEquipe += a->donjon.elements[a->equipe[k]].niveau;
    }
    if (a->nbEquipe > 0) niveauEquipe /= a->nbEquipe;

    /* Mode : expédition (progression, niveau requis) ou combat libre (entraînement). */
    if (uiOption(ui, (SDL_Rect){ 20, 62, 222, 34 }, "Expédition", !a->modeLibre, 1)) {
        a->modeLibre = 0;
    }
    if (uiOption(ui, (SDL_Rect){ 248, 62, 222, 34 }, "Combat libre", a->modeLibre, 1)) {
        a->modeLibre = 1;
    }
    for (int e = 1; e <= ETAGE_MAX; e++) {
        const InfoEtage *info = combatEtage(e);
        SDL_Rect r = { 20, 104 + (e - 1) * 47, 450, 43 };
        int requis = combatNiveauRequis(e);
        int debloque = e <= a->prog.etageMax;
        int niveauOk = a->modeLibre || a->nbEquipe == 0 || niveauEquipe >= requis;
        int libre = debloque && niveauOk;
        char buf[160];
        if (debloque) {
            snprintf(buf, sizeof buf, "%d. %s", e, info->nom);
        } else {
            snprintf(buf, sizeof buf, "%d. ?????  (verrouillé)", e);
        }
        if (uiOption(ui, r, "", a->etageChoisi == e, libre)) {
            a->etageChoisi = e;
        }
        SDL_Color sombre = { 40, 26, 8, 255 };
        int choisi = a->etageChoisi == e;
        uiTexte(ui, ui->normale, buf, r.x + 12, r.y + 2, choisi ? sombre : (libre ? UI_TEXTE : UI_GRIS));
        if (debloque) {
            if (a->modeLibre) {
                snprintf(buf, sizeof buf, "2 vagues, sans boss");
            } else {
                snprintf(buf, sizeof buf, "%d vagues%s%s%s", info->nbVagues, info->boss != NB_MONSTRES ? "  -  BOSS : " : "",
                         info->boss != NB_MONSTRES ? nomMonstre(info->boss) : "",
                         (a->prog.etagesNettoyes & (1u << (e - 1))) ? "  -  déjà nettoyé" : "");
            }
            uiTexte(ui, ui->petite, buf, r.x + 12, r.y + 23, choisi ? sombre : UI_GRIS);
            if (!a->modeLibre) {
                snprintf(buf, sizeof buf, "niv. %d requis", requis);
                uiTexteDroite(ui, ui->petite, buf, r.x + r.w - 10, r.y + 4, niveauOk ? (choisi ? sombre : UI_VERT) : UI_ROUGE);
            }
        }
    }
    /* Si l'étage choisi n'est plus permis (équipe changée, mode changé), on recule. */
    while (a->etageChoisi > 1 && !(a->etageChoisi <= a->prog.etageMax
                                   && (a->modeLibre || a->nbEquipe == 0 || niveauEquipe >= combatNiveauRequis(a->etageChoisi)))) {
        a->etageChoisi--;
    }

    /* Sélection de l'équipe : cartes de héros + 4 emplacements numérotés. */
    uiTexte(ui, ui->normale, "Équipe", 500, 66, UI_GRIS);
    uiTexteDroite(ui, ui->petite, "1 à 4 héros en vie · clic pour choisir / retirer", UI_LARGEUR - 20, 70, UI_GRIS);
    int clic = grilleHeros(a, (SDL_Rect){ 500, 94, 480, 282 });
    if (clic >= 0) {
        basculerEquipe(a, clic);
    }
    for (int k = 0; k < COMBAT_MAX_ALLIES; k++) {
        SDL_Rect r = { 500 + k * 122, 392, 114, 66 };
        int rempli = k < a->nbEquipe;
        uiCadre(ui, r, rempli ? UI_PANNEAU2 : UI_PANNEAU, rempli ? UI_OR : UI_BORD);
        char num[4];
        snprintf(num, sizeof num, "%d", k + 1);
        uiDisque(ui, r.x + 14, r.y + 14, 9, rempli ? UI_OR : UI_PANNEAU2);
        uiTexteCentre(ui, ui->petite, num, (SDL_Rect){ r.x + 5, r.y + 5, 18, 18 }, rempli ? (SDL_Color){ 40, 26, 8, 255 } : UI_GRIS);
        if (rempli) {
            const Personnage *p = &a->donjon.elements[a->equipe[k]];
            uiHeros(ui, (SDL_Rect){ r.x + 40, r.y + 14, 26, 26 }, (int)p->classe, uiCouleurElement((int)p->element), 1, 1, k * 1.1);
            uiTexteCentre(ui, ui->petite, p->nom, (SDL_Rect){ r.x, r.y + 44, r.w, 18 }, UI_TEXTE);
            if (ui->clic && uiDans(r, ui->sx, ui->sy)) {
                ui->clic = 0;
                basculerEquipe(a, a->equipe[k]);
                break;
            }
        } else {
            uiTexteCentre(ui, ui->petite, "libre", (SDL_Rect){ r.x, r.y + 24, r.w, 20 }, UI_GRIS);
        }
    }

    char buf[96];
    if (a->modeLibre) {
        snprintf(buf, sizeof buf, "Combat libre : XP et butin, sans niveau requis ni déblocage (foie gras réduit)");
    } else if (a->nbEquipe > 0) {
        snprintf(buf, sizeof buf, "Niveau moyen de l'équipe : %d · l'étage suivant demande plus : entraînez-vous en libre", niveauEquipe);
    } else {
        snprintf(buf, sizeof buf, "Choisissez 1 à 4 héros : les monstres seront aussi nombreux");
    }
    uiTexte(ui, ui->petite, buf, 500, 474, UI_GRIS);
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
    return combatNiveauRequis(etage);
}

static void ecranBestiaire(App *a)
{
    Ui *ui = &a->ui;
    titre(a, "Bestiaire");
    aide(a, "Clic|monstre;Échap|retour");
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
    aide(a, "Échap|retour");
    uiTexte(ui, ui->normale, "Fort contre : +25 % de dégâts et la garde de la cible baisse. Faible contre : -20 %.", 20, 66,
            UI_GRIS);
    for (int e = 0; e < NB_ELEMENTS; e++) {
        SDL_Rect r = { 20, 100 + e * 64, 960, 58 };
        uiCadre(ui, r, UI_PANNEAU, UI_BORD);
        uiElementIcone(ui, r.x + 26, r.y + r.h / 2, 16, e);
        char forts[64], faibles[64], buf[256];
        ecrireElements(elementsForts((enum Element)e), forts, sizeof forts);
        ecrireElements(elementsFaibles((enum Element)e), faibles, sizeof faibles);
        uiTexte(ui, ui->normale, nomElement((enum Element)e), r.x + 52, r.y + 8, uiCouleurElement(e));
        snprintf(buf, sizeof buf, "fort contre %s   -   faible contre %s", forts, faibles);
        uiTexte(ui, ui->normale, buf, r.x + 170, r.y + 8, UI_TEXTE);
        snprintf(buf, sizeof buf, "Effet des attaques - %s : %s", nomEffetElement((enum Element)e),
                 effetElement((enum Element)e));
        uiTexte(ui, ui->petite, buf, r.x + 170, r.y + 34, UI_GRIS);
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
/*  Emplacements de sauvegarde                                        */
/* ------------------------------------------------------------------ */

/** Saisie du nom de la partie (mêmes règles que le nom d'un héros). */
static void saisirNomPartie(App *a)
{
    size_t n = strlen(a->nomPartie);
    for (const char *s = a->ui.texte; *s != '\0'; s++) {
        if ((unsigned char)*s >= 32 && n + 1 < sizeof a->nomPartie) {
            a->nomPartie[n++] = *s;
        }
    }
    a->nomPartie[n] = '\0';
    if (a->ui.touche == SDLK_BACKSPACE && n > 0) {
        n--;
        while (n > 0 && ((unsigned char)a->nomPartie[n] & 0xC0) == 0x80) {
            n--;
        }
        a->nomPartie[n] = '\0';
    }
}

static void ecranSlots(App *a)
{
    Ui *ui = &a->ui;
    titre(a, a->slotCharger ? "Charger une partie" : "Sauvegarder la partie");
    aide(a, a->slotCharger ? "Clic|emplacement;Échap|retour"
                           : "Clic|emplacement;A-Z|nom;Entrée|sauvegarder;Échap|retour");
    if (a->slotSel >= 0 && !a->slotCharger) {
        saisirNomPartie(a);
    }

    for (int k = 0; k < SLOTS_MAX; k++) {
        const InfoSlot *s = &a->slots[k];
        SDL_Rect r = { 20, 76 + k * 82, 960, 74 };
        int choisi = a->slotSel == k;
        int actif = !a->slotCharger || s->existe;
        uiCadre(ui, r, choisi ? UI_PANNEAU2 : UI_PANNEAU, choisi ? UI_OR : UI_BORD);
        char buf[160];
        snprintf(buf, sizeof buf, "Emplacement %d", k + 1);
        uiTexte(ui, ui->petite, buf, r.x + 14, r.y + 8, UI_GRIS);
        if (s->existe) {
            uiTexte(ui, ui->grande, s->nom, r.x + 14, r.y + 26, actif ? UI_TEXTE : UI_GRIS);
            snprintf(buf, sizeof buf, "%d héros · niveau max %d", s->nbHeros, s->niveauMax);
            uiTexteDroite(ui, ui->normale, buf, r.x + r.w - 16, r.y + 12, UI_TEXTE);
            uiTexteDroite(ui, ui->petite, s->date, r.x + r.w - 16, r.y + 42, UI_GRIS);
        } else {
            uiTexte(ui, ui->grande, "Vide", r.x + 14, r.y + 26, UI_GRIS);
        }
        if (actif && ui->clic && uiDans(r, ui->sx, ui->sy)) {
            ui->clic = 0;
            a->slotSel = k;
            ui->texte[0] = '\0';
            if (!a->slotCharger) {
                /* on propose le nom existant, ou « Partie N » */
                snprintf(a->nomPartie, sizeof a->nomPartie, "%s", s->existe ? s->nom : "");
                if (a->nomPartie[0] == '\0') {
                    snprintf(a->nomPartie, sizeof a->nomPartie, "Partie %d", k + 1);
                }
            }
        }
    }

    if (a->slotSel >= 0) {
        const InfoSlot *s = &a->slots[a->slotSel];
        if (a->slotCharger) {
            if (uiBouton(ui, (SDL_Rect){ 760, 500, 220, 40 }, "Charger", 1) || ui->touche == SDLK_RETURN) {
                if (a->donjon.taille > 0) {
                    a->ecran = ECRAN_CONFIRMER;
                } else {
                    chargerDonjon(a, a->slotSel);
                    a->ecran = ECRAN_MENU;
                }
            }
        } else {
            uiTexte(ui, ui->normale, "Nom :", 20, 512, UI_GRIS);
            SDL_Rect champ = { 80, 506, 420, 34 };
            uiCadre(ui, champ, UI_PANNEAU, UI_OR);
            char saisie[SLOT_NOM_MAX + 2];
            snprintf(saisie, sizeof saisie, "%s%s", a->nomPartie, (SDL_GetTicks() / 500) % 2 ? "|" : "");
            uiTexte(ui, ui->normale, saisie, champ.x + 8, champ.y + 6, UI_TEXTE);
            int ok = a->nomPartie[0] != '\0';
            if (uiBouton(ui, (SDL_Rect){ 760, 500, 220, 40 }, s->existe ? "Écraser" : "Sauvegarder", ok)
                || (ok && ui->touche == SDLK_RETURN)) {
                sauvegarderDonjon(a, a->slotSel);
                a->ecran = ECRAN_MENU;
            }
            if (s->existe) {
                uiTexte(ui, ui->petite, "Cet emplacement contient déjà une partie : elle sera remplacée.", 80, 548, UI_ORANGE);
            }
        }
    }
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
    aide(a, "Entrée|confirmer;Échap|annuler");
    char buf[160];
    snprintf(buf, sizeof buf, "Les %d personnage(s) actuel(s) seront remplacés par « %s ».", a->donjon.taille,
             a->slots[a->slotSel].nom);
    uiTexteCentre(ui, ui->normale, buf, (SDL_Rect){ 0, 200, UI_LARGEUR, 40 }, UI_TEXTE);
    uiTexteCentre(ui, ui->petite, "Pensez à sauvegarder la partie en cours si vous voulez la garder.",
                  (SDL_Rect){ 0, 236, UI_LARGEUR, 20 }, UI_GRIS);
    if (uiBouton(ui, (SDL_Rect){ 300, 280, 190, 48 }, "Confirmer", 1) || ui->touche == SDLK_RETURN) {
        chargerDonjon(a, a->slotSel);
        a->ecran = ECRAN_MENU;
    }
    if (uiBouton(ui, (SDL_Rect){ 510, 280, 190, 48 }, "Annuler", 1) || ui->touche == SDLK_ESCAPE) {
        a->ecran = ECRAN_SLOTS;
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
    uiTexte(&a->ui, a->ui.normale, a->message, 20, 586, c);
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
        case ECRAN_SLOTS: ecranSlots(a); break;
        case ECRAN_BOUTIQUE: ecranBoutique(a); break;
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
