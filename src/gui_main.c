/**
 * @file gui_main.c
 * @brief Duckgeon - version graphique (SDL2) du Donjon : programme principal,
 *        boucle d'événements et écran titre.
 *
 * La logique métier (personnage.c, inventaire.c, donjon.c...) est la même
 * que celle de la version console ; ce programme ne fait que l'afficher et
 * la piloter à la souris et au clavier.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gui_app.h"
#include "gui_sprite.h"
#include "inventaire.h"
#include "sauvegarde.h"

#define PI_F 3.14159265f

/* ------------------------------------------------------------------ */
/*  Outils communs aux écrans                                         */
/* ------------------------------------------------------------------ */

void changerEcran(App *app, enum Ecran e)
{
    app->prochain = e;
}

void dessinerFond(Gui *g, SDL_Color haut, SDL_Color bas)
{
    float t = g->temps / 1000.0f;
    degradeVertical(g, 0, 0, LARGEUR, HAUTEUR, haut, bas);

    /* nébuleuses qui dérivent lentement */
    halo(g, 300 + sinf(t * 0.1f) * 80, 200, 420, rgba(120, 70, 200, 40));
    halo(g, 1000 + cosf(t * 0.08f) * 90, 480, 460, rgba(60, 120, 220, 35));
    halo(g, 700, 720, 500, rgba(230, 150, 90, 25));

    /* étoiles scintillantes (positions pseudo-aléatoires fixes) */
    for (int i = 0; i < 90; i++) {
        unsigned h = (unsigned)i * 2654435761u;
        float x = (float)(h % LARGEUR);
        float y = (float)((h / LARGEUR) % (HAUTEUR - 120));
        float sc = 0.5f + 0.5f * sinf(t * (0.8f + (i % 7) * 0.3f) + i);
        cercle(g, x, y, 0.8f + (i % 3) * 0.5f, rgba(255, 255, 255, (int)(60 + 150 * sc)));
    }
    /* poussière lumineuse qui monte */
    for (int i = 0; i < 24; i++) {
        float ph = fmodf(t * 0.05f + i * 0.0417f, 1.0f);
        float x = fmodf(i * 211.0f + sinf(t * 0.5f + i) * 30, LARGEUR);
        cercle(g, x, HAUTEUR - ph * HAUTEUR, 1.5f, rgba(255, 220, 160, (int)(120 * (1 - ph))));
    }
}

