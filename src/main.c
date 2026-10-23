/**
 * @file main.c
 * @brief Le Donjon - Gestion RPG (Parties 1 et 2 : fondamentaux,
 *        sauvegarde / chargement en fichier binaire), et le jeu complet
 *        en mode texte (exploration des étages, duels en équipe,
 *        bestiaire...) grâce au module console_jeu.
 *
 * Les personnages sont stockés dans un tableau alloué dynamiquement
 * (malloc) dont la capacité double avec realloc quand il est plein
 * (module donjon.c, partagé avec la version graphique).
 * Toute la mémoire est libérée (free) avant de quitter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h> /* SetConsoleOutputCP : accents UTF-8 dans la console */
#endif

#include "main.h"
#include "console_jeu.h"
#include "combat.h"
#include "personnage.h"
#include "inventaire.h"
#include "saisie.h"
#include "sauvegarde.h"

void afficherTous(const Donjon *d)
{
    if (d->taille == 0) {
        printf("Le donjon est vide.\n");
        return;
    }
    printf("\n #  %-20s %-12s %-10s %-6s %-22s %-7s\n", "Nom", "Classe", "Element", "Niveau", "Vie", "PV");
    for (int i = 0; i < d->taille; i++) {
        printf("%2d. ", i + 1);
        afficherPersonnage(&d->elements[i]);
    }
}

void menuCreer(Donjon *d)
{
    Personnage *p = donjonNouvelleCase(d);
    if (p == NULL) {
        printf("Memoire insuffisante, personnage non cree.\n");
        return;
    }
    creerPersonnage(p);
    d->taille++; /* la case n'est comptée qu'une fois remplie */
    printf("Personnage cree :\n    ");
    afficherPersonnage(p);
}

/**
 * Demande à l'utilisateur de choisir un personnage vivant.
 * @param exclu indice à ne pas accepter (-1 pour aucun)
 * @return l'indice choisi, ou -1 en cas d'EOF.
 */
int choisirCombattant(const Donjon *d, const char *invite, int exclu)
{
    for (;;) {
        int choix = lireEntier(invite, 1, d->taille);
        if (choix == -1) {
            return -1;
        }
        int i = choix - 1;
        if (i == exclu) {
            printf("  Un personnage ne peut pas s'attaquer lui-meme.\n");
        } else if (!estVivant(&d->elements[i])) {
            printf("  %s est K.O., choisissez-en un autre.\n", d->elements[i].nom);
        } else {
            return i;
        }
    }
}

void menuCombat(Donjon *d, Progression *prog)
{
    printf("\n--- Lancer un combat ---\n");
    printf("1. Explorer le donjon (equipe contre monstres, etage %d debloque)\n", prog->etageMax);
    printf("2. Duel en equipe (2 joueurs sur le meme clavier)\n");
    printf("3. Duel classique 1 contre 1\n");
    printf("0. Retour\n");
    switch (lireEntier("Votre choix : ", 0, 3)) {
    case 1: menuExplorer(d, prog); break;
    case 2: menuDuelEquipe(d); break;
    case 3: menuDuelSimple(d); break;
    default: break;
    }
}

/** Indice du premier objet de soin du sac, ou -1 s'il n'y en a pas. */
static int premierSoin(const Personnage *p)
{
    for (int i = 0; i < p->nbObjets; i++) {
        if (p->inventaire[i].type == SOIN) {
            return i;
        }
    }
    return -1;
}

/** Se mettre en garde : la défense augmente jusqu'au prochain tour du
 *  personnage. Retourne le bonus ajouté (à retirer ensuite). */
static int seDefendre(Personnage *p)
{
    int bonus = p->def > 2 ? p->def : 2; /* défense doublée, au moins +2 */
    p->def += bonus;
    printf("%s se met en garde (DEF %d jusqu'a son prochain tour).\n", p->nom, p->def);
    return bonus;
}

/** Affiche les PV des deux duellistes en début de tour. */
static void etatDuel(const Personnage *joueur, const Personnage *bot)
{
    printf("  Vous : %-20s %4d / %-4d PV\n", joueur->nom, joueur->pv, joueur->pvMax);
    printf("  Bot  : %-20s %4d / %-4d PV\n", bot->nom, bot->pv, bot->pvMax);
}

/**
 * Tour du bot : il boit une potion quand il est presque K.O., se met
 * parfois en garde quand il est blessé, et attaque le reste du temps.
 * @return le bonus de défense pris (0 s'il n'a pas paré).
 */
