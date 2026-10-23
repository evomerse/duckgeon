/**
 * @file saisie.c
 * @brief Implémentation des saisies clavier robustes.
 *
 * On lit toujours une ligne complète (getchar) puis on la convertit avec
 * strtol : contrairement à scanf("%d"), une saisie invalide ("abc") ne
 * reste pas bloquée dans le tampon et ne provoque pas de boucle infinie.
 */

/* [DONJON - terminal] Saisies au clavier dans le terminal. Le fichier est
 * compilé dans les deux versions (creerPersonnage s'en sert) mais seul
 * Donjon l'utilise vraiment : Duckgeon passe par la souris et SDL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "saisie.h"

int lireLigne(char *buffer, size_t taille)
{
    if (buffer == NULL || taille == 0) {
        return 0; /* pas de place pour ranger la saisie */
    }

    /* On lit caractère par caractère plutôt qu'avec fgets : si l'utilisateur
     * tape un octet nul (Ctrl+@), fgets le garde dans la chaîne et la coupe
     * en deux, et la suite de la ligne pollue la saisie suivante. */
    size_t n = 0;
    int c;
    int luQuelqueChose = 0;
    while ((c = getchar()) != EOF) {
        luQuelqueChose = 1;
        if (c == '\n') {
            break;
        }
        if (c == '\0' || c == '\r') {
            continue; /* ignorés (\r : fin de ligne Windows) */
        }
        if (n + 1 < taille) {
            buffer[n++] = (char)c;
        }
        /* sinon ligne trop longue : on jette le reste */
    }
    buffer[n] = '\0';
    return luQuelqueChose; /* 0 seulement si EOF tout de suite */
}

int lireEntier(const char *invite, int min, int max)
{
    char buffer[64];

    if (min > max) {
        return -1; /* aucun nombre possible : on boucle pas à l'infini */
    }
    for (;;) {
        printf("%s", invite != NULL ? invite : "> ");
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
