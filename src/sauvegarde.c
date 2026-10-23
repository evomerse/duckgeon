/**
 * @file sauvegarde.c
 * @brief Sauvegarde et chargement des personnages et de la progression
 *        (fichiers binaires).
 */

/* [COMMUN] Logique partagée par Donjon (terminal) et Duckgeon (SDL).
 * Les blocs qui ne servent qu'à une des deux versions sont marqués
 * [DONJON - terminal] ou [DUCKGEON - SDL]. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sauvegarde.h"

static const char SIGNATURE[4] = { 'D', 'O', 'N', 'J' };
static const char SIGNATURE_PROGRESSION[4] = { 'P', 'R', 'O', 'G' };

/* À incrémenter si une structure sauvegardée change. */
#define VERSION_FORMAT 2

/* Taille d'un Personnage en version 1 : tout ce qui précède les champs
 * ajoutés en version 2. */
#define TAILLE_PERSONNAGE_V1 offsetof(Personnage, pointsStats)

/* ------------------------------------------------------------------ */
/*  Personnages                                                       */
/* ------------------------------------------------------------------ */

int sauvegarderFichier(const char *nomFichier, const Personnage *liste, int n)
{
    if (nomFichier == NULL || n < 0 || (n > 0 && liste == NULL)) {
        fprintf(stderr, "sauvegarder : parametres invalides\n");
        return 0;
    }

    /* Fichier temporaire « donjon.sav.tmp » */
    char temporaire[512];
    if (snprintf(temporaire, sizeof temporaire, "%s.tmp", nomFichier) >= (int)sizeof temporaire) {
        fprintf(stderr, "sauvegarder : nom de fichier trop long\n");
        return 0;
    }

    FILE *f = fopen(temporaire, "wb");
    if (f == NULL) {
        perror(temporaire);
        return 0;
    }

    int version = VERSION_FORMAT;
    int taille = (int)sizeof(Personnage);
    int ok = fwrite(SIGNATURE, sizeof SIGNATURE, 1, f) == 1
          && fwrite(&version, sizeof version, 1, f) == 1
          && fwrite(&taille, sizeof taille, 1, f) == 1
          && fwrite(&n, sizeof n, 1, f) == 1
          && (n == 0 || fwrite(liste, sizeof *liste, (size_t)n, f) == (size_t)n);

    /* fclose peut aussi échouer (disque plein au vidage du tampon). */
    if (fclose(f) != 0) {
        ok = 0;
    }
    if (!ok) {
        perror("sauvegarder : ecriture");
        remove(temporaire);
        return 0;
    }
    if (rename(temporaire, nomFichier) != 0) {
        perror("sauvegarder : rename");
        remove(temporaire);
        return 0;
    }
    return 1;
}

void sauvegarder(const char *nomFichier, Personnage *liste, int n)
{
    sauvegarderFichier(nomFichier, liste, n);
}

/** Corrige les valeurs incohérentes d'un personnage lu sur le disque, pour
 *  qu'un fichier abîmé ne fasse jamais sortir le programme d'un tableau. */
static void verifierPersonnage(Personnage *p)
{
    p->nom[NOM_MAX - 1] = '\0';
    if ((unsigned)p->classe >= NB_CLASSES) p->classe = GUERRIER;
    if ((unsigned)p->element >= NB_ELEMENTS) p->element = PHYSIQUE;
    if (p->nbObjets < 0) p->nbObjets = 0;
    if (p->nbObjets > INVENTAIRE_MAX) p->nbObjets = INVENTAIRE_MAX;
    if (p->niveau < 1) p->niveau = 1;
    if (p->pvMax < 1) p->pvMax = 1;
    if (p->pv < 0) p->pv = 0;
    if (p->pv > p->pvMax) p->pv = p->pvMax;
    if (p->xp < 0) p->xp = 0;
    if (p->pointsStats < 0) p->pointsStats = 0;
    if (p->victoires < 0) p->victoires = 0;
    p->arme.nom[NOM_OBJET_MAX - 1] = '\0';
    for (int i = 0; i < INVENTAIRE_MAX; i++) {
        p->inventaire[i].nom[NOM_OBJET_MAX - 1] = '\0';
    }
    Apparence *a = &p->apparence;
    a->plumage %= NB_PLUMAGES;
    a->coupe %= NB_COUPES;
    a->coiffe %= NB_COIFFES;
    a->couleurCheveux %= NB_COULEURS;
    a->couleurYeux %= NB_COULEURS;
    a->regard %= NB_REGARDS;
    a->tenue %= NB_COULEURS;
    a->tenue2 %= NB_COULEURS;
    a->pilosite %= NB_PILOSITES;
    a->marque %= NB_MARQUES;
    a->cape %= 2;
    a->aura %= NB_COULEURS + 1;
    a->corpulence %= NB_CORPULENCES;
}

