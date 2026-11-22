/**
 * @file sdl_ui.c
 * @brief Mini bibliothèque d'interface de Donjon SDL (voir sdl_ui.h).
 */

/* [DONJON SDL - partie 3] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdl_ui.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const SDL_Color UI_FOND = { 18, 18, 20, 255 };
const SDL_Color UI_PANNEAU = { 34, 34, 38, 255 };
const SDL_Color UI_PANNEAU2 = { 54, 54, 60, 255 };
const SDL_Color UI_BORD = { 98, 98, 106, 255 };
const SDL_Color UI_TEXTE = { 232, 234, 242, 255 };
const SDL_Color UI_GRIS = { 152, 152, 160, 255 };
const SDL_Color UI_OR = { 244, 190, 62, 255 };
const SDL_Color UI_VERT = { 96, 204, 124, 255 };
const SDL_Color UI_ROUGE = { 232, 84, 84, 255 };
const SDL_Color UI_BLEU = { 92, 164, 244, 255 };
const SDL_Color UI_ORANGE = { 240, 150, 60, 255 };

/* Polices essayées dans l'ordre (DONJON_POLICE passe en premier). */
static const char *const POLICES[] = {
    "assets/police.ttf",
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    NULL
};

static TTF_Font *ouvrirPolice(int taille)
{
    const char *imposee = getenv("DONJON_POLICE");
    if (imposee != NULL) {
        TTF_Font *f = TTF_OpenFont(imposee, taille);
        if (f != NULL) {
            return f;
        }
    }
    for (int i = 0; POLICES[i] != NULL; i++) {
        TTF_Font *f = TTF_OpenFont(POLICES[i], taille);
        if (f != NULL) {
            return f;
        }
    }
    return NULL;
}

int uiInit(Ui *ui, const char *titre)
{
    if (ui == NULL) {
        return 0;
    }
    memset(ui, 0, sizeof *ui);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
        return 0;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init : %s\n", TTF_GetError());
        SDL_Quit();
        return 0;
    }
    ui->fenetre = SDL_CreateWindow(titre, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, UI_LARGEUR,
                                   UI_HAUTEUR, SDL_WINDOW_RESIZABLE);
    if (ui->fenetre != NULL) {
        ui->rend = SDL_CreateRenderer(ui->fenetre, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ui->rend == NULL) {
            ui->rend = SDL_CreateRenderer(ui->fenetre, -1, SDL_RENDERER_SOFTWARE);
        }
    }
    ui->petite = ouvrirPolice(14);
    ui->normale = ouvrirPolice(18);
    ui->grande = ouvrirPolice(30);
    ui->enorme = ouvrirPolice(50);
    if (ui->rend == NULL || ui->petite == NULL || ui->normale == NULL || ui->grande == NULL || ui->enorme == NULL) {
        fprintf(stderr, "Fenetre ou police introuvable (%s). Indiquez une police avec DONJON_POLICE=fichier.ttf\n",
                SDL_GetError());
        uiFermer(ui);
        return 0;
    }
    SDL_RenderSetLogicalSize(ui->rend, UI_LARGEUR, UI_HAUTEUR);
    SDL_SetRenderDrawBlendMode(ui->rend, SDL_BLENDMODE_BLEND);
    SDL_StartTextInput();
    return 1;
}

void uiFermer(Ui *ui)
{
    if (ui == NULL) {
        return;
    }
    if (ui->petite) TTF_CloseFont(ui->petite);
    if (ui->normale) TTF_CloseFont(ui->normale);
    if (ui->grande) TTF_CloseFont(ui->grande);
    if (ui->enorme) TTF_CloseFont(ui->enorme);
    if (ui->rend) SDL_DestroyRenderer(ui->rend);
    if (ui->fenetre) SDL_DestroyWindow(ui->fenetre);
    ui->petite = ui->normale = ui->grande = ui->enorme = NULL;
    ui->rend = NULL;
    ui->fenetre = NULL;
    TTF_Quit();
    SDL_Quit();
}

void uiEvenements(Ui *ui)
{
    if (ui == NULL) {
        return;
    }
    ui->temps = SDL_GetTicks();
    ui->clic = 0;
    ui->molette = 0;
    ui->touche = 0;
    ui->texte[0] = '\0';

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            ui->quitter = 1;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                ui->appui = 1;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) {
                ui->appui = 0;
                ui->clic = 1;
            }
            break;
        case SDL_MOUSEWHEEL:
            ui->molette += e.wheel.y > 0 ? 1 : (e.wheel.y < 0 ? -1 : 0);
            break;
        case SDL_KEYDOWN:
            ui->touche = e.key.keysym.sym;
            break;
        case SDL_TEXTINPUT:
            strncat(ui->texte, e.text.text, sizeof ui->texte - strlen(ui->texte) - 1);
            break;
        default:
            break;
        }
    }
    /* Position de la souris, ramenée aux coordonnées logiques. */
    int px, py;
    SDL_GetMouseState(&px, &py);
    float lx, ly;
    SDL_RenderWindowToLogical(ui->rend, px, py, &lx, &ly);
    ui->sx = (int)lx;
    ui->sy = (int)ly;
}

