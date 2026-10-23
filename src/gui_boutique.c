/**
 * @file gui_boutique.c
 * @brief Écran « Boutique » : dépenser le foie gras gagné au combat pour
 *        débloquer des cosmétiques, essayés en direct sur un héros.
 *
 * Toute la logique (prix, achat, déblocage) est dans boutique.c ; cet
 * écran ne fait que l'afficher et la piloter.
 */

/* [DUCKGEON - SDL] Fichier de la version graphique (./duckgeon) uniquement. */
#include <stdio.h>

#include "gui_app.h"
#include "gui_sprite.h"
#include "boutique.h"

static char messageBoutique[96];
static float dureeMessage = 0;

static void annoncerBoutique(const char *message)
{
    snprintf(messageBoutique, sizeof messageBoutique, "%s", message);
    dureeMessage = 2.5f;
}

/** Copie du héros portant l'article (pour l'aperçu, sans le modifier). */
static Personnage essayage(const Personnage *p, const Cosmetique *c)
{
    Personnage copie = *p;
    if (c->type == COSMETIQUE_COIFFE) {
        copie.apparence.coiffe = c->valeur;
    } else {
        copie.apparence.plumage = c->valeur;
    }
    return copie;
}

/** Carte d'un article ; retourne 1 si elle est cliquée. */
static int carteArticle(App *app, int i, const Personnage *modele, float x, float y, float w, float h)
{
    Gui *g = &app->g;
    const Cosmetique *c = cosmetique(i);
    int achete = cosmetiqueAchete(&app->prog, i);
    int choisi = app->articleBoutique == i;
    int dessus = survol(g, x, y, w, h);

    rectArrondi(g, x, y, w, h, 14, dessus ? rgba(46, 40, 86, 240) : rgba(28, 24, 54, 230));
    contourArrondi(g, x, y, w, h, 14, choisi ? 2.5f : 1.2f, choisi ? COUL_OR_CLAIR : COUL_BORDURE);

    /* miniature : le héros sélectionné porte l'article */
    float r = h / 2 - 14;
    if (modele != NULL) {
        Personnage apercu = essayage(modele, c);
        portraitHeros(g, &apercu, x + 14 + r, y + h / 2, r, g->temps / 1000.0f);
    }

    float tx = x + 30 + 2 * r;
    texte(g, POLICE_MOYENNE, c->nom, tx, y + 12, COUL_TEXTE, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, c->type == COSMETIQUE_COIFFE ? "Coiffe" : "Plumage", tx, y + 42,
          COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, c->description, tx, y + 62, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

    char prix[32];
    if (achete) {
        texte(g, POLICE_NORMALE, "Acheté", x + w - 16, y + h - 32, COUL_VIE, ALIGN_DROITE);
    } else {
        snprintf(prix, sizeof prix, "%d", c->prix);
        SDL_Color cp = app->prog.foieGras >= c->prix ? COUL_OR_CLAIR : COUL_DANGER;
        texte(g, POLICE_NORMALE, prix, x + w - 16, y + h - 32, cp, ALIGN_DROITE);
        int lw;
        tailleTexte(g, POLICE_NORMALE, prix, &lw, NULL);
        iconeFoieGras(g, x + w - 30 - lw, y + h - 21, 9);
        iconeCadenas(g, x + w - 22, y + 22, 8);
    }

    if (dessus) {
        g->survolCliquable = 1;
    }
    return dessus && g->clic;
}

void ecranBoutique(App *app)
{
    Gui *g = &app->g;
    Donjon *d = &app->donjon;
    float t = g->temps / 1000.0f;
    dessinerFond(g, rgba(30, 16, 30, 255), rgba(50, 30, 40, 255));

    if (enTete(app, "Boutique", "Dépensez votre foie gras : chaque cosmétique sert à tous vos héros")) {
        changerEcran(app, ECRAN_MENU);
    }

    /* réserve */
    char fg[48];
    snprintf(fg, sizeof fg, "%d foie gras", app->prog.foieGras);
    iconeFoieGras(g, 1080, 48, 14);
    texteOmbre(g, POLICE_MOYENNE, fg, 1104, 34, COUL_OR_CLAIR, ALIGN_GAUCHE);

    if (app->articleBoutique < 0 || app->articleBoutique >= NB_COSMETIQUES) {
        app->articleBoutique = 0;
    }
    if (app->selection < 0 || app->selection >= d->taille) {
        app->selection = 0;
    }
    const Personnage *heros = d->taille > 0 ? &d->elements[app->selection] : NULL;
    const Cosmetique *article = cosmetique(app->articleBoutique);

    /* aperçu : le héros choisi essaie l'article sélectionné */
    panneau(g, 30, 100, 420, 600);
    if (heros != NULL) {
        Personnage apercu = essayage(heros, article);
        halo(g, 240, 380, 240, rgba(255, 190, 120, 50));
        ellipse(g, 240, 566, 150, 30, rgba(30, 24, 56, 255));
        ellipse(g, 240, 560, 140, 24, rgba(76, 64, 124, 255));
        Pose pose = poseDefaut(t);
        dessinerHeros(g, &apercu, 240, 560, 2.15f, &pose);

        texte(g, POLICE_PETITE, "Essayer sur", 240, 116, COUL_TEXTE_DOUX, ALIGN_CENTRE);
        int dh = selecteur(g, 80, 138, 320, heros->nom);
        if (dh != 0 && d->taille > 0) {
            app->selection = (app->selection + d->taille + dh) % d->taille;
        }
    } else {
        texte(g, POLICE_NORMALE, "Aucun héros pour essayer.", 240, 380, COUL_TEXTE_DOUX, ALIGN_CENTRE);
    }

    int achete = cosmetiqueAchete(&app->prog, app->articleBoutique);
    char lib[64];
    if (achete) {
        snprintf(lib, sizeof lib, "Déjà acheté");
    } else {
        snprintf(lib, sizeof lib, "Acheter (%d foie gras)", article->prix);
    }
    if (bouton(g, 50, 600, 380, 40, lib, BOUTON_PRINCIPAL, !achete && app->prog.foieGras >= article->prix)) {
        if (acheterCosmetique(&app->prog, app->articleBoutique)) {
            char msg[96];
            snprintf(msg, sizeof msg, "%s débloqué !", article->nom);
            annoncerBoutique(msg);
        }
    }
    if (heros != NULL) {
        snprintf(lib, sizeof lib, "Équiper sur %s", heros->nom);
        if (bouton(g, 50, 648, 380, 40, lib, BOUTON_NORMAL, achete)) {
            d->elements[app->selection] = essayage(heros, article);
            char msg[96];
            snprintf(msg, sizeof msg, "%s porte maintenant : %s", heros->nom, article->nom);
            annoncerBoutique(msg);
        }
    }

    /* articles (2 colonnes) */
    const float cw = 380, ch = 120, gap = 18;
    for (int i = 0; i < NB_COSMETIQUES; i++) {
        float x = 470 + (i % 2) * (cw + gap), y = 100 + (i / 2) * (ch + gap);
        if (carteArticle(app, i, heros, x, y, cw, ch)) {
            app->articleBoutique = i;
        }
    }
    if (guiTouche(g, SDLK_RIGHT)) app->articleBoutique = (app->articleBoutique + 1) % NB_COSMETIQUES;
    if (guiTouche(g, SDLK_LEFT)) app->articleBoutique = (app->articleBoutique + NB_COSMETIQUES - 1) % NB_COSMETIQUES;

    texte(g, POLICE_PETITE, "Le foie gras se gagne en nettoyant des étages (bonus la première fois, double avec un boss).",
          470 + cw + gap / 2, 660, COUL_TEXTE_DOUX, ALIGN_CENTRE);

    /* message temporaire */
    if (dureeMessage > 0) {
        dureeMessage -= g->dt;
        int alpha = dureeMessage > 0.5f ? 255 : (int)(510 * dureeMessage);
        int w;
        tailleTexte(g, POLICE_NORMALE, messageBoutique, &w, NULL);
        float mx = 470 + cw + gap / 2;
        rectArrondi(g, mx - w / 2.0f - 20, 646, w + 40, 40, 20, rgba(20, 16, 40, alpha * 9 / 10));
        contourArrondi(g, mx - w / 2.0f - 20, 646, w + 40, 40, 20, 1.5f, avecAlpha(COUL_OR, alpha));
        texte(g, POLICE_NORMALE, messageBoutique, mx, 656, avecAlpha(COUL_TEXTE, alpha), ALIGN_CENTRE);
    }
}
