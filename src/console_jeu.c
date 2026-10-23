/**
 * @file console_jeu.c
 * @brief Le jeu complet en mode texte (voir console_jeu.h).
 *
 * Le combat en équipe est piloté par le moteur de combat.c : ce fichier
 * ne fait qu'afficher l'état du combat, demander les actions au clavier et
 * raconter ce qui se passe (journal du moteur + liste des effets).
 */

/* [DONJON - terminal] Fichier de la version terminal (./donjon) uniquement. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console_jeu.h"
#include "combat.h"
#include "boutique.h"
#include "inventaire.h"
#include "saisie.h"

#define LARGEUR_BARRE 12

/* ------------------------------------------------------------------ */
/*  Petits outils d'affichage                                         */
/* ------------------------------------------------------------------ */

/** Barre texte proportionnelle : [#####.......] */
static void barre(char *buf, int largeur, int valeur, int max)
{
    int pleines = max > 0 ? valeur * largeur / max : 0;
    if (valeur > 0 && pleines == 0) {
        pleines = 1; /* encore en vie : au moins une case */
    }
    buf[0] = '[';
    for (int i = 0; i < largeur; i++) {
        buf[i + 1] = i < pleines ? '#' : '.';
    }
    buf[largeur + 1] = ']';
    buf[largeur + 2] = '\0';
}

/** Ajoute du texte formaté à la fin de buf (sans jamais déborder). */
static void ajouter(char *buf, size_t taille, const char *format, ...)
{
    size_t n = strlen(buf);
    if (n + 1 >= taille) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(buf + n, taille - n, format, args);
    va_end(args);
}

static void ligneSeparation(void)
{
    printf("--------------------------------------------------------------------------\n");
}

/** Attend Entrée (pour laisser le temps de lire). Retourne 0 sur EOF. */
static int attendreEntree(void)
{
    char tampon[8];
    printf("(Entree pour continuer)");
    fflush(stdout);
    int ok = lireLigne(tampon, sizeof tampon);
    printf("\n");
    return ok;
}

/** Nombre de personnages vivants. */
static int compterVivants(const Donjon *d)
{
    int n = 0;
    for (int i = 0; i < d->taille; i++) {
        n += estVivant(&d->elements[i]);
    }
    return n;
}

/** Liste des personnages avec leur élément. */
static void listerPersonnages(const Donjon *d)
{
    for (int i = 0; i < d->taille; i++) {
        const Personnage *p = &d->elements[i];
        printf("%2d. %-20s %-12s %-10s Niv %2d  PV %3d/%-3d%s\n", i + 1, p->nom, nomClasse(p->classe),
               nomElement(p->element), p->niveau, p->pv, p->pvMax, estVivant(p) ? "" : "  (K.O.)");
    }
}

/**
 * Compose une équipe de 1 à COMBAT_MAX_ALLIES personnages distincts.
 * @param vivantsSeulement 1 : les K.O. sont refusés
 * @return la taille de l'équipe, 0 si annulé (EOF ou 0).
 */
static int choisirEquipe(Donjon *d, Personnage *equipe[], const char *titre, int vivantsSeulement)
{
    int disponibles = vivantsSeulement ? compterVivants(d) : d->taille;
    int max = disponibles < COMBAT_MAX_ALLIES ? disponibles : COMBAT_MAX_ALLIES;
    if (max == 0) {
        printf("Aucun personnage disponible.\n");
        return 0;
    }
    printf("\n%s\n", titre);
    listerPersonnages(d);
    char invite[64];
    snprintf(invite, sizeof invite, "Nombre de heros dans l'equipe (1-%d, 0 = annuler) : ", max);
    int n = lireEntier(invite, 0, max);
    if (n <= 0) {
        return 0;
    }
    int choisis[COMBAT_MAX_ALLIES];
    for (int k = 0; k < n; k++) {
        for (;;) {
            snprintf(invite, sizeof invite, "Heros %d/%d (numero) : ", k + 1, n);
            int c = lireEntier(invite, 1, d->taille);
            if (c == -1) {
                return 0;
            }
            int deja = 0;
            for (int j = 0; j < k; j++) {
                deja |= choisis[j] == c - 1;
            }
            if (deja) {
                printf("  Deja dans l'equipe.\n");
            } else if (vivantsSeulement && !estVivant(&d->elements[c - 1])) {
                printf("  %s est K.O. : reposez-vous ou utilisez une potion.\n", d->elements[c - 1].nom);
            } else {
                choisis[k] = c - 1;
                break;
            }
        }
    }
    for (int k = 0; k < n; k++) {
        equipe[k] = &d->elements[choisis[k]];
    }
    return n;
}

