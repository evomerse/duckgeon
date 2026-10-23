/**
 * @file gui.c
 * @brief Initialisation SDL, boucle d'événements, texte et widgets.
 */

/* [DUCKGEON - SDL] Fichier de la version graphique (./duckgeon) uniquement. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui.h"

/* ------------------------------------------------------------------ */
/*  Polices                                                           */
/* ------------------------------------------------------------------ */

/* Polices essayées dans l'ordre (la variable d'environnement DONJON_POLICE
 * ou le fichier assets/police.ttf passent en premier). */
static const char *const POLICES_TITRE[] = {
    "assets/police-titre.ttf",
    "/usr/share/fonts/julietaula-montserrat-fonts/Montserrat-ExtraBold.otf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "C:/Windows/Fonts/arialbd.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    NULL
};
static const char *const POLICES_TEXTE[] = {
    "assets/police.ttf",
    "/usr/share/fonts/julietaula-montserrat-fonts/Montserrat-SemiBold.otf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    NULL
};

static TTF_Font *ouvrirPolice(const char *const *candidats, int taille)
{
    const char *imposee = getenv("DONJON_POLICE");
    if (imposee != NULL) {
        TTF_Font *f = TTF_OpenFont(imposee, taille);
        if (f != NULL) {
            return f;
        }
    }
    for (int i = 0; candidats[i] != NULL; i++) {
        TTF_Font *f = TTF_OpenFont(candidats[i], taille);
        if (f != NULL) {
            return f;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Cache de textures de texte                                        */
/* ------------------------------------------------------------------ */

/* Créer une texture de texte à chaque image serait coûteux : on garde les
 * textes récents. Le texte est rendu en blanc puis teinté avec
 * SDL_SetTextureColorMod, donc la couleur ne fait pas partie de la clé. */
#define CACHE_MAX 512
#define CACHE_TEXTE 192

typedef struct {
    Uint32 hachage;
    int police;
    int largeurBloc;      /* 0 = une ligne, sinon largeur de retour à la ligne */
    char s[CACHE_TEXTE];
    SDL_Texture *texture;
    int w, h;
    Uint32 usage;
} EntreeCache;

static EntreeCache cache[CACHE_MAX];
static Uint32 horlogeCache = 0;

static Uint32 hacher(const char *s, int police, int largeur)
{
    Uint32 h = 2166136261u;
    for (; *s; s++) {
        h = (h ^ (unsigned char)*s) * 16777619u;
    }
    return h ^ (Uint32)(police * 31 + largeur * 7919);
}

static EntreeCache *texteCache(Gui *g, enum Police p, const char *s, int largeurBloc)
{
    if (s == NULL || s[0] == '\0' || g->polices[p] == NULL) {
        return NULL;
    }
    Uint32 h = hacher(s, p, largeurBloc);
    EntreeCache *libre = &cache[0];
    for (int i = 0; i < CACHE_MAX; i++) {
        EntreeCache *e = &cache[i];
        if (e->texture != NULL && e->hachage == h && e->police == (int)p
            && e->largeurBloc == largeurBloc && strcmp(e->s, s) == 0) {
            e->usage = ++horlogeCache;
            return e;
        }
        if (e->usage < libre->usage) {
            libre = e; /* la moins récemment utilisée */
        }
    }

    SDL_Color blanc = { 255, 255, 255, 255 };
    SDL_Surface *surf = largeurBloc > 0
        ? TTF_RenderUTF8_Blended_Wrapped(g->polices[p], s, blanc, (Uint32)largeurBloc)
        : TTF_RenderUTF8_Blended(g->polices[p], s, blanc);
    if (surf == NULL) {
        return NULL;
    }
    SDL_Texture *t = SDL_CreateTextureFromSurface(g->rendu, surf);
    int w = surf->w, hauteur = surf->h;
    SDL_FreeSurface(surf);
    if (t == NULL) {
        return NULL;
    }

    if (libre->texture != NULL) {
        SDL_DestroyTexture(libre->texture);
    }
    libre->hachage = h;
    libre->police = p;
    libre->largeurBloc = largeurBloc;
    snprintf(libre->s, CACHE_TEXTE, "%s", s);
    libre->texture = t;
    libre->w = w;
    libre->h = hauteur;
    libre->usage = ++horlogeCache;
    return libre;
}

static void viderCache(void)
{
    for (int i = 0; i < CACHE_MAX; i++) {
        if (cache[i].texture != NULL) {
            SDL_DestroyTexture(cache[i].texture);
        }
    }
    memset(cache, 0, sizeof cache);
}

/* ------------------------------------------------------------------ */
/*  Cycle de vie                                                      */
/* ------------------------------------------------------------------ */

int guiInit(Gui *g, const char *titre)
{
    memset(g, 0, sizeof *g);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
        return 0;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init : %s\n", TTF_GetError());
        SDL_Quit();
        return 0;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    g->fenetre = SDL_CreateWindow(titre, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  LARGEUR, HAUTEUR, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (g->fenetre == NULL) {
        fprintf(stderr, "SDL_CreateWindow : %s\n", SDL_GetError());
        guiQuitter(g);
        return 0;
    }
    g->rendu = SDL_CreateRenderer(g->fenetre, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g->rendu == NULL) {
        /* Repli logiciel (machines virtuelles, pilotes absents). */
        g->rendu = SDL_CreateRenderer(g->fenetre, -1, SDL_RENDERER_SOFTWARE);
    }
    if (g->rendu == NULL) {
        fprintf(stderr, "SDL_CreateRenderer : %s\n", SDL_GetError());
        guiQuitter(g);
        return 0;
    }
    /* Taille logique fixe : la fenêtre peut être redimensionnée, SDL
     * met à l'échelle et convertit aussi les coordonnées de la souris. */
    SDL_RenderSetLogicalSize(g->rendu, LARGEUR, HAUTEUR);
    SDL_SetRenderDrawBlendMode(g->rendu, SDL_BLENDMODE_BLEND);

    static const int tailles[NB_POLICES] = { 13, 16, 20, 28, 44, 84 };
    for (int i = 0; i < NB_POLICES; i++) {
        g->polices[i] = ouvrirPolice(i >= POLICE_MOYENNE ? POLICES_TITRE : POLICES_TEXTE, tailles[i]);
        if (g->polices[i] == NULL) {
            fprintf(stderr, "Aucune police trouvee. Placez un fichier .ttf dans "
                            "assets/police.ttf ou definissez DONJON_POLICE.\n");
            guiQuitter(g);
            return 0;
        }
    }

    g->curseurFleche = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    g->curseurMain = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    g->temps = SDL_GetTicks();
    return 1;
}

void guiQuitter(Gui *g)
{
    viderCache();
    for (int i = 0; i < NB_POLICES; i++) {
        if (g->polices[i] != NULL) {
            TTF_CloseFont(g->polices[i]);
            g->polices[i] = NULL;
        }
    }
    if (g->curseurFleche != NULL) SDL_FreeCursor(g->curseurFleche);
    if (g->curseurMain != NULL) SDL_FreeCursor(g->curseurMain);
    g->curseurFleche = g->curseurMain = NULL;
    if (g->rendu != NULL) {
        SDL_DestroyRenderer(g->rendu);
        g->rendu = NULL;
    }
    if (g->fenetre != NULL) {
        SDL_DestroyWindow(g->fenetre);
        g->fenetre = NULL;
    }
    TTF_Quit();
    SDL_Quit();
}

void guiDebutImage(Gui *g)
{
    Uint32 maintenant = SDL_GetTicks();
    g->dt = (maintenant - g->temps) / 1000.0f;
    if (g->dt > 0.1f) {
        g->dt = 0.1f; /* évite les sauts après une pause */
    }
    g->temps = maintenant;

    g->clic = g->clicDroit = g->molette = 0;
    g->nbTouches = 0;
    g->saisie[0] = '\0';
    g->survolCliquable = 0;

    /* Boucle d'événements non bloquante : on traite tout ce qui est en
     * attente puis on rend la main pour dessiner l'image. */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            g->quitter = 1;
            break;
        case SDL_MOUSEMOTION:
            g->sourisX = e.motion.x;
            g->sourisY = e.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            g->sourisX = e.button.x;
            g->sourisY = e.button.y;
            if (e.button.button == SDL_BUTTON_LEFT) g->appui = 1;
            break;
        case SDL_MOUSEBUTTONUP:
            g->sourisX = e.button.x;
            g->sourisY = e.button.y;
            if (e.button.button == SDL_BUTTON_LEFT) {
                g->appui = 0;
                g->clic = 1;
            } else if (e.button.button == SDL_BUTTON_RIGHT) {
                g->clicDroit = 1;
            }
            break;
        case SDL_MOUSEWHEEL:
            g->molette += e.wheel.y;
            break;
        case SDL_KEYDOWN:
            if (g->nbTouches < TOUCHES_MAX) {
                g->touches[g->nbTouches++] = e.key.keysym.sym;
            }
            break;
        case SDL_TEXTINPUT:
            if (strlen(g->saisie) + strlen(e.text.text) < sizeof g->saisie) {
                strcat(g->saisie, e.text.text);
            }
            break;
        default:
            break;
        }
    }
}

/** Enregistre l'image affichée dans un fichier BMP (touche F12). */
static void capturerEcran(Gui *g)
{
    int w, h;
    if (SDL_GetRendererOutputSize(g->rendu, &w, &h) != 0) {
        return;
    }
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (s == NULL) {
        return;
    }
    if (SDL_RenderReadPixels(g->rendu, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0) {
        char nom[64];
        snprintf(nom, sizeof nom, "capture-%u.bmp", (unsigned)SDL_GetTicks());
        if (SDL_SaveBMP(s, nom) == 0) {
            printf("Capture enregistree : %s\n", nom);
        }
    }
    SDL_FreeSurface(s);
}

void guiFinImage(Gui *g)
{
    if (guiTouche(g, SDLK_F12)) {
        capturerEcran(g);
    }
    SDL_SetCursor(g->survolCliquable ? g->curseurMain : g->curseurFleche);
    SDL_RenderPresent(g->rendu);
}

int guiTouche(const Gui *g, SDL_Keycode touche)
{
    for (int i = 0; i < g->nbTouches; i++) {
        if (g->touches[i] == touche) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Texte                                                             */
/* ------------------------------------------------------------------ */

static void dessinerEntree(Gui *g, EntreeCache *e, float x, float y, SDL_Color c, enum Alignement a)
{
    SDL_FRect dst = { x, y, (float)e->w, (float)e->h };
    if (a == ALIGN_CENTRE) dst.x -= e->w / 2.0f;
    else if (a == ALIGN_DROITE) dst.x -= e->w;
    SDL_SetTextureColorMod(e->texture, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(e->texture, c.a);
    SDL_RenderCopyF(g->rendu, e->texture, NULL, &dst);
}

int texte(Gui *g, enum Police p, const char *s, float x, float y, SDL_Color c, enum Alignement a)
{
    EntreeCache *e = texteCache(g, p, s, 0);
    if (e == NULL) {
        return 0;
    }
    dessinerEntree(g, e, x, y, c, a);
    return e->w;
}

int texteOmbre(Gui *g, enum Police p, const char *s, float x, float y, SDL_Color c, enum Alignement a)
{
    EntreeCache *e = texteCache(g, p, s, 0);
    if (e == NULL) {
        return 0;
    }
    float d = p >= POLICE_GRANDE ? 3.0f : 1.5f;
    dessinerEntree(g, e, x + d, y + d, rgba(0, 0, 0, c.a * 3 / 4), a);
    dessinerEntree(g, e, x, y, c, a);
    return e->w;
}

int texteBloc(Gui *g, enum Police p, const char *s, float x, float y, int largeur, SDL_Color c)
{
    EntreeCache *e = texteCache(g, p, s, largeur);
    if (e == NULL) {
        return 0;
    }
    dessinerEntree(g, e, x, y, c, ALIGN_GAUCHE);
    return e->h;
}

void tailleTexte(Gui *g, enum Police p, const char *s, int *w, int *h)
{
    EntreeCache *e = texteCache(g, p, s, 0);
    if (w != NULL) *w = e != NULL ? e->w : 0;
    if (h != NULL) *h = e != NULL ? e->h : TTF_FontHeight(g->polices[p]);
}

/* ------------------------------------------------------------------ */
/*  Widgets                                                           */
/* ------------------------------------------------------------------ */

int survol(const Gui *g, float x, float y, float w, float h)
{
    return g->sourisX >= x && g->sourisX < x + w && g->sourisY >= y && g->sourisY < y + h;
}

int zoneClic(Gui *g, float x, float y, float w, float h)
{
    if (!survol(g, x, y, w, h)) {
        return 0;
    }
    g->survolCliquable = 1;
    return g->clic;
}

void panneau(Gui *g, float x, float y, float w, float h)
{
    rectArrondi(g, x + 3, y + 5, w, h, 14, rgba(0, 0, 0, 90));
    rectArrondi(g, x, y, w, h, 14, COUL_PANNEAU);
    contourArrondi(g, x, y, w, h, 14, 1.5f, COUL_BORDURE);
}

int bouton(Gui *g, float x, float y, float w, float h, const char *libelle,
           enum StyleBouton style, int actif)
{
    int dessus = actif && survol(g, x, y, w, h);
    int enfonce = dessus && g->appui;
    if (dessus) {
        g->survolCliquable = 1;
    }

    SDL_Color fond, bord, coulTexte = COUL_TEXTE;
    switch (style) {
    case BOUTON_PRINCIPAL:
        fond = rgba(200, 160, 80, 255);
        bord = COUL_OR_CLAIR;
        coulTexte = rgba(30, 20, 10, 255);
        break;
    case BOUTON_DANGER:
        fond = rgba(150, 40, 55, 255);
        bord = rgba(255, 120, 130, 200);
        break;
    case BOUTON_DISCRET:
        fond = rgba(40, 36, 70, 160);
        bord = rgba(140, 130, 190, 90);
        break;
    default:
        fond = rgba(55, 48, 100, 235);
        bord = rgba(170, 160, 230, 150);
        break;
    }
    if (!actif) {
        fond = rgba(45, 42, 60, 200);
        bord = rgba(90, 88, 110, 120);
        coulTexte = rgba(120, 118, 140, 255);
    } else if (dessus) {
        fond = melanger(fond, rgba(255, 255, 255, fond.a), 0.18f);
    }

    float dy = enfonce ? 2.0f : 0.0f;
    if (dessus && !enfonce) {
        rectArrondi(g, x - 3, y - 3, w + 6, h + 6, h / 2 + 3, avecAlpha(bord, 50));
    }
    rectArrondi(g, x, y + dy, w, h, h / 2, fond);
    /* reflet en haut du bouton */
    rectArrondi(g, x + 4, y + dy + 2, w - 8, h / 2 - 2, h / 4, rgba(255, 255, 255, actif ? 22 : 8));
    contourArrondi(g, x, y + dy, w, h, h / 2, 1.5f, bord);

    int th;
    tailleTexte(g, POLICE_NORMALE, libelle, NULL, &th);
    texte(g, POLICE_NORMALE, libelle, x + w / 2, y + dy + (h - th) / 2, coulTexte, ALIGN_CENTRE);

    return dessus && g->clic;
}

int selecteur(Gui *g, float x, float y, float w, const char *valeur)
{
    const float h = 30;
    int resultat = 0;
    rectArrondi(g, x, y, w, h, h / 2, rgba(30, 27, 55, 230));
    contourArrondi(g, x, y, w, h, h / 2, 1.2f, COUL_BORDURE);

    for (int sens = -1; sens <= 1; sens += 2) {
        float bx = sens < 0 ? x + 2 : x + w - 28;
        int dessus = survol(g, bx, y + 2, 26, h - 4);
        cercle(g, bx + 13, y + h / 2, 12, dessus ? rgba(120, 105, 200, 255) : rgba(70, 62, 120, 255));
        float cx = bx + 13, cy = y + h / 2;
        float xy[6] = { cx + 4 * sens, cy, cx - 3 * sens, cy - 6, cx - 3 * sens, cy + 6 };
        polygone(g, xy, 3, COUL_TEXTE);
        if (dessus) {
            g->survolCliquable = 1;
            if (g->clic) resultat = sens;
        }
    }
    int th;
    tailleTexte(g, POLICE_NORMALE, valeur, NULL, &th);
    texte(g, POLICE_NORMALE, valeur, x + w / 2, y + (h - th) / 2, COUL_TEXTE, ALIGN_CENTRE);
    return resultat;
}

int nuancier(Gui *g, float x, float y, const SDL_Color *couleurs, int n, int selection, float taille)
{
    int clique = -1;
    for (int i = 0; i < n; i++) {
        float cx = x + i * (taille + 5) + taille / 2;
        float cy = y + taille / 2;
        int dessus = survol(g, cx - taille / 2, cy - taille / 2, taille, taille);
        if (i == selection) {
            cercle(g, cx, cy, taille / 2 + 4, COUL_OR_CLAIR);
            cercle(g, cx, cy, taille / 2 + 2, rgba(20, 18, 40, 255));
        } else if (dessus) {
            cercle(g, cx, cy, taille / 2 + 3, rgba(255, 255, 255, 140));
        }
        cercle(g, cx, cy, taille / 2, couleurs[i]);
        cercle(g, cx - taille / 7, cy - taille / 7, taille / 6, rgba(255, 255, 255, 60));
        if (dessus) {
            g->survolCliquable = 1;
            if (g->clic) clique = i;
        }
    }
    return clique;
}

void barre(Gui *g, float x, float y, float w, float h, float ratio, SDL_Color plein, SDL_Color fond)
{
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    rectArrondi(g, x, y, w, h, h / 2, fond);
    if (ratio > 0) {
        float lw = w * ratio;
        if (lw < h) lw = h; /* garde une forme arrondie */
        rectArrondi(g, x, y, lw, h, h / 2, plein);
        rectArrondi(g, x + 2, y + 1, lw - 4, h / 2.5f, h / 4, rgba(255, 255, 255, 50));
    }
}
