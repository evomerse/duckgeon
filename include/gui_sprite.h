/**
 * @file gui_sprite.h
 * @brief Dessin vectoriel des héros (des canards, selon leur apparence) et
 *        des monstres.
 *
 * Aucune image n'est chargée : chaque personnage est construit à partir
 * de formes simples, ce qui permet de changer chaque détail (coupe, couleur,
 * coiffe, arme de classe...) instantanément dans l'éditeur.
 */

/* [DUCKGEON - SDL] Fichier de la version graphique (./duckgeon) uniquement. */
#ifndef GUI_SPRITE_H
#define GUI_SPRITE_H

#include "gui.h"
#include "personnage.h"
#include "combat.h"

/** Paramètres d'animation d'un sprite. */
typedef struct {
    float flash;    /**< 0..1 : blanchit le sprite (coup reçu)         */
    float alpha;    /**< 0..1 : transparence (K.O., apparition)        */
    float attaque;  /**< 0..1 : bras armé levé                         */
    int sens;       /**< 1 = regarde à droite, -1 = à gauche           */
    float temps;    /**< secondes (respiration, cape, aura...)         */
} Pose;

/** Pose par défaut (regarde à droite, opaque). */
Pose poseDefaut(float temps);

/* Palettes et libellés des options d'apparence. */
extern const SDL_Color PALETTE[NB_COULEURS];
extern const SDL_Color PLUMAGES[NB_PLUMAGES];
extern const char *const NOMS_COUPES[NB_COUPES];
extern const char *const NOMS_COIFFES[NB_COIFFES];
extern const char *const NOMS_PILOSITES[NB_PILOSITES];
extern const char *const NOMS_MARQUES[NB_MARQUES];
extern const char *const NOMS_CORPULENCES[NB_CORPULENCES];
extern const char *const NOMS_REGARDS[NB_REGARDS];

/** Libellé accentué d'une classe (« Prêtre », « Nécromancien »...). */
const char *libelleClasse(enum Classe c);
/** Couleur emblématique d'une classe. */
SDL_Color couleurClasse(enum Classe c);
/** Couleur d'un élément. */
SDL_Color couleurElement(enum Element e);

/** Petite pastille représentant un élément (rayon r). */
void iconeElement(Gui *g, enum Element e, float cx, float cy, float r);

/**
 * « Fort contre [icônes] · Faible contre [icônes] » pour un élément, à
 * partir de (x, y) : sur une ligne (retourne la largeur) ou en colonne si
 * `colonne` vaut 1 (retourne la hauteur).
 */
float afficherAffinites(Gui *g, enum Element e, float x, float y, int colonne);

/** Dessine un héros, pieds en (x, y), à l'échelle s (1 = ~170 px). */
void dessinerHeros(Gui *g, const Personnage *p, float x, float y, float s, const Pose *pose);

/** Dessine un monstre, pieds en (x, y). */
void dessinerMonstre(Gui *g, enum Monstre m, float x, float y, float s, const Pose *pose);

/** Hauteur approximative d'un monstre à l'échelle 1 (placement des barres). */
float hauteurMonstre(enum Monstre m);

/** Portrait (tête cadrée) d'un héros dans un cercle de rayon r. */
void portraitHeros(Gui *g, const Personnage *p, float cx, float cy, float r, float temps);

/** Portrait d'un monstre dans un cercle de rayon r. */
void portraitMonstre(Gui *g, enum Monstre m, float cx, float cy, float r, float temps);

/** Petit cadenas (cosmétique verrouillé), centré, hauteur ~2r. */
void iconeCadenas(Gui *g, float cx, float cy, float r);

/** Petite icône de foie gras (la monnaie des cosmétiques), centrée. */
void iconeFoieGras(Gui *g, float cx, float cy, float r);

#endif /* GUI_SPRITE_H */