/* ------------------------------------------------------------------ */
/*  Affichage d'un combat                                             */
/* ------------------------------------------------------------------ */

/** Lettre affichée pour une case (A, B... pour le camp 1). */
static char lettreCase(int i)
{
    return combatEstAllie(i) ? (char)('1' + i) : (char)('A' + i - PREMIER_ENNEMI);
}

/** Une ligne par combattant d'un camp. */
static void afficherCamp(const Combat *cb, int camp)
{
    int debut = camp == 0 ? 0 : PREMIER_ENNEMI;
    int fin = camp == 0 ? PREMIER_ENNEMI : COMBAT_MAX;
    char b[LARGEUR_BARRE + 3], faibles[64];
    for (int i = debut; i < fin; i++) {
        const Combattant *c = &cb->c[i];
        if (!c->present) {
            continue;
        }
        const Personnage *p = c->p;
        barre(b, LARGEUR_BARRE, p->pv, p->pvMax);
        printf(" %c. %-16s %-10s PV %s %4d/%-4d", lettreCase(i), p->nom, nomElement(p->element), b, p->pv,
               p->pvMax);
        if (!estVivant(p)) {
            printf("  K.O.\n");
            continue;
        }
        if (c->heros) {
            printf("  Energie %3d/%-3d", c->energie, c->energieMax);
        }
        if (c->resistanceMax > 0) {
            printf("  Garde %3d/%-3d", c->brise ? 0 : c->resistance, c->resistanceMax);
        }
        printf("\n");

        /* deuxième ligne : boss, faiblesses et altérations (si besoin) */
        char etat[256] = "";
        if (c->boss) ajouter(etat, sizeof etat, "%s ", c->phase == 2 ? "[BOSS - PHASE 2]" : "[BOSS]");
        if (c->resistanceMax > 0) {
            ecrireElements(c->faiblesses, faibles, sizeof faibles);
            ajouter(etat, sizeof etat, "Faible a : %s  ", faibles);
        }
        if (c->bouclier > 0) ajouter(etat, sizeof etat, "[bouclier %d] ", c->bouclier);
        if (c->recharge > 0) ajouter(etat, sizeof etat, "[competence : recharge %d] ", c->recharge);
        if (c->brise) ajouter(etat, sizeof etat, "[BRISE] ");
        if (c->gele) ajouter(etat, sizeof etat, "[gele] ");
        if (c->dot > 0) ajouter(etat, sizeof etat, "[%d degats/tour] ", c->dot);
        if (c->buffAtk > 0) ajouter(etat, sizeof etat, "[ATK +%d %%] ", c->buffAtk);
        if (c->malusDef > 0) ajouter(etat, sizeof etat, "[DEF -%d %%] ", c->malusDef);
        if (etat[0] != '\0') {
            printf("     %s\n", etat);
        }
    }
}

