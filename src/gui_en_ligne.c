/**
 * @file gui_en_ligne.c
 * @brief Écran du duel en ligne : composer son équipe, puis héberger la
 *        partie (on choisit le format et on affiche son adresse IP) ou la
 *        rejoindre (on tape celle de l'hôte).
 *
 * Une fois reliés : l'hôte envoie le format (héros par équipe, niveau) et
 * son équipe ; l'invité répond avec la sienne, construite selon ce format.
 * Le duel commence alors sur les deux machines.
 */

/* [DUCKGEON - SDL] Fichier de la version graphique (./duckgeon) uniquement. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui_app.h"
#include "gui_sprite.h"

void ouvrirEnLigne(App *app)
{
    EnLigne *el = &app->enLigne;
    reseauFermer(&el->net);
    el->bonjourEnvoye = el->bonjourRecu = 0;
    el->saisieAdresse = 0;
    el->manche = 0;
    if (el->format.taille < 1 || el->format.taille > COMBAT_MAX_ALLIES) el->format.taille = 1;

    /* on garde l'équipe de la dernière fois si les héros existent encore */
    int n = 0;
    for (int k = 0; k < el->nbChoisis; k++) {
        if (el->choisis[k] < app->donjon.taille) el->choisis[n++] = el->choisis[k];
    }
    el->nbChoisis = n;
    if (el->nbChoisis == 0 && app->donjon.taille > 0) {
        el->choisis[0] = 0;
        el->nbChoisis = 1;
    }
    if (reseauAdressesLocales(el->ips, sizeof el->ips) == 0) {
        snprintf(el->ips, sizeof el->ips, "aucune (pas de réseau ?)");
    }
    changerEcran(app, ECRAN_EN_LIGNE);
}

/** Quitte l'écran (et la connexion éventuelle). */
static void quitter(App *app)
{
    reseauEnvoyerQuitter(&app->enLigne.net);
    reseauFermer(&app->enLigne.net);
    app->enLigne.saisieAdresse = 0;
    SDL_StopTextInput();
    changerEcran(app, ECRAN_MENU);
}

/** Position de `i` dans notre équipe, -1 s'il n'y est pas. */
static int positionChoix(const EnLigne *el, int i)
{
    for (int k = 0; k < el->nbChoisis; k++) {
        if (el->choisis[k] == i) return k;
    }
    return -1;
}

/** Ajoute un héros du donjon à l'équipe envoyée (copie au format du duel). */
static void ajouterAEnvoyer(App *app, int i)
{
    EnLigne *el = &app->enLigne;
    Personnage copie = app->donjon.elements[i];
    imposerNiveau(&copie, el->format.niveau); /* ne fait rien pour « niveaux actuels » */
    reseauPreparerHeros(&copie, &el->moi[el->nbMoi++]);
}

/** Équipe envoyée : nos héros choisis d'abord, complétée par les suivants
 *  du donjon s'il en manque pour atteindre la taille fixée par l'hôte. */
static void preparerEquipe(App *app)
{
    EnLigne *el = &app->enLigne;
    int taille = el->format.taille;
    el->nbMoi = 0;
    for (int k = 0; k < el->nbChoisis && el->nbMoi < taille; k++) {
        ajouterAEnvoyer(app, el->choisis[k]);
    }
    for (int i = 0; i < app->donjon.taille && el->nbMoi < taille; i++) {
        if (positionChoix(el, i) < 0) ajouterAEnvoyer(app, i);
    }
}

