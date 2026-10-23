/**
 * @file reseau.c
 * @brief Connexion TCP du duel en ligne (sockets POSIX, non bloquants).
 *
 * Format d'un message : 2 octets de longueur (taille de la suite), 1 octet
 * de type, puis les données. Les entiers sont écrits octet par octet
 * (gros-boutiste) et le héros champ par champ : le format ne dépend ni du
 * compilateur ni de la machine.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE /* getifaddrs, IFF_LOOPBACK */

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "reseau.h"

#define VERSION_PROTOCOLE 1
#define DELAI_CONNEXION   10 /* secondes avant d'abandonner une connexion */

static time_t debutConnexion;

/* ------------------------------------------------------------------ */
/*  Écriture / lecture des données                                    */
/* ------------------------------------------------------------------ */

/** Tampon d'écriture d'un message. */
typedef struct {
    unsigned char o[512];
    int n;
} Ecriture;

static void ecrireOctet(Ecriture *e, int v)
{
    if (e->n < (int)sizeof e->o) e->o[e->n++] = (unsigned char)v;
}

static void ecrireEntier(Ecriture *e, unsigned v)
{
    ecrireOctet(e, (int)(v >> 24) & 0xFF);
    ecrireOctet(e, (int)(v >> 16) & 0xFF);
    ecrireOctet(e, (int)(v >> 8) & 0xFF);
    ecrireOctet(e, (int)v & 0xFF);
}

static void ecrireTexte(Ecriture *e, const char *s, int taille)
{
    for (int k = 0; k < taille; k++) ecrireOctet(e, (unsigned char)s[k]);
}

/** Curseur de lecture dans un message reçu. */
typedef struct {
    const unsigned char *o;
    int n, pos;
} Lecture;

static int lireOctet(Lecture *l)
{
    return l->pos < l->n ? l->o[l->pos++] : 0;
}

static unsigned lireEntier(Lecture *l)
{
    unsigned v = 0;
    for (int k = 0; k < 4; k++) v = (v << 8) | (unsigned)lireOctet(l);
    return v;
}

static void lireTexte(Lecture *l, char *s, int taille)
{
    for (int k = 0; k < taille; k++) s[k] = (char)lireOctet(l);
    s[taille - 1] = '\0';
}

/** Ramène une valeur dans [min, max]. */
static int borner(int v, int min, int max)
{
    return v < min ? min : (v > max ? max : v);
}

/** Écrit les champs du héros utiles au duel. */
static void ecrireHeros(Ecriture *e, const Personnage *p)
{
    ecrireTexte(e, p->nom, NOM_MAX);
    ecrireEntier(e, (unsigned)p->classe);
    ecrireEntier(e, (unsigned)p->element);
    ecrireEntier(e, (unsigned)p->niveau);
    ecrireEntier(e, (unsigned)p->pvMax);
    ecrireEntier(e, (unsigned)p->atk);
    ecrireEntier(e, (unsigned)p->def);
    ecrireEntier(e, (unsigned)p->vit);
    ecrireTexte(e, p->arme.nom, NOM_OBJET_MAX);
    ecrireEntier(e, (unsigned)p->arme.type);
    ecrireEntier(e, (unsigned)p->arme.valeur);
    const Apparence *a = &p->apparence;
    const unsigned char champs[] = { a->plumage, a->coupe, a->couleurCheveux, a->couleurYeux, a->regard,
                                     a->tenue, a->tenue2, a->coiffe, a->pilosite, a->marque, a->cape,
                                     a->aura, a->corpulence };
    for (size_t k = 0; k < sizeof champs; k++) ecrireOctet(e, champs[k]);
}

/** Lit un héros et corrige toute valeur hors limites (un message abîmé
 *  ne doit pas pouvoir faire planter l'affichage). */