static int tourBot(Personnage *bot, Personnage *joueur)
{
    int soin = premierSoin(bot);
    if (soin != -1 && bot->pv * 100 < bot->pvMax * 30) {
        utiliserObjet(bot, soin);
        return 0;
    }
    if (bot->pv * 2 < bot->pvMax && rand() % 100 < 35) {
        return seDefendre(bot);
    }
    attaquer(bot, joueur);
    return 0;
}

void menuDuelSimple(Donjon *d)
{
    int vivants = 0;
    for (int i = 0; i < d->taille; i++) {
        vivants += estVivant(&d->elements[i]);
    }
    if (vivants < 2) {
        printf("Il faut au moins 2 personnages en vie pour combattre.\n");
        return;
    }

    afficherTous(d);
    int a = choisirCombattant(d, "Votre personnage (numero) : ", -1);
    if (a == -1) return;
    int c = choisirCombattant(d, "Adversaire, joue par le bot (numero) : ", a);
    if (c == -1) return;

    /* On travaille sur des pointeurs vers les éléments du tableau : les
     * PV modifiés par attaquer() sont donc ceux du donjon lui-même. */
    Personnage *joueur = &d->elements[a];
    Personnage *bot = &d->elements[c];
    printf("\n%s affronte %s (bot) ! Combat tour par tour jusqu'au K.O.\n", joueur->nom, bot->nom);

    /* Bonus de garde en cours : retiré au début du tour suivant de celui
     * qui a paré (et en fin de combat). attaquer() inflige toujours au
     * moins 1 dégât, donc le combat finit forcément. */
    int gardeJoueur = 0, gardeBot = 0;
    int fuite = 0;
    for (int tour = 1; estVivant(joueur) && estVivant(bot); tour++) {
        printf("\n-- Tour %d --\n", tour);
        etatDuel(joueur, bot);

        /* Tour du joueur */
        joueur->def -= gardeJoueur;
        gardeJoueur = 0;
        int action = 0;
        for (;;) {
            printf("1. Attaquer\n2. Se defendre\n3. Boire une potion\n0. Fuir\n");
            action = lireEntier("Votre action : ", 0, 3);
            if (action == 3 && premierSoin(joueur) == -1) {
                printf("  Vous n'avez aucune potion.\n");
                continue;
            }
            break;
        }
        if (action <= 0) { /* fuite ou EOF */
            printf("%s prend la fuite !\n", joueur->nom);
            fuite = 1;
            break;
        }
        if (action == 1) {
            attaquer(joueur, bot);
        } else if (action == 2) {
            gardeJoueur = seDefendre(joueur);
        } else {
            utiliserObjet(joueur, premierSoin(joueur));
        }
        if (!estVivant(bot)) {
            break;
        }

        /* Réponse du bot */
        printf("> Tour du bot :\n");
        bot->def -= gardeBot;
        gardeBot = tourBot(bot, joueur);
    }

    joueur->def -= gardeJoueur;
    bot->def -= gardeBot;
    if (!fuite) {
        printf("\nVainqueur : %s !\n", estVivant(joueur) ? joueur->nom : bot->nom);
    }
}

/** Sous-menu de l'inventaire d'un personnage (K.O. compris : une potion
 *  permet de le ranimer). */
void menuInventaire(Donjon *d)
{
    if (d->taille == 0) {
        printf("Le donjon est vide.\n");
        return;
    }

    afficherTous(d);
    int choix = lireEntier("Personnage (numero) : ", 1, d->taille);
    if (choix == -1) return;
    Personnage *p = &d->elements[choix - 1];

    for (;;) {
        printf("\n--- Inventaire de %s ---\n", p->nom);
        afficherPersonnage(p);
        afficherInventaire(p);
        printf("1. Utiliser / equiper un objet\n2. Jeter un objet\n0. Retour\n");
        int action = lireEntier("Votre choix : ", 0, 2);
        if (action <= 0) return; /* 0 ou EOF */

        if (p->nbObjets == 0) {
            printf("Le sac est vide.\n");
            continue;
        }
        int index = lireEntier("Objet (numero) : ", 1, p->nbObjets);
        if (index == -1) return;

        if (action == 1) {
            utiliserObjet(p, index - 1);
        } else {
            Objet jete;
            retirerObjet(p, index - 1, &jete);
            printf("%s jette %s.\n", p->nom, jete.nom);
        }
    }
}

