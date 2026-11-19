/**
 * @file sdl_ui.h
 * @brief Mini bibliothèque d'interface pour Donjon SDL (partie 3) :
 *        fenêtre, texte, boutons et barres, en mode « immédiat » (chaque
 *        image redessine tout, un bouton retourne 1 quand on clique dessus).
 *
 * Indépendant de Duckgeon : aucun fichier gui_*.c n'est utilisé ici.
 */

/* [DONJON SDL - partie 3] Fichier de la version graphique simple. */
#ifndef SDL_UI_H
#define SDL_UI_H

#include <SDL.h>
#include <SDL_ttf.h>

#define UI_LARGEUR 1000
#define UI_HAUTEUR 640

/** État de la fenêtre et des entrées pour l'image en cours. */
typedef struct {
    SDL_Window *fenetre;
    SDL_Renderer *rend;
    TTF_Font *petite;  /**< Détails (14 px)   */
    TTF_Font *normale; /**< Texte (18 px)     */
    TTF_Font *grande;  /**< Titres (30 px)    */

    int sx, sy;        /**< Souris (coordonnées logiques)              */
    int clic;          /**< 1 : clic gauche relâché pendant cette image */
    int appui;         /**< 1 : bouton gauche enfoncé                   */
    int molette;       /**< Défilement de la molette (+1 haut, -1 bas)  */
    int touche;        /**< Dernière touche pressée (SDLK_*), 0 sinon   */
    char texte[32];    /**< Texte tapé pendant cette image              */
    int quitter;       /**< 1 : la fenêtre a été fermée                 */
} Ui;

/* Palette commune. */
extern const SDL_Color UI_FOND, UI_PANNEAU, UI_PANNEAU2, UI_BORD, UI_TEXTE, UI_GRIS, UI_OR,
    UI_VERT, UI_ROUGE, UI_BLEU, UI_ORANGE;

/** Crée la fenêtre, le moteur de rendu et les polices. Retourne 1 si OK. */
int uiInit(Ui *ui, const char *titre);

/** Libère tout (fenêtre, polices, SDL). */
void uiFermer(Ui *ui);

/** Lit les événements de l'image (souris, clavier, fermeture). */
void uiEvenements(Ui *ui);

/** Efface l'écran avec la couleur de fond. */
void uiDebut(Ui *ui);

/** Affiche l'image dessinée. */
void uiFin(Ui *ui);

/** Retourne 1 si le point (x, y) est dans le rectangle. */
int uiDans(SDL_Rect r, int x, int y);

/** Rectangle plein. */
void uiRect(Ui *ui, SDL_Rect r, SDL_Color c);

/** Rectangle avec contour. */
void uiCadre(Ui *ui, SDL_Rect r, SDL_Color fond, SDL_Color bord);

/** Largeur en pixels d'un texte avec cette police. */
int uiLargeur(TTF_Font *f, const char *texte);

/** Texte aligné à gauche (x, y = coin haut gauche). */
void uiTexte(Ui *ui, TTF_Font *f, const char *texte, int x, int y, SDL_Color c);

/** Texte aligné à droite (x = bord droit). */
void uiTexteDroite(Ui *ui, TTF_Font *f, const char *texte, int x, int y, SDL_Color c);

/** Texte centré dans un rectangle. */
void uiTexteCentre(Ui *ui, TTF_Font *f, const char *texte, SDL_Rect r, SDL_Color c);

/** Paragraphe qui retourne à la ligne dans une largeur donnée. Retourne la
 *  hauteur utilisée. */
int uiParagraphe(Ui *ui, TTF_Font *f, const char *texte, int x, int y, int largeur, SDL_Color c);

/**
 * Bouton. Retourne 1 si on vient de cliquer dessus (et consomme le clic).
 * @param actif 0 : grisé et insensible
 */
int uiBouton(Ui *ui, SDL_Rect r, const char *texte, int actif);

/** Comme uiBouton mais « sélectionné » (contour doré) quand choisi vaut 1. */
int uiOption(Ui *ui, SDL_Rect r, const char *texte, int choisi, int actif);

/** Barre de progression (val sur max). */
void uiBarre(Ui *ui, SDL_Rect r, int val, int max, SDL_Color c);

/**
 * Personnage « cube » : un carré de couleur avec un petit relief et deux
 * yeux. `droite` = 1 : il regarde vers la droite (héros), 0 : vers la gauche.
 * `lettre` (1 caractère, peut être vide) est écrite dessus.
 */
void uiCube(Ui *ui, SDL_Rect r, SDL_Color c, const char *lettre, int droite);

/** Losange plein de centre (cx, cy) et de demi-largeur rayon. */
void uiLosange(Ui *ui, int cx, int cy, int rayon, SDL_Color c);

/** Repère triangulaire pointant vers le bas (cible). */
void uiRepere(Ui *ui, int cx, int y, int taille, SDL_Color c);

/** Couleur plus claire / plus sombre (pourcentage, 100 = inchangée). */
SDL_Color uiTeinte(SDL_Color c, int pourcent);

/** Couleur associée à un élément (0..6, voir enum Element). */
SDL_Color uiCouleurElement(int element);

#endif /* SDL_UI_H */