int chargerFichier(const char *nomFichier, Personnage **liste, int *n)
{
    if (nomFichier == NULL || liste == NULL || n == NULL) {
        fprintf(stderr, "charger : parametres invalides\n");
        return 0;
    }

    FILE *f = fopen(nomFichier, "rb");
    if (f == NULL) {
        perror(nomFichier);
        return 0;
    }

    char signature[4];
    int version, nombre;
    int taille = (int)TAILLE_PERSONNAGE_V1;
    int ok = fread(signature, sizeof signature, 1, f) == 1
          && memcmp(signature, SIGNATURE, sizeof SIGNATURE) == 0
          && fread(&version, sizeof version, 1, f) == 1
          && (version == 1 || version == VERSION_FORMAT);
    if (ok && version >= 2) {
        ok = fread(&taille, sizeof taille, 1, f) == 1;
    }
    ok = ok && fread(&nombre, sizeof nombre, 1, f) == 1
            && nombre >= 0 && nombre <= SAUVEGARDE_MAX_PERSOS
            && taille > 0 && taille <= (int)sizeof(Personnage);
    if (!ok) {
        fprintf(stderr, "charger : %s n'est pas une sauvegarde valide\n", nomFichier);
        fclose(f);
        return 0;
    }

    /* On lit dans un nouveau bloc : en cas d'échec, l'ancien tableau reste
     * utilisable. Au moins 1 case pour que malloc ne reçoive jamais 0. */
    size_t cases = nombre > 0 ? (size_t)nombre : 1;
    Personnage *nouveau = malloc(cases * sizeof *nouveau);
    if (nouveau == NULL) {
        perror("charger : malloc");
        fclose(f);
        return 0;
    }
    /* Un enregistrement par personnage : s'il est plus court que la
     * structure actuelle (ancienne version), la fin reste à 0. */
    for (int i = 0; i < nombre; i++) {
        memset(&nouveau[i], 0, sizeof nouveau[i]);
        if (fread(&nouveau[i], (size_t)taille, 1, f) != 1) {
            fprintf(stderr, "charger : %s est incomplet\n", nomFichier);
            free(nouveau);
            fclose(f);
            return 0;
        }
        verifierPersonnage(&nouveau[i]);
    }
    fclose(f);

    free(*liste); /* l'ancien tableau est remplacé */
    *liste = nouveau;
    *n = nombre;
    return 1;
}

void charger(const char *nomFichier, Personnage **liste, int *n)
{
    chargerFichier(nomFichier, liste, n);
}

int sauvegardeExiste(const char *nomFichier)
{
    if (nomFichier == NULL) {
        return 0;
    }
    FILE *f = fopen(nomFichier, "rb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Progression                                                       */
/* ------------------------------------------------------------------ */

int sauvegarderProgression(const char *nomFichier, const Progression *prog)
{
    if (nomFichier == NULL || prog == NULL) {
        return 0;
    }
    FILE *f = fopen(nomFichier, "wb");
    if (f == NULL) {
        perror(nomFichier);
        return 0;
    }
    int version = VERSION_FORMAT;
    int ok = fwrite(SIGNATURE_PROGRESSION, sizeof SIGNATURE_PROGRESSION, 1, f) == 1
          && fwrite(&version, sizeof version, 1, f) == 1
          && fwrite(prog, sizeof *prog, 1, f) == 1;
    if (fclose(f) != 0 || !ok) {
        perror("sauvegarderProgression");
        return 0;
    }
    return 1;
}

int chargerProgression(const char *nomFichier, Progression *prog)
{
    if (nomFichier == NULL || prog == NULL) {
        return 0;
    }
    FILE *f = fopen(nomFichier, "rb");
    if (f == NULL) {
        return 0; /* pas encore de progression : ce n'est pas une erreur */
    }
    char signature[4];
    int version;
    Progression lu;
    memset(&lu, 0, sizeof lu);
    int ok = fread(signature, sizeof signature, 1, f) == 1
          && memcmp(signature, SIGNATURE_PROGRESSION, sizeof signature) == 0
          && fread(&version, sizeof version, 1, f) == 1;
    if (ok && version == 1) {
        ok = fread(&lu.etageMax, sizeof lu.etageMax, 1, f) == 1; /* étage seul */
    } else if (ok && version == VERSION_FORMAT) {
        ok = fread(&lu, sizeof lu, 1, f) == 1;
    } else {
        ok = 0;
    }
    fclose(f);
    if (!ok || lu.etageMax < 1 || lu.foieGras < 0) {
        fprintf(stderr, "chargerProgression : %s invalide\n", nomFichier);
        return 0;
    }
    *prog = lu;
    return 1;
}