void uiDebut(Ui *ui)
{
    /* On efface toute la fenêtre (bandes noires comprises) et on coupe ce qui
     * dépasserait de la zone logique (tremblements, effets de caméra) :
     * sinon des restes d'anciennes images restent dans les marges. */
    SDL_RenderSetClipRect(ui->rend, NULL);
    SDL_SetRenderDrawColor(ui->rend, 0, 0, 0, 255);
    SDL_RenderClear(ui->rend);
    SDL_Rect zone = { 0, 0, UI_LARGEUR, UI_HAUTEUR };
    SDL_RenderSetClipRect(ui->rend, &zone);
    /* Dégradé gris anthracite (haut sombre, bas un peu plus clair). */
    for (int y = 0; y < UI_HAUTEUR; y += 4) {
        int t = y * 100 / UI_HAUTEUR;
        Uint8 v = (Uint8)(20 + t * 14 / 100);
        SDL_SetRenderDrawColor(ui->rend, v, v, (Uint8)(v + 2), 255);
        SDL_Rect bande = { 0, y, UI_LARGEUR, 4 };
        SDL_RenderFillRect(ui->rend, &bande);
    }
    /* Étoiles fixes qui scintillent doucement. */
    unsigned g = 12345u;
    for (int k = 0; k < 70; k++) {
        g = g * 1103515245u + 12345u;
        int x = (int)((g >> 8) % UI_LARGEUR);
        g = g * 1103515245u + 12345u;
        int y = (int)((g >> 8) % UI_HAUTEUR);
        int vive = 60 + (int)(50 * (1 + sin(ui->temps / 700.0 + k)));
        SDL_SetRenderDrawColor(ui->rend, 240, 240, 240, (Uint8)vive);
        SDL_Rect e = { x, y, k % 5 == 0 ? 2 : 1, k % 5 == 0 ? 2 : 1 };
        SDL_RenderFillRect(ui->rend, &e);
    }
}

void uiFin(Ui *ui)
{
    SDL_RenderPresent(ui->rend);
}