/** État complet : en-tête, équipes, ordre des prochains tours. */
static void afficherEtat(const Combat *cb)
{
    printf("\n");
    ligneSeparation();
    if (cb->mode == MODE_DUEL) {
        printf(" DUEL - action %d%s\n", cb->tour,
               combatFatigue(cb) > 0 ? "  (fatigue : les degats augmentent)" : "");
    } else {
        printf(" Etage %d : %s - vague %d/%d\n", cb->etage, combatEtage(cb->etage)->nom, cb->vague, cb->nbVagues);
    }
    ligneSeparation();
    printf("%s   (points de competence : %d/%d)\n", cb->mode == MODE_DUEL ? "JOUEUR 1" : "VOTRE EQUIPE",
           cb->sp[0], SP_MAX);
    afficherCamp(cb, 0);
    if (cb->mode == MODE_DUEL) {
        printf("JOUEUR 2   (points de competence : %d/%d)\n", cb->sp[1], SP_MAX);
    } else {
        printf("ENNEMIS\n");
    }
    afficherCamp(cb, 1);

    int ordre[8];
    double avs[8];
    int n = combatOrdre(cb, ordre, avs, 8);
    printf("Ordre des tours :");
    for (int k = 0; k < n; k++) {
        printf("%s %s", k > 0 ? " >" : "", cb->c[ordre[k]].p->nom);
    }
    printf("\n");
    ligneSeparation();
}

/** Affiche jusqu'à `max` lignes du journal pas encore lues. */
static void afficherJournal(const Combat *cb, int *lues, int max)
{
    int nouvelles = cb->nbJournalTotal - *lues;
    if (nouvelles > cb->nbJournal) {
        *lues += nouvelles - cb->nbJournal; /* trop anciennes : perdues */
        nouvelles = cb->nbJournal;
    }
    for (int k = 0; k < nouvelles && k < max; k++) {
        printf(" > %s\n", cb->journal[cb->nbJournal - nouvelles + k]);
        (*lues)++;
    }
}

/** Raconte les effets de la dernière action (dégâts, soins, altérations). */
static void afficherEffets(const Combat *cb)
{
    for (int k = 0; k < cb->nbEffets; k++) {
        const Effet *e = &cb->effets[k];
        const Personnage *p = cb->c[e->cible].p;
        switch (e->type) {
        case EFFET_DEGATS: {
            int m = multiplicateurElement(e->element, p->element);
            printf("     %s : -%d PV (%s)%s%s\n", p->nom, e->valeur, nomElement(e->element),
                   e->critique ? " CRITIQUE !" : "", m > 100 ? " efficace !" : (m < 100 ? " peu efficace" : ""));
            break;
        }
        case EFFET_SOIN:
            if (e->valeur > 0) printf("     %s : +%d PV\n", p->nom, e->valeur);
            break;
        case EFFET_BOUCLIER:
            printf("     %s : bouclier de %d\n", p->nom, e->valeur);
            break;
        case EFFET_BONUS:
        case EFFET_MALUS:
        case EFFET_TEXTE:
            if (e->texte[0] != '\0') printf("     %s : %s\n", p->nom, e->texte);
            break;
        default:
            break; /* K.O. et rupture : déjà dans le journal */
        }
    }
}

/** Action suivie de ses effets puis des messages qu'elle a provoqués. */
static void raconterAction(const Combat *cb, int *lues)
{
    afficherJournal(cb, lues, 1);
    afficherEffets(cb);
    afficherJournal(cb, lues, JOURNAL_MAX);
}

/* ------------------------------------------------------------------ */
/*  Tour d'un héros                                                   */
/* ------------------------------------------------------------------ */

static const char *const TYPES_ACTION[NB_ACTIONS] = { "Attaque de base", "Competence", "Ultime" };

