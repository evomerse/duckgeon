/**
 * @file gui.h
 * @brief Couche graphique commune de l'interface SDL : fenêtre, entrées,
 *        formes vectorielles, texte (SDL_ttf) et widgets simples.
 *
 * Les widgets sont en « mode immédiat » : on les appelle à chaque image,
 * ils se dessinent et retournent s'ils ont été cliqués. Il n'y a donc
 * aucun objet d'interface à créer ni à libérer.
 */
#ifndef GUI_H
#define GUI_H

#include <SDL.h>
#include <SDL_ttf.h>

#define LARGEUR 1280
#define HAUTEUR 720
#define TOUCHES_MAX 16

/** Tailles de police disponibles. */
enum Police {
    POLICE_PETITE, POLICE_NORMALE, POLICE_MOYENNE,
    POLICE_GRANDE, POLICE_TITRE, POLICE_ENORME, NB_POLICES
};

enum Alignement { ALIGN_GAUCHE, ALIGN_CENTRE, ALIGN_DROITE };

/** Styles de boutons. */
enum StyleBouton { BOUTON_NORMAL, BOUTON_PRINCIPAL, BOUTON_DANGER, BOUTON_DISCRET };

/** Contexte graphique et état des entrées pour l'image en cours. */
typedef struct {
    SDL_Window *fenetre;
    SDL_Renderer *rendu;
    TTF_Font *polices[NB_POLICES];
    SDL_Cursor *curseurFleche;
    SDL_Cursor *curseurMain;

    /* Entrées de l'image courante (remises à zéro par guiDebutImage). */
    int sourisX, sourisY;
    int clic;            /**< bouton gauche relâché pendant l'image */
    int clicDroit;
    int appui;           /**< bouton gauche maintenu                */
    int molette;         /**< défilement (+ vers le haut)           */
    SDL_Keycode touches[TOUCHES_MAX];
    int nbTouches;
    char saisie[64];     /**< texte tapé pendant l'image (UTF-8)    */
    int quitter;

    Uint32 temps;        /**< millisecondes depuis le lancement     */
    float dt;            /**< durée de l'image précédente (s)       */
    int survolCliquable; /**< un élément cliquable est survolé      */
} Gui;

/** Construit une couleur. */
static inline SDL_Color rgba(int r, int g, int b, int a)
{
    SDL_Color c = { (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a };
    return c;
}

/* Thème de l'interface. */
#define COUL_TEXTE       rgba(236, 232, 245, 255)
#define COUL_TEXTE_DOUX  rgba(160, 156, 185, 255)
#define COUL_OR          rgba(232, 196, 110, 255)
#define COUL_OR_CLAIR    rgba(255, 228, 160, 255)
#define COUL_PANNEAU     rgba(18, 16, 36, 215)
#define COUL_BORDURE     rgba(120, 110, 170, 110)
#define COUL_ACCENT      rgba(120, 200, 255, 255)
#define COUL_VIE         rgba(110, 220, 120, 255)
#define COUL_DANGER      rgba(235, 80, 90, 255)

/* ---- Cycle de vie ---- */

/** Ouvre la fenêtre, le renderer et les polices. Retourne 0 en cas d'échec
 *  (tout ce qui a été créé est alors libéré). */
int guiInit(Gui *g, const char *titre);

/** Libère toutes les ressources SDL (textures, polices, fenêtre). */
void guiQuitter(Gui *g);

/** Lit les événements (SDL_PollEvent, non bloquant) et prépare l'image. */
void guiDebutImage(Gui *g);

/** Affiche l'image dessinée. */
void guiFinImage(Gui *g);

/** Retourne 1 si la touche a été pressée pendant cette image. */
int guiTouche(const Gui *g, SDL_Keycode touche);

/* ---- Utilitaires de couleur ---- */

SDL_Color melanger(SDL_Color a, SDL_Color b, float t);
SDL_Color assombrir(SDL_Color c, float facteur);
SDL_Color avecAlpha(SDL_Color c, int alpha);

/* ---- Formes ---- */

void remplirRect(Gui *g, float x, float y, float w, float h, SDL_Color c);
void degradeVertical(Gui *g, float x, float y, float w, float h, SDL_Color haut, SDL_Color bas);
void degradeHorizontal(Gui *g, float x, float y, float w, float h, SDL_Color gauche, SDL_Color droite);
void rectArrondi(Gui *g, float x, float y, float w, float h, float r, SDL_Color c);
void contourArrondi(Gui *g, float x, float y, float w, float h, float r, float ep, SDL_Color c);
void cercle(Gui *g, float cx, float cy, float r, SDL_Color c);
void ellipse(Gui *g, float cx, float cy, float rx, float ry, SDL_Color c);
/** Disque dont la couleur s'estompe vers le bord (lueur). */
void halo(Gui *g, float cx, float cy, float r, SDL_Color c);
/** Arc de cercle épais entre deux angles (radians, 0 = droite). */
void arc(Gui *g, float cx, float cy, float r, float ep, float debut, float fin, SDL_Color c);
/** Contour d'ellipse (socles, cercles de sélection) ; `rotation` fait
 *  tourner les pointillés si `pointilles` vaut 1. */
void anneauEllipse(Gui *g, float cx, float cy, float rx, float ry, float ep,
                   SDL_Color c, int pointilles, float rotation);
/** Polygone convexe (xy = x0,y0,x1,y1...). */
void polygone(Gui *g, const float *xy, int n, SDL_Color c);
void ligne(Gui *g, float x1, float y1, float x2, float y2, float ep, SDL_Color c);
/** Losange centré (icônes, points de compétence). */
void losange(Gui *g, float cx, float cy, float rx, float ry, SDL_Color c);

/* ---- Texte ---- */

/** Dessine du texte UTF-8 ; retourne sa largeur en pixels. */
int texte(Gui *g, enum Police p, const char *s, float x, float y, SDL_Color c, enum Alignement a);
/** Texte avec une ombre portée (lisible sur tous les fonds). */
int texteOmbre(Gui *g, enum Police p, const char *s, float x, float y, SDL_Color c, enum Alignement a);
/** Texte renvoyé à la ligne dans une largeur donnée ; retourne la hauteur. */
int texteBloc(Gui *g, enum Police p, const char *s, float x, float y, int largeur, SDL_Color c);
/** Mesure un texte. */
void tailleTexte(Gui *g, enum Police p, const char *s, int *w, int *h);

/* ---- Widgets ---- */

/** Retourne 1 si la souris est dans le rectangle. */
int survol(const Gui *g, float x, float y, float w, float h);
/** Zone cliquable invisible (change le curseur au survol). */
int zoneClic(Gui *g, float x, float y, float w, float h);
/** Panneau translucide à bord arrondi. */
void panneau(Gui *g, float x, float y, float w, float h);
/** Bouton : retourne 1 s'il a été cliqué (et qu'il est actif). */
int bouton(Gui *g, float x, float y, float w, float h, const char *libelle,
           enum StyleBouton style, int actif);
/** Sélecteur « < valeur > » : retourne -1, 0 ou +1. */
int selecteur(Gui *g, float x, float y, float w, const char *valeur);
/** Rangée de pastilles de couleur : retourne l'indice cliqué ou -1. */
int nuancier(Gui *g, float x, float y, const SDL_Color *couleurs, int n,
             int selection, float taille);
/** Barre de progression (vie, XP...). */
void barre(Gui *g, float x, float y, float w, float h, float ratio, SDL_Color plein, SDL_Color fond);

#endif /* GUI_H */
