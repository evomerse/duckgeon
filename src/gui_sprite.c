/**
 * @file gui_sprite.c
 * @brief Dessin vectoriel des héros et des monstres.
 *
 * Chaque sprite est décrit dans un repère local (pieds en 0,0, y vers le
 * haut négatif, regard vers +x) puis transformé par un « pinceau » qui
 * applique position, échelle, miroir (sens), flash et transparence.
 */
#include <math.h>
#include <string.h>

#include "gui_sprite.h"

#define PI_F 3.14159265f

/* ------------------------------------------------------------------ */
/*  Palettes et libellés                                              */
/* ------------------------------------------------------------------ */

const SDL_Color PALETTE[NB_COULEURS] = {
    {  38,  34,  44, 255 }, {  78,  50,  34, 255 }, { 128,  84,  52, 255 },
    { 238, 204, 118, 255 }, { 206,  90,  44, 255 }, { 238, 238, 244, 255 },
    { 140, 146, 164, 255 }, { 210,  52,  64, 255 }, { 242, 128, 176, 255 },
    { 146,  86, 206, 255 }, {  84,  72, 196, 255 }, {  62, 124, 226, 255 },
    {  76, 206, 226, 255 }, {  52, 184, 150, 255 }, {  84, 172,  84, 255 },
    { 176, 214,  76, 255 }, { 222, 172,  62, 255 }, { 242, 142,  52, 255 },
};

/* Plumages : blanc, caneton jaune, cane brune, gris, noir, colvert,
 * bleu, rose. */
const SDL_Color PLUMAGES[NB_PLUMAGES] = {
    { 248, 246, 238, 255 }, { 252, 218,  86, 255 }, { 176, 128,  84, 255 },
    { 158, 160, 172, 255 }, {  62,  60,  72, 255 }, {  52, 128,  86, 255 },
    { 104, 150, 226, 255 }, { 244, 160, 196, 255 },
    /* boutique : laqué, doré, Néant */
    { 196,  88,  40, 255 }, { 244, 192,  52, 255 }, {  76,  44, 120, 255 },
};

const char *const NOMS_COUPES[NB_COUPES] = {
    "Plume rebelle", "Longue", "Queue de canard", "Hérissée", "Lisse",
    "Tresses", "Iroquoise", "Carré", "Chignon", "Ondulée",
};
const char *const NOMS_COIFFES[NB_COIFFES] = {
    "Aucune", "Couronne", "Capuche", "Cornes", "Auréole",
    "Bandeau", "Chapeau de mage", "Casque", "Oreilles de chat", "Fleur",
    /* boutique */
    "Toque de chef", "Haut-de-forme", "Bicorne de pirate", "Casque viking", "Couronne du Néant",
};
const char *const NOMS_PILOSITES[NB_PILOSITES] = { "Aucune", "Barbe", "Moustache", "Bouc" };
const char *const NOMS_MARQUES[NB_MARQUES] = {
    "Aucune", "Cicatrice", "Peinture de guerre", "Tatouage frontal", "Taches de rousseur",
};
const char *const NOMS_CORPULENCES[NB_CORPULENCES] = { "Mince", "Normale", "Massive" };
const char *const NOMS_REGARDS[NB_REGARDS] = { "Ronds", "Déterminés", "Endormis", "Joyeux" };

const char *libelleClasse(enum Classe c)
{
    static const char *const NOMS[NB_CLASSES] = {
        "Guerrier", "Mage", "Voleur", "Paladin", "Archer", "Prêtre", "Nécromancien", "Barde",
    };
    return (c >= 0 && c < NB_CLASSES) ? NOMS[c] : "?";
}

SDL_Color couleurClasse(enum Classe c)
{
    static const SDL_Color C[NB_CLASSES] = {
        { 214, 92, 72, 255 }, { 236, 130, 60, 255 }, { 90, 200, 150, 255 }, { 236, 206, 110, 255 },
        { 150, 120, 240, 255 }, { 130, 210, 240, 255 }, { 170, 100, 220, 255 }, { 240, 130, 190, 255 },
    };
    return (c >= 0 && c < NB_CLASSES) ? C[c] : COUL_TEXTE;
}

SDL_Color couleurElement(enum Element e)
{
    static const SDL_Color C[NB_ELEMENTS] = {
        { 210, 210, 220, 255 }, { 250, 110, 60, 255 }, { 120, 210, 250, 255 },
        { 190, 110, 250, 255 }, { 100, 220, 160, 255 }, { 120, 110, 240, 255 },
        { 250, 220, 100, 255 },
    };
    return (e >= 0 && e < NB_ELEMENTS) ? C[e] : COUL_TEXTE;
}

Pose poseDefaut(float temps)
{
    Pose p = { 0, 1, 0, 1, temps };
    return p;
}

/* ------------------------------------------------------------------ */
/*  Pinceau : repère local -> écran                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    Gui *g;
    float ox, oy, s;
    int sens;
    float flash, alpha;
} Pinceau;

static float PX(const Pinceau *p, float x) { return p->ox + p->sens * x * p->s; }
static float PY(const Pinceau *p, float y) { return p->oy + y * p->s; }

static SDL_Color pc(const Pinceau *p, SDL_Color c)
{
    if (p->flash > 0) {
        c = melanger(c, rgba(255, 255, 255, c.a), p->flash);
    }
    c.a = (Uint8)(c.a * p->alpha);
    return c;
}

static void pCercle(const Pinceau *p, float x, float y, float r, SDL_Color c)
{
    cercle(p->g, PX(p, x), PY(p, y), r * p->s, pc(p, c));
}

static void pEllipse(const Pinceau *p, float x, float y, float rx, float ry, SDL_Color c)
{
    ellipse(p->g, PX(p, x), PY(p, y), rx * p->s, ry * p->s, pc(p, c));
}

static void pHalo(const Pinceau *p, float x, float y, float r, SDL_Color c)
{
    halo(p->g, PX(p, x), PY(p, y), r * p->s, pc(p, c));
}

static void pLigne(const Pinceau *p, float x1, float y1, float x2, float y2, float ep, SDL_Color c)
{
    ligne(p->g, PX(p, x1), PY(p, y1), PX(p, x2), PY(p, y2), ep * p->s, pc(p, c));
}

static void pPoly(const Pinceau *p, const float *xy, int n, SDL_Color c)
{
    float t[64];
    if (n > 32) n = 32;
    for (int i = 0; i < n; i++) {
        t[i * 2] = PX(p, xy[i * 2]);
        t[i * 2 + 1] = PY(p, xy[i * 2 + 1]);
    }
    polygone(p->g, t, n, pc(p, c));
}

static void pTri(const Pinceau *p, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color c)
{
    float xy[6] = { x1, y1, x2, y2, x3, y3 };
    pPoly(p, xy, 3, c);
}

static void pQuad(const Pinceau *p, float x1, float y1, float x2, float y2,
                  float x3, float y3, float x4, float y4, SDL_Color c)
{
    float xy[8] = { x1, y1, x2, y2, x3, y3, x4, y4 };
    pPoly(p, xy, 4, c);
}

static void pRect(const Pinceau *p, float x, float y, float w, float h, SDL_Color c)
{
    pQuad(p, x, y, x + w, y, x + w, y + h, x, y + h, c);
}

/** Arc dans le repère local (les angles sont symétrisés si sens = -1). */
static void pArc(const Pinceau *p, float x, float y, float r, float ep, float a0, float a1, SDL_Color c)
{
    if (p->sens < 0) {
        float t = PI_F - a1;
        a1 = PI_F - a0;
        a0 = t;
    }
    arc(p->g, PX(p, x), PY(p, y), r * p->s, ep * p->s, a0, a1, pc(p, c));
}

/** Anneau elliptique (auréole) construit avec des segments. */
static void pAnneau(const Pinceau *p, float x, float y, float rx, float ry, float ep, SDL_Color c)
{
    const int n = 20;
    for (int i = 0; i < n; i++) {
        float a0 = 2 * PI_F * i / n, a1 = 2 * PI_F * (i + 1) / n;
        pLigne(p, x + cosf(a0) * rx, y + sinf(a0) * ry, x + cosf(a1) * rx, y + sinf(a1) * ry, ep, c);
    }
}

/* ------------------------------------------------------------------ */
/*  Icône d'élément                                                   */
/* ------------------------------------------------------------------ */

/** Icônes et noms des éléments d'un masque ; retourne la position finale. */
static float listeElements(Gui *g, unsigned masque, float x, float y)
{
    for (int e = 0; e < NB_ELEMENTS; e++) {
        if (masque & (1u << e)) {
            iconeElement(g, (enum Element)e, x + 8, y + 9, 7);
            x += 20 + texte(g, POLICE_PETITE, nomElement((enum Element)e), x + 19, y, COUL_TEXTE, ALIGN_GAUCHE);
            x += 8;
        }
    }
    return x;
}

float afficherAffinites(Gui *g, enum Element e, float x, float y, int colonne)
{
    if (colonne) {
        float cy = y;
        texte(g, POLICE_PETITE, "Fort contre", x, cy, COUL_VIE, ALIGN_GAUCHE);
        for (int k = 0; k < NB_ELEMENTS; k++) {
            if (elementsForts(e) & (1u << k)) {
                cy += 20;
                listeElements(g, 1u << k, x, cy);
            }
        }
        cy += 26;
        texte(g, POLICE_PETITE, "Faible contre", x, cy, COUL_DANGER, ALIGN_GAUCHE);
        for (int k = 0; k < NB_ELEMENTS; k++) {
            if (elementsFaibles(e) & (1u << k)) {
                cy += 20;
                listeElements(g, 1u << k, x, cy);
            }
        }
        return cy + 20 - y;
    }
    float cx = x;
    cx += texte(g, POLICE_PETITE, "Fort contre", cx, y, COUL_VIE, ALIGN_GAUCHE) + 8;
    cx = listeElements(g, elementsForts(e), cx, y);
    cx += texte(g, POLICE_PETITE, "Faible contre", cx + 6, y, COUL_DANGER, ALIGN_GAUCHE) + 14;
    cx = listeElements(g, elementsFaibles(e), cx, y);
    return cx - x;
}