int enTete(App *app, const char *titre, const char *sousTitre)
{
    Gui *g = &app->g;
    degradeVertical(g, 0, 0, LARGEUR, 90, rgba(8, 6, 20, 200), rgba(8, 6, 20, 0));
    texteOmbre(g, POLICE_TITRE, titre, 130, 18, COUL_OR_CLAIR, ALIGN_GAUCHE);
    if (sousTitre != NULL) {
        texte(g, POLICE_NORMALE, sousTitre, 132, 68, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    }
    return bouton(g, 24, 26, 90, 40, "Retour", BOUTON_DISCRET, 1) || guiTouche(g, SDLK_ESCAPE);
}

/* ------------------------------------------------------------------ */
/*  Écran titre                                                       */
/* ------------------------------------------------------------------ */

void ecranMenu(App *app)
{
    Gui *g = &app->g;
    float t = g->temps / 1000.0f;
    dessinerFond(g, rgba(20, 12, 44, 255), rgba(46, 24, 60, 255));

    /* estrade et héros du donjon qui posent */
    float sx = 900, sy = 560;
    halo(g, sx, sy - 120, 380, rgba(255, 200, 120, 45));
    ellipse(g, sx, sy + 8, 330, 44, rgba(30, 22, 50, 255));
    ellipse(g, sx, sy, 320, 40, rgba(70, 56, 110, 255));
    ellipse(g, sx, sy - 4, 300, 32, rgba(96, 80, 150, 255));
    anneauEllipse(g, sx, sy - 4, 300, 32, 2, rgba(255, 220, 150, 120), 0, 0);

    int n = app->donjon.taille < 4 ? app->donjon.taille : 4;
    for (int i = 0; i < n; i++) {
        float hx = sx + (i - (n - 1) / 2.0f) * 140;
        float hy = sy + ((i % 2) ? -6 : 8);
        Pose pose = poseDefaut(t + i * 0.7f);
        pose.sens = (hx < sx) ? 1 : -1;
        if (!estVivant(&app->donjon.elements[i])) pose.alpha = 0.5f;
        dessinerHeros(g, &app->donjon.elements[i], hx, hy, 1.25f, &pose);
    }

    /* titre */
    float flotte = sinf(t * 1.2f) * 4;
    halo(g, 330, 170, 280, rgba(255, 190, 90, 40));
    texteOmbre(g, POLICE_ENORME, "DUCKGEON", 80, 100 + flotte, COUL_OR_CLAIR, ALIGN_GAUCHE);
    texte(g, POLICE_MOYENNE, "CHRONIQUES DES CANARDS", 86, 200 + flotte, rgba(200, 180, 255, 255), ALIGN_GAUCHE);
    ligne(g, 86, 238, 460, 238, 2, avecAlpha(COUL_OR, 140));
    losange(g, 470, 238, 6, 6, COUL_OR);

    float bx = 86, by = 268;
    const float pas = 58, h = 46;
    if (bouton(g, bx, by, 320, 50, "Combattre (bots ou duel)", BOUTON_PRINCIPAL, app->donjon.taille > 0)
        || (guiTouche(g, SDLK_RETURN) && app->donjon.taille > 0)) {
        changerEcran(app, ECRAN_EQUIPE);
    }
    if (bouton(g, bx, by + pas, 320, h, "Duel 1 contre 1 (local)", BOUTON_NORMAL, app->donjon.taille > 0)) {
        ouvrirDuel1v1(app);
    }
    if (bouton(g, bx, by + 2 * pas, 320, h, "Mes héros", BOUTON_NORMAL, 1)) {
        changerEcran(app, ECRAN_HEROS);
    }
    if (bouton(g, bx, by + 3 * pas, 320, h, "Créer un héros", BOUTON_NORMAL, 1)) {
        ouvrirEditeur(app, -1);
    }
    if (bouton(g, bx, by + 4 * pas, 320, h, "Boutique de cosmétiques", BOUTON_NORMAL, 1)) {
        changerEcran(app, ECRAN_BOUTIQUE);
    }
    if (bouton(g, bx, by + 5 * pas, 320, h, "Bestiaire et éléments", BOUTON_NORMAL, 1)) {
        changerEcran(app, ECRAN_BESTIAIRE);
    }
    if (bouton(g, bx, by + 6 * pas, 320, h, "Quitter", BOUTON_DISCRET, 1)
        || guiTouche(g, SDLK_ESCAPE)) {
        g->quitter = 1;
    }

    /* réserve de foie gras */
    char fg[48];
    snprintf(fg, sizeof fg, "%d foie gras", app->prog.foieGras);
    iconeFoieGras(g, 1080, 40, 14);
    texteOmbre(g, POLICE_MOYENNE, fg, 1104, 26, COUL_OR_CLAIR, ALIGN_GAUCHE);

    char info[96];
    snprintf(info, sizeof info, "%d héros dans le donjon  ·  étage max débloqué : %d",
             app->donjon.taille, app->prog.etageMax);
    texte(g, POLICE_PETITE, info, 86, 680, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
}

/* ------------------------------------------------------------------ */
/*  Héros de départ                                                   */
/* ------------------------------------------------------------------ */

/** Ajoute un héros prédéfini au donjon. */
static void ajouterHeros(Donjon *d, const char *nom, enum Classe classe, Apparence apparence)
{
    Personnage *p = donjonNouvelleCase(d);
    if (p == NULL) {
        return;
    }
    memset(p, 0, sizeof *p);
    snprintf(p->nom, NOM_MAX, "%s", nom);
    initialiserStats(p, classe);
    p->apparence = apparence;
    d->taille++;
}

static void creerHerosDeDepart(Donjon *d)
{
    /*                       plum. coupe chev yeux regard tenue tenue2 coiffe pilo marque cape aura corp */
    ajouterHeros(d, "Aldric", GUERRIER, (Apparence){ 1, 0, 1, 11, 1, 7, 16, 0, 3, 1, 1, 0, 2 });
    ajouterHeros(d, "Séléné", MAGE, (Apparence){ 0, 1, 9, 12, 0, 10, 16, 6, 0, 0, 0, 10, 0 });
    ajouterHeros(d, "Lysandre", PRETRE, (Apparence){ 2, 8, 3, 13, 3, 5, 11, 4, 0, 4, 1, 0, 1 });
    ajouterHeros(d, "Kaze", VOLEUR, (Apparence){ 3, 3, 0, 14, 1, 13, 0, 5, 0, 2, 0, 0, 1 });
}

/* ------------------------------------------------------------------ */
/*  Programme principal                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    srand((unsigned)time(NULL));

    App *app = calloc(1, sizeof *app);
    if (app == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }
    if (!donjonInit(&app->donjon)) {
        free(app);
        return EXIT_FAILURE;
    }
    /* Sauvegarde locale : on reprend les héros de la dernière partie,
     * sinon (premier lancement ou fichier illisible) les héros de départ. */
    if (!sauvegardeExiste(FICHIER_SAUVEGARDE) || !donjonCharger(&app->donjon, FICHIER_SAUVEGARDE)) {
        creerHerosDeDepart(&app->donjon);
    }
    app->prog.etageMax = 1;
    chargerProgression(FICHIER_PROGRESSION, &app->prog);
    if (app->prog.etageMax > ETAGE_MAX) app->prog.etageMax = ETAGE_MAX;
    app->prep.etage = app->prog.etageMax;
    app->prep.taille = 4;

    if (!guiInit(&app->g, "Duckgeon - Chroniques des Canards")) {
        donjonLiberer(&app->donjon);
        free(app);
        return EXIT_FAILURE;
    }
    Gui *g = &app->g;
    SDL_StopTextInput(); /* la saisie de texte n'est active que dans le champ du nom */

    while (!g->quitter) {
        guiDebutImage(g);

        /* Pendant un fondu, on ignore les entrées pour éviter les doubles clics. */
        if (app->prochain != app->ecran) {
            g->clic = g->clicDroit = 0;
            g->nbTouches = 0;
        }

        switch (app->ecran) {
        case ECRAN_MENU: ecranMenu(app); break;
        case ECRAN_HEROS: ecranHeros(app); break;
        case ECRAN_EDITEUR: ecranEditeur(app); break;
        case ECRAN_EQUIPE: ecranPreparation(app); break;
        case ECRAN_COMBAT: ecranCombat(app); break;
        case ECRAN_BOUTIQUE: ecranBoutique(app); break;
        case ECRAN_BESTIAIRE: ecranBestiaire(app); break;
        }

        /* Transition en fondu au noir. */
        if (app->prochain != app->ecran) {
            app->fondu += g->dt * 5;
            if (app->fondu >= 1) {
                app->fondu = 1;
                app->ecran = app->prochain;
                /* Sauvegarde automatique à chaque changement d'écran :
                 * création, édition, suppression, fin de combat... */
                donjonSauvegarder(&app->donjon, FICHIER_SAUVEGARDE);
                sauvegarderProgression(FICHIER_PROGRESSION, &app->prog);
                if (app->ecran != ECRAN_EDITEUR) {
                    SDL_StopTextInput();
                }
            }
        } else if (app->fondu > 0) {
            app->fondu -= g->dt * 4;
            if (app->fondu < 0) app->fondu = 0;
        }
        if (app->fondu > 0) {
            remplirRect(g, 0, 0, LARGEUR, HAUTEUR, rgba(0, 0, 0, (int)(255 * app->fondu)));
        }

        guiFinImage(g);
    }

    /* Dernière sauvegarde, puis nettoyage : combat éventuel, SDL, tableau. */
    donjonSauvegarder(&app->donjon, FICHIER_SAUVEGARDE);
    sauvegarderProgression(FICHIER_PROGRESSION, &app->prog);
    combatGuiDetruire(app->combat);
    app->combat = NULL;
    guiQuitter(g);
    donjonLiberer(&app->donjon);
    free(app);
    return EXIT_SUCCESS;
}