/** Détail des trois capacités du héros, efficacité contre chaque adversaire. */
static void afficherDetails(const Combat *cb, int lanceur)
{
    const Personnage *p = cb->c[lanceur].p;
    char nom[64], desc[256];
    printf("\n%s (%s, element %s)\n", p->nom, nomClasse(p->classe), nomElement(p->element));
    for (int a = 0; a < NB_ACTIONS; a++) {
        nomCapacite(p, (enum TypeAction)a, nom, sizeof nom);
        descriptionCapacite(p, (enum TypeAction)a, desc, sizeof desc);
        printf("  %-16s %s\n  %16s %s\n", TYPES_ACTION[a], nom, "", desc);
    }
    printf("  Efficacite de l'element %s :\n", nomElement(p->element));
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (combatVivant(cb, i) && combatCamp(i) != combatCamp(lanceur)) {
            int m = multiplicateurElement(p->element, cb->c[i].p->element);
            printf("    %c. %-16s (%s) : %s\n", lettreCase(i), cb->c[i].p->nom, nomElement(cb->c[i].p->element),
                   m > 100 ? "efficace, +25 % de degats" : (m < 100 ? "peu efficace, -20 % de degats" : "normal"));
        }
    }
}

/**
 * Demande la cible d'une action. Les actions de zone totale n'en
 * demandent pas. Retourne la case visée, ou -1 si annulé.
 */
static int choisirCible(const Combat *cb, int lanceur, enum TypeAction action)
{
    const Capacite *cap = capaciteClasse(cb->c[lanceur].p->classe, action);
    if (cap->ciblage == CIBLE_TOUS_ENNEMIS || cap->ciblage == CIBLE_TOUS_ALLIES) {
        return combatCibleParDefaut(cb, lanceur, action);
    }
    int cases[COMBAT_MAX], n = 0;
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (combatCibleValide(cb, lanceur, action, i)) {
            cases[n++] = i;
        }
    }
    if (n == 0) {
        return -1;
    }
    if (n == 1) {
        return cases[0];
    }
    const Personnage *p = cb->c[lanceur].p;
    printf("Cible :\n");
    for (int k = 0; k < n; k++) {
        const Personnage *c = cb->c[cases[k]].p;
        printf("  %d. %-16s PV %d/%d", k + 1, c->nom, c->pv, c->pvMax);
        if (capaciteOffensive(p->classe, action)) {
            int m = multiplicateurElement(p->element, c->element);
            printf("  %s%s", nomElement(c->element), m > 100 ? " (efficace)" : (m < 100 ? " (peu efficace)" : ""));
            if (cb->c[cases[k]].faiblesses & (1u << p->element)) {
                printf(" [entame sa garde]");
            }
        }
        printf("\n");
    }
    int choix = lireEntier("Votre choix (0 = retour) : ", 0, n);
    return choix <= 0 ? -1 : cases[choix - 1];
}

/** Liste les héros du camp de l'actif dont l'ultime est prêt. */
static int ultimesPrets(const Combat *cb, int liste[])
{
    int n = 0;
    for (int i = 0; i < COMBAT_MAX; i++) {
        if (combatPeutAgir(cb, i, ACTION_ULTIME)) {
            liste[n++] = i;
        }
    }
    return n;
}

/** Ce qui s'est passé pendant la saisie d'un tour. */
enum ResultatTour {
    TOUR_MENU,       /**< rien de joué (détails, retour, action refusée) */
    TOUR_ACTION,     /**< une action a été jouée                        */
    TOUR_FUITE,      /**< fuite ou abandon                              */
    TOUR_FIN_SAISIE  /**< entrée standard fermée                        */
};

/** Fait jouer le héros actif (ou un ultime). */
static enum ResultatTour tourHeros(Combat *cb, int *lues)
{
    int l = cb->actif;
    Combattant *c = &cb->c[l];
    const Personnage *p = c->p;
    char nom[64];