void menuSauvegarder(const Donjon *d)
{
    if (donjonSauvegarder(d, FICHIER_SAUVEGARDE)) {
        printf("%d personnage(s) sauvegarde(s) dans %s.\n", d->taille, FICHIER_SAUVEGARDE);
    } else {
        printf("Echec de la sauvegarde.\n");
    }
}

void menuCharger(Donjon *d)
{
    if (!sauvegardeExiste(FICHIER_SAUVEGARDE)) {
        printf("Aucune sauvegarde trouvee (%s).\n", FICHIER_SAUVEGARDE);
        return;
    }
    if (d->taille > 0) {
        printf("Les %d personnage(s) actuel(s) seront remplaces.\n", d->taille);
        if (lireEntier("Confirmer ? (1 = oui, 0 = non) : ", 0, 1) != 1) {
            return;
        }
    }
    if (donjonCharger(d, FICHIER_SAUVEGARDE)) {
        printf("%d personnage(s) charge(s).\n", d->taille);
    } else {
        printf("Echec du chargement : le donjon n'a pas ete modifie.\n");
    }
}

void menuAmeliorer(Donjon *d)
{
    if (d->taille == 0) {
        printf("Le donjon est vide.\n");
        return;
    }
    afficherTous(d);
    int choix = lireEntier("Personnage (numero) : ", 1, d->taille);
    if (choix == -1) return;
    Personnage *p = &d->elements[choix - 1];

    while (p->pointsStats > 0) {
        printf("\n%s : %d point(s) de stats a repartir\n", p->nom, p->pointsStats);
        for (int s = 0; s < NB_STATS; s++) {
            printf("%d. %s (+%d)\n", s + 1, nomStat((enum Stat)s), gainStat((enum Stat)s));
        }
        printf("0. Retour\n");
        int stat = lireEntier("Votre choix : ", 0, NB_STATS);
        if (stat <= 0) return; /* 0 ou EOF */
        ameliorerStat(p, (enum Stat)(stat - 1));
        afficherPersonnage(p);
    }
    printf("%s n'a aucun point a repartir : gagnez des combats !\n", p->nom);
}

int main(void)
{
    srand((unsigned)time(NULL)); /* critiques, butin, apparence aleatoire */
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Donjon donjon;
    if (!donjonInit(&donjon)) {
        return EXIT_FAILURE;
    }

    /* Progression du joueur (étages débloqués, foie gras, cosmétiques) :
     * le même fichier que Duckgeon, enregistré à chaque changement. */
    Progression prog = { 1, 0, 0, 0 };
    chargerProgression(FICHIER_PROGRESSION, &prog);
    if (prog.etageMax < 1) prog.etageMax = 1;
    if (prog.etageMax > ETAGE_MAX) prog.etageMax = ETAGE_MAX;

    int choix;
    do {
        printf("\n===== LE DONJON - GESTION RPG =====\n");
        printf("1. Creer un personnage\n");
        printf("2. Afficher les personnages\n");
        printf("3. Lancer un combat\n");
        printf("4. Sauvegarder la partie\n");
        printf("5. Charger une partie\n");
        printf("6. Inventaire\n");
        printf("7. Ameliorer un personnage (points de stats)\n");
        printf("8. Changer l'element d'un personnage\n");
        printf("9. Bestiaire\n");
        printf("10. Table des elements\n");
        printf("11. Boutique de cosmetiques (%d foie gras)\n", prog.foieGras);
        printf("12. Se reposer au camp (soigne tout le monde)\n");
        printf("0. Quitter\n");
        choix = lireEntier("Votre choix : ", 0, 12);

        switch (choix) {
        case 1: menuCreer(&donjon); break;
        case 2: afficherTous(&donjon); break;
        case 3: menuCombat(&donjon, &prog); break;
        case 4: menuSauvegarder(&donjon); break;
        case 5: menuCharger(&donjon); break;
        case 6: menuInventaire(&donjon); break;
        case 7: menuAmeliorer(&donjon); break;
        case 8: menuElement(&donjon); break;
        case 9: menuBestiaire(); break;
        case 10: afficherTableElements(); break;
        case 11: menuBoutique(&donjon, &prog); break;
        case 12: menuRepos(&donjon); break;
        case 0: printf("A bientot dans le donjon !\n"); break;
        default: choix = 0; break; /* EOF : on quitte proprement */
        }
    } while (choix != 0);

    donjonLiberer(&donjon);
    return EXIT_SUCCESS;
}