static void lireHeros(Lecture *l, Personnage *p)
{
    memset(p, 0, sizeof *p);
    lireTexte(l, p->nom, NOM_MAX);
    p->classe = (enum Classe)borner((int)lireEntier(l), 0, NB_CLASSES - 1);
    p->element = (enum Element)borner((int)lireEntier(l), 0, NB_ELEMENTS - 1);
    p->niveau = borner((int)lireEntier(l), 1, 999);
    p->pvMax = borner((int)lireEntier(l), 1, 99999);
    p->atk = borner((int)lireEntier(l), 0, 9999);
    p->def = borner((int)lireEntier(l), 0, 9999);
    p->vit = borner((int)lireEntier(l), 1, 9999);
    lireTexte(l, p->arme.nom, NOM_OBJET_MAX);
    p->arme.type = lireEntier(l) == ARME ? ARME : SOIN;
    p->arme.valeur = borner((int)lireEntier(l), 0, 9999);
    p->pv = p->pvMax;

    Apparence *a = &p->apparence;
    a->plumage = (unsigned char)(lireOctet(l) % NB_PLUMAGES);
    a->coupe = (unsigned char)(lireOctet(l) % NB_COUPES);
    a->couleurCheveux = (unsigned char)(lireOctet(l) % NB_COULEURS);
    a->couleurYeux = (unsigned char)(lireOctet(l) % NB_COULEURS);
    a->regard = (unsigned char)(lireOctet(l) % NB_REGARDS);
    a->tenue = (unsigned char)(lireOctet(l) % NB_COULEURS);
    a->tenue2 = (unsigned char)(lireOctet(l) % NB_COULEURS);
    a->coiffe = (unsigned char)(lireOctet(l) % NB_COIFFES);
    a->pilosite = (unsigned char)(lireOctet(l) % NB_PILOSITES);
    a->marque = (unsigned char)(lireOctet(l) % NB_MARQUES);
    a->cape = (unsigned char)(lireOctet(l) % 2);
    a->aura = (unsigned char)(lireOctet(l) % (NB_COULEURS + 1));
    a->corpulence = (unsigned char)(lireOctet(l) % NB_CORPULENCES);
}

void reseauPreparerHeros(const Personnage *source, Personnage *copie)
{
    Ecriture e = { .n = 0 };
    ecrireHeros(&e, source);
    Lecture l = { e.o, e.n, 0 };
    lireHeros(&l, copie);
}

/* ------------------------------------------------------------------ */
/*  Connexion                                                         */
/* ------------------------------------------------------------------ */

void reseauInit(Reseau *r)
{
    memset(r, 0, sizeof *r);
    r->ecoute = r->sock = -1;
    r->etat = RESEAU_INACTIF;
}

void reseauFermer(Reseau *r)
{
    if (r->sock >= 0) close(r->sock);
    if (r->ecoute >= 0) close(r->ecoute);
    reseauInit(r);
}

/** Passe en erreur (et ferme les sockets) avec un message lisible. Les
 *  messages déjà reçus restent dans la file (« je quitte » juste avant la
 *  fermeture, par exemple). */
static void echec(Reseau *r, const char *raison)
{
    if (r->sock >= 0) close(r->sock);
    if (r->ecoute >= 0) close(r->ecoute);
    r->sock = r->ecoute = -1;
    r->nbRecu = 0;
    snprintf(r->erreur, sizeof r->erreur, "%s", raison);
    r->etat = RESEAU_ERREUR;
}

static int nonBloquant(int s)
{
    int f = fcntl(s, F_GETFL, 0);
    return f >= 0 && fcntl(s, F_SETFL, f | O_NONBLOCK) == 0;
}

/** Réglages de la socket reliée à l'autre joueur. */
static void reglerSocket(int s)
{
    int un = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &un, sizeof un); /* pas d'attente pour les petits messages */
    nonBloquant(s);
}

int reseauHeberger(Reseau *r)
{
    reseauFermer(r);
    r->hote = 1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        echec(r, "Impossible de créer la connexion.");
        return 0;
    }
    int un = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &un, sizeof un);
    struct sockaddr_in adr;
    memset(&adr, 0, sizeof adr);
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = htonl(INADDR_ANY);
    adr.sin_port = htons(RESEAU_PORT);
    if (bind(s, (struct sockaddr *)&adr, sizeof adr) < 0 || listen(s, 1) < 0 || !nonBloquant(s)) {
        close(s);
        char msg[96];
        snprintf(msg, sizeof msg, "Le port %d est déjà utilisé.", RESEAU_PORT);
        echec(r, msg);
        return 0;
    }
    r->ecoute = s;
    r->etat = RESEAU_ATTENTE;
    return 1;
}