void iconeElement(Gui *g, enum Element e, float cx, float cy, float r)
{
    SDL_Color c = couleurElement(e);
    cercle(g, cx, cy, r + 1.5f, rgba(15, 12, 30, 230));
    cercle(g, cx, cy, r, assombrir(c, 0.45f));
    SDL_Color b = c;
    switch (e) {
    case PHYSIQUE: /* poing : carré arrondi */
        rectArrondi(g, cx - r * 0.45f, cy - r * 0.45f, r * 0.9f, r * 0.9f, r * 0.25f, b);
        break;
    case FEU: { /* flamme */
        float xy[8] = { cx, cy - r * 0.7f, cx + r * 0.45f, cy + r * 0.15f,
                        cx, cy + r * 0.6f, cx - r * 0.45f, cy + r * 0.15f };
        polygone(g, xy, 4, b);
        cercle(g, cx, cy + r * 0.2f, r * 0.42f, b);
        break;
    }
    case GLACE: /* flocon */
        for (int k = 0; k < 3; k++) {
            float a = k * PI_F / 3;
            ligne(g, cx - cosf(a) * r * 0.65f, cy - sinf(a) * r * 0.65f,
                  cx + cosf(a) * r * 0.65f, cy + sinf(a) * r * 0.65f, r * 0.18f, b);
        }
        break;
    case FOUDRE: { /* éclair */
        float h1[6] = { cx + r * 0.15f, cy - r * 0.7f, cx - r * 0.4f, cy + r * 0.1f, cx + r * 0.05f, cy + r * 0.05f };
        float h2[6] = { cx - r * 0.15f, cy + r * 0.7f, cx + r * 0.4f, cy - r * 0.1f, cx - r * 0.05f, cy - r * 0.05f };
        polygone(g, h1, 3, b);
        polygone(g, h2, 3, b);
        break;
    }
    case VENT: /* tourbillon */
        arc(g, cx, cy, r * 0.5f, r * 0.2f, 0.3f, PI_F * 1.6f, b);
        arc(g, cx + r * 0.1f, cy, r * 0.22f, r * 0.16f, PI_F, PI_F * 2.4f, b);
        break;
    case QUANTIQUE: /* cube */
        losange(g, cx, cy, r * 0.6f, r * 0.6f, b);
        losange(g, cx, cy, r * 0.28f, r * 0.28f, assombrir(c, 0.45f));
        break;
    case IMAGINAIRE: { /* étoile */
        for (int k = 0; k < 4; k++) {
            float a = k * PI_F / 2;
            float xy[6] = { cx + cosf(a) * r * 0.75f, cy + sinf(a) * r * 0.75f,
                            cx + cosf(a + 0.6f) * r * 0.22f, cy + sinf(a + 0.6f) * r * 0.22f,
                            cx + cosf(a - 0.6f) * r * 0.22f, cy + sinf(a - 0.6f) * r * 0.22f };
            polygone(g, xy, 3, b);
        }
        cercle(g, cx, cy, r * 0.25f, b);
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Héros                                                             */
/* ------------------------------------------------------------------ */

static int estEnRobe(enum Classe c)
{
    return c == MAGE || c == PRETRE || c == NECROMANCIEN;
}

/** Cheveux de l'arrière (dessinés avant la tête). */
static void cheveuxArriere(const Pinceau *p, const Apparence *a, float hy, float t)
{
    SDL_Color c = PALETTE[a->couleurCheveux % NB_COULEURS];
    SDL_Color f = assombrir(c, 0.78f);
    switch (a->coupe) {
    case 0: case 2: case 8:
        pCercle(p, 0, hy - 3, 32, c);
        if (a->coupe == 2) { /* queue de cheval qui se balance */
            float sw = sinf(t * 2.6f) * 3;
            pEllipse(p, -31 + sw, hy + 12, 9, 25, f);
            pCercle(p, -27, hy - 8, 6, assombrir(c, 0.6f));
        }
        if (a->coupe == 8) {
            pCercle(p, 0, hy - 36, 13, c);
            pCercle(p, -3, hy - 40, 5, rgba(255, 255, 255, 40));
        }
        break;
    case 1:
        pCercle(p, 0, hy - 3, 33, c);
        pQuad(p, -33, hy - 3, 33, hy - 3, 30, hy + 44, -30, hy + 44, f);
        pEllipse(p, 0, hy + 44, 30, 7, f);
        break;
    case 3:
        for (int k = 0; k < 7; k++) {
            float ang = PI_F + 0.25f + k * (PI_F - 0.5f) / 6;
            float b1 = ang - 0.3f, b2 = ang + 0.3f;
            pTri(p, cosf(b1) * 30, hy - 3 + sinf(b1) * 30, cosf(b2) * 30, hy - 3 + sinf(b2) * 30,
                 cosf(ang) * 48, hy - 3 + sinf(ang) * 48, k % 2 ? c : f);
        }
        pCercle(p, 0, hy - 3, 32, c);
        break;
    case 5:
        pCercle(p, 0, hy - 3, 32, c);
        for (int cote = -1; cote <= 1; cote += 2) {
            for (int k = 0; k < 5; k++) {
                pCercle(p, cote * 27, hy + 8 + k * 8.5f, 6.5f - k * 0.4f, k % 2 ? f : c);
            }
        }
        break;
    case 7:
        pCercle(p, 0, hy - 3, 33, c);
        break;
    case 9:
        pCercle(p, 0, hy - 2, 35, c);
        for (int cote = -1; cote <= 1; cote += 2) {
            pCercle(p, cote * 28, hy + 8, 11, c);
            pCercle(p, cote * 30, hy + 24, 10, f);
            pCercle(p, cote * 27, hy + 39, 9, c);
        }
        break;
    default: /* 4 chauve, 6 iroquoise : rien derrière */
        break;
    }
}

/** Frange et mèches (dessinées sur le visage). */
static void cheveuxAvant(const Pinceau *p, const Apparence *a, float hy, SDL_Color peau)
{
    SDL_Color c = PALETTE[a->couleurCheveux % NB_COULEURS];
    SDL_Color f = assombrir(c, 0.8f);
    switch (a->coupe) {
    case 4: /* lisse : une seule petite plume bouclée */
        pArc(p, 2, hy - 30, 6, 2.5f, PI_F * 0.9f, PI_F * 2.2f, c);
        pEllipse(p, -8, hy - 18, 9, 5, rgba(255, 255, 255, 55));
        return;
    case 6: /* iroquoise : crête centrale, côtés rasés */
        pEllipse(p, 0, hy - 18, 29, 14, avecAlpha(assombrir(c, 0.6f), 90));
        for (int k = 0; k < 5; k++) {
            float x = -10 + k * 5;
            pTri(p, x - 6, hy - 22, x + 6, hy - 22, x + 2, hy - 54 + (k == 2 ? -6 : 0), k % 2 ? f : c);
        }
        return;
    case 7: /* carré : frange droite et longues mèches */
        pEllipse(p, 0, hy - 20, 31, 14, c);
        pRect(p, -30, hy - 20, 60, 11, c);
        pQuad(p, -34, hy - 8, -21, hy - 8, -21, hy + 24, -34, hy + 24, f);
        pQuad(p, 21, hy - 8, 34, hy - 8, 34, hy + 24, 21, hy + 24, f);
        return;
    default:
        break;
    }

    pEllipse(p, 0, hy - 21, 31, 13, c);
    if (a->coupe == 2) {
        /* mèche balayée sur le côté */
        pTri(p, -30, hy - 20, 26, hy - 22, -18, hy - 2, f);
        pTri(p, 10, hy - 22, 30, hy - 18, 26, hy - 4, c);
        return;
    }
    int pointes = (a->coupe == 3) ? 5 : 4;
    float largeur = 60.0f / pointes;
    for (int k = 0; k < pointes; k++) {
        float x0 = -30 + k * largeur;
        float bout = (a->coupe == 3) ? hy - 1 : hy - 5 - (k % 2) * 3;
        pTri(p, x0, hy - 18, x0 + largeur, hy - 18, x0 + largeur * 0.6f, bout, k % 2 ? f : c);
    }
    if (a->coupe == 1 || a->coupe == 9 || a->coupe == 5) {
        /* mèches qui encadrent le visage */
        pQuad(p, -31, hy - 12, -24, hy - 12, -25, hy + 18, -31, hy + 12, f);
        pQuad(p, 24, hy - 12, 31, hy - 12, 31, hy + 12, 25, hy + 18, f);
    }
    (void)peau;
}

/** Couvre-chef. */
static void coiffe(const Pinceau *p, const Apparence *a, float hy, float t)
{
    SDL_Color or = rgba(236, 190, 70, 255);
    SDL_Color t1 = PALETTE[a->tenue % NB_COULEURS];
    SDL_Color t2 = PALETTE[a->tenue2 % NB_COULEURS];
    SDL_Color metal = rgba(176, 182, 198, 255);
    switch (a->coiffe) {
    case 1: /* couronne */
        pRect(p, -19, hy - 34, 38, 8, or);
        for (int k = 0; k < 5; k++) {
            float x = -19 + k * 9.5f;
            pTri(p, x - 4, hy - 33, x + 4, hy - 33, x, hy - 47 - (k == 2 ? 5 : 0), or);
        }
        pCercle(p, 0, hy - 30, 3, rgba(220, 40, 70, 255));
        pCercle(p, -11, hy - 30, 2.2f, rgba(60, 140, 240, 255));
        pCercle(p, 11, hy - 30, 2.2f, rgba(60, 200, 120, 255));
        break;
    case 2: /* capuche : rebord autour du visage */
        pArc(p, 0, hy + 2, 34, 9, PI_F + 0.05f, 2 * PI_F - 0.05f, t2);
        pQuad(p, -38, hy + 2, -29, hy + 2, -27, hy + 30, -36, hy + 26, assombrir(t2, 0.85f));
        pQuad(p, 29, hy + 2, 38, hy + 2, 36, hy + 26, 27, hy + 30, assombrir(t2, 0.85f));
        break;
    case 3: /* cornes */
        for (int cote = -1; cote <= 1; cote += 2) {
            pQuad(p, cote * 14, hy - 26, cote * 23, hy - 22, cote * 33, hy - 48, cote * 27, hy - 52,
                  rgba(232, 222, 200, 255));
            pTri(p, cote * 33, hy - 48, cote * 27, hy - 52, cote * 36, hy - 62, rgba(120, 100, 90, 255));
        }
        break;
    case 4: { /* auréole lumineuse */
        float b = sinf(t * 2) * 2;
        pHalo(p, 0, hy - 44 + b, 34, rgba(255, 230, 140, 90));
        pAnneau(p, 0, hy - 44 + b, 22, 6, 3.5f, rgba(255, 226, 120, 255));
        break;
    }
    case 5: /* bandeau */
        pRect(p, -31, hy - 17, 62, 7, t2);
        pLigne(p, -30, hy - 13, -46, hy - 4 + sinf(t * 3) * 3, 4, t2);
        pLigne(p, -30, hy - 13, -43, hy + 6 + sinf(t * 3 + 1) * 3, 4, assombrir(t2, 0.8f));
        break;
    case 6: /* chapeau de mage */
        pEllipse(p, 0, hy - 24, 46, 9, assombrir(t1, 0.7f));
        pTri(p, -25, hy - 26, 25, hy - 26, 12 + sinf(t * 1.5f) * 2, hy - 86, t1);
        pQuad(p, -23, hy - 30, 23, hy - 30, 21, hy - 37, -21, hy - 37, t2);
        pCercle(p, 6, hy - 52, 3, or);
        break;
    case 7: { /* casque */
        float xy[2 * 13];
        for (int i = 0; i <= 12; i++) {
            float ang = PI_F + PI_F * i / 12;
            xy[i * 2] = cosf(ang) * 34;
            xy[i * 2 + 1] = hy - 6 + sinf(ang) * 32;
        }
        pPoly(p, xy, 13, metal);
        pRect(p, -34, hy - 9, 68, 6, assombrir(metal, 0.75f));
        pRect(p, 1, hy - 6, 5, 15, assombrir(metal, 0.8f));
        pEllipse(p, -6, hy - 26, 8, 4, rgba(255, 255, 255, 70));
        pEllipse(p, -4, hy - 44, 6, 12, t2);
        break;
    }
    case 8: { /* oreilles de chat */
        SDL_Color c = PALETTE[a->couleurCheveux % NB_COULEURS];
        for (int cote = -1; cote <= 1; cote += 2) {
            pTri(p, cote * 10, hy - 28, cote * 28, hy - 20, cote * 25, hy - 50, c);
            pTri(p, cote * 14, hy - 28, cote * 25, hy - 23, cote * 23, hy - 42, rgba(250, 170, 190, 255));
        }
        break;
    }
    case 9: /* fleur */
        for (int k = 0; k < 5; k++) {
            float ang = k * 2 * PI_F / 5 + t * 0.3f;
            pCercle(p, 22 + cosf(ang) * 5.5f, hy - 20 + sinf(ang) * 5.5f, 4.5f, rgba(250, 150, 190, 255));
        }
        pCercle(p, 22, hy - 20, 3.5f, rgba(255, 220, 90, 255));
        break;
    case 10: { /* toque de chef */
        SDL_Color blanc = rgba(250, 250, 246, 255);
        SDL_Color ombre = rgba(214, 214, 222, 255);
        pRect(p, -19, hy - 42, 38, 18, blanc);
        pCercle(p, -13, hy - 48, 13, ombre);
        pCercle(p, 13, hy - 48, 13, ombre);
        pCercle(p, 0, hy - 56, 15, blanc);
        pCercle(p, -12, hy - 50, 11, blanc);
        pCercle(p, 12, hy - 50, 11, blanc);
        pLigne(p, -19, hy - 30, 19, hy - 30, 2, ombre);
        break;
    }
    case 11: { /* haut-de-forme */
        SDL_Color noir = rgba(34, 30, 40, 255);
        pEllipse(p, 0, hy - 25, 36, 7, noir);
        pRect(p, -19, hy - 70, 38, 46, noir);
        pEllipse(p, 0, hy - 70, 19, 4, rgba(60, 54, 70, 255));
        pRect(p, -19, hy - 36, 38, 7, t2);
        pLigne(p, -14, hy - 64, -14, hy - 40, 2, rgba(255, 255, 255, 40));
        break;
    }
    case 12: { /* bicorne de pirate */
        SDL_Color noir = rgba(36, 30, 36, 255);
        float xy[12] = { -44, hy - 20, -30, hy - 44, 0, hy - 52, 30, hy - 44, 44, hy - 20, 0, hy - 30 };
        pPoly(p, xy, 6, noir);
        pLigne(p, -44, hy - 20, 0, hy - 30, 2.5f, or);
        pLigne(p, 0, hy - 30, 44, hy - 20, 2.5f, or);
        pCercle(p, 0, hy - 40, 5.5f, rgba(240, 236, 224, 255));
        pRect(p, -3, hy - 37, 6, 4, rgba(240, 236, 224, 255));
        pCercle(p, -2, hy - 41, 1.4f, noir);
        pCercle(p, 2, hy - 41, 1.4f, noir);
        break;
    }
    case 13: { /* casque viking */
        for (int cote = -1; cote <= 1; cote += 2) {
            pQuad(p, cote * 26, hy - 26, cote * 32, hy - 18, cote * 48, hy - 40, cote * 44, hy - 48,
                  rgba(240, 232, 210, 255));
            pTri(p, cote * 44, hy - 48, cote * 48, hy - 40, cote * 50, hy - 62, rgba(240, 232, 210, 255));
        }
        float xy[2 * 13];
        for (int i = 0; i <= 12; i++) {
            float ang = PI_F + PI_F * i / 12;
            xy[i * 2] = cosf(ang) * 32;
            xy[i * 2 + 1] = hy - 8 + sinf(ang) * 28;
        }
        pPoly(p, xy, 13, metal);
        pRect(p, -33, hy - 11, 66, 7, rgba(150, 110, 60, 255));
        pLigne(p, 0, hy - 36, 0, hy - 11, 4, rgba(150, 110, 60, 255));
        for (int k = -2; k <= 2; k++) pCercle(p, k * 12, hy - 7.5f, 1.8f, or);
        break;
    }
    case 14: { /* couronne du Néant : flotte et brille */
        float b = sinf(t * 2) * 2;
        SDL_Color violet = rgba(150, 70, 230, 255);
        pHalo(p, 0, hy - 40 + b, 40, rgba(170, 90, 255, 90));
        pRect(p, -19, hy - 36 + b, 38, 7, rgba(40, 20, 60, 255));
        for (int k = 0; k < 5; k++) {
            float x = -19 + k * 9.5f;
            pTri(p, x - 4, hy - 35 + b, x + 4, hy - 35 + b, x, hy - 50 - (k == 2 ? 7 : 0) + b, violet);
        }
        pCercle(p, 0, hy - 32 + b, 3, rgba(230, 170, 255, 255));
        for (int k = 0; k < 3; k++) {
            float ang = t * 1.5f + k * 2 * PI_F / 3;
            pCercle(p, cosf(ang) * 28, hy - 44 + b + sinf(ang) * 7, 2.2f, rgba(220, 160, 255, 220));
        }
        break;
    }
    default:
        break;
    }
}

/** Yeux, sourcils, bec, marques et pilosité (oui, nos canards peuvent
 *  porter la barbe). Le bec pointe vers le regard (+x). */
static void visage(const Pinceau *p, const Apparence *a, float hy, SDL_Color plume)
{
    SDL_Color iris = PALETTE[a->couleurYeux % NB_COULEURS];
    SDL_Color cheveux = PALETTE[a->couleurCheveux % NB_COULEURS];
    SDL_Color sombre = rgba(40, 30, 40, 255);
    SDL_Color t2 = PALETTE[a->tenue2 % NB_COULEURS];
    SDL_Color bec = rgba(252, 158, 40, 255);
    SDL_Color becSombre = rgba(214, 112, 26, 255);
    float ey = hy - 2;

    /* joues */
    pEllipse(p, -13, hy + 7, 5, 3, rgba(255, 120, 130, 60));
    pEllipse(p, 15, hy + 7, 5, 3, rgba(255, 120, 130, 60));

    /* marques */
    switch (a->marque) {
    case 1: pLigne(p, -16, hy - 12, -5, hy + 8, 2.2f, rgba(160, 60, 60, 230)); break;
    case 2:
        for (int cote = -1; cote <= 1; cote += 2) {
            float cx = cote * 12;
            pLigne(p, cx - 5, hy + 6, cx + 4, hy + 6, 2.6f, t2);
            pLigne(p, cx - 4, hy + 10, cx + 3, hy + 10, 2.6f, t2);
        }
        break;
    case 3: {
        SDL_Color c = a->aura ? PALETTE[(a->aura - 1) % NB_COULEURS] : t2;
        float xy[8] = { 1, hy - 20, 4, hy - 15, 1, hy - 10, -2, hy - 15 };
        pPoly(p, xy, 4, c);
        break;
    }
    case 4:
        for (int k = 0; k < 3; k++) {
            pCercle(p, -15 + k * 3, hy + 5 + (k % 2) * 2, 1.1f, rgba(170, 100, 70, 200));
            pCercle(p, 11 + k * 3, hy + 5 + (k % 2) * 2, 1.1f, rgba(170, 100, 70, 200));
        }
        break;
    default:
        break;
    }

    /* yeux (légèrement décalés vers le regard) */
    for (int cote = -1; cote <= 1; cote += 2) {
        float ex = cote * 10 + 1;
        if (a->regard == 3) {
            pArc(p, ex, ey + 2, 5, 2.4f, PI_F + 0.35f, 2 * PI_F - 0.35f, sombre);
        } else {
            pEllipse(p, ex, ey, 5.5f, 7, rgba(255, 255, 255, 255));
            pEllipse(p, ex + 1, ey + 0.8f, 4.1f, 5.4f, iris);
            pEllipse(p, ex + 1, ey + 1.5f, 2.2f, 3, assombrir(iris, 0.3f));
            pCercle(p, ex - 0.8f, ey - 2.4f, 1.7f, rgba(255, 255, 255, 240));
            if (a->regard == 2) { /* paupières tombantes */
                pRect(p, ex - 6.5f, ey - 8, 13, 7.5f, plume);
                pLigne(p, ex - 6, ey - 0.8f, ex + 6, ey - 0.8f, 2, sombre);
            } else {
                pArc(p, ex, ey, 6, 1.8f, PI_F + 0.2f, 2 * PI_F - 0.2f, sombre);
            }
            if (a->regard == 1) { /* regard déterminé : paupière inclinée */
                pLigne(p, ex - 6 * cote, ey - 7.5f, ex + 6 * cote, ey - 4, 2.6f, sombre);
            }
        }
    }

    /* barbe et bouc sous le bec */
    if (a->pilosite == 1) {
        pQuad(p, -4, hy + 12, 30, hy + 12, 24, hy + 28, 2, hy + 28, cheveux);
        pTri(p, 2, hy + 28, 24, hy + 28, 13, hy + 36, cheveux);
    } else if (a->pilosite == 3) {
        pTri(p, 8, hy + 14, 20, hy + 14, 14, hy + 26, cheveux);
    }

    /* bec : mandibule inférieure (ouverte si le canard est joyeux) puis
     * supérieure */
    float ouvert = (a->regard == 3) ? 3 : 0;
    pEllipse(p, 17, hy + 12 + ouvert, 12, 4.5f + ouvert * 0.4f, becSombre);
    if (ouvert > 0) {
        pEllipse(p, 18, hy + 11 + ouvert, 7, 2.2f, rgba(150, 50, 60, 255));
    }
    pEllipse(p, 19, hy + 8, 15, 6, bec);
    pEllipse(p, 16, hy + 6, 8, 2, rgba(255, 220, 150, 140));
    pCercle(p, 26, hy + 6, 1.2f, becSombre);

    if (a->pilosite == 2) { /* moustache posée sur le bec */
        pEllipse(p, 12, hy + 5, 7, 2.6f, cheveux);
        pEllipse(p, 24, hy + 5, 6, 2.4f, cheveux);
    }

    /* sourcils */
    SDL_Color sc = assombrir(cheveux, 0.7f);
    for (int cote = -1; cote <= 1; cote += 2) {
        float ex = cote * 10 + 1;
        float inc = (a->regard == 1) ? 2.5f * cote : -0.8f * cote;
        pLigne(p, ex - 5, ey - 11 - inc, ex + 5, ey - 11 + inc, 2, sc);
    }
}

/** Arme tenue dans la main avant (hx, hy), selon la classe. */
static void arme(const Pinceau *p, const Personnage *perso, float hx, float hy, float atk, float t)
{
    SDL_Color bois = rgba(116, 78, 46, 255);
    SDL_Color acier = rgba(214, 220, 232, 255);
    SDL_Color or = rgba(236, 190, 70, 255);
    SDL_Color elem = couleurElement(perso->element);
    float ang = -1.25f + atk * 1.35f; /* l'arme s'abat vers l'avant */
    float dx = cosf(ang), dy = sinf(ang);

    switch (perso->classe) {
    case GUERRIER:
        pLigne(p, hx - dx * 8, hy - dy * 8, hx + dx * 62, hy + dy * 62, 7, acier);
        pLigne(p, hx + dx * 6, hy + dy * 6, hx + dx * 60, hy + dy * 60, 2, rgba(255, 255, 255, 180));
        pLigne(p, hx - dy * 11 + dx * 5, hy + dx * 11 + dy * 5, hx + dy * 11 + dx * 5, hy - dx * 11 + dy * 5, 4.5f, or);
        pCercle(p, hx - dx * 10, hy - dy * 10, 3.5f, or);
        break;
    case MAGE:
        pLigne(p, hx - 4, hy + 32, hx + 6, hy - 62, 5, bois);
        pHalo(p, hx + 6, hy - 66, 24 + atk * 18 + sinf(t * 4) * 2, avecAlpha(elem, 120));
        pCercle(p, hx + 6, hy - 66, 9, elem);
        pCercle(p, hx + 3, hy - 69, 3, rgba(255, 255, 255, 200));
        break;
    case VOLEUR:
        pLigne(p, hx, hy, hx + dx * 30, hy + dy * 30, 5, acier);
        pLigne(p, hx - dy * 6, hy + dx * 6, hx + dy * 6, hy - dx * 6, 3, rgba(60, 50, 50, 255));
        break;
    case PALADIN:
        pLigne(p, hx - dx * 6, hy - dy * 6, hx + dx * 34, hy + dy * 34, 5, bois);
        pCercle(p, hx + dx * 38, hy + dy * 38, 9, acier);
        for (int k = 0; k < 4; k++) {
            float a2 = k * PI_F / 2 + 0.4f;
            pCercle(p, hx + dx * 38 + cosf(a2) * 9, hy + dy * 38 + sinf(a2) * 9, 3, or);
        }
        break;
    case ARCHER:
        pArc(p, hx - 8, hy, 42, 4.5f, -1.15f, 1.15f, bois);
        pLigne(p, hx - 8 + cosf(-1.15f) * 42, hy + sinf(-1.15f) * 42,
               hx - 8 - atk * 14 + 6, hy, 1.2f, rgba(240, 240, 240, 200));
        pLigne(p, hx - 8 - atk * 14 + 6, hy, hx - 8 + cosf(1.15f) * 42, hy + sinf(1.15f) * 42,
               1.2f, rgba(240, 240, 240, 200));
        if (atk > 0.05f) {
            pLigne(p, hx - atk * 14, hy, hx + 40, hy, 2.5f, acier);
            pTri(p, hx + 40, hy - 4, hx + 40, hy + 4, hx + 48, hy, elem);
        }
        break;
    case PRETRE:
        pLigne(p, hx - 3, hy + 32, hx + 4, hy - 60, 5, rgba(236, 226, 196, 255));
        pHalo(p, hx + 4, hy - 64, 26 + atk * 14, rgba(255, 230, 140, 110));
        for (int k = 0; k < 8; k++) {
            float a2 = k * PI_F / 4 + t * 0.8f;
            pLigne(p, hx + 4 + cosf(a2) * 9, hy - 64 + sinf(a2) * 9,
                   hx + 4 + cosf(a2) * 16, hy - 64 + sinf(a2) * 16, 2, or);
        }
        pCercle(p, hx + 4, hy - 64, 8, or);
        break;
    case NECROMANCIEN:
        pLigne(p, hx - 6, hy + 36, hx + 8, hy - 70, 5, rgba(70, 56, 70, 255));
        /* lame tournée vers l'avant pour ne pas cacher le visage */
        pQuad(p, hx + 8, hy - 70, hx + 42, hy - 66, hx + 58, hy - 50, hx + 32, hy - 62, rgba(196, 204, 216, 255));
        pHalo(p, hx + 30, hy - 62, 22 + atk * 14, avecAlpha(elem, 90));
        break;
    case BARDE:
        pLigne(p, hx - 14, hy + 8, hx + 20, hy - 30, 4, rgba(90, 56, 34, 255));
        pEllipse(p, hx - 16, hy + 10, 15, 12, rgba(196, 128, 66, 255));
        pCercle(p, hx - 14, hy + 8, 4, rgba(60, 36, 24, 255));
        if (atk > 0.05f || sinf(t * 1.7f) > 0.7f) { /* notes de musique */
            float n = fmodf(t * 1.3f, 1.0f);
            pCercle(p, hx + 20, hy - 40 - n * 30, 3.5f, avecAlpha(elem, (int)(255 * (1 - n))));
            pLigne(p, hx + 23, hy - 40 - n * 30, hx + 23, hy - 54 - n * 30, 1.5f, avecAlpha(elem, (int)(255 * (1 - n))));
        }
        break;
    default:
        break;
    }
}

void dessinerHeros(Gui *g, const Personnage *perso, float x, float y, float s, const Pose *pose)
{
    Pose def = poseDefaut(0);
    if (pose == NULL) {
        pose = &def;
    }
    Pinceau p = { g, x, y, s, pose->sens < 0 ? -1 : 1, pose->flash, pose->alpha };
    const Apparence *a = &perso->apparence;
    float t = pose->temps;
    float cw = (a->corpulence == 0) ? 0.86f : (a->corpulence == 2 ? 1.2f : 1.0f);
    float b = sinf(t * 2.2f) * 1.6f; /* respiration */
    float hy = -108 + b;             /* centre de la tête */
    float atk = pose->attaque;

    SDL_Color plume = PLUMAGES[a->plumage % NB_PLUMAGES];
    SDL_Color plumeSombre = assombrir(plume, 0.8f);
    SDL_Color ventre = melanger(plume, rgba(255, 250, 235, 255), 0.45f);
    SDL_Color patte = rgba(250, 150, 40, 255);
    SDL_Color t1 = PALETTE[a->tenue % NB_COULEURS];
    SDL_Color t2 = PALETTE[a->tenue2 % NB_COULEURS];

    /* ombre au sol et aura */
    ellipse(g, x, y, 38 * s * cw, 9 * s, rgba(0, 0, 0, (int)(90 * pose->alpha)));
    if (a->aura) {
        SDL_Color ca = PALETTE[(a->aura - 1) % NB_COULEURS];
        pHalo(&p, 0, -70, 95 + sinf(t * 3) * 6, avecAlpha(ca, 85));
        for (int k = 0; k < 6; k++) {
            float ph = fmodf(t * 0.6f + k / 6.0f, 1.0f);
            pCercle(&p, sinf(k * 2.1f + t) * 34, -10 - ph * 140, 2.5f * (1 - ph) + 0.5f,
                    avecAlpha(ca, (int)(220 * (1 - ph))));
        }
    }

    /* cape (derrière) */
    if (a->cape) {
        float sw = sinf(t * 1.8f) * 4 - 4;
        pQuad(&p, -20 * cw, -80 + b, 20 * cw, -80 + b, 30 * cw + sw, -6, -38 * cw + sw, -6, assombrir(t2, 0.7f));
    }
    if (a->coiffe == 2) {
        pCercle(&p, 0, hy - 2, 37, assombrir(t2, 0.75f)); /* fond de capuche */
    } else {
        cheveuxArriere(&p, a, hy, t);
    }

    /* queue en plumes qui frétille */
    float fr = sinf(t * 5) * 3;
    pTri(&p, -26 * cw, -60 + b, -52 * cw, -80 + fr, -34 * cw, -38, plumeSombre);
    pTri(&p, -28 * cw, -54 + b, -48 * cw, -66 + fr, -30 * cw, -38, plume);

    /* aile arrière */
    float bx = -28 * cw, by = -50 + b;
    pEllipse(&p, -24 * cw, -60 + b, 9, 18, assombrir(plume, 0.7f));
    if (perso->classe == VOLEUR) {
        pLigne(&p, bx, by, bx - 14, by - 24, 4.5f, rgba(200, 206, 220, 255));
    }

    /* pattes palmées */
    pLigne(&p, -9 * cw, -24, -9 * cw, -5, 4, patte);
    pLigne(&p, 9 * cw, -24, 9 * cw, -5, 4, patte);
    pTri(&p, -19 * cw, 0, -1 * cw + 4, 0, -9 * cw, -7, patte);
    pTri(&p, 1 * cw, 0, 21 * cw + 4, 0, 9 * cw, -7, assombrir(patte, 0.9f));

    /* corps rond et ventre clair */
    pEllipse(&p, 0, -50 + b * 0.5f, 32 * cw, 36, plume);
    pEllipse(&p, 6 * cw, -42 + b * 0.5f, 21 * cw, 25, ventre);

    /* tenue */
    if (estEnRobe(perso->classe)) {
        pQuad(&p, -22 * cw, -78 + b, 22 * cw, -78 + b, 34 * cw, -12, -34 * cw, -12, t1);
        pQuad(&p, -32 * cw, -22, 32 * cw, -22, 34 * cw, -12, -34 * cw, -12, t2);
        pLigne(&p, 2, -72 + b, 2, -14, 3, t2);
        pQuad(&p, -22 * cw, -78 + b, 0, -78 + b, 0, -12, -34 * cw, -12, rgba(0, 0, 0, 34));
    } else {
        pQuad(&p, -27 * cw, -74 + b, 27 * cw, -74 + b, 31 * cw, -36, -31 * cw, -36, t1);
        pEllipse(&p, 0, -36, 31 * cw, 7, assombrir(t1, 0.75f));
        pQuad(&p, -27 * cw, -74 + b, 0, -74 + b, 0, -36, -31 * cw, -36, rgba(0, 0, 0, 34));
        pRect(&p, -30 * cw, -48, 60 * cw, 6, t2);
        pRect(&p, -4, -49, 8, 8, rgba(236, 190, 70, 255));
    }
    pLigne(&p, -10, -76 + b, 1, -66 + b, 2.2f, t2);
    pLigne(&p, 12, -76 + b, 1, -66 + b, 2.2f, t2);

    switch (perso->classe) {
    case GUERRIER: case PALADIN: { /* épaulières */
        SDL_Color m = rgba(176, 182, 198, 255);
        pEllipse(&p, -22 * cw, -72 + b, 11, 7.5f, assombrir(m, 0.85f));
        pEllipse(&p, 22 * cw, -72 + b, 11, 7.5f, m);
        pEllipse(&p, 20 * cw, -74 + b, 5, 2.5f, rgba(255, 255, 255, 90));
        break;
    }
    case VOLEUR: case ARCHER: /* écharpe */
        pEllipse(&p, 0, -78 + b, 17 * cw, 6, t2);
        pLigne(&p, -6, -76 + b, -18 + sinf(t * 3) * 3, -56 + b, 5, assombrir(t2, 0.8f));
        break;
    case BARDE: /* jabot */
        for (int k = 0; k < 3; k++) pCercle(&p, 2, -74 + b + k * 5, 4.5f, t2);
        break;
    default:
        break;
    }
    if (perso->classe == PALADIN) { /* bouclier */
        float sx = -26 * cw, sy = -50 + b;
        pQuad(&p, sx - 15, sy - 20, sx + 15, sy - 20, sx + 13, sy + 6, sx - 13, sy + 6, assombrir(t2, 0.9f));
        pTri(&p, sx - 13, sy + 6, sx + 13, sy + 6, sx, sy + 24, assombrir(t2, 0.9f));
        pRect(&p, sx - 2.5f, sy - 15, 5, 30, rgba(236, 190, 70, 255));
        pRect(&p, sx - 10, sy - 6, 20, 5, rgba(236, 190, 70, 255));
    }

    /* tête */
    pCercle(&p, 0, hy, 28, plume);
    pEllipse(&p, -11, hy + 5, 13, 17, rgba(0, 0, 0, 14));
    pEllipse(&p, -6, hy - 14, 9, 5, rgba(255, 255, 255, 40));

    visage(&p, a, hy, plume);
    cheveuxAvant(&p, a, hy, plume);
    coiffe(&p, a, hy, t);

    /* aile avant (elle tient l'arme) */
    float hx = 28 * cw + atk * 6, hyMain = -48 + b - atk * 34;
    arme(&p, perso, hx, hyMain, atk, t);
    pLigne(&p, 18 * cw, -70 + b, hx, hyMain, 12, plumeSombre);
    pEllipse(&p, hx, hyMain, 7, 6, plume);
    pTri(&p, hx - 3, hyMain + 3, hx + 5, hyMain + 3, hx + 1, hyMain + 10, plumeSombre);
}

/* ------------------------------------------------------------------ */
/*  Foie gras                                                         */
/* ------------------------------------------------------------------ */

void iconeCadenas(Gui *g, float cx, float cy, float r)
{
    arc(g, cx, cy - r * 0.25f, r * 0.5f, r * 0.22f, PI_F, 2 * PI_F, rgba(220, 210, 240, 255));
    rectArrondi(g, cx - r * 0.8f, cy - r * 0.25f, r * 1.6f, r * 1.2f, r * 0.25f, rgba(236, 190, 70, 255));
    cercle(g, cx, cy + r * 0.25f, r * 0.2f, rgba(60, 40, 20, 255));
}

void iconeFoieGras(Gui *g, float cx, float cy, float r)
{
    /* une tranche ronde : bord de graisse doré, cœur rosé, reflet */
    ellipse(g, cx, cy + r * 0.25f, r * 1.05f, r * 0.75f, rgba(150, 100, 60, 255));
    ellipse(g, cx, cy, r, r * 0.72f, rgba(244, 214, 140, 255));
    ellipse(g, cx, cy, r * 0.78f, r * 0.54f, rgba(232, 168, 150, 255));
    ellipse(g, cx - r * 0.25f, cy - r * 0.18f, r * 0.3f, r * 0.14f, rgba(255, 240, 230, 170));
}

/* ------------------------------------------------------------------ */
/*  Monstres                                                          */
/* ------------------------------------------------------------------ */

/** Le Canard Noir, boss final : un héros canard corrompu, dessiné avec
 *  le même code que nos héros (il en a été un, autrefois...). */
static const Personnage *canardNoir(void)
{
    static Personnage p;
    static int pret = 0;
    if (!pret) {
        memset(&p, 0, sizeof p);
        strcpy(p.nom, "Canard Noir");
        p.classe = NECROMANCIEN;
        p.element = IMAGINAIRE;
        /* plumage noir, iroquoise rouge, yeux rouges, cornes, cicatrice,
         * cape, aura violette, massif */
        p.apparence = (Apparence){ 4, 6, 7, 7, 1, 0, 7, 3, 0, 1, 1, 10, 2 };
        pret = 1;
    }
    return &p;
}
#define ECHELLE_CANARD_NOIR 1.45f

/** Corps d'une gelée de couleur c (utilisé par la gelée et son roi). */
static void dessinerGelee(Gui *g, const Pinceau *p, float t, float alpha, SDL_Color c)
{
    float w = sinf(t * 4) * 4;
    ellipse(g, p->ox, p->oy, 44 * p->s, 9 * p->s, rgba(0, 0, 0, (int)(80 * alpha)));
    pEllipse(p, 0, -32 + w * 0.5f, 44 + w, 34 - w, c);
    pEllipse(p, 0, -14, 46 + w, 14, assombrir(c, 0.75f));
    pEllipse(p, -4, -30, 18, 14, avecAlpha(assombrir(c, 0.6f), 160));
    pEllipse(p, -16, -50, 10, 6, rgba(255, 255, 255, 110));
    pCercle(p, 6, -40, 8, rgba(255, 255, 255, 255));
    pCercle(p, 24, -38, 7, rgba(255, 255, 255, 255));
    pCercle(p, 8, -39, 4, rgba(20, 40, 20, 255));
    pCercle(p, 26, -37, 3.5f, rgba(20, 40, 20, 255));
    pArc(p, 16, -26, 7, 2.5f, 0.3f, PI_F - 0.3f, assombrir(c, 0.4f));
}

float hauteurMonstre(enum Monstre m)
{
    switch (m) {
    case MONSTRE_GELEE: return 80;
    case MONSTRE_LOUP: return 100;
    case MONSTRE_CHAMPIGNON: return 110;
    case MONSTRE_DIABLOTIN: return 130;
    case MONSTRE_ROI_GELEE: return 175;
    case MONSTRE_GARGOUILLE: return 170;
    case MONSTRE_GOLEM: return 190;
    case MONSTRE_LICHE: return 220;
    case MONSTRE_DRAGON: return 240;
    case MONSTRE_CANARD_NOIR: return 185 * ECHELLE_CANARD_NOIR;
    default: return 170;
    }
}

void dessinerMonstre(Gui *g, enum Monstre m, float x, float y, float s, const Pose *pose)
{
    Pose def = poseDefaut(0);
    if (pose == NULL) {
        pose = &def;
    }
    Pinceau p = { g, x, y, s, pose->sens < 0 ? -1 : 1, pose->flash, pose->alpha };
    float t = pose->temps;
    float atk = pose->attaque;
    SDL_Color yeux = rgba(255, 230, 120, 255);

    switch (m) {
    case MONSTRE_GELEE:
        dessinerGelee(g, &p, t, pose->alpha, rgba(110, 220, 120, 215));
        break;
    case MONSTRE_ROI_GELEE: {
        /* une gelée géante et royale : même corps, à l'échelle x2 */
        Pinceau grand = p;
        grand.s = s * 2.0f;
        SDL_Color c = rgba(90, 150, 240, 220);
        halo(g, x, y - 70 * s, 120 * s, rgba(120, 170, 255, (int)(40 * pose->alpha)));
        dessinerGelee(g, &grand, t * 0.7f, pose->alpha, c);
        float w = sinf(t * 2.8f) * 4 * 2; /* suit l'ondulation du corps */
        float cy = -128 + w - atk * 10;
        SDL_Color or = rgba(240, 196, 70, 255);
        pRect(&p, -30, cy, 60, 14, or);
        for (int k = 0; k < 5; k++) {
            float bx = -30 + k * 15;
            pTri(&p, bx - 3, cy, bx + 3, cy, bx, cy - 18 - (k == 2 ? 8 : 0), or);
        }
        pCercle(&p, 0, cy + 7, 4, rgba(220, 40, 70, 255));
        pCercle(&p, -18, cy + 7, 3, rgba(60, 200, 120, 255));
        pCercle(&p, 18, cy + 7, 3, rgba(60, 200, 120, 255));
        /* sourcils froncés */
        pLigne(&p, 2, -96, 22, -90, 3, assombrir(c, 0.4f));
        pLigne(&p, 38, -90, 56, -96, 3, assombrir(c, 0.4f));
        break;
    }
    case MONSTRE_CHAMPIGNON: {
        float b = sinf(t * 3) * 2;
        float lev = atk * 6;
        SDL_Color pied = rgba(236, 222, 190, 255);
        SDL_Color chap = rgba(170, 60, 150, 255);
        ellipse(g, x, y, 36 * s, 8 * s, rgba(0, 0, 0, (int)(80 * pose->alpha)));
        pEllipse(&p, -10, -4, 10, 5, assombrir(pied, 0.8f));
        pEllipse(&p, 12, -4, 10, 5, assombrir(pied, 0.8f));
        pQuad(&p, -18, -6, 18, -6, 14, -62 + b, -14, -62 + b, pied);
        pHalo(&p, 0, -70 + b - lev, 54, rgba(220, 120, 255, 45));
        pEllipse(&p, 0, -66 + b - lev, 50, 30, chap);
        pEllipse(&p, 0, -56 + b - lev, 48, 9, assombrir(chap, 0.65f));
        pCercle(&p, -22, -76 + b - lev, 7, rgba(255, 240, 250, 210));
        pCercle(&p, 8, -86 + b - lev, 9, rgba(255, 240, 250, 210));
        pCercle(&p, 30, -70 + b - lev, 6, rgba(255, 240, 250, 210));
        pCercle(&p, 0, -36 + b, 5, rgba(255, 255, 255, 255));
        pCercle(&p, 12, -36 + b, 5, rgba(255, 255, 255, 255));
        pCercle(&p, 1.5f, -35 + b, 2.5f, rgba(30, 20, 30, 255));
        pCercle(&p, 13.5f, -35 + b, 2.5f, rgba(30, 20, 30, 255));
        pArc(&p, 7, -26 + b, 5, 2, 0.3f, PI_F - 0.3f, rgba(90, 60, 50, 255));
        if (atk > 0.05f) { /* nuage de spores */
            for (int k = 0; k < 7; k++) {
                float sx = 20 + k * 12 * atk + sinf(t * 6 + k) * 4, sy = -60 + cosf(k * 1.7f) * 22;
                pCercle(&p, sx, sy, 4 + (k % 3), rgba(200, 130, 230, (int)(170 * atk)));
            }
        }
        break;
    }
    case MONSTRE_DIABLOTIN: {
        float b = sinf(t * 5) * 5;
        float y0 = -70 + b; /* il vole */
        SDL_Color c = rgba(220, 70, 50, 255);
        SDL_Color f = assombrir(c, 0.7f);
        SDL_Color corne = rgba(60, 30, 30, 255);
        float battement = sinf(t * 14) * 10;
        ellipse(g, x, y, 26 * s, 6 * s, rgba(0, 0, 0, (int)(60 * pose->alpha)));
        pTri(&p, -6, y0 - 16, -54, y0 - 46 + battement, -36, y0 + 4, rgba(100, 30, 40, 230));
        pTri(&p, -6, y0 - 16, -36, y0 + 4, -14, y0 + 8, rgba(70, 20, 30, 230));
        pLigne(&p, -14, y0 + 20, -36, y0 + 36, 3, f);
        pTri(&p, -34, y0 + 30, -46, y0 + 38, -34, y0 + 44, f);
        pLigne(&p, -6, y0 + 28, -8, y0 + 44, 4, f);
        pLigne(&p, 6, y0 + 28, 8, y0 + 44, 4, f);
        pEllipse(&p, 0, y0 + 10, 20, 24, c);
        pEllipse(&p, 4, y0 + 14, 11, 14, rgba(250, 150, 110, 255));
        pCercle(&p, 4, y0 - 22, 18, c);
        pTri(&p, -10, y0 - 32, -2, y0 - 37, -16, y0 - 56, corne);
        pTri(&p, 10, y0 - 37, 18, y0 - 32, 22, y0 - 56, corne);
        pHalo(&p, 12, y0 - 24, 8, rgba(255, 220, 80, 160));
        pCercle(&p, 12, y0 - 24, 3.5f, rgba(255, 230, 120, 255));
        pArc(&p, 10, y0 - 14, 6, 2, 0.2f, PI_F - 0.2f, rgba(70, 20, 20, 255));
        float hx = 24 + atk * 16, hy = y0 + 6 - atk * 10;
        pLigne(&p, hx - 10, hy + 30, hx + 14, hy - 40, 3, rgba(60, 50, 50, 255));
        pLigne(&p, hx + 6, hy - 38, hx + 22, hy - 42, 3, rgba(60, 50, 50, 255));
        pLigne(&p, hx + 6, hy - 38, hx + 4, hy - 54, 2.5f, rgba(60, 50, 50, 255));
        pLigne(&p, hx + 14, hy - 40, hx + 14, hy - 56, 2.5f, rgba(60, 50, 50, 255));
        pLigne(&p, hx + 22, hy - 42, hx + 24, hy - 58, 2.5f, rgba(60, 50, 50, 255));
        pCercle(&p, hx - 2, hy, 4, c);
        if (atk > 0.05f) {
            pHalo(&p, hx + 14, hy - 56, 10 + 20 * atk, rgba(255, 150, 40, 200));
        }
        break;
    }
    case MONSTRE_GARGOUILLE: {
        float b = sinf(t * 2) * 2;
        SDL_Color c = rgba(112, 122, 132, 255);
        SDL_Color f = assombrir(c, 0.7f);
        SDL_Color rune = rgba(150, 200, 255, 255);
        float aile = sinf(t * 3) * 8;
        ellipse(g, x, y, 50 * s, 10 * s, rgba(0, 0, 0, (int)(90 * pose->alpha)));
        pTri(&p, -10, -110 + b, -86, -176 + aile, -74, -78 + b, f);
        pTri(&p, -10, -110 + b, -74, -78 + b, -30, -60 + b, assombrir(f, 0.85f));
        pTri(&p, -4, -116 + b, -56, -190 + aile, -40, -100 + b, assombrir(f, 0.9f));
        pEllipse(&p, -18, -26, 16, 24, f);
        pEllipse(&p, 20, -26, 16, 24, f);
        pEllipse(&p, -12, -5, 15, 5, f);
        pEllipse(&p, 28, -5, 15, 5, f);
        pEllipse(&p, 0, -74 + b, 34, 40, c);
        pLigne(&p, -10, -94 + b, 4, -74 + b, 2, avecAlpha(rune, 200));
        pLigne(&p, 4, -74 + b, -6, -52 + b, 2, avecAlpha(rune, 200));
        float hx = 42 + atk * 22, hy = -50 + b - atk * 26;
        pLigne(&p, 20, -92 + b, hx, hy, 10, c);
        pTri(&p, hx - 4, hy, hx + 4, hy, hx + 8, hy + 14, f);
        pTri(&p, hx + 4, hy - 2, hx + 10, hy + 2, hx + 16, hy + 12, f);
        pCercle(&p, 16, -124 + b, 22, c);
        pTri(&p, 0, -138 + b, 10, -142 + b, -12, -168 + b, f);
        pTri(&p, 22, -142 + b, 30, -138 + b, 36, -170 + b, f);
        pEllipse(&p, 34, -116 + b, 13, 9, f);
        pHalo(&p, 22, -128 + b, 10, avecAlpha(rune, 170));
        pCercle(&p, 22, -128 + b, 3, rune);
        pTri(&p, 30, -110 + b, 34, -110 + b, 32, -104 + b, rgba(230, 230, 230, 255));
        if (atk > 0.1f) {
            pLigne(&p, hx + 4, hy + 12, hx + 30, hy + 40, 3, avecAlpha(rune, (int)(255 * atk)));
            pLigne(&p, hx + 30, hy + 40, hx + 22, hy + 60, 3, avecAlpha(rune, (int)(255 * atk)));
        }
        break;
    }
    case MONSTRE_LICHE: {
        float b = sinf(t * 1.6f) * 6;
        SDL_Color robe = rgba(52, 32, 84, 255);
        SDL_Color robe2 = rgba(96, 64, 150, 255);
        SDL_Color os = rgba(232, 226, 206, 255);
        SDL_Color ame = rgba(120, 255, 170, 255);
        SDL_Color or = rgba(220, 180, 70, 255);
        halo(g, x, y - 110 * s, 120 * s, rgba(120, 255, 170, (int)(35 * pose->alpha)));
        ellipse(g, x, y, 40 * s, 8 * s, rgba(0, 0, 0, (int)(60 * pose->alpha)));
        pLigne(&p, -20, -140 + b, -42, -96 + b, 9, robe);
        pCercle(&p, -42, -94 + b, 5, os);
        pQuad(&p, -26, -150 + b, 26, -150 + b, 46, -22 + b, -46, -22 + b, robe);
        for (int k = 0; k < 5; k++) {
            float x0 = -46 + k * 18.4f;
            pTri(&p, x0, -22 + b, x0 + 18.4f, -22 + b, x0 + 9, -4 + b + sinf(t * 3 + k) * 4, robe);
        }
        pLigne(&p, 0, -148 + b, 0, -26 + b, 4, robe2);
        pEllipse(&p, 0, -150 + b, 30, 10, robe2);
        pCercle(&p, 4, -174 + b, 20, os);
        pRect(&p, -6, -162 + b, 20, 10, os);
        for (int k = 0; k < 4; k++) pRect(&p, -4 + k * 5, -160 + b, 2, 5, rgba(60, 50, 50, 255));
        pCercle(&p, -3, -176 + b, 5, rgba(20, 10, 30, 255));
        pCercle(&p, 12, -176 + b, 5, rgba(20, 10, 30, 255));
        pHalo(&p, 12, -176 + b, 10, avecAlpha(ame, 200));
        pCercle(&p, 12, -176 + b, 2, ame);
        pCercle(&p, -3, -176 + b, 2, ame);
        pRect(&p, -14, -200 + b, 36, 8, or);
        for (int k = 0; k < 4; k++) {
            float bx = -14 + k * 12;
            pTri(&p, bx, -200 + b, bx + 6, -200 + b, bx + 3, -216 + b, or);
        }
        pCercle(&p, 4, -196 + b, 3, ame);
        float hx = 36 + atk * 12, hy = -118 + b - atk * 22;
        pLigne(&p, 16, -140 + b, hx, hy, 9, robe);
        pLigne(&p, hx - 4, hy + 64, hx + 6, hy - 80, 4, rgba(70, 60, 50, 255));
        pCercle(&p, hx, hy, 5, os);
        pHalo(&p, hx + 6, hy - 88, 18 + atk * 24, avecAlpha(ame, 140));
        pCercle(&p, hx + 6, hy - 88, 8, ame);
        pCercle(&p, hx + 4, hy - 90, 3, rgba(255, 255, 255, 220));
        break;
    }
    case MONSTRE_CANARD_NOIR: {
        const float e = ECHELLE_CANARD_NOIR;
        float pulse = 0.5f + 0.5f * sinf(t * 2.5f);
        halo(g, x, y - 130 * s * e * 0.7f, 150 * s * e * 0.7f,
             rgba(150, 60, 220, (int)((40 + 30 * pulse) * pose->alpha)));
        /* éclats de Néant qui tournent autour de lui */
        for (int k = 0; k < 6; k++) {
            float ang = t * 0.9f + k * PI_F / 3;
            float ex = cosf(ang) * 90, ey = -120 + sinf(ang) * 26;
            pTri(&p, ex, ey - 9, ex + 5, ey, ex - 5, ey, rgba(190, 120, 255, 200));
            pTri(&p, ex - 5, ey, ex + 5, ey, ex, ey + 9, rgba(120, 60, 200, 200));
        }
        Pose heros = *pose;
        heros.sens = pose->sens;
        dessinerHeros(g, canardNoir(), x, y, s * e, &heros);
        break;
    }
    case MONSTRE_SQUELETTE: {
        SDL_Color os = rgba(232, 226, 206, 255);
        SDL_Color f = rgba(180, 172, 150, 255);
        float b = sinf(t * 3) * 2;
        ellipse(g, x, y, 32 * s, 8 * s, rgba(0, 0, 0, (int)(80 * pose->alpha)));
        pLigne(&p, -8, -4, -6, -60, 5, f);
        pLigne(&p, 8, -4, 6, -60, 5, os);
        pLigne(&p, 0, -62, 0, -118 + b, 6, os);
        for (int k = 0; k < 4; k++) {
            pArc(&p, 0, -88 + b + k * 8, 16 - k * 1.5f, 3, PI_F * 0.15f, PI_F * 0.85f, os);
        }
        pEllipse(&p, 0, -60, 16, 5, f);
        /* bras et épée rouillée */
        pLigne(&p, -14, -110 + b, -22, -72 + b, 4, f);
        float hx = 22 + atk * 10, hy = -76 + b - atk * 30;
        pLigne(&p, 14, -110 + b, hx, hy, 4, os);
        pLigne(&p, hx, hy, hx + 8 + atk * 30, hy - 52 + atk * 40, 6, rgba(150, 120, 100, 255));
        pCercle(&p, 0, -132 + b, 21, os);
        pRect(&p, -10, -118 + b, 20, 10, os);
        pCercle(&p, 0, -133 + b, 5, rgba(40, 20, 30, 255));
        pCercle(&p, 12, -133 + b, 5, rgba(40, 20, 30, 255));
        pHalo(&p, 12, -133 + b, 9, rgba(255, 70, 60, 200));
        pCercle(&p, 12, -133 + b, 2, rgba(255, 90, 70, 255));
        for (int k = 0; k < 4; k++) pRect(&p, -8 + k * 5, -114 + b, 2, 5, rgba(60, 50, 50, 255));
        break;
    }
    case MONSTRE_LOUP: {
        SDL_Color c = rgba(80, 76, 110, 255);
        SDL_Color f = assombrir(c, 0.7f);
        float b = sinf(t * 5) * 2;
        float lunge = atk * 18;
        ellipse(g, x, y, 50 * s, 9 * s, rgba(0, 0, 0, (int)(80 * pose->alpha)));
        pTri(&p, -40, -48 + b, -70, -70 + sinf(t * 6) * 6, -44, -38 + b, f);
        pLigne(&p, -28, -36, -32, -2, 8, f);
        pLigne(&p, 22, -36, 26 + lunge * 0.3f, -2, 8, f);
        pEllipse(&p, 0, -46 + b, 46, 22, c);
        pLigne(&p, -20, -36, -16, -2, 8, c);
        pLigne(&p, 30, -36, 36 + lunge * 0.3f, -2, 8, c);
        float hx = 46 + lunge, hy = -60 + b;
        pCercle(&p, hx, hy, 19, c);
        pEllipse(&p, hx + 18, hy + 6, 14, 8, f);
        pTri(&p, hx - 12, hy - 12, hx - 2, hy - 16, hx - 10, hy - 34, f);
        pTri(&p, hx, hy - 16, hx + 10, hy - 12, hx + 4, hy - 34, c);
        pHalo(&p, hx + 6, hy - 3, 9, rgba(255, 210, 80, 180));
        pCercle(&p, hx + 6, hy - 3, 3, yeux);
        pCercle(&p, hx + 31, hy + 4, 3, rgba(30, 20, 30, 255));
        if (atk > 0.1f) pTri(&p, hx + 14, hy + 12, hx + 20, hy + 12, hx + 17, hy + 20, rgba(255, 255, 255, 255));
        break;
    }
    case MONSTRE_GOLEM: {
        SDL_Color c = rgba(128, 116, 104, 255);
        SDL_Color f = assombrir(c, 0.72f);
        SDL_Color rune = rgba(90, 230, 250, 255);
        float b = sinf(t * 1.5f) * 2;
        ellipse(g, x, y, 60 * s, 12 * s, rgba(0, 0, 0, (int)(90 * pose->alpha)));
        pRect(&p, -34, -50, 24, 50, f);
        pRect(&p, 10, -50, 24, 50, f);
        float corps[12] = { -46, -150 + b, 46, -150 + b, 56, -110 + b, 44, -52, -44, -52, -56, -110 + b };
        pPoly(&p, corps, 6, c);
        pCercle(&p, -62, -120 + b, 22, f);
        pCercle(&p, -66, -76 + b, 20, f);
        float bx = 62 + atk * 18, by = -120 + b + atk * 30;
        pCercle(&p, bx, by, 24, c);
        pCercle(&p, bx + 4, by + 44, 22, c);
        pRect(&p, -22, -186 + b, 44, 38, c);
        pRect(&p, -22, -160 + b, 44, 8, f);
        pHalo(&p, 8, -172 + b, 16, avecAlpha(rune, 160));
        pRect(&p, -4, -176 + b, 20, 6, rune);
        pLigne(&p, -20, -130 + b, 0, -100 + b, 3, rune);
        pLigne(&p, 0, -100 + b, 20, -126 + b, 3, rune);
        pCercle(&p, 0, -100 + b, 5, rune);
        break;
    }
    case MONSTRE_SPECTRE: {
        float b = sinf(t * 2) * 8;
        SDL_Color c = rgba(200, 190, 255, 175);
        halo(g, x, y - 90 * s, 90 * s, rgba(150, 120, 255, (int)(60 * pose->alpha)));
        ellipse(g, x, y, 30 * s, 7 * s, rgba(0, 0, 0, (int)(50 * pose->alpha)));
        float corps[16];
        for (int k = 0; k < 8; k++) {
            float ang = PI_F + PI_F * k / 7;
            corps[k * 2] = cosf(ang) * 38;
            corps[k * 2 + 1] = -110 + b + sinf(ang) * 42;
        }
        pPoly(&p, corps, 8, c);
        pQuad(&p, -38, -110 + b, 38, -110 + b, 30, -40 + b, -30, -40 + b, c);
        for (int k = 0; k < 4; k++) {
            float x0 = -30 + k * 15;
            pTri(&p, x0, -42 + b, x0 + 15, -42 + b, x0 + 7 + sinf(t * 4 + k) * 4, -20 + b, c);
        }
        pLigne(&p, 30, -100 + b, 54 + atk * 20, -80 + b - atk * 10, 9, c);
        pEllipse(&p, -2, -122 + b, 7, 11, rgba(30, 20, 60, 255));
        pEllipse(&p, 18, -122 + b, 7, 11, rgba(30, 20, 60, 255));
        pEllipse(&p, 8, -96 + b, 6, 8 + atk * 4, rgba(30, 20, 60, 255));
        break;
    }
    case MONSTRE_DRAGON: {
        SDL_Color c = rgba(190, 56, 60, 255);
        SDL_Color f = assombrir(c, 0.65f);
        SDL_Color ventre = rgba(240, 190, 120, 255);
        float b = sinf(t * 1.8f) * 3;
        float aile = sinf(t * 2.5f) * 14;
        ellipse(g, x, y, 90 * s, 16 * s, rgba(0, 0, 0, (int)(90 * pose->alpha)));
        /* aile arrière */
        pTri(&p, -10, -130 + b, -120, -230 + aile, -110, -120 + aile * 0.5f, f);
        pTri(&p, -10, -130 + b, -110, -120 + aile * 0.5f, -60, -90 + b, assombrir(f, 0.85f));
        /* queue */
        pQuad(&p, -50, -60, -40, -90, -110, -40, -130, -30, f);
        pTri(&p, -110, -40, -130, -30, -150, -50, rgba(240, 200, 120, 255));
        /* pattes */
        pRect(&p, -38, -50, 22, 50, f);
        pRect(&p, 20, -50, 22, 50, f);
        pEllipse(&p, 0, -86 + b, 70, 48, c);
        pEllipse(&p, 14, -72 + b, 40, 30, ventre);
        /* cou et tête */
        float lunge = atk * 30;
        float hx = 74 + lunge, hy = -190 + b + atk * 30;
        pQuad(&p, 30, -120 + b, 60, -110 + b, hx + 10, hy + 20, hx - 16, hy + 10, c);
        pEllipse(&p, hx, hy, 34, 24, c);
        pEllipse(&p, hx + 30, hy + 8, 22, 13, c);
        pTri(&p, hx - 22, hy - 14, hx - 8, hy - 20, hx - 44, hy - 50, rgba(240, 220, 190, 255));
        pTri(&p, hx - 4, hy - 20, hx + 8, hy - 22, hx - 16, hy - 56, rgba(240, 220, 190, 255));
        pHalo(&p, hx + 8, hy - 4, 14, rgba(255, 200, 60, 200));
        pEllipse(&p, hx + 8, hy - 4, 5, 3.5f, yeux);
        pCercle(&p, hx + 46, hy + 4, 2.5f, rgba(40, 10, 10, 255));
        if (atk > 0.1f) { /* flammes */
            pHalo(&p, hx + 80, hy + 18, 50 * atk, rgba(255, 150, 40, 200));
            pCercle(&p, hx + 64, hy + 14, 14 * atk, rgba(255, 220, 90, 230));
        }
        /* aile avant */
        pTri(&p, 10, -130 + b, 60, -250 + aile, 110, -150 + aile * 0.6f, assombrir(c, 0.85f));
        pTri(&p, 10, -130 + b, 110, -150 + aile * 0.6f, 50, -110 + b, f);
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Portraits                                                         */
/* ------------------------------------------------------------------ */

/** Fond et cadre d'un portrait carré ; active le découpage. */
static void debutPortrait(Gui *g, float cx, float cy, float r, SDL_Color fond)
{
    rectArrondi(g, cx - r, cy - r, 2 * r, 2 * r, r * 0.3f, assombrir(fond, 0.35f));
    halo(g, cx, cy + r * 0.3f, r * 1.1f, avecAlpha(fond, 150));
    SDL_Rect clip = { (int)(cx - r + 2), (int)(cy - r + 2), (int)(2 * r - 4), (int)(2 * r - 4) };
    SDL_RenderSetClipRect(g->rendu, &clip);
}

static void finPortrait(Gui *g, float cx, float cy, float r, SDL_Color bord)
{
    SDL_RenderSetClipRect(g->rendu, NULL);
    contourArrondi(g, cx - r, cy - r, 2 * r, 2 * r, r * 0.3f, 2, bord);
}

void portraitHeros(Gui *g, const Personnage *p, float cx, float cy, float r, float temps)
{
    SDL_Color fond = couleurElement(p->element);
    debutPortrait(g, cx, cy, r, fond);
    float s = r / 40.0f;
    Pose pose = poseDefaut(temps);
    dessinerHeros(g, p, cx - 6 * s, cy + 106 * s, s, &pose);
    finPortrait(g, cx, cy, r, avecAlpha(fond, 200));
}

void portraitMonstre(Gui *g, enum Monstre m, float cx, float cy, float r, float temps)
{
    /* point à cadrer (tête) et zoom, pour chaque monstre */
    static const float teteY[NB_MONSTRES] = {
        [MONSTRE_GELEE] = 38, [MONSTRE_SQUELETTE] = 130, [MONSTRE_LOUP] = 60, [MONSTRE_GOLEM] = 165,
        [MONSTRE_SPECTRE] = 110, [MONSTRE_DRAGON] = 190, [MONSTRE_CHAMPIGNON] = 56,
        [MONSTRE_DIABLOTIN] = 90, [MONSTRE_GARGOUILLE] = 128, [MONSTRE_ROI_GELEE] = 92,
        [MONSTRE_LICHE] = 178, [MONSTRE_CANARD_NOIR] = 0,
    };
    static const float teteX[NB_MONSTRES] = {
        [MONSTRE_GELEE] = -12, [MONSTRE_SQUELETTE] = -6, [MONSTRE_LOUP] = -46, [MONSTRE_GOLEM] = 0,
        [MONSTRE_SPECTRE] = -8, [MONSTRE_DRAGON] = -78, [MONSTRE_CHAMPIGNON] = -2,
        [MONSTRE_DIABLOTIN] = -6, [MONSTRE_GARGOUILLE] = -14, [MONSTRE_ROI_GELEE] = -10,
        [MONSTRE_LICHE] = -6, [MONSTRE_CANARD_NOIR] = 0,
    };
    static const float echelle[NB_MONSTRES] = {
        [MONSTRE_GELEE] = 1.0f, [MONSTRE_SQUELETTE] = 1.3f, [MONSTRE_LOUP] = 1.2f, [MONSTRE_GOLEM] = 1.0f,
        [MONSTRE_SPECTRE] = 1.0f, [MONSTRE_DRAGON] = 0.8f, [MONSTRE_CHAMPIGNON] = 0.8f,
        [MONSTRE_DIABLOTIN] = 1.1f, [MONSTRE_GARGOUILLE] = 1.0f, [MONSTRE_ROI_GELEE] = 0.5f,
        [MONSTRE_LICHE] = 1.1f, [MONSTRE_CANARD_NOIR] = 1.0f,
    };
    if (m < 0 || m >= NB_MONSTRES) {
        return;
    }
    if (m == MONSTRE_CANARD_NOIR) { /* cadré comme un portrait de héros */
        debutPortrait(g, cx, cy, r, rgba(150, 60, 220, 255));
        float s = r / 40.0f;
        Pose pose = poseDefaut(temps);
        pose.sens = -1;
        dessinerHeros(g, canardNoir(), cx + 6 * s, cy + 106 * s, s, &pose);
        finPortrait(g, cx, cy, r, rgba(190, 120, 255, 220));
        return;
    }
    debutPortrait(g, cx, cy, r, rgba(200, 70, 80, 255));
    float s = r / 40.0f * echelle[m];
    Pose pose = poseDefaut(temps);
    pose.sens = -1;
    dessinerMonstre(g, m, cx - teteX[m] * s, cy + teteY[m] * s, s, &pose);
    finPortrait(g, cx, cy, r, rgba(230, 110, 120, 200));
}
