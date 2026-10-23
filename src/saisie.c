/**
 * @file saisie.c
 * @brief Implémentation des saisies clavier robustes.
 *
 * On lit toujours une ligne complète avec fgets puis on la convertit avec
 * strtol : contrairement à scanf("%d"), une saisie invalide ("abc") ne
 * reste pas bloquée dans le tampon et ne provoque pas de boucle infinie.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "saisie.h"

int lireLigne(char *buffer, size_t taille)
{
    if (fgets(buffer, (int)taille, stdin) == NULL) {
        return 0; /* EOF ou erreur de lecture */
    }

    char *finLigne = strchr(buffer, '\n');
    if (finLigne != NULL) {
        *finLigne = '\0';
    } else {
        /* Ligne trop longue : on jette le reste pour ne pas polluer
         * la prochaine lecture. */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
        }
    }
    return 1;
}

int lireEntier(const char *invite, int min, int max)
{
    char buffer[64];

    for (;;) {
        printf("%s", invite);
        fflush(stdout);

        if (!lireLigne(buffer, sizeof buffer)) {
            return -1;
        }

        char *fin;
        errno = 0;
        long valeur = strtol(buffer, &fin, 10);

        /* Saisie valide : au moins un chiffre, rien d'autre derrière,
         * pas de dépassement et dans l'intervalle demandé. */
        if (fin != buffer && *fin == '\0' && errno == 0
            && valeur >= min && valeur <= max) {
            return (int)valeur;
        }
        printf("  Saisie invalide : entrez un nombre entre %d et %d.\n", min, max);
    }
}