int reseauRejoindre(Reseau *r, const char *adresse)
{
    reseauFermer(r);
    r->hote = 0;

    /* « ip » ou « ip:port » (espaces ignorés) */
    char hote[64];
    int k = 0;
    for (const char *c = adresse; *c != '\0' && k < (int)sizeof hote - 1; c++) {
        if (*c != ' ') hote[k++] = *c;
    }
    hote[k] = '\0';
    char port[8];
    snprintf(port, sizeof port, "%d", RESEAU_PORT);
    char *deuxPoints = strchr(hote, ':');
    if (deuxPoints != NULL) {
        *deuxPoints = '\0';
        snprintf(port, sizeof port, "%.7s", deuxPoints + 1);
    }
    if (hote[0] == '\0') {
        echec(r, "Entrez l'adresse IP de l'hôte.");
        return 0;
    }

    struct addrinfo indices, *res = NULL;
    memset(&indices, 0, sizeof indices);
    indices.ai_family = AF_INET;
    indices.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hote, port, &indices, &res) != 0 || res == NULL) {
        echec(r, "Adresse invalide.");
        return 0;
    }
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0 || !nonBloquant(s)) {
        if (s >= 0) close(s);
        freeaddrinfo(res);
        echec(r, "Impossible de créer la connexion.");
        return 0;
    }
    int rc = connect(s, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc < 0 && errno != EINPROGRESS) {
        close(s);
        echec(r, "Connexion refusée.");
        return 0;
    }
    r->sock = s;
    r->etat = RESEAU_CONNEXION;
    debutConnexion = time(NULL);
    return 1;
}

/** Découpe les messages complets du tampon de réception. */
static void decouper(Reseau *r)
{
    int pos = 0;
    while (r->nbRecu - pos >= 2) {
        int longueur = (r->recu[pos] << 8) | r->recu[pos + 1];
        if (longueur < 1 || longueur > RESEAU_TAMPON - 2) {
            echec(r, "Message invalide reçu.");
            return;
        }
        if (r->nbRecu - pos - 2 < longueur) break; /* message pas encore complet */
        Lecture l = { r->recu + pos + 2, longueur, 0 };
        pos += 2 + longueur;

        Message m;
        memset(&m, 0, sizeof m);
        m.type = (enum TypeMessage)lireOctet(&l);
        switch (m.type) {
        case MSG_BONJOUR:
            if (lireOctet(&l) != 'D' || lireOctet(&l) != 'U' || lireOctet(&l) != 'C' || lireOctet(&l) != 'K') {
                echec(r, "Ce n'est pas un joueur de Duckgeon.");
                return;
            }
            if (lireOctet(&l) != VERSION_PROTOCOLE) {
                echec(r, "L'autre joueur n'a pas la même version du jeu.");
                return;
            }
            m.graine = lireEntier(&l);
            lireHeros(&l, &m.heros);
            break;
        case MSG_ACTION:
            m.manche = (int)lireEntier(&l);
            m.tour = (int)lireEntier(&l);
            m.lanceur = (int)lireEntier(&l);
            m.action = (int)lireEntier(&l);
            m.cible = (int)lireEntier(&l);
            m.controle = lireEntier(&l);
            break;
        case MSG_REVANCHE:
            m.manche = (int)lireEntier(&l);
            break;
        case MSG_QUITTER:
            break;
        default:
            continue; /* type inconnu : ignoré */
        }
        if (r->nbFile < RESEAU_FILE_MAX) {
            r->file[r->nbFile++] = m;
        }
    }
    memmove(r->recu, r->recu + pos, (size_t)(r->nbRecu - pos));
    r->nbRecu -= pos;
}

void reseauMaj(Reseau *r)
{
    if (r->etat == RESEAU_ATTENTE) {
        int s = accept(r->ecoute, NULL, NULL);
        if (s >= 0) {
            reglerSocket(s);
            close(r->ecoute); /* un seul adversaire */
            r->ecoute = -1;
            r->sock = s;
            r->etat = RESEAU_CONNECTE;
        }
        return;
    }

    if (r->etat == RESEAU_CONNEXION) {
        struct pollfd pf = { r->sock, POLLOUT, 0 };
        if (poll(&pf, 1, 0) > 0) {
            int err = 0;
            socklen_t lg = sizeof err;
            getsockopt(r->sock, SOL_SOCKET, SO_ERROR, &err, &lg);
            if (err != 0) {
                echec(r, err == ECONNREFUSED ? "Personne n'héberge à cette adresse."
                                             : "Impossible de joindre cette adresse.");
                return;
            }
            reglerSocket(r->sock);
            r->etat = RESEAU_CONNECTE;
        } else if (time(NULL) - debutConnexion > DELAI_CONNEXION) {
            echec(r, "Pas de réponse (adresse ou pare-feu ?).");
        }
        return;
    }

    if (r->etat != RESEAU_CONNECTE) return;
    for (;;) {
        if (r->nbRecu >= RESEAU_TAMPON) {
            echec(r, "Message invalide reçu.");
            return;
        }
        ssize_t n = recv(r->sock, r->recu + r->nbRecu, (size_t)(RESEAU_TAMPON - r->nbRecu), 0);
        if (n > 0) {
            r->nbRecu += (int)n;
            decouper(r);
            if (r->etat != RESEAU_CONNECTE) return;
        } else if (n == 0) {
            echec(r, "L'adversaire s'est déconnecté.");
            return;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                echec(r, "La connexion a été coupée.");
            }
            return;
        }
    }
}

