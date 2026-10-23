/**
 * @file gui_en_ligne.c
 * @brief Écran du duel en ligne : choix du héros, puis héberger la partie
 *        (on affiche son adresse IP) ou la rejoindre (on tape celle de
 *        l'hôte). Une fois reliés, les deux joueurs s'envoient leur héros
 *        et le duel commence sur les deux machines.
 */
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
    if (el->heros < 0 || el->heros >= app->donjon.taille) el->heros = 0;
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

/** Échange des héros, puis lancement du duel quand les deux sont arrivés. */
static void echangerHeros(App *app)
{
    EnLigne *el = &app->enLigne;
    Reseau *r = &el->net;
    if (r->etat == RESEAU_CONNECTE && !el->bonjourEnvoye) {
        reseauPreparerHeros(&app->donjon.elements[el->heros], &el->moi);
        if (r->hote) {
            el->graine = (unsigned)rand() ^ (SDL_GetTicks() << 16); /* l'hôte choisit le hasard */
        }
        el->bonjourEnvoye = reseauEnvoyerBonjour(r, &el->moi, r->hote ? el->graine : 0);
    }

    const Message *m;
    while ((m = reseauPremier(r)) != NULL && m->type != MSG_ACTION) {
        if (m->type == MSG_BONJOUR) {
            el->adversaire = m->heros;
            if (!r->hote) el->graine = m->graine;
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

/** Aperçu du héros choisi (et sélecteur tant qu'on n'est pas connecté). */
static void panneauHeros(App *app, int modifiable)
{
    Gui *g = &app->g;
    EnLigne *el = &app->enLigne;
    float t = g->temps / 1000.0f;
    float x = 60, y = 120, w = 440, h = 540;
    panneau(g, x, y, w, h);
    texte(g, POLICE_MOYENNE, "Votre héros", x + 24, y + 18, COUL_OR, ALIGN_GAUCHE);

    Personnage *p = &app->donjon.elements[el->heros];
    float cx = x + w / 2;
    halo(g, cx, y + 300, 160, avecAlpha(couleurClasse(p->classe), 50));
    ellipse(g, cx, y + 330, 110, 18, rgba(40, 32, 70, 255));
    Pose pose = poseDefaut(t);
    dessinerHeros(g, p, cx, y + 330, 1.5f, &pose);

    if (modifiable) {
        int d = selecteur(g, cx - 160, y + 360, 320, p->nom);
        if (d != 0) {
            el->heros = (el->heros + d + app->donjon.taille) % app->donjon.taille;
            p = &app->donjon.elements[el->heros];
        }
    } else {
        texte(g, POLICE_MOYENNE, p->nom, cx, y + 366, COUL_TEXTE, ALIGN_CENTRE);
    }

    char buf[96];
    snprintf(buf, sizeof buf, "%s niveau %d  ·  %s", libelleClasse(p->classe), p->niveau, nomElement(p->element));
    texte(g, POLICE_NORMALE, buf, cx, y + 412, couleurClasse(p->classe), ALIGN_CENTRE);
    snprintf(buf, sizeof buf, "PV %d   ATK %d   DEF %d   VIT %d", p->pvMax, attaqueTotale(p), p->def, p->vit);
    texte(g, POLICE_NORMALE, buf, cx, y + 442, COUL_TEXTE, ALIGN_CENTRE);
    texte(g, POLICE_PETITE, "Le duel n'abîme pas le héros : il commence avec tous ses PV.", cx, y + 490,
          COUL_TEXTE_DOUX, ALIGN_CENTRE);
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

void ecranEnLigne(App *app)
{
    Gui *g = &app->g;
    EnLigne *el = &app->enLigne;
    Reseau *r = &el->net;
    dessinerFond(g, rgba(14, 12, 40, 255), rgba(30, 20, 56, 255));

    reseauMaj(r);
    echangerHeros(app);

    if (app->donjon.taille == 0) {
        changerEcran(app, ECRAN_MENU);
        return;
    }
    if (el->heros >= app->donjon.taille) el->heros = 0;

    int libre = r->etat == RESEAU_INACTIF || r->etat == RESEAU_ERREUR;
    panneauHeros(app, libre);

    float x = 540, y = 120, w = 680;
    if (libre) {
        /* héberger */
        panneau(g, x, y, w, 200);
        texte(g, POLICE_MOYENNE, "Héberger la partie", x + 24, y + 18, COUL_OR, ALIGN_GAUCHE);
        texteBloc(g, POLICE_NORMALE,
                  "Votre adresse IP s'affichera : donnez-la à l'autre joueur pour qu'il se connecte. "
                  "Vous serez le joueur 1.",
                  x + 24, y + 58, (int)w - 48, COUL_TEXTE_DOUX);
        if (bouton(g, x + 24, y + 128, 240, 50, "Héberger", BOUTON_PRINCIPAL, 1)) {
            el->saisieAdresse = 0;
            SDL_StopTextInput();
            reseauHeberger(r);
        }

        /* rejoindre */
        float y2 = y + 220;
        panneau(g, x, y2, w, 200);
        texte(g, POLICE_MOYENNE, "Rejoindre une partie", x + 24, y2 + 18, COUL_OR, ALIGN_GAUCHE);
        texte(g, POLICE_NORMALE, "Adresse IP de l'hôte :", x + 24, y2 + 62, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
        int entree = champAdresse(app, x + 24, y2 + 90, 380, 42);
        if (bouton(g, x + 420, y2 + 86, 236, 50, "Se connecter", BOUTON_PRINCIPAL, el->adresse[0] != '\0')
            || (entree && el->adresse[0] != '\0')) {
            el->saisieAdresse = 0;
            SDL_StopTextInput();
            reseauRejoindre(r, el->adresse);
        }
        texte(g, POLICE_PETITE, "Ajoutez « :port » si l'hôte n'utilise pas le port par défaut.", x + 24,
              y2 + 150, COUL_TEXTE_DOUX, ALIGN_GAUCHE);

        if (r->etat == RESEAU_ERREUR) {
            texte(g, POLICE_NORMALE, r->erreur, x + w / 2, y2 + 236, COUL_DANGER, ALIGN_CENTRE);
        }
    } else {
        panneau(g, x, y, w, 540);
        char buf[160];
        if (r->etat == RESEAU_ATTENTE) {
            snprintf(buf, sizeof buf, "En attente d'un adversaire%s", points(g));
            texteOmbre(g, POLICE_GRANDE, buf, x + 34, y + 40, COUL_OR_CLAIR, ALIGN_GAUCHE);
            texte(g, POLICE_NORMALE, "Donnez-lui votre adresse IP :", x + 34, y + 120, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
            texteBloc(g, POLICE_GRANDE, el->ips, x + 34, y + 150, (int)w - 68, COUL_TEXTE);
            snprintf(buf, sizeof buf, "Port %d", RESEAU_PORT);
            texte(g, POLICE_MOYENNE, buf, x + 34, y + 260, COUL_ACCENT, ALIGN_GAUCHE);
            texteBloc(g, POLICE_PETITE,
                      "Sur le même réseau (Wi-Fi, partage de connexion), l'adresse en 192.168... ou 10... "
                      "suffit. Par Internet, il faut rediriger ce port sur la box et donner l'IP publique. "
                      "Si la connexion échoue, autorisez le port dans le pare-feu "
                      "(Fedora : sudo firewall-cmd --add-port=5050/tcp).",
                      x + 34, y + 310, (int)w - 68, COUL_TEXTE_DOUX);
        } else if (r->etat == RESEAU_CONNEXION) {
            snprintf(buf, sizeof buf, "Connexion à %s%s", el->adresse, points(g));
            texteOmbre(g, POLICE_GRANDE, buf, x + 34, y + 40, COUL_OR_CLAIR, ALIGN_GAUCHE);
        } else {
            texteOmbre(g, POLICE_GRANDE, "Connecté !", x + 34, y + 40, COUL_VIE, ALIGN_GAUCHE);
            if (el->bonjourRecu) {
                snprintf(buf, sizeof buf, "Adversaire : %s (%s niveau %d)", el->adversaire.nom,
                         libelleClasse(el->adversaire.classe), el->adversaire.niveau);
                texte(g, POLICE_MOYENNE, buf, x + 34, y + 120, COUL_TEXTE, ALIGN_GAUCHE);
            } else {
                snprintf(buf, sizeof buf, "Échange des héros%s", points(g));
                texte(g, POLICE_MOYENNE, buf, x + 34, y + 120, COUL_TEXTE_DOUX, ALIGN_GAUCHE);
            }
        }
        if (r->etat != RESEAU_CONNECTE && bouton(g, x + w - 214, y + 470, 190, 46, "Annuler", BOUTON_NORMAL, 1)) {
            reseauFermer(r);
        }
    }

    if (enTete(app, "Duel en ligne", "Un joueur héberge, l'autre tape son adresse IP pour le rejoindre.")) {
        quitter(app);
    }
}
