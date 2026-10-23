/**
 * @file sauvegarde.h
 * @brief Persistance des personnages et de la progression dans des
 *        fichiers binaires (Partie 2).
 *
 * Format de donjon.sav (version 2) :
 *   - signature "DONJ" (4 octets) pour reconnaître nos fichiers ;
 *   - version du format (int) ;
 *   - taille d'un enregistrement (int) = sizeof(Personnage) à l'écriture ;
 *   - nombre de personnages n (int) ;
 *   - n structures Personnage écrites telles quelles (fwrite). C'est
 *     possible car Personnage ne contient aucun pointeur.
 *
 * La version 1 (sans le champ « taille ») est toujours relue : les champs
 * ajoutés depuis (en fin de structure) sont alors mis à 0.
 */
#ifndef SAUVEGARDE_H
#define SAUVEGARDE_H

#include "personnage.h"

/** Fichier de sauvegarde utilisé par la console et l'interface graphique. */
#define FICHIER_SAUVEGARDE "donjon.sav"

/** Fichier de la progression de l'interface graphique. */
#define FICHIER_PROGRESSION "progression.sav"

/** Nombre maximal de personnages accepté à la lecture (protège contre un
 *  fichier corrompu qui annoncerait un nombre absurde). */
#define SAUVEGARDE_MAX_PERSOS 100000

/** Progression du joueur dans la version graphique. */
typedef struct {
    int etageMax;            /**< Plus haut étage débloqué (1..ETAGE_MAX)   */
    int foieGras;            /**< Monnaie des cosmétiques                   */
    unsigned cosmetiques;    /**< Bit i à 1 : cosmétique i acheté           */
    unsigned etagesNettoyes; /**< Bit e-1 à 1 : étage e déjà terminé         */
} Progression;

/**
 * Écrit le tableau de personnages dans un fichier binaire.
 * On écrit d'abord dans un fichier temporaire puis on le renomme : si
 * l'écriture échoue, l'ancienne sauvegarde reste intacte.
 * En cas d'erreur, un message est affiché (perror) et le fichier précédent
 * n'est pas modifié.
 */
void sauvegarder(const char *nomFichier, Personnage *liste, int n);

/**
 * Relit le fichier binaire et réalloue dynamiquement le tableau.
 * En cas de succès, l'ancien tableau *liste est libéré et remplacé par un
 * bloc de max(n, 1) cases ; *n reçoit le nombre de personnages lus.
 * En cas d'erreur (fichier absent, illisible ou corrompu, mémoire
 * insuffisante), *liste et *n ne sont pas modifiés.
 */
void charger(const char *nomFichier, Personnage **liste, int *n);

/** Comme sauvegarder(), mais retourne 1 en cas de succès, 0 sinon. */
int sauvegarderFichier(const char *nomFichier, const Personnage *liste, int n);

/** Comme charger(), mais retourne 1 en cas de succès, 0 sinon. */
int chargerFichier(const char *nomFichier, Personnage **liste, int *n);

/** Écrit la progression (signature "PROG", version, structure). 1 si OK. */
int sauvegarderProgression(const char *nomFichier, const Progression *prog);

/**
 * Relit la progression. Un fichier de version 1 (étage seul) est accepté.
 * @return 1 si OK ; sinon *prog n'est pas modifié (premier lancement).
 */
int chargerProgression(const char *nomFichier, Progression *prog);

/** Retourne 1 si le fichier de sauvegarde existe (et peut être ouvert). */
int sauvegardeExiste(const char *nomFichier);

#endif /* SAUVEGARDE_H */
