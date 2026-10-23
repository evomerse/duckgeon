/**
 * @file main.c
 * @brief Le Donjon - Gestion RPG (Parties 1 et 2 : fondamentaux,
 *        sauvegarde / chargement en fichier binaire).
 *
 * Les personnages sont stockés dans un tableau alloué dynamiquement
 * (malloc) dont la capacité double avec realloc quand il est plein
 * (module donjon.c, partagé avec la version graphique).
 * Toute la mémoire est libérée (free) avant de quitter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "main.h"
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
    printf("\n #  %-20s %-12s %-6s %-22s %-7s\n", "Nom", "Classe", "Niveau", "Vie", "PV");
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

void menuCombat(Donjon *d)
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
    int a = choisirCombattant(d, "Attaquant (numero) : ", -1);
    if (a == -1) return;
    int c = choisirCombattant(d, "Cible (numero) : ", a);
    if (c == -1) return;

    /* On travaille sur des pointeurs vers les éléments du tableau : les
     * PV modifiés par attaquer() sont donc ceux du donjon lui-même. */
    Personnage *attaquant = &d->elements[a];
    Personnage *cible = &d->elements[c];

    printf("\n1. Une seule attaque\n2. Combat jusqu'au K.O. (tour par tour)\n");
    int mode = lireEntier("Votre choix : ", 1, 2);
    if (mode == -1) return;

    printf("\n");
    if (mode == 1) {
        attaquer(attaquant, cible);
        return;
    }

    /* Combat complet : les deux personnages s'échangent les coups.
     * attaquer() inflige toujours au moins 1 dégât, donc la boucle
     * se termine forcément. */
    int tour = 1;
    while (estVivant(attaquant) && estVivant(cible)) {
        printf("-- Tour %d --\n", tour++);
        attaquer(attaquant, cible);
        if (estVivant(cible)) {
            attaquer(cible, attaquant);
        }
    }
    printf("\nVainqueur : %s !\n", estVivant(attaquant) ? attaquant->nom : cible->nom);
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

    Donjon donjon;
    if (!donjonInit(&donjon)) {
        return EXIT_FAILURE;
    }

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
        printf("0. Quitter\n");
        choix = lireEntier("Votre choix : ", 0, 7);

        switch (choix) {
        case 1: menuCreer(&donjon); break;
        case 2: afficherTous(&donjon); break;
        case 3: menuCombat(&donjon); break;
        case 4: menuSauvegarder(&donjon); break;
        case 5: menuCharger(&donjon); break;
        case 6: menuInventaire(&donjon); break;
        case 7: menuAmeliorer(&donjon); break;
        case 0: printf("A bientot dans le donjon !\n"); break;
        default: choix = 0; break; /* EOF : on quitte proprement */
        }
    } while (choix != 0);

    donjonLiberer(&donjon);
    return EXIT_SUCCESS;
}
