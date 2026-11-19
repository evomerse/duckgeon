/**
 * @file sdl_ui.c
 * @brief Mini bibliothèque d'interface de Donjon SDL (voir sdl_ui.h).
 */

/* [DONJON SDL - partie 3] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdl_ui.h"

const SDL_Color UI_FOND = { 16, 18, 26, 255 };
const SDL_Color UI_PANNEAU = { 28, 32, 44, 255 };
const SDL_Color UI_PANNEAU2 = { 42, 48, 66, 255 };
const SDL_Color UI_BORD = { 78, 88, 114, 255 };
const SDL_Color UI_TEXTE = { 232, 234, 242, 255 };
const SDL_Color UI_GRIS = { 140, 148, 168, 255 };
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
    if (ui->rend == NULL || ui->petite == NULL || ui->normale == NULL || ui->grande == NULL) {
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
    if (ui->rend) SDL_DestroyRenderer(ui->rend);
    if (ui->fenetre) SDL_DestroyWindow(ui->fenetre);
    ui->petite = ui->normale = ui->grande = NULL;
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
    SDL_SetRenderDrawColor(ui->rend, UI_FOND.r, UI_FOND.g, UI_FOND.b, 255);
    SDL_RenderClear(ui->rend);
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

void uiCadre(Ui *ui, SDL_Rect r, SDL_Color fond, SDL_Color bord)
{
    uiRect(ui, r, fond);
    SDL_SetRenderDrawColor(ui->rend, bord.r, bord.g, bord.b, bord.a);
    SDL_RenderDrawRect(ui->rend, &r);
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
    SDL_Color fond = !actif ? UI_PANNEAU : (survol ? (ui->appui ? UI_BORD : UI_PANNEAU2) : UI_PANNEAU);
    if (choisi) {
        fond = UI_PANNEAU2;
    }
    uiCadre(ui, r, fond, choisi ? UI_OR : UI_BORD);
    uiTexteCentre(ui, ui->normale, texte, r, actif ? UI_TEXTE : UI_GRIS);
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
    uiRect(ui, r, (SDL_Color){ 10, 12, 18, 255 });
    if (max > 0 && val > 0) {
        int w = (int)((long long)r.w * (val > max ? max : val) / max);
        if (w < 1) {
            w = 1;
        }
        uiRect(ui, (SDL_Rect){ r.x, r.y, w, r.h }, c);
    }
    SDL_SetRenderDrawColor(ui->rend, UI_BORD.r, UI_BORD.g, UI_BORD.b, 255);
    SDL_RenderDrawRect(ui->rend, &r);
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

void uiCube(Ui *ui, SDL_Rect r, SDL_Color c, const char *lettre, int droite)
{
    int bord = r.w / 9 > 3 ? r.w / 9 : 3;
    /* ombre au sol */
    uiRect(ui, (SDL_Rect){ r.x - 4, r.y + r.h - 2, r.w + 8, 8 }, (SDL_Color){ 0, 0, 0, 110 });
    uiRect(ui, r, c);
    uiRect(ui, (SDL_Rect){ r.x, r.y, r.w, bord }, uiTeinte(c, 135));                  /* dessus  */
    uiRect(ui, (SDL_Rect){ r.x + r.w - bord, r.y + bord, bord, r.h - bord }, uiTeinte(c, 70)); /* côté */
    SDL_SetRenderDrawColor(ui->rend, 10, 10, 16, 255);
    SDL_RenderDrawRect(ui->rend, &r);
    /* deux yeux tournés du côté où le cube regarde */
    int oeil = r.w / 6 > 3 ? r.w / 6 : 3;
    int ex = droite ? r.x + r.w / 2 : r.x + r.w / 8;
    int ey = r.y + r.h / 4;
    for (int k = 0; k < 2; k++) {
        SDL_Rect o = { ex + k * (oeil + oeil / 2), ey, oeil, oeil + 2 };
        uiRect(ui, o, (SDL_Color){ 245, 245, 250, 255 });
        uiRect(ui, (SDL_Rect){ o.x + (droite ? oeil / 2 : 0), o.y + 2, oeil / 2 + 1, oeil / 2 + 1 }, (SDL_Color){ 15, 15, 25, 255 });
    }
    if (lettre != NULL && lettre[0] != '\0' && ui->normale != NULL) {
        uiTexteCentre(ui, ui->normale, lettre, (SDL_Rect){ r.x, r.y + r.h / 2, r.w, r.h / 2 }, uiTeinte(c, 45));
    }
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