    if (cb->mode == MODE_DUEL) {
        printf("JOUEUR %d - ", combatCamp(l) + 1);
    }
    printf("Tour de %s (%s, %s) - energie %d/%d\n", p->nom, nomClasse(p->classe), nomElement(p->element),
           c->energie, c->energieMax);
    nomCapacite(p, ACTION_BASIQUE, nom, sizeof nom);
    printf("  1. %s (attaque de base, +1 point de competence)\n", nom);
    nomCapacite(p, ACTION_COMPETENCE, nom, sizeof nom);
    printf("  2. %s (competence, 1 point)", nom);
    if (c->recharge > 0) {
        printf("  [en recharge : %d tour%s]", c->recharge, c->recharge > 1 ? "s" : "");
    } else if (cb->sp[combatCamp(l)] == 0) {
        printf("  [plus de points]");
    }
    printf("\n");
    int prets[COMBAT_MAX];
    int nbPrets = ultimesPrets(cb, prets);
    printf("  3. Ultime");
    if (nbPrets == 0) {
        printf("  [aucun heros pret : energie insuffisante]");
    } else {
        printf("  [pret :");
        for (int k = 0; k < nbPrets; k++) printf(" %s", cb->c[prets[k]].p->nom);
        printf("]");
    }
    printf("\n  4. Details des capacites et efficacite\n");
    printf("  0. %s\n", cb->mode == MODE_DUEL ? "Abandonner le duel" : "Fuir le combat");

    int choix = lireEntier("Votre choix : ", 0, 4);
    if (choix == -1) {
        return TOUR_FIN_SAISIE;
    }
    if (choix == 0) {
        if (lireEntier("Vraiment ? (1 = oui, 0 = non) : ", 0, 1) == 1) {
            return TOUR_FUITE;
        }
        return TOUR_MENU;
    }
    if (choix == 4) {
        afficherDetails(cb, l);
        return TOUR_MENU;
    }

    int lanceur = l;
    enum TypeAction action = (enum TypeAction)(choix - 1);
    if (action == ACTION_ULTIME) {
        if (nbPrets == 0) {
            printf("Aucun ultime n'est pret (l'energie monte en agissant et en encaissant des coups).\n");
            return TOUR_MENU;
        }
        lanceur = prets[0];
        if (nbPrets > 1) {
            for (int k = 0; k < nbPrets; k++) {
                nomCapacite(cb->c[prets[k]].p, ACTION_ULTIME, nom, sizeof nom);
                printf("  %d. %s : %s\n", k + 1, cb->c[prets[k]].p->nom, nom);
            }
            int u = lireEntier("Quel ultime ? (0 = retour) : ", 0, nbPrets);
            if (u <= 0) {
                return TOUR_MENU;
            }
            lanceur = prets[u - 1];
        }
    } else if (!combatPeutAgir(cb, l, action)) {
        if (c->recharge > 0) {
            printf("%s doit encore attendre %d tour%s avant de relancer sa competence.\n", p->nom, c->recharge,
                   c->recharge > 1 ? "s" : "");
        } else {
            printf("Plus de points de competence : utilisez une attaque de base pour en regagner.\n");
        }
        return TOUR_MENU;
    }

    int cible = choisirCible(cb, lanceur, action);
    if (cible < 0) {
        return TOUR_MENU;
    }
    printf("\n");
    if (!combatJouer(cb, lanceur, action, cible)) {
        return TOUR_MENU;
    }
    raconterAction(cb, lues);
    return TOUR_ACTION;
}

/**
 * Déroule un combat jusqu'à la fin.
 * @return 1 si le combat est allé à son terme, 0 si le joueur a fui (ou
 *         si l'entrée standard est fermée).
 */
