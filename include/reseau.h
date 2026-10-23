/**
 * @file reseau.h
 * @brief Connexion TCP entre deux joueurs pour le duel en ligne.
 *
 * Un joueur héberge (il attend sur le port RESEAU_PORT), l'autre se
 * connecte en tapant son adresse IP. Les deux programmes font ensuite
 * tourner le même combat avec la même graine aléatoire : seules les
 * actions choisies (lanceur, action, cible) circulent sur le réseau.
 *
 * Tout est non bloquant : on appelle reseauMaj() à chaque image, qui
 * avance la connexion et range les messages reçus dans une file.
 * Fonctionne avec les sockets POSIX (Linux, macOS) et Winsock (Windows).
 */
#ifndef RESEAU_H
#define RESEAU_H

#include <stdint.h>

#include "personnage.h"

#define RESEAU_PORT       5050
#define RESEAU_FILE_MAX   32
#define RESEAU_TAMPON     4096
#define RESEAU_HEROS_MAX  4   /* héros par équipe (= COMBAT_MAX_ALLIES) */

/** Où en est la connexion. */
enum EtatReseau {
    RESEAU_INACTIF,     /**< rien d'ouvert                         */
    RESEAU_ATTENTE,     /**< hôte : on attend que quelqu'un arrive */
    RESEAU_CONNEXION,   /**< invité : connexion en cours           */
    RESEAU_CONNECTE,    /**< les deux joueurs sont reliés          */
    RESEAU_ERREUR       /**< échec ou déconnexion (voir erreur)    */
};

/** Types de messages échangés. */
enum TypeMessage {
    MSG_BONJOUR = 1,    /**< équipe du joueur (+ format et graine si hôte) */
    MSG_ACTION,         /**< une action jouée pendant le duel      */
    MSG_REVANCHE,       /**< relancer le duel (numéro de manche)   */
    MSG_QUITTER         /**< le joueur quitte le duel              */
};

/** Format d'un duel en ligne, choisi par l'hôte. */
typedef struct {
    int taille;          /**< héros par équipe (1 à RESEAU_HEROS_MAX)    */
    int niveau;          /**< 0 : niveaux actuels, sinon niveau imposé   */
    unsigned graine;     /**< hasard du duel                             */
} FormatDuel;

/** Un message reçu (champs utiles selon le type). */
typedef struct {
    enum TypeMessage type;
    FormatDuel format;   /**< MSG_BONJOUR (envoyé par l'hôte)        */
    Personnage heros[RESEAU_HEROS_MAX]; /**< MSG_BONJOUR : l'équipe   */
    int nbHeros;
    int manche;          /**< MSG_ACTION, MSG_REVANCHE              */
    int tour;            /**< MSG_ACTION : tour du combat           */
    int lanceur, action, cible;
    unsigned controle;   /**< MSG_ACTION : empreinte de l'état      */
} Message;

/** Une connexion (hôte ou invité). Les sockets sont rangées dans un
 *  intptr_t pour couvrir les deux systèmes (-1 : aucune). */
typedef struct {
    enum EtatReseau etat;
    int hote;            /**< 1 : on héberge, 0 : on a rejoint      */
    intptr_t ecoute;     /**< socket d'écoute (hôte), -1 sinon       */
    intptr_t sock;       /**< socket vers l'autre joueur, -1 sinon   */
    char erreur[96];     /**< raison du dernier échec               */

    unsigned char recu[RESEAU_TAMPON]; /**< octets pas encore découpés */
    int nbRecu;
    Message file[RESEAU_FILE_MAX];     /**< messages complets reçus   */
    int nbFile;
} Reseau;

/** Met la structure dans l'état « inactif » (à appeler une fois). */
void reseauInit(Reseau *r);

/** Ferme tout ce qui est ouvert et revient à l'état inactif. */
void reseauFermer(Reseau *r);

/** Commence à héberger sur RESEAU_PORT. @return 1 si le port est ouvert. */
int reseauHeberger(Reseau *r);

/** Commence à se connecter à `adresse` (« 192.168.1.20 » ou
 *  « 192.168.1.20:5050 »). @return 1 si la tentative a démarré. */
int reseauRejoindre(Reseau *r, const char *adresse);

/** Fait avancer la connexion et lit les messages reçus (non bloquant). */
void reseauMaj(Reseau *r);

/** Premier message de la file (NULL si vide), sans le retirer. */
const Message *reseauPremier(const Reseau *r);

/** Retire le premier message de la file. */
void reseauRetirer(Reseau *r);

/** Copie du héros telle que l'autre joueur la recevra (seuls les champs
 *  utiles au duel, PV au maximum) : les deux joueurs partent ainsi
 *  exactement des mêmes données. */
void reseauPreparerHeros(const Personnage *source, Personnage *copie);

/** Envoie notre équipe (n héros) et, pour l'hôte, le format du duel. */
int reseauEnvoyerBonjour(Reseau *r, const Personnage heros[], int n, const FormatDuel *format);

/** Envoie une action jouée. */
int reseauEnvoyerAction(Reseau *r, int manche, int tour, int lanceur, int action, int cible,
                        unsigned controle);

/** Demande de revanche (numéro de la nouvelle manche). */
int reseauEnvoyerRevanche(Reseau *r, int manche);

/** Prévient l'autre joueur qu'on quitte. */
void reseauEnvoyerQuitter(Reseau *r);

/**
 * Écrit dans `buf` les adresses IPv4 de cette machine sur le réseau local
 * (« 192.168.1.20 », plusieurs séparées par « · »).
 * @return le nombre d'adresses trouvées.
 */
int reseauAdressesLocales(char *buf, int taille);

#endif /* RESEAU_H */