const Message *reseauPremier(const Reseau *r)
{
    return r->nbFile > 0 ? &r->file[0] : NULL;
}

void reseauRetirer(Reseau *r)
{
    if (r->nbFile > 0) {
        memmove(&r->file[0], &r->file[1], sizeof r->file[0] * (size_t)(r->nbFile - 1));
        r->nbFile--;
    }
}

/* ------------------------------------------------------------------ */
/*  Envoi                                                             */
/* ------------------------------------------------------------------ */

/** Envoie un message préparé (sa longueur est ajoutée devant). */
static int envoyer(Reseau *r, const Ecriture *e)
{
    if (r->etat != RESEAU_CONNECTE) return 0;
    unsigned char trame[2 + sizeof e->o];
    trame[0] = (unsigned char)(e->n >> 8);
    trame[1] = (unsigned char)(e->n & 0xFF);
    memcpy(trame + 2, e->o, (size_t)e->n);
    int total = e->n + 2, fait = 0;
    while (fait < total) {
        ssize_t n = send(r->sock, trame + fait, (size_t)(total - fait), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pf = { r->sock, POLLOUT, 0 };
                poll(&pf, 1, 200); /* tampon plein (très rare) : on patiente un peu */
                continue;
            }
            echec(r, "La connexion a été coupée.");
            return 0;
        }
        fait += (int)n;
    }
    return 1;
}

int reseauEnvoyerBonjour(Reseau *r, const Personnage *heros, unsigned graine)
{
    Ecriture e = { .n = 0 };
    ecrireOctet(&e, MSG_BONJOUR);
    ecrireTexte(&e, "DUCK", 4);
    ecrireOctet(&e, VERSION_PROTOCOLE);
    ecrireEntier(&e, graine);
    ecrireHeros(&e, heros);
    return envoyer(r, &e);
}

int reseauEnvoyerAction(Reseau *r, int manche, int tour, int lanceur, int action, int cible,
                        unsigned controle)
{
    Ecriture e = { .n = 0 };
    ecrireOctet(&e, MSG_ACTION);
    ecrireEntier(&e, (unsigned)manche);
    ecrireEntier(&e, (unsigned)tour);
    ecrireEntier(&e, (unsigned)lanceur);
    ecrireEntier(&e, (unsigned)action);
    ecrireEntier(&e, (unsigned)cible);
    ecrireEntier(&e, controle);
    return envoyer(r, &e);
}

int reseauEnvoyerRevanche(Reseau *r, int manche)
{
    Ecriture e = { .n = 0 };
    ecrireOctet(&e, MSG_REVANCHE);
    ecrireEntier(&e, (unsigned)manche);
    return envoyer(r, &e);
}

void reseauEnvoyerQuitter(Reseau *r)
{
    Ecriture e = { .n = 0 };
    ecrireOctet(&e, MSG_QUITTER);
    envoyer(r, &e);
}

int reseauAdressesLocales(char *buf, int taille)
{
    buf[0] = '\0';
    struct ifaddrs *liste = NULL;
    if (getifaddrs(&liste) != 0) return 0;
    int n = 0;
    for (struct ifaddrs *i = liste; i != NULL; i = i->ifa_next) {
        if (i->ifa_addr == NULL || i->ifa_addr->sa_family != AF_INET) continue;
        if ((i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP)) continue;
        /* ponts virtuels (Docker, machines virtuelles) : inutiles pour l'autre joueur */
        const char *nom = i->ifa_name;
        if (strncmp(nom, "docker", 6) == 0 || strncmp(nom, "br-", 3) == 0 || strncmp(nom, "veth", 4) == 0
            || strncmp(nom, "virbr", 5) == 0) {
            continue;
        }
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, ip, sizeof ip);
        size_t lg = strlen(buf);
        snprintf(buf + lg, (size_t)taille - lg, "%s%s", n > 0 ? "  ·  " : "", ip);
        n++;
    }
    freeifaddrs(liste);
    return n;
}