int uiDans(SDL_Rect r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void uiRect(Ui *ui, SDL_Rect r, SDL_Color c)
{
    SDL_SetRenderDrawColor(ui->rend, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(ui->rend, &r);
}

/** Décalage horizontal d'un coin arrondi pour la ligne dy (0 = haut). */
static int decalageCoin(int dy, int h, int rayon)
{
    int d = dy < rayon ? rayon - dy : (dy >= h - rayon ? dy - (h - rayon) + 1 : 0);
    if (d <= 0) {
        return 0;
    }
    double dx = sqrt((double)rayon * rayon - (double)(d - 0.5) * (d - 0.5));
    return rayon - (int)(dx + 0.5);
}

static SDL_Color melange(SDL_Color a, SDL_Color b, int pourcent)
{
    SDL_Color c;
    c.r = (Uint8)(a.r + (b.r - a.r) * pourcent / 100);
    c.g = (Uint8)(a.g + (b.g - a.g) * pourcent / 100);
    c.b = (Uint8)(a.b + (b.b - a.b) * pourcent / 100);
    c.a = (Uint8)(a.a + (b.a - a.a) * pourcent / 100);
    return c;
}

void uiRectDegrade(Ui *ui, SDL_Rect r, int rayon, SDL_Color haut, SDL_Color bas)
{
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    if (rayon * 2 > r.h) rayon = r.h / 2;
    if (rayon * 2 > r.w) rayon = r.w / 2;
    for (int dy = 0; dy < r.h; dy++) {
        int in = decalageCoin(dy, r.h, rayon);
        SDL_Color c = melange(haut, bas, r.h > 1 ? dy * 100 / (r.h - 1) : 0);
        SDL_SetRenderDrawColor(ui->rend, c.r, c.g, c.b, c.a);
        SDL_RenderDrawLine(ui->rend, r.x + in, r.y + dy, r.x + r.w - 1 - in, r.y + dy);
    }
}

void uiRectArrondi(Ui *ui, SDL_Rect r, int rayon, SDL_Color c)
{
    uiRectDegrade(ui, r, rayon, c, c);
}

void uiDisque(Ui *ui, int cx, int cy, int rayon, SDL_Color c)
{
    SDL_SetRenderDrawColor(ui->rend, c.r, c.g, c.b, c.a);
    for (int dy = -rayon; dy <= rayon; dy++) {
        int w = (int)(sqrt((double)rayon * rayon - (double)dy * dy) + 0.5);
        SDL_RenderDrawLine(ui->rend, cx - w, cy + dy, cx + w, cy + dy);
    }
}

void uiLueur(Ui *ui, SDL_Rect r, SDL_Color c, int intensite)
{
    for (int k = 5; k >= 1; k--) {
        SDL_Color g = c;
        g.a = (Uint8)(intensite * (6 - k) / 30);
        uiRectArrondi(ui, (SDL_Rect){ r.x - k * 3, r.y - k * 3, r.w + k * 6, r.h + k * 6 }, 5 + k * 2, g);
    }
}

void uiCadre(Ui *ui, SDL_Rect r, SDL_Color fond, SDL_Color bord)
{
    /* ombre portée, contour, puis fond légèrement dégradé */
    uiRectArrondi(ui, (SDL_Rect){ r.x + 2, r.y + 3, r.w, r.h }, 5, (SDL_Color){ 0, 0, 0, 70 });
    uiRectArrondi(ui, r, 5, bord);
    SDL_Rect in = { r.x + 2, r.y + 2, r.w - 4, r.h - 4 };
    uiRectDegrade(ui, in, 4, uiTeinte(fond, 118), fond);
}

int uiLargeur(TTF_Font *f, const char *texte)
{
    int w = 0, h = 0;
    if (f != NULL && texte != NULL) {
        TTF_SizeUTF8(f, texte, &w, &h);
    }
    return w;
}

/** Dessine une surface de texte déjà produite. */
static void poser(Ui *ui, SDL_Surface *s, int x, int y)
{
    if (s == NULL) {
        return;
    }
    SDL_Texture *t = SDL_CreateTextureFromSurface(ui->rend, s);
    if (t != NULL) {
        SDL_Rect r = { x, y, s->w, s->h };
        SDL_RenderCopy(ui->rend, t, NULL, &r);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

void uiTexte(Ui *ui, TTF_Font *f, const char *texte, int x, int y, SDL_Color c)
{
    if (ui == NULL || f == NULL || texte == NULL || texte[0] == '\0') {
        return;
    }
    poser(ui, TTF_RenderUTF8_Blended(f, texte, c), x, y);
}

void uiTexteDroite(Ui *ui, TTF_Font *f, const char *texte, int x, int y, SDL_Color c)
{
    uiTexte(ui, f, texte, x - uiLargeur(f, texte), y, c);
}

void uiTexteCentre(Ui *ui, TTF_Font *f, const char *texte, SDL_Rect r, SDL_Color c)
{
    if (f == NULL || texte == NULL) {
        return;
    }
    int w = 0, h = 0;
    TTF_SizeUTF8(f, texte, &w, &h);
    uiTexte(ui, f, texte, r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, c);
}

int uiParagraphe(Ui *ui, TTF_Font *f, const char *texte, int x, int y, int largeur, SDL_Color c)
{
    if (ui == NULL || f == NULL || texte == NULL || texte[0] == '\0') {
        return 0;
    }
    SDL_Surface *s = TTF_RenderUTF8_Blended_Wrapped(f, texte, c, (Uint32)largeur);
    if (s == NULL) {
        return 0;
    }
    int h = s->h;
    poser(ui, s, x, y);
    return h;
}

int uiOption(Ui *ui, SDL_Rect r, const char *texte, int choisi, int actif)
{
    int survol = actif && uiDans(r, ui->sx, ui->sy);
    SDL_Color haut = { 74, 74, 80, 255 }, bas = { 52, 52, 58, 255 };
    SDL_Color bord = { 112, 112, 120, 255 }, txt = UI_TEXTE;
    if (!actif) {
        haut = (SDL_Color){ 46, 46, 50, 255 };
        bas = (SDL_Color){ 38, 38, 42, 255 };
        bord = (SDL_Color){ 66, 66, 72, 255 };
        txt = UI_GRIS;
    } else if (choisi) {
        haut = (SDL_Color){ 224, 182, 96, 255 };
        bas = (SDL_Color){ 186, 142, 62, 255 };
        bord = (SDL_Color){ 255, 226, 150, 255 };
        txt = (SDL_Color){ 40, 26, 8, 255 };
    } else if (survol) {
        haut = uiTeinte(haut, ui->appui ? 85 : 124);
        bas = uiTeinte(bas, ui->appui ? 85 : 124);
        bord = UI_OR;
    }
    int rayon = 6;
    uiRectArrondi(ui, (SDL_Rect){ r.x, r.y + 3, r.w, r.h }, rayon, (SDL_Color){ 0, 0, 0, 80 });
    uiRectArrondi(ui, r, rayon, bord);
    uiRectDegrade(ui, (SDL_Rect){ r.x + 2, r.y + 2, r.w - 4, r.h - 4 }, 4, haut, bas);
    uiTexteCentre(ui, ui->normale, texte, r, txt);
    if (survol && ui->clic) {
        ui->clic = 0;
        return 1;
    }
    return 0;
}

int uiBouton(Ui *ui, SDL_Rect r, const char *texte, int actif)
{
    return uiOption(ui, r, texte, 0, actif);
}

void uiBarre(Ui *ui, SDL_Rect r, int val, int max, SDL_Color c)
{
    int rayon = r.h / 2 < 3 ? r.h / 2 : 3;
    uiRectArrondi(ui, r, rayon, (SDL_Color){ 8, 8, 10, 255 });
    if (max > 0 && val > 0) {
        int w = (int)((long long)(r.w - 2) * (val > max ? max : val) / max);
        if (w < r.h - 2) {
            w = r.h < 4 ? r.w - 2 : 3;
        }
        uiRectDegrade(ui, (SDL_Rect){ r.x + 1, r.y + 1, w, r.h - 2 }, rayon > 1 ? rayon - 1 : 0, uiTeinte(c, 130), uiTeinte(c, 85));
    }
}

SDL_Color uiCouleurElement(int element)
{
    static const SDL_Color COULEURS[] = {
        { 170, 174, 190, 255 }, /* Physique   */
        { 240, 110, 60, 255 },  /* Feu        */
        { 110, 208, 240, 255 }, /* Glace      */
        { 184, 120, 240, 255 }, /* Foudre     */
        { 110, 220, 150, 255 }, /* Vent       */
        { 120, 130, 240, 255 }, /* Quantique  */
        { 244, 214, 90, 255 },  /* Imaginaire */
    };
    if (element < 0 || element >= (int)(sizeof COULEURS / sizeof COULEURS[0])) {
        return UI_GRIS;
    }
    return COULEURS[element];
}

SDL_Color uiTeinte(SDL_Color c, int pourcent)
{
    int r = c.r * pourcent / 100, g = c.g * pourcent / 100, b = c.b * pourcent / 100;
    c.r = (Uint8)(r > 255 ? 255 : r);
    c.g = (Uint8)(g > 255 ? 255 : g);
    c.b = (Uint8)(b > 255 ? 255 : b);
    return c;
}

void uiElementIcone(Ui *ui, int cx, int cy, int rayon, int element)
{
    SDL_Color c = uiCouleurElement(element);
    uiDisque(ui, cx, cy, rayon + 1, (SDL_Color){ 12, 12, 14, 255 });
    uiDisque(ui, cx, cy, rayon, uiTeinte(c, 70));
    uiDisque(ui, cx, cy - rayon / 6, rayon - 2 > 0 ? rayon - 2 : 1, c);
    SDL_Color sombre = { 20, 20, 22, 255 };
    SDL_SetRenderDrawColor(ui->rend, sombre.r, sombre.g, sombre.b, 255);
    int u = rayon / 2 > 1 ? rayon / 2 : 1;
    switch (element) {
    case 1: /* Feu : goutte pointue */
        uiDisque(ui, cx, cy + u / 3, u * 2 / 3 + 1, sombre);
        for (int dy = 0; dy < u + 2; dy++) {
            int w = (u * 2 / 3 + 1) * (u + 2 - dy) / (u + 2);
            SDL_RenderDrawLine(ui->rend, cx - w, cy - u / 2 - dy + 1, cx + w, cy - u / 2 - dy + 1);
        }
        break;
    case 2: /* Glace : flocon */
        for (int k = 0; k < 3; k++) {
            double a = k * M_PI / 3;
            int dx = (int)(cos(a) * u), dy = (int)(sin(a) * u);
            SDL_RenderDrawLine(ui->rend, cx - dx, cy - dy, cx + dx, cy + dy);
            SDL_RenderDrawLine(ui->rend, cx - dx + 1, cy - dy, cx + dx + 1, cy + dy);
        }
        break;
    case 3: /* Foudre : éclair */
        for (int t = 0; t < 2; t++) {
            SDL_RenderDrawLine(ui->rend, cx + u / 2 + t, cy - u, cx - u / 2 + t, cy);
            SDL_RenderDrawLine(ui->rend, cx - u / 2 + t, cy, cx + u / 2 + t, cy);
            SDL_RenderDrawLine(ui->rend, cx + u / 2 + t, cy, cx - u / 2 + t, cy + u);
        }
        break;
    case 4: /* Vent : deux traits courbes */
        for (int k = -1; k <= 1; k += 2) {
            for (int dx = -u; dx <= u; dx++) {
                int dy = (int)(sin(dx * 1.4 / u) * u / 3);
                SDL_RenderDrawPoint(ui->rend, cx + dx, cy + k * u / 2 + dy);
                SDL_RenderDrawPoint(ui->rend, cx + dx, cy + k * u / 2 + dy + 1);
            }
        }
        break;
    case 5: /* Quantique : anneau */
        uiDisque(ui, cx, cy, u + 1, sombre);
        uiDisque(ui, cx, cy, u - 1 > 0 ? u - 1 : 1, c);
        uiDisque(ui, cx, cy, u / 3 + 1, sombre);
        break;
    case 6: /* Imaginaire : étoile à quatre branches */
        uiLosange(ui, cx, cy, u + 2, sombre);
        uiLosange(ui, cx, cy, u / 2 + 1, c);
        SDL_RenderDrawLine(ui->rend, cx - u - 2, cy, cx + u + 2, cy);
        SDL_RenderDrawLine(ui->rend, cx, cy - u - 2, cx, cy + u + 2);
        break;
    default: /* Physique : poing / losange simple */
        uiRectArrondi(ui, (SDL_Rect){ cx - u, cy - u / 2, u * 2, u }, 2, sombre);
        break;
    }
}

void uiFleche(Ui *ui, int cx, int cy, int taille, int haut, SDL_Color c)
{
    SDL_SetRenderDrawColor(ui->rend, c.r, c.g, c.b, c.a);
    for (int dy = 0; dy < taille; dy++) {
        int w = haut ? dy : taille - 1 - dy;
        SDL_RenderDrawLine(ui->rend, cx - w, cy - taille / 2 + dy, cx + w, cy - taille / 2 + dy);
    }
}

/** Cube de couleur c : dessus clair, côté sombre, contour. Retourne l'épaisseur du relief. */
static int dessinerCube(Ui *ui, SDL_Rect r, SDL_Color c)
{
    int bord = r.w / 9 > 3 ? r.w / 9 : 3;
    uiRect(ui, r, c);
    uiRect(ui, (SDL_Rect){ r.x, r.y, r.w, bord }, uiTeinte(c, 135));                            /* dessus */
    uiRect(ui, (SDL_Rect){ r.x + r.w - bord, r.y + bord, bord, r.h - bord }, uiTeinte(c, 70)); /* côté   */
    SDL_SetRenderDrawColor(ui->rend, 10, 10, 16, 255);
    SDL_RenderDrawRect(ui->rend, &r);
    return bord;
}

/** Deux yeux carrés tournés du côté regardé, qui clignent de temps en temps,
 *  et un petit bec de canard dessous. `phase` décale le clignement. */
static void dessinerYeux(Ui *ui, SDL_Rect r, int droite, int y, double phase)
{
    int oeil = r.w / 6 > 3 ? r.w / 6 : 3;
    int ex = droite ? r.x + r.w / 2 - oeil / 2 : r.x + r.w / 8;
    int cligne = (ui->temps + (Uint32)(phase * 977)) % 3400 < 130;
    for (int k = 0; k < 2; k++) {
        SDL_Rect o = { ex + k * (oeil + oeil / 2), y, oeil, oeil + 2 };
        if (cligne) {
            uiRect(ui, (SDL_Rect){ o.x, o.y + oeil / 2 + 1, o.w, 2 }, (SDL_Color){ 15, 15, 25, 255 });
            continue;
        }
        uiRect(ui, o, (SDL_Color){ 245, 245, 250, 255 });
        uiRect(ui, (SDL_Rect){ o.x + (droite ? oeil / 2 : 0), o.y + 2, oeil / 2 + 1, oeil / 2 + 1 }, (SDL_Color){ 15, 15, 25, 255 });
    }
    /* bec : plat, orange, avancé du côté regardé */
    int bw = oeil * 5 / 2 + 2;
    int bx = droite ? ex : ex - 2;
    uiRect(ui, (SDL_Rect){ bx, y + oeil + 5, bw + 2, oeil / 2 + 3 }, (SDL_Color){ 244, 150, 40, 255 });
    uiRect(ui, (SDL_Rect){ bx, y + oeil + 5, bw + 2, 2 }, (SDL_Color){ 255, 196, 90, 255 });
    uiRect(ui, (SDL_Rect){ bx, y + oeil + 5 + oeil / 2 + 2, bw + 2, 1 }, (SDL_Color){ 170, 90, 20, 255 });
}

/** Cape / écharpe qui flotte derrière le cube, du côté opposé au regard. */
static void dessinerCape(Ui *ui, SDL_Rect r, int droite, int u, SDL_Color c, double phase)
{
    int sens = droite ? -1 : 1;
    int base = droite ? r.x : r.x + r.w;
    for (int k = 0; k < 4; k++) {
        int bat = (int)(sin(ui->temps / 210.0 + phase + k * 0.9) * (k + 1) * 1.2);
        int w = u * (k + 2);
        SDL_Color ck = uiTeinte(c, 100 - k * 8);
        SDL_Rect seg = { sens > 0 ? base : base - w, r.y + r.h / 4 + k * (r.h / 7) + bat / 2, w, r.h / 7 + 2 };
        uiRect(ui, seg, ck);
    }
}

void uiHeros(Ui *ui, SDL_Rect r, int classe, SDL_Color c, int droite, int vivant, double phase)
{
    SDL_Color metal = { 200, 206, 226, 255 }, or_ = { 244, 196, 80, 255 }, bois = { 140, 98, 60, 255 };
    int sens = droite ? 1 : -1;
    if (!vivant) {
        /* Un cube K.O. est écrasé au sol. */
        SDL_Rect plat = { r.x, r.y + r.h - 14, r.w, 14 };
        uiRect(ui, plat, uiTeinte(c, 40));
        uiTexteCentre(ui, ui->petite, "K.O.", (SDL_Rect){ r.x, r.y + r.h - 40, r.w, 20 }, UI_ROUGE);
        return;
    }
    double resp = sin(ui->temps / 260.0 + phase);
    uiRect(ui, (SDL_Rect){ r.x - 4, r.y + r.h - 2, r.w + 8, 8 }, (SDL_Color){ 0, 0, 0, 110 }); /* ombre */
    /* respiration : le cube s'étire un peu vers le haut puis se tasse (pieds au sol) */
    r.y -= (int)(resp * 2 + 2);
    r.h += (int)(resp * 2 + 2);
    int bord = dessinerCube(ui, r, c);
    dessinerYeux(ui, r, droite, r.y + r.h / 3, phase);
    int cx = r.x + r.w / 2;
    int main_x = droite ? r.x + r.w : r.x; /* côté de la main */
    int u = r.w / 8 > 2 ? r.w / 8 : 2;      /* unité de dessin */
    double bat = sin(ui->temps / 300.0 + phase * 2);

    /* capes et écharpes (derrière : on les redessine avant le cube serait mieux,
       mais le cube est plein, elles dépassent simplement sur le côté) */
    switch (classe) {
    case 0: dessinerCape(ui, r, droite, u, (SDL_Color){ 190, 50, 50, 255 }, phase); break;
    case 2: dessinerCape(ui, r, droite, u, (SDL_Color){ 64, 62, 84, 255 }, phase); break;
    case 3: dessinerCape(ui, r, droite, u, (SDL_Color){ 236, 232, 214, 255 }, phase); break;
    case 6: dessinerCape(ui, r, droite, u, (SDL_Color){ 54, 34, 80, 255 }, phase); break;
    case 7: dessinerCape(ui, r, droite, u, (SDL_Color){ 210, 70, 110, 255 }, phase); break;
    default: break;
    }

    switch (classe) {
    case 0: /* Guerrier : casque à crête + épée */
        uiRect(ui, (SDL_Rect){ r.x - 2, r.y - u * 2, r.w + 4, u * 3 }, metal);
        uiRect(ui, (SDL_Rect){ r.x - 2, r.y - u * 2, r.w + 4, u }, uiTeinte(metal, 120));
        uiRect(ui, (SDL_Rect){ cx - u / 2 - 1, r.y - u * 5, u + 2, u * 3 }, (SDL_Color){ 220, 70, 70, 255 });
        uiRect(ui, (SDL_Rect){ main_x + sens * u - (droite ? 0 : u), r.y + r.h / 6, u, r.h * 2 / 3 }, metal);
        uiRect(ui, (SDL_Rect){ main_x + sens * u - u * 2 + (droite ? 0 : u), r.y + r.h / 2, u * 3, u }, or_);
        break;
    case 1: /* Mage : chapeau pointu + bâton à orbe */
        for (int k = 0; k < 4; k++) {
            int w = r.w * (k + 2) / 7;
            uiRect(ui, (SDL_Rect){ cx - w / 2, r.y - (4 - k) * u * 2 + u, w, u * 2 }, (SDL_Color){ 92, 74, 190, 255 });
        }
        uiRect(ui, (SDL_Rect){ r.x - u, r.y - u, r.w + u * 2, u * 2 }, (SDL_Color){ 70, 54, 160, 255 });
        uiRect(ui, (SDL_Rect){ main_x + sens * u - (droite ? 0 : u), r.y - u * 2, u, r.h + u * 2 }, bois);
        uiLueur(ui, (SDL_Rect){ main_x + sens * u - u - (droite ? 0 : u), r.y - u * 4, u * 3, u * 3 }, (SDL_Color){ 255, 120, 90, 255 },
                (int)(90 + 70 * bat));
        uiRect(ui, (SDL_Rect){ main_x + sens * u - u - (droite ? 0 : u), r.y - u * 4, u * 3, u * 3 }, (SDL_Color){ 255, 120, 90, 255 });
        break;
    case 2: /* Voleur : capuche sombre + dague */
        uiRect(ui, (SDL_Rect){ r.x - 1, r.y - u, r.w + 2, u * 3 }, (SDL_Color){ 46, 44, 66, 255 });
        uiRect(ui, (SDL_Rect){ r.x - 1, r.y - u, u * 2, r.h / 2 }, (SDL_Color){ 46, 44, 66, 255 });
        uiRect(ui, (SDL_Rect){ main_x + sens * u - (droite ? 0 : u), r.y + r.h / 3, u, r.h / 3 }, metal);
        uiRect(ui, (SDL_Rect){ main_x + sens * u - (droite ? 0 : u), r.y + r.h * 2 / 3, u, u * 2 }, bois);
        break;
    case 3: /* Paladin : auréole + bouclier */
        uiRect(ui, (SDL_Rect){ cx - r.w / 3, r.y - u * 3 - (int)(bat * 2), r.w * 2 / 3, u }, or_);
        uiRect(ui, (SDL_Rect){ droite ? r.x + r.w - u : r.x - u * 3, r.y + r.h / 2 - u, u * 4, u * 5 }, or_);
        uiRect(ui, (SDL_Rect){ droite ? r.x + r.w : r.x - u * 2, r.y + r.h / 2 + u / 2, u * 2, u * 2 }, (SDL_Color){ 200, 60, 60, 255 });
        break;
    case 4: /* Archer : bonnet vert à plume + arc */
        uiRect(ui, (SDL_Rect){ r.x, r.y - u * 2, r.w, u * 3 }, (SDL_Color){ 60, 150, 84, 255 });
        uiRect(ui, (SDL_Rect){ cx + sens * r.w / 4 - u / 2, r.y - u * 5, u, u * 3 }, (SDL_Color){ 245, 245, 252, 255 });
        for (int k = -3; k <= 3; k++) {
            int dx = (4 - (k < 0 ? -k : k)) * u / 3;
            uiRect(ui, (SDL_Rect){ main_x + sens * (u + dx) - (droite ? 0 : 2), r.y + r.h / 2 + k * u, 2, u }, bois);
        }
        break;
    case 5: /* Prêtre : auréole + croix blanche */
        uiRect(ui, (SDL_Rect){ cx - r.w / 3, r.y - u * 3 - (int)(bat * 2), r.w * 2 / 3, u }, (SDL_Color){ 255, 240, 150, 255 });
        uiRect(ui, (SDL_Rect){ cx - u / 2, r.y + r.h * 11 / 20, u + 1, u * 4 }, (SDL_Color){ 250, 250, 255, 255 });
        uiRect(ui, (SDL_Rect){ cx - u * 3 / 2 - 1, r.y + r.h * 11 / 20 + u, u * 3 + 2, u + 1 }, (SDL_Color){ 250, 250, 255, 255 });
        break;
    case 6: /* Nécromancien : capuche violette à cornes + orbe vert */
        uiRect(ui, (SDL_Rect){ r.x - 1, r.y - u, r.w + 2, u * 3 }, (SDL_Color){ 70, 40, 100, 255 });
        for (int k = -1; k <= 1; k += 2) {
            uiRect(ui, (SDL_Rect){ cx + k * (r.w / 3) - u / 2, r.y - u * 4, u, u * 3 }, (SDL_Color){ 228, 222, 206, 255 });
        }
        uiLueur(ui, (SDL_Rect){ main_x + sens * u - (droite ? 0 : u * 2), r.y + r.h / 2 + (int)(bat * 2), u * 2, u * 2 },
                (SDL_Color){ 130, 240, 170, 255 }, (int)(90 + 70 * bat));
        uiRect(ui, (SDL_Rect){ main_x + sens * u - (droite ? 0 : u * 2), r.y + r.h / 2 + (int)(bat * 2), u * 2, u * 2 }, (SDL_Color){ 130, 240, 170, 255 });
        break;
    default: /* Barde : béret rose à plume + luth doré */
        uiRect(ui, (SDL_Rect){ r.x + u / 2, r.y - u * 2, r.w - u, u * 3 }, (SDL_Color){ 220, 80, 120, 255 });
        uiRect(ui, (SDL_Rect){ cx + sens * r.w / 4 - u / 2, r.y - u * 4, u, u * 2 }, or_);
        uiRect(ui, (SDL_Rect){ main_x + sens * u - (droite ? 0 : u * 2), r.y + r.h / 2, u * 2, u * 3 }, or_);
        uiRect(ui, (SDL_Rect){ main_x + sens * u * 2 - (droite ? 0 : u), r.y + r.h / 2 - u * 2, u / 2 + 1, u * 3 }, bois);
        break;
    }
    (void)bord;
}

void uiMonstre(Ui *ui, SDL_Rect r, SDL_Color c, int boss, int vivant)
{
    SDL_Color noir = { 22, 18, 20, 255 };
    if (!vivant) {
        SDL_Rect plat = { r.x, r.y + r.h - 14, r.w, 14 };
        uiRect(ui, plat, uiTeinte(c, 40));
        uiTexteCentre(ui, ui->petite, "K.O.", (SDL_Rect){ r.x, r.y + r.h - 40, r.w, 20 }, UI_ROUGE);
        return;
    }
    r.y += (int)(sin(ui->temps / 330.0 + r.x) * 2);
    uiRect(ui, (SDL_Rect){ r.x - 4, r.y + r.h - 2, r.w + 8, 8 }, (SDL_Color){ 0, 0, 0, 110 });
    dessinerCube(ui, r, c);
    int u = r.w / 9 > 2 ? r.w / 9 : 2;
    if (boss) {
        /* cornes et couronne */
        for (int k = -1; k <= 1; k += 2) {
            uiRect(ui, (SDL_Rect){ r.x + r.w / 2 + k * (r.w / 3) - u, r.y - u * 3, u * 2, u * 3 }, (SDL_Color){ 236, 226, 200, 255 });
            uiRect(ui, (SDL_Rect){ r.x + r.w / 2 + k * (r.w / 3) - u / 2 + k * u, r.y - u * 5, u, u * 2 }, (SDL_Color){ 236, 226, 200, 255 });
        }
        uiRect(ui, (SDL_Rect){ r.x + r.w / 2 - u * 2, r.y - u * 2, u * 4, u * 2 }, (SDL_Color){ 244, 196, 80, 255 });
    }
    /* yeux méchants : sourcils en biais */
    int ey = r.y + r.h * 2 / 5;
    for (int k = -1; k <= 1; k += 2) {
        int x = r.x + r.w / 2 + k * r.w / 5 - u;
        uiRect(ui, (SDL_Rect){ x, ey, u * 2, u * 2 }, (SDL_Color){ 250, 248, 255, 255 });
        uiRect(ui, (SDL_Rect){ x + u / 2, ey + u / 2, u, u }, noir);
        SDL_SetRenderDrawColor(ui->rend, noir.r, noir.g, noir.b, 255);
        for (int t = 0; t < 3; t++) {
            SDL_RenderDrawLine(ui->rend, x - 1, ey - 2 - k * u / 2 + t, x + u * 2 + 1, ey - 2 + k * u / 2 + t);
        }
    }
    /* bouche à dents */
    uiRect(ui, (SDL_Rect){ r.x + r.w / 2 - u * 2, ey + u * 4, u * 4, u }, noir);
    uiRect(ui, (SDL_Rect){ r.x + r.w / 2 - u * 2 + 1, ey + u * 4, u / 2 + 1, u / 2 + 1 }, (SDL_Color){ 250, 248, 255, 255 });
    uiRect(ui, (SDL_Rect){ r.x + r.w / 2 + u, ey + u * 4, u / 2 + 1, u / 2 + 1 }, (SDL_Color){ 250, 248, 255, 255 });
}

void uiLosange(Ui *ui, int cx, int cy, int rayon, SDL_Color c)
{
    SDL_SetRenderDrawColor(ui->rend, c.r, c.g, c.b, c.a);
    for (int dy = -rayon; dy <= rayon; dy++) {
        int w = rayon - (dy < 0 ? -dy : dy);
        SDL_RenderDrawLine(ui->rend, cx - w, cy + dy, cx + w, cy + dy);
    }
}

void uiRepere(Ui *ui, int cx, int y, int taille, SDL_Color c)
{
    SDL_SetRenderDrawColor(ui->rend, c.r, c.g, c.b, c.a);
    for (int dy = 0; dy < taille; dy++) {
        int w = taille - dy;
        SDL_RenderDrawLine(ui->rend, cx - w, y + dy, cx + w, y + dy);
    }
}

/* ------------------------------------------------------------------ */
/*  Touches encadrées                                                 */
/* ------------------------------------------------------------------ */

int uiLargeurTouche(Ui *ui, const char *touche)
{
    int w = uiLargeur(ui->petite, touche) + 14;
    return w < 24 ? 24 : w;
}

int uiTouche(Ui *ui, const char *touche, int x, int y)
{
    int w = uiLargeurTouche(ui, touche);
    SDL_Rect r = { x, y, w, 20 };
    uiRectArrondi(ui, (SDL_Rect){ r.x, r.y + 2, r.w, r.h }, 5, (SDL_Color){ 10, 10, 12, 255 });
    uiRectArrondi(ui, r, 5, (SDL_Color){ 150, 150, 158, 255 });
    uiRectDegrade(ui, (SDL_Rect){ r.x + 1, r.y + 1, r.w - 2, r.h - 3 }, 4, (SDL_Color){ 84, 84, 92, 255 },
                  (SDL_Color){ 56, 56, 62, 255 });
    uiTexteCentre(ui, ui->petite, touche, (SDL_Rect){ r.x, r.y - 2, r.w, r.h }, UI_TEXTE);
    return w;
}

/** Découpe « touche|action;touche|action » et appelle f pour chaque paire. */
static int parcourirRaccourcis(Ui *ui, const char *spec, int x, int y, int dessiner)
{
    int debut = x;
    char copie[256];
    snprintf(copie, sizeof copie, "%s", spec);
    char *reste = copie;
    while (reste != NULL && *reste != '\0') {
        char *fin = strchr(reste, ';');
        if (fin != NULL) *fin = '\0';
        char *barre = strchr(reste, '|');
        const char *touche = reste, *action = "";
        if (barre != NULL) {
            *barre = '\0';
            action = barre + 1;
        }
        int wt = dessiner ? uiTouche(ui, touche, x, y) : uiLargeurTouche(ui, touche);
        x += wt + 6;
        if (dessiner) uiTexte(ui, ui->petite, action, x, y + 1, UI_GRIS);
        x += uiLargeur(ui->petite, action) + 16;
        reste = fin != NULL ? fin + 1 : NULL;
    }
    return x - debut - 16;
}

void uiRaccourcis(Ui *ui, const char *spec, int xDroite, int y)
{
    int w = parcourirRaccourcis(ui, spec, 0, y, 0);
    parcourirRaccourcis(ui, spec, xDroite - w, y, 1);
}