/** Échange des équipes, puis lancement du duel quand les deux sont arrivées. */
static void echangerEquipes(App *app)
{
    EnLigne *el = &app->enLigne;
    Reseau *r = &el->net;

    const Message *m;
    while ((m = reseauPremier(r)) != NULL && m->type != MSG_ACTION) {
        if (m->type == MSG_BONJOUR) {
            if (!r->hote) {
                el->format = m->format; /* l'hôte décide du format */
            }
            el->nbAdversaire = m->nbHeros;
            memcpy(el->adversaire, m->heros, sizeof m->heros[0] * (size_t)m->nbHeros);
            el->bonjourRecu = 1;
        } else if (m->type == MSG_QUITTER) {
            reseauFermer(r);
            r->etat = RESEAU_ERREUR;
            snprintf(r->erreur, sizeof r->erreur, "L'autre joueur a annulé.");
            el->bonjourEnvoye = el->bonjourRecu = 0;
            return;
        }
        reseauRetirer(r);
    }

    /* L'hôte parle en premier ; l'invité répond une fois le format connu. */
    if (r->etat == RESEAU_CONNECTE && !el->bonjourEnvoye && (r->hote || el->bonjourRecu)) {
        if (r->hote) {
            el->format.graine = (unsigned)rand() ^ (SDL_GetTicks() << 16); /* l'hôte choisit le hasard */
        }
        preparerEquipe(app);
        el->bonjourEnvoye = reseauEnvoyerBonjour(r, el->moi, el->nbMoi, &el->format);
    }

    if (el->bonjourEnvoye && el->bonjourRecu && app->prochain == ECRAN_EN_LIGNE) {
        el->manche = 0;
        combatGuiDetruire(app->combat);
        app->combat = combatGuiCreerEnLigne(app);
        if (app->combat != NULL) {
            changerEcran(app, ECRAN_COMBAT);
        } else {
            reseauFermer(r);
        }
    }
}

