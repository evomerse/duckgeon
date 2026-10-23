/**
 * @file gui_formes.c
 * @brief Formes vectorielles dessinées avec SDL_RenderGeometry (triangles
 *        colorés) : cercles, ellipses, arcs, polygones, dégradés.
 */
#include <math.h>

#include "gui.h"

#define SEGMENTS_MAX 96
#define PI_F 3.14159265f

SDL_Color melanger(SDL_Color a, SDL_Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return rgba((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t),
                (int)(a.b + (b.b - a.b) * t), (int)(a.a + (b.a - a.a) * t));
}

SDL_Color assombrir(SDL_Color c, float facteur)
{
    return rgba((int)(c.r * facteur), (int)(c.g * facteur), (int)(c.b * facteur), c.a);
}

SDL_Color avecAlpha(SDL_Color c, int alpha)
{
    c.a = (Uint8)(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
    return c;
}

/** Remplit un sommet. */
static SDL_Vertex sommet(float x, float y, SDL_Color c)
{
    SDL_Vertex v;
    v.position.x = x;
    v.position.y = y;
    v.color = c;
    v.tex_coord.x = 0;
    v.tex_coord.y = 0;
    return v;
}

/** Nombre de segments adapté à la taille (assez lisse, pas trop coûteux). */
static int segments(float r)
{
    int n = (int)(r * 0.9f) + 12;
    return n > SEGMENTS_MAX ? SEGMENTS_MAX : n;
}

void remplirRect(Gui *g, float x, float y, float w, float h, SDL_Color c)
{
    degradeVertical(g, x, y, w, h, c, c);
}

void degradeVertical(Gui *g, float x, float y, float w, float h, SDL_Color haut, SDL_Color bas)
{
    SDL_Vertex v[4] = {
        sommet(x, y, haut), sommet(x + w, y, haut),
        sommet(x + w, y + h, bas), sommet(x, y + h, bas),
    };
    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(g->rendu, NULL, v, 4, indices, 6);
}

void degradeHorizontal(Gui *g, float x, float y, float w, float h, SDL_Color gauche, SDL_Color droite)
{
    SDL_Vertex v[4] = {
        sommet(x, y, gauche), sommet(x + w, y, droite),
        sommet(x + w, y + h, droite), sommet(x, y + h, gauche),
    };
    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(g->rendu, NULL, v, 4, indices, 6);
}

void ellipse(Gui *g, float cx, float cy, float rx, float ry, SDL_Color c)
{
    if (rx <= 0 || ry <= 0) {
        return;
    }
    int n = segments(rx > ry ? rx : ry);
    SDL_Vertex v[SEGMENTS_MAX + 1];
    int indices[SEGMENTS_MAX * 3];
    v[0] = sommet(cx, cy, c);
    for (int i = 0; i < n; i++) {
        float a = 2 * PI_F * i / n;
        v[i + 1] = sommet(cx + cosf(a) * rx, cy + sinf(a) * ry, c);
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % n + 1;
    }
    SDL_RenderGeometry(g->rendu, NULL, v, n + 1, indices, n * 3);
}

void cercle(Gui *g, float cx, float cy, float r, SDL_Color c)
{
    ellipse(g, cx, cy, r, r, c);
}

void halo(Gui *g, float cx, float cy, float r, SDL_Color c)
{
    if (r <= 0) {
        return;
    }
    int n = segments(r);
    SDL_Vertex v[SEGMENTS_MAX + 1];
    int indices[SEGMENTS_MAX * 3];
    v[0] = sommet(cx, cy, c);
    SDL_Color bord = avecAlpha(c, 0);
    for (int i = 0; i < n; i++) {
        float a = 2 * PI_F * i / n;
        v[i + 1] = sommet(cx + cosf(a) * r, cy + sinf(a) * r, bord);
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % n + 1;
    }
    SDL_RenderGeometry(g->rendu, NULL, v, n + 1, indices, n * 3);
}

void arc(Gui *g, float cx, float cy, float r, float ep, float debut, float fin, SDL_Color c)
{
    if (fin <= debut || r <= 0) {
        return;
    }
    int n = (int)(segments(r) * (fin - debut) / (2 * PI_F)) + 2;
    if (n > SEGMENTS_MAX / 2) {
        n = SEGMENTS_MAX / 2;
    }
    SDL_Vertex v[SEGMENTS_MAX + 2];
    int indices[SEGMENTS_MAX * 3];
    float ri = r - ep / 2, re = r + ep / 2;
    for (int i = 0; i <= n; i++) {
        float a = debut + (fin - debut) * i / n;
        v[i * 2] = sommet(cx + cosf(a) * ri, cy + sinf(a) * ri, c);
        v[i * 2 + 1] = sommet(cx + cosf(a) * re, cy + sinf(a) * re, c);
    }
    for (int i = 0; i < n; i++) {
        int b = i * 2;
        int *t = &indices[i * 6];
        t[0] = b; t[1] = b + 1; t[2] = b + 3;
        t[3] = b; t[4] = b + 3; t[5] = b + 2;
    }
    SDL_RenderGeometry(g->rendu, NULL, v, (n + 1) * 2, indices, n * 6);
}

void anneauEllipse(Gui *g, float cx, float cy, float rx, float ry, float ep,
                   SDL_Color c, int pointilles, float rotation)
{
    const int n = 48;
    for (int i = 0; i < n; i++) {
        if (pointilles && i % 2) {
            continue;
        }
        float a0 = 2 * PI_F * i / n + rotation, a1 = 2 * PI_F * (i + 1) / n + rotation;
        ligne(g, cx + cosf(a0) * rx, cy + sinf(a0) * ry, cx + cosf(a1) * rx, cy + sinf(a1) * ry, ep, c);
    }
}

void polygone(Gui *g, const float *xy, int n, SDL_Color c)
{
    if (n < 3 || n > SEGMENTS_MAX) {
        return;
    }
    SDL_Vertex v[SEGMENTS_MAX];
    int indices[SEGMENTS_MAX * 3];
    for (int i = 0; i < n; i++) {
        v[i] = sommet(xy[i * 2], xy[i * 2 + 1], c);
    }
    for (int i = 0; i < n - 2; i++) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(g->rendu, NULL, v, n, indices, (n - 2) * 3);
}

void ligne(Gui *g, float x1, float y1, float x2, float y2, float ep, SDL_Color c)
{
    float dx = x2 - x1, dy = y2 - y1;
    float l = sqrtf(dx * dx + dy * dy);
    if (l < 0.001f) {
        return;
    }
    float nx = -dy / l * ep / 2, ny = dx / l * ep / 2;
    float xy[8] = { x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny };
    polygone(g, xy, 4, c);
}

void losange(Gui *g, float cx, float cy, float rx, float ry, SDL_Color c)
{
    float xy[8] = { cx, cy - ry, cx + rx, cy, cx, cy + ry, cx - rx, cy };
    polygone(g, xy, 4, c);
}

/** Construit le contour d'un rectangle arrondi (4 quarts de cercle). */
static int contourRect(float x, float y, float w, float h, float r, float *xy)
{
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    const int pas = 6;
    const float centres[4][2] = {
        { x + w - r, y + r }, { x + w - r, y + h - r }, { x + r, y + h - r }, { x + r, y + r },
    };
    int n = 0;
    for (int coin = 0; coin < 4; coin++) {
        for (int i = 0; i <= pas; i++) {
            float a = -PI_F / 2 + (coin + (float)i / pas) * PI_F / 2;
            xy[n * 2] = centres[coin][0] + cosf(a) * r;
            xy[n * 2 + 1] = centres[coin][1] + sinf(a) * r;
            n++;
        }
    }
    return n;
}

void rectArrondi(Gui *g, float x, float y, float w, float h, float r, SDL_Color c)
{
    float xy[2 * 28];
    int n = contourRect(x, y, w, h, r, xy);
    polygone(g, xy, n, c);
}

void contourArrondi(Gui *g, float x, float y, float w, float h, float r, float ep, SDL_Color c)
{
    float xy[2 * 28];
    int n = contourRect(x, y, w, h, r, xy);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        ligne(g, xy[i * 2], xy[i * 2 + 1], xy[j * 2], xy[j * 2 + 1], ep, c);
    }
}