static int deroulerCombat(Combat *cb)
{
    int lues = 0;
    afficherJournal(cb, &lues, JOURNAL_MAX);
    afficherEtat(cb);
    int dernierActif = cb->etat == COMBAT_TOUR_JOUEUR ? cb->actif : -1;
    while (cb->etat != COMBAT_VICTOIRE && cb->etat != COMBAT_DEFAITE) {
        switch (cb->etat) {
        case COMBAT_FIN_VAGUE:
            if (!attendreEntree()) return 0;
            combatVagueSuivante(cb);
            afficherJournal(cb, &lues, JOURNAL_MAX);
            afficherEffets(cb);
            dernierActif = -1;
            break;

        case COMBAT_TOUR_ENNEMI:
            combatTourEnnemi(cb);
            raconterAction(cb, &lues);
            if (!cb->tourFini && cb->etat == COMBAT_TOUR_ENNEMI) {
                cb->tourFini = 1; /* sécurité : ne jamais rester bloqué */
            }
            break;

        case COMBAT_TOUR_JOUEUR: {
            /* l'état n'est réaffiché qu'au début du tour d'un héros */
            if (cb->actif != dernierActif || cb->tourFini) {
                afficherEtat(cb);
                dernierActif = cb->actif;
            }
            enum ResultatTour r = tourHeros(cb, &lues);
            if (r == TOUR_FIN_SAISIE) return 0;
            if (r == TOUR_FUITE) {
                printf(cb->mode == MODE_DUEL ? "Duel abandonne.\n" : "L'equipe prend la fuite !\n");
                return 0;
            }
            if (!cb->tourFini && cb->etat == COMBAT_TOUR_JOUEUR) {
                if (r == TOUR_ACTION) {
                    dernierActif = -1; /* après un ultime : état mis à jour */
                }
                continue; /* même héros */
            }
            break;
        }
        default:
            break;
        }

        if (cb->tourFini && cb->etat != COMBAT_VICTOIRE && cb->etat != COMBAT_DEFAITE
            && cb->etat != COMBAT_FIN_VAGUE) {
            combatAvancer(cb);
            afficherEffets(cb); /* dégâts sur la durée, gel... */
            afficherJournal(cb, &lues, JOURNAL_MAX);
            dernierActif = -1;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Explorer le donjon                                                */
/* ------------------------------------------------------------------ */

static void afficherEtages(const Progression *prog)
{
    printf("\n=== Les etages du donjon ===\n");
    for (int e = 1; e <= ETAGE_MAX; e++) {
        const InfoEtage *info = combatEtage(e);
        if (e > prog->etageMax) {
            printf(" %2d. ????? (verrouille)\n", e);
            continue;
        }
        printf(" %2d. %-24s %d vagues", e, info->nom, info->nbVagues);
        if (info->boss != NB_MONSTRES) {
            printf("  BOSS : %s", nomMonstre(info->boss));
        }
        if (prog->etagesNettoyes & (1u << (e - 1))) {
            printf("  (deja nettoye)");
        }
        printf("\n     %s\n", info->description);
    }
}

void menuExplorer(Donjon *d, Progression *prog)
{
    if (d == NULL || prog == NULL) {
        return;
    }
    if (compterVivants(d) == 0) {
        printf("Aucun personnage en etat de combattre : reposez-vous d'abord.\n");
        return;
    }
    afficherEtages(prog);
    char invite[64];
    snprintf(invite, sizeof invite, "Etage (1-%d, 0 = retour) : ", prog->etageMax);
    int etage = lireEntier(invite, 0, prog->etageMax);
    if (etage <= 0) {
        return;
    }

    Personnage *equipe[COMBAT_MAX_ALLIES];
    int n = choisirEquipe(d, equipe, "Composez votre equipe (les monstres seront aussi nombreux) :", 1);
    if (n == 0) {
        return;
    }

    Combat *cb = malloc(sizeof *cb); /* grosse structure : sur le tas */
    if (cb == NULL) {
        perror("malloc");
        return;
    }
    if (!combatInit(cb, equipe, n, etage)) {
        printf("Le combat n'a pas pu commencer.\n");
        free(cb);
        return;
    }
    printf("\n*** %s ***\n%s\n", combatEtage(etage)->nom, combatEtage(etage)->description);

    if (deroulerCombat(cb)) {
        printf("\n");
        ligneSeparation();
        if (cb->etat == COMBAT_VICTOIRE) {
            int boss = combatEtage(etage)->boss != NB_MONSTRES;
            int foieGras = recompenseFoieGras(prog, etage, boss);
            printf(" VICTOIRE ! Etage %d nettoye.\n", etage);
            printf(" Survivants : +%d XP, +%d point(s) de stats chacun. +%d foie gras (total %d).\n",
                   cb->xpGagnee, cb->pointsGagnes, foieGras, prog->foieGras);
            for (int k = 0; k < cb->nbButins; k++) {
                printf(" Butin : %s pour %s\n", cb->butins[k].nom, cb->receveurs[k]);
            }
            if (etage == prog->etageMax && prog->etageMax < ETAGE_MAX) {
                prog->etageMax++;
                printf(" Nouvel etage debloque : %d - %s !\n", prog->etageMax, combatEtage(prog->etageMax)->nom);
            } else if (etage == ETAGE_MAX) {
                printf(" Le Canard Noir est vaincu : le donjon est libere !\n");
            }
            if (!sauvegarderProgression(FICHIER_PROGRESSION, prog)) {
                printf(" (la progression n'a pas pu etre enregistree)\n");
            }
        } else {
            printf(" DEFAITE... L'equipe est K.O.\n");
            printf(" Reposez-vous au camp (menu 12) ou utilisez une potion, puis retentez.\n");
        }
        ligneSeparation();
        printf("Pensez a sauvegarder vos heros (menu 4).\n");
    }
    free(cb);
}

/* ------------------------------------------------------------------ */
/*  Duel en équipe                                                    */
/* ------------------------------------------------------------------ */

void menuDuelEquipe(Donjon *d)
{
    if (d == NULL) {
        return;
    }
    if (d->taille == 0) {
        printf("Le donjon est vide.\n");
        return;
    }
    printf("\nDuel en equipe : les heros sont copies avec tous leurs PV, le duel ne\n"
           "change rien au donjon. Un meme heros peut jouer dans les deux equipes.\n");
    Personnage *e1[COMBAT_MAX_ALLIES], *e2[COMBAT_MAX_ALLIES];
    int n1 = choisirEquipe(d, e1, "JOUEUR 1 - choisissez votre equipe :", 0);
    if (n1 == 0) return;
    int n2 = choisirEquipe(d, e2, "JOUEUR 2 - choisissez votre equipe :", 0);
    if (n2 == 0) return;

    Combat *cb = malloc(sizeof *cb);
    if (cb == NULL) {
        perror("malloc");
        return;
    }
    if (combatInitDuel(cb, e1, n1, e2, n2) && deroulerCombat(cb)) {
        printf("\n*** %s remporte le duel en %d actions ! ***\n",
               cb->etat == COMBAT_VICTOIRE ? "Le JOUEUR 1" : "Le JOUEUR 2", cb->tour);
    }
    free(cb);
}

/* ------------------------------------------------------------------ */
/*  Bestiaire et éléments                                             */
/* ------------------------------------------------------------------ */

/** Niveau moyen supposé de l'équipe à un étage (le même que Duckgeon). */
static int niveauTypique(int etage)
{
    return 1 + (etage - 1) * 3 / 2;
}

static void ficheMonstre(enum Monstre m)
{
    const InfoMonstre *info = infoMonstre(m);
    char buf[96];
    printf("\n");
    ligneSeparation();
    printf(" %s%s - element %s\n", info->nom, info->boss ? " (BOSS)" : "", nomElement(info->element));
    printf(" %s\n", info->description);
    ligneSeparation();
    ecrireElements(elementsForts(info->element), buf, sizeof buf);
    printf(" Fort contre   : %s (ses coups font +25 %%)\n", buf);
    ecrireElements(elementsFaibles(info->element), buf, sizeof buf);
    printf(" Faible contre : %s (ces elements lui font +25 %%)\n", buf);
    ecrireElements(faiblessesMonstre(m), buf, sizeof buf);
    printf(" Garde brisee par : %s\n", buf);
    printf("\n Attaque normale : %s (%d %% ATK sur un heros)\n", info->attaque, info->boss ? 130 : 100);
    printf(" Attaque speciale : %s (%d %% des tours)\n   %s\n", info->special, info->chanceSpecial,
           info->effetSpecial);
    if (info->boss) {
        printf(" Phase 2 sous 50 %% PV : ATK +30 %%, garde restauree, speciale plus frequente.\n");
    }
    printf("\n Statistiques (equipe de 4 heros) :\n");
    printf("   Etage  Niv. equipe    PV   ATK  DEF  VIT  Garde\n");
    unsigned etages = etagesMonstre(m);
    for (int e = 1; e <= ETAGE_MAX; e++) {
        if (etages & (1u << (e - 1))) {
            Personnage s;
            int garde;
            statsMonstre(m, e, niveauTypique(e), 4, &s, &garde);
            printf("   %5d  %11d  %5d  %4d  %3d  %3d  %5d\n", e, niveauTypique(e), s.pvMax, s.atk, s.def, s.vit,
                   garde);
        }
    }
}

void menuBestiaire(void)
{
    for (;;) {
        printf("\n=== Bestiaire ===\n");
        for (int m = 0; m < NB_MONSTRES; m++) {
            const InfoMonstre *info = infoMonstre((enum Monstre)m);
            printf(" %2d. %-16s %-10s%s\n", m + 1, info->nom, nomElement(info->element), info->boss ? " BOSS" : "");
        }
        int choix = lireEntier("Monstre (0 = retour) : ", 0, NB_MONSTRES);
        if (choix <= 0) {
            return;
        }
        ficheMonstre((enum Monstre)(choix - 1));
        if (!attendreEntree()) {
            return;
        }
    }
}

void afficherTableElements(void)
{
    char forts[64], faibles[64];
    printf("\n=== Table des elements ===\n");
    printf("Fort contre : +25 %% de degats et la garde de la cible baisse.\n");
    printf("Faible contre : -20 %% de degats.\n\n");
    for (int e = 0; e < NB_ELEMENTS; e++) {
        ecrireElements(elementsForts((enum Element)e), forts, sizeof forts);
        ecrireElements(elementsFaibles((enum Element)e), faibles, sizeof faibles);
        printf(" %-10s fort contre %-22s faible contre %s\n", nomElement((enum Element)e), forts, faibles);
        printf("            Effet sur les attaques - %s : %s\n", nomEffetElement((enum Element)e),
               effetElement((enum Element)e));
    }
    printf("\nCycle : chaque element bat les deux suivants.\n ");
    for (int k = 0; k < NB_ELEMENTS; k++) {
        printf("%s > ", nomElement(elementCycle(k)));
    }
    printf("%s\n", nomElement(elementCycle(0)));
}

/* ------------------------------------------------------------------ */
/*  Élément, repos                                                    */
/* ------------------------------------------------------------------ */

void menuElement(Donjon *d)
{
    if (d == NULL) {
        return;
    }
    if (d->taille == 0) {
        printf("Le donjon est vide.\n");
        return;
    }
    listerPersonnages(d);
    int choix = lireEntier("Personnage (numero) : ", 1, d->taille);
    if (choix == -1) {
        return;
    }
    Personnage *p = &d->elements[choix - 1];
    choisirElement(p);
    char nom[64];
    printf("Ses attaques deviennent :");
    for (int a = 0; a < NB_ACTIONS; a++) {
        if (capaciteOffensive(p->classe, (enum TypeAction)a)) {
            nomCapacite(p, (enum TypeAction)a, nom, sizeof nom);
            printf(" %s ;", nom);
        }
    }
    printf("\nEffet : %s - %s\n", nomEffetElement(p->element), effetElement(p->element));
}

void menuRepos(Donjon *d)
{
    if (d == NULL) {
        return;
    }
    for (int i = 0; i < d->taille; i++) {
        d->elements[i].pv = d->elements[i].pvMax;
    }
    printf("Toute l'equipe se repose au camp et recupere ses PV.\n");
}