/** Panneau « Votre équipe » : emplacements + liste des héros du donjon. */
static void panneauEquipe(App *app, int modifiable)
{
    Gui *g = &app->g;
    EnLigne *el = &app->enLigne;
    Donjon *d = &app->donjon;
    float t = g->temps / 1000.0f;
    float x = 40, y = 110, w = 520, h = 590;
    panneau(g, x, y, w, h);

    /* l'hôte fixe la taille ; avant la connexion, l'invité peut en préparer 4 */
    int max = (el->net.hote || el->bonjourRecu) ? el->format.taille : COMBAT_MAX_ALLIES;
    if (el->nbChoisis > max) el->nbChoisis = max;
    char titre[48];
    snprintf(titre, sizeof titre, "Votre équipe  (%d/%d)", el->nbChoisis, max);
    texte(g, POLICE_MOYENNE, titre, x + 24, y + 14, COUL_OR, ALIGN_GAUCHE);

    /* emplacements (clic : retirer) */
    float gap = 10, sw = (w - 48 - gap * 3) / 4, sh = 150, sy = y + 52;
    for (int k = 0; k < max; k++) {
        float sx = x + 24 + k * (sw + gap);
        rectArrondi(g, sx, sy, sw, sh, 10, rgba(24, 20, 46, 230));
        contourArrondi(g, sx, sy, sw, sh, 10, 1.2f, COUL_BORDURE);
        if (k >= el->nbChoisis) {
            texte(g, POLICE_PETITE, "Libre", sx + sw / 2, sy + sh / 2 - 8, COUL_TEXTE_DOUX, ALIGN_CENTRE);
            continue;
        }
        const Personnage *p = &d->elements[el->choisis[k]];
        Pose pose = poseDefaut(t + k);
        halo(g, sx + sw / 2, sy + 70, 60, avecAlpha(couleurElement(p->element), 45));
        dessinerHeros(g, p, sx + sw / 2, sy + sh - 34, 0.55f, &pose);
        texte(g, POLICE_PETITE, p->nom, sx + sw / 2, sy + sh - 28, COUL_TEXTE, ALIGN_CENTRE);
        char niv[24];
        if (el->format.niveau > 0 && (el->net.hote || el->bonjourRecu)) {
            snprintf(niv, sizeof niv, "Niv %d (imposé)", el->format.niveau);
        } else {
            snprintf(niv, sizeof niv, "Niv %d", p->niveau);
        }
        texte(g, POLICE_PETITE, niv, sx + sw / 2, sy + sh - 14, couleurClasse(p->classe), ALIGN_CENTRE);
        if (modifiable && zoneClic(g, sx, sy, sw, sh)) {
            for (int j = k; j < el->nbChoisis - 1; j++) el->choisis[j] = el->choisis[j + 1];
            el->nbChoisis--;
        }
    }

    const char *aide = !modifiable ? "Équipe envoyée à l'adversaire."
                       : el->net.hote ? "Cliquez sur un héros pour l'ajouter ou le retirer."
                       : "L'hôte fixe le nombre de héros : vos premiers sont pris, complétés si besoin.";
    texte(g, POLICE_PETITE, aide, x + 24, sy + sh + 10, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

    /* liste défilante des héros du donjon */
    const float lx = x + 24, ly = sy + sh + 36, lw = w - 48, lh = y + h - 20 - ly;
    const float ch = 84, cgap = 8;
    int maxDefil = (int)(d->taille * (ch + cgap) - cgap - lh);
    if (maxDefil < 0) maxDefil = 0;
    if (survol(g, lx, ly, lw, lh)) el->defilement -= g->molette * 40;
    if (el->defilement < 0) el->defilement = 0;
    if (el->defilement > maxDefil) el->defilement = maxDefil;
    int sourisDansListe = survol(g, lx, ly, lw, lh);

    SDL_Rect clip = { (int)lx - 4, (int)ly - 4, (int)lw + 8, (int)lh + 8 };
    SDL_RenderSetClipRect(g->rendu, &clip);
    for (int i = 0; i < d->taille; i++) {
        float cy = ly + i * (ch + cgap) - el->defilement;
        if (cy + ch < ly || cy > ly + lh) continue;
        int pos = positionChoix(el, i);
        if (carteHeros(g, &d->elements[i], lx, cy, lw - 10, ch, pos >= 0, modifiable && sourisDansListe)) {
            if (pos >= 0) {
                for (int j = pos; j < el->nbChoisis - 1; j++) el->choisis[j] = el->choisis[j + 1];
                el->nbChoisis--;
            } else if (el->nbChoisis < max) {
                el->choisis[el->nbChoisis++] = i;
            }
        }
    }
    SDL_RenderSetClipRect(g->rendu, NULL);
    if (maxDefil > 0) {
        float hb = lh * lh / (lh + maxDefil);
        rectArrondi(g, lx + lw - 4, ly + (lh - hb) * el->defilement / maxDefil, 5, hb, 2.5f, rgba(200, 190, 255, 90));
    }
}

/** Champ de saisie de l'adresse IP ; retourne 1 si Entrée a été pressée. */
static int champAdresse(App *app, float x, float y, float w, float h)
{
    Gui *g = &app->g;
    EnLigne *el = &app->enLigne;
    if (g->clic) {
        int avant = el->saisieAdresse;
        el->saisieAdresse = survol(g, x, y, w, h);
        if (el->saisieAdresse && !avant) SDL_StartTextInput();
        if (!el->saisieAdresse && avant) SDL_StopTextInput();
    }
    if (survol(g, x, y, w, h)) g->survolCliquable = 1;
    rectArrondi(g, x, y, w, h, 10, rgba(12, 10, 28, 240));
    contourArrondi(g, x, y, w, h, 10, el->saisieAdresse ? 2 : 1.2f, el->saisieAdresse ? COUL_ACCENT : COUL_BORDURE);

    if (el->saisieAdresse) {
        /* chiffres, points, deux-points (port) et lettres (nom de machine) */
        for (const char *c = g->saisie; *c != '\0'; c++) {
            size_t n = strlen(el->adresse);
            int ok = (*c >= '0' && *c <= '9') || *c == '.' || *c == ':' || *c == '-'
                     || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z');
            if (ok && n + 1 < sizeof el->adresse) {
                el->adresse[n] = *c;
                el->adresse[n + 1] = '\0';
            }
        }
        size_t n = strlen(el->adresse);
        if (guiTouche(g, SDLK_BACKSPACE) && n > 0) el->adresse[n - 1] = '\0';
    }
    int lg = 0;
    if (el->adresse[0] != '\0') {
        lg = texte(g, POLICE_MOYENNE, el->adresse, x + 14, y + 9, COUL_TEXTE, ALIGN_GAUCHE);
    } else if (!el->saisieAdresse) {
        texte(g, POLICE_NORMALE, "ex. 192.168.1.20", x + 14, y + 12, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    }
    if (el->saisieAdresse && (g->temps / 500) % 2 == 0) {
        remplirRect(g, x + 16 + lg, y + 9, 2, 24, COUL_ACCENT);
    }
    return el->saisieAdresse && (guiTouche(g, SDLK_RETURN) || guiTouche(g, SDLK_KP_ENTER));
}

/** Points de suspension animés. */
static const char *points(const Gui *g)
{
    static const char *const P[3] = { ".", "..", "..." };
    return P[(g->temps / 400) % 3];
}

/** « 2 contre 2  ·  Tous niveau 5 » */
static void libelleFormat(const FormatDuel *f, char *buf, int taille)
{
    char niv[32];
    libelleNiveauDuel(f->niveau, niv, sizeof niv);
    snprintf(buf, (size_t)taille, "%d contre %d  ·  %s", f->taille, f->taille, niv);
}

/** Cartes « Héberger » (avec le format) et « Rejoindre ». */
static void cartesConnexion(App *app, float x, float y, float w)
{
    Gui *g = &app->g;
    EnLigne *el = &app->enLigne;
    Reseau *r = &el->net;

    /* héberger : l'hôte choisit le format */
    panneau(g, x, y, w, 270);
    texte(g, POLICE_MOYENNE, "Héberger la partie", x + 24, y + 16, COUL_OR, ALIGN_GAUCHE);
    texte(g, POLICE_PETITE, "Votre IP s'affichera : donnez-la à l'autre joueur. Vous serez le joueur 1.", x + 24,
          y + 50, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

    char buf[48];
    texte(g, POLICE_NORMALE, "Héros par équipe", x + 24, y + 90, COUL_TEXTE, ALIGN_GAUCHE);
    snprintf(buf, sizeof buf, "%d contre %d", el->format.taille, el->format.taille);
    el->format.taille += selecteur(g, x + 250, y + 84, 260, buf);
    if (el->format.taille < 1) el->format.taille = 1;
    if (el->format.taille > COMBAT_MAX_ALLIES) el->format.taille = COMBAT_MAX_ALLIES;

    texte(g, POLICE_NORMALE, "Niveau des héros", x + 24, y + 140, COUL_TEXTE, ALIGN_GAUCHE);
    libelleNiveauDuel(el->format.niveau, buf, sizeof buf);
    el->format.niveau += selecteur(g, x + 250, y + 134, 260, buf);
    if (el->format.niveau < 0) el->format.niveau = NIVEAU_DUEL_MAX;
    if (el->format.niveau > NIVEAU_DUEL_MAX) el->format.niveau = 0;
    texte(g, POLICE_PETITE, el->format.niveau > 0 ? "Stats de la classe à ce niveau, sans arme ni points bonus : duel équilibré."
                                                  : "Chacun joue ses héros tels qu'ils sont (niveau, points, arme).",
          x + 24, y + 176, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

    if (bouton(g, x + 24, y + 206, 240, 46, "Héberger", BOUTON_PRINCIPAL, 1)) {
        el->saisieAdresse = 0;
        SDL_StopTextInput();
        reseauHeberger(r);
    }

    /* rejoindre */
    float y2 = y + 290;
    panneau(g, x, y2, w, 190);
    texte(g, POLICE_MOYENNE, "Rejoindre une partie", x + 24, y2 + 16, COUL_OR, ALIGN_GAUCHE);
    texte(g, POLICE_NORMALE, "Adresse IP de l'hôte :", x + 24, y2 + 54, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
    int entree = champAdresse(app, x + 24, y2 + 82, 360, 42);
    if (bouton(g, x + 400, y2 + 78, w - 424, 50, "Se connecter", BOUTON_PRINCIPAL, el->adresse[0] != '\0')
        || (entree && el->adresse[0] != '\0')) {
        el->saisieAdresse = 0;
        SDL_StopTextInput();
        reseauRejoindre(r, el->adresse);
    }
    texte(g, POLICE_PETITE, "Le nombre de héros et le niveau sont choisis par l'hôte.  Port autre que 5050 : « IP:port ».",
          x + 24, y2 + 144, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

    if (r->etat == RESEAU_ERREUR) {
        texte(g, POLICE_NORMALE, r->erreur, x + w / 2, y2 + 206, COUL_DANGER, ALIGN_CENTRE);
    }
}

void ecranEnLigne(App *app)
{
    Gui *g = &app->g;
    EnLigne *el = &app->enLigne;
    Reseau *r = &el->net;
    dessinerFond(g, rgba(14, 12, 40, 255), rgba(30, 20, 56, 255));

    reseauMaj(r);
    echangerEquipes(app);

    if (app->donjon.taille == 0) {
        changerEcran(app, ECRAN_MENU);
        return;
    }

    int libre = r->etat == RESEAU_INACTIF || r->etat == RESEAU_ERREUR;
    if (libre) r->hote = 0; /* avant de choisir, l'équipe peut aller jusqu'à 4 */
    panneauEquipe(app, !el->bonjourEnvoye);

    float x = 590, y = 110, w = 650;
    char buf[160];
    if (libre) {
        cartesConnexion(app, x, y, w);
    } else {
        panneau(g, x, y, w, 590);
        if (r->etat == RESEAU_ATTENTE) {
            snprintf(buf, sizeof buf, "En attente d'un adversaire%s", points(g));
            texteOmbre(g, POLICE_GRANDE, buf, x + 34, y + 34, COUL_OR_CLAIR, ALIGN_GAUCHE);
            libelleFormat(&el->format, buf, sizeof buf);
            texte(g, POLICE_MOYENNE, buf, x + 34, y + 96, COUL_ACCENT, ALIGN_GAUCHE);
            texte(g, POLICE_NORMALE, "Donnez-lui votre adresse IP :", x + 34, y + 150, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
            texteBloc(g, POLICE_GRANDE, el->ips, x + 34, y + 180, (int)w - 68, COUL_TEXTE);
            snprintf(buf, sizeof buf, "Port %d", RESEAU_PORT);
            texte(g, POLICE_MOYENNE, buf, x + 34, y + 290, COUL_ACCENT, ALIGN_GAUCHE);
            texteBloc(g, POLICE_PETITE,
                      "Même réseau (Wi-Fi) : l'adresse en 192.168... ou 10... suffit. À distance : installez "
                      "Tailscale tous les deux et donnez l'adresse en 100... (voir le README). Si la "
                      "connexion échoue, autorisez le port 5050 dans le pare-feu.",
                      x + 34, y + 336, (int)w - 68, COUL_TEXTE_DOUX);
        } else if (r->etat == RESEAU_CONNEXION) {
            snprintf(buf, sizeof buf, "Connexion à %s%s", el->adresse, points(g));
            texteOmbre(g, POLICE_GRANDE, buf, x + 34, y + 34, COUL_OR_CLAIR, ALIGN_GAUCHE);
        } else {
            texteOmbre(g, POLICE_GRANDE, "Connecté !", x + 34, y + 34, COUL_VIE, ALIGN_GAUCHE);
            if (el->bonjourRecu) {
                libelleFormat(&el->format, buf, sizeof buf);
                texte(g, POLICE_MOYENNE, buf, x + 34, y + 96, COUL_ACCENT, ALIGN_GAUCHE);
                texte(g, POLICE_NORMALE, "Équipe adverse :", x + 34, y + 146, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
                for (int k = 0; k < el->nbAdversaire; k++) {
                    const Personnage *p = &el->adversaire[k];
                    snprintf(buf, sizeof buf, "%s  ·  %s niveau %d", p->nom, libelleClasse(p->classe), p->niveau);
                    texte(g, POLICE_NORMALE, buf, x + 34, y + 176 + k * 28, COUL_TEXTE, ALIGN_GAUCHE);
                }
            } else {
                snprintf(buf, sizeof buf, "Échange des équipes%s", points(g));
                texte(g, POLICE_MOYENNE, buf, x + 34, y + 96, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
            }
        }
        if (r->etat != RESEAU_CONNECTE && bouton(g, x + w - 214, y + 520, 190, 46, "Annuler", BOUTON_NORMAL, 1)) {
            reseauFermer(r);
        }
    }

    if (enTete(app, "Duel en ligne", "Un joueur héberge, l'autre tape son adresse IP pour le rejoindre.")) {
        quitter(app);
    }
}
