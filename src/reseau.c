/**
 * @file reseau.c
 * @brief Connexion TCP du duel en ligne (sockets non bloquants).
 *
 * Format d'un message : 2 octets de longueur (taille de la suite), 1 octet
 * de type, puis les données. Les entiers sont écrits octet par octet
 * (gros-boutiste) et les héros champ par champ : le format ne dépend ni du
 * compilateur ni de la machine (un PC Windows peut jouer contre un Linux).
 *
 * Les sockets de Windows (Winsock) et de Linux/macOS (POSIX) s'utilisent
 * presque pareil ; les quelques différences sont regroupées juste en
 * dessous (fermeture, mode non bloquant, codes d'erreur, adresses locales).
 */
#ifdef _WIN32
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600 /* Vista et plus : inet_ntop */
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#  include <wchar.h>
#  include <stdlib.h>
typedef SOCKET Sock;
#  define SOCK_INVALIDE   INVALID_SOCKET
#  define fermerSock      closesocket
#  define derniereErreur  WSAGetLastError
#  define ERR_EN_COURS(e) ((e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS)
#  define ERR_ATTENDRE(e) ((e) == WSAEWOULDBLOCK || (e) == WSAEINTR)
#  define ERR_REFUSEE     WSAECONNREFUSED
#  define SANS_SIGNAL     0
#else
#  define _POSIX_C_SOURCE 200809L
#  define _DEFAULT_SOURCE /* getifaddrs, IFF_LOOPBACK */
#  include <errno.h>
#  include <fcntl.h>
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <arpa/inet.h>
typedef int Sock;
#  define SOCK_INVALIDE   (-1)
#  define fermerSock      close
#  define derniereErreur() errno
#  define ERR_EN_COURS(e) ((e) == EINPROGRESS)
#  define ERR_ATTENDRE(e) ((e) == EAGAIN || (e) == EWOULDBLOCK || (e) == EINTR)
#  define ERR_REFUSEE     ECONNREFUSED
#  ifdef MSG_NOSIGNAL
#    define SANS_SIGNAL   MSG_NOSIGNAL /* pas de SIGPIPE si l'autre est parti */
#  else
#    define SANS_SIGNAL   0
#  endif
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "reseau.h"

#define VERSION_PROTOCOLE 2
#define DELAI_CONNEXION   10 /* secondes avant d'abandonner une connexion */

static time_t debutConnexion;

/** Windows : Winsock doit être démarré une fois avant tout appel réseau. */
static int demarrerSockets(void)
{
#ifdef _WIN32
    static int demarre = 0;
    if (!demarre) {
        WSADATA donnees;
        if (WSAStartup(MAKEWORD(2, 2), &donnees) != 0) return 0;
        demarre = 1;
    }
#endif
    return 1;
}

static int nonBloquant(Sock s)
{
#ifdef _WIN32
    u_long oui = 1;
    return ioctlsocket(s, FIONBIO, &oui) == 0;
#else
    int f = fcntl(s, F_GETFL, 0);
    return f >= 0 && fcntl(s, F_SETFL, f | O_NONBLOCK) == 0;
#endif
}

/** Attend au plus `ms` millisecondes que la socket soit prête en écriture.
 *  @return 1 si prête, -1 en cas d'erreur signalée, 0 sinon. */
static int pretEcriture(Sock s, int ms)
{
    fd_set ecr, exc;
    FD_ZERO(&ecr);
    FD_ZERO(&exc);
    FD_SET(s, &ecr);
    FD_SET(s, &exc); /* Windows signale un échec de connexion ici */
    struct timeval delai = { ms / 1000, (ms % 1000) * 1000 };
    if (select((int)s + 1, NULL, &ecr, &exc, &delai) <= 0) return 0;
    return FD_ISSET(s, &exc) ? -1 : 1;
}

/* ------------------------------------------------------------------ */
/*  Écriture / lecture des données                                    */
/* ------------------------------------------------------------------ */

/** Tampon d'écriture d'un message. */
typedef struct {
    unsigned char o[1024];
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

/** Ferme les sockets ouvertes (sans toucher à la file des messages). */
static void fermerSockets(Reseau *r)
{
    if (r->sock != -1) fermerSock((Sock)r->sock);
    if (r->ecoute != -1) fermerSock((Sock)r->ecoute);
    r->sock = r->ecoute = -1;
}

void reseauFermer(Reseau *r)
{
    fermerSockets(r);
    reseauInit(r);
}

/** Passe en erreur avec un message lisible. Les messages déjà reçus
 *  restent dans la file (« je quitte » juste avant la fermeture...). */
static void echec(Reseau *r, const char *raison)
{
    fermerSockets(r);
    r->nbRecu = 0;
    snprintf(r->erreur, sizeof r->erreur, "%s", raison);
    r->etat = RESEAU_ERREUR;
}

/** Réglages de la socket reliée à l'autre joueur. */
static void reglerSocket(Sock s)
{
    int un = 1;
    /* pas d'attente pour regrouper les petits messages */
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&un, sizeof un);
    nonBloquant(s);
}

int reseauHeberger(Reseau *r)
{
    reseauFermer(r);
    r->hote = 1;
    if (!demarrerSockets()) {
        echec(r, "Le réseau n'est pas disponible.");
        return 0;
    }
    Sock s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCK_INVALIDE) {
        echec(r, "Impossible de créer la connexion.");
        return 0;
    }
#ifndef _WIN32
    int un = 1; /* relancer tout de suite après une partie */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&un, sizeof un);
#endif
    struct sockaddr_in adr;
    memset(&adr, 0, sizeof adr);
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = htonl(INADDR_ANY);
    adr.sin_port = htons(RESEAU_PORT);
    if (bind(s, (struct sockaddr *)&adr, sizeof adr) != 0 || listen(s, 1) != 0 || !nonBloquant(s)) {
        fermerSock(s);
        char msg[96];
        snprintf(msg, sizeof msg, "Le port %d est déjà utilisé.", RESEAU_PORT);
        echec(r, msg);
        return 0;
    }
    r->ecoute = (intptr_t)s;
    r->etat = RESEAU_ATTENTE;
    return 1;
}

int reseauRejoindre(Reseau *r, const char *adresse)
{
    reseauFermer(r);
    r->hote = 0;
    if (!demarrerSockets()) {
        echec(r, "Le réseau n'est pas disponible.");
        return 0;
    }

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
    Sock s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == SOCK_INVALIDE || !nonBloquant(s)) {
        if (s != SOCK_INVALIDE) fermerSock(s);
        freeaddrinfo(res);
        echec(r, "Impossible de créer la connexion.");
        return 0;
    }
    int rc = connect(s, res->ai_addr, (int)res->ai_addrlen);
    int err = derniereErreur();
    freeaddrinfo(res);
    if (rc != 0 && !ERR_EN_COURS(err)) {
        fermerSock(s);
        echec(r, "Connexion refusée.");
        return 0;
    }
    r->sock = (intptr_t)s;
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
            m.format.graine = lireEntier(&l);
            m.format.taille = borner(lireOctet(&l), 1, RESEAU_HEROS_MAX);
            m.format.niveau = borner(lireOctet(&l), 0, 255);
            m.nbHeros = borner(lireOctet(&l), 1, RESEAU_HEROS_MAX);
            for (int k = 0; k < m.nbHeros; k++) lireHeros(&l, &m.heros[k]);
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
        Sock s = accept((Sock)r->ecoute, NULL, NULL);
        if (s != SOCK_INVALIDE) {
            reglerSocket(s);
            fermerSock((Sock)r->ecoute); /* un seul adversaire */
            r->ecoute = -1;
            r->sock = (intptr_t)s;
            r->etat = RESEAU_CONNECTE;
        }
        return;
    }

    if (r->etat == RESEAU_CONNEXION) {
        Sock s = (Sock)r->sock;
        int pret = pretEcriture(s, 0);
        if (pret != 0) {
            int err = 0;
            socklen_t lg = sizeof err;
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &lg);
            if (pret < 0 || err != 0) {
                echec(r, err == ERR_REFUSEE ? "Personne n'héberge à cette adresse."
                                            : "Impossible de joindre cette adresse.");
                return;
            }
            reglerSocket(s);
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
        int n = (int)recv((Sock)r->sock, (char *)r->recu + r->nbRecu, RESEAU_TAMPON - r->nbRecu, 0);
        if (n > 0) {
            r->nbRecu += n;
            decouper(r);
            if (r->etat != RESEAU_CONNECTE) return;
        } else if (n == 0) {
            echec(r, "L'adversaire s'est déconnecté.");
            return;
        } else {
            if (!ERR_ATTENDRE(derniereErreur())) {
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
    int total = e->n + 2, fait = 0, essais = 0;
    while (fait < total) {
        int n = (int)send((Sock)r->sock, (const char *)trame + fait, total - fait, SANS_SIGNAL);
        if (n < 0) {
            /* tampon plein (très rare pour d'aussi petits messages) : on patiente un peu */
            if (ERR_ATTENDRE(derniereErreur()) && essais++ < 10) {
                pretEcriture((Sock)r->sock, 200);
                continue;
            }
            echec(r, "La connexion a été coupée.");
            return 0;
        }
        fait += n;
    }
    return 1;
}

int reseauEnvoyerBonjour(Reseau *r, const Personnage heros[], int n, const FormatDuel *format)
{
    n = borner(n, 1, RESEAU_HEROS_MAX);
    Ecriture e = { .n = 0 };
    ecrireOctet(&e, MSG_BONJOUR);
    ecrireTexte(&e, "DUCK", 4);
    ecrireOctet(&e, VERSION_PROTOCOLE);
    ecrireEntier(&e, format->graine);
    ecrireOctet(&e, borner(format->taille, 1, RESEAU_HEROS_MAX));
    ecrireOctet(&e, borner(format->niveau, 0, 255));
    ecrireOctet(&e, n);
    for (int k = 0; k < n; k++) ecrireHeros(&e, &heros[k]);
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

/** Ajoute une adresse à la liste affichée. */
static void ajouterAdresse(char *buf, int taille, const struct sockaddr_in *adr, int *n)
{
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, (void *)&adr->sin_addr, ip, sizeof ip) == NULL) return;
    size_t lg = strlen(buf);
    snprintf(buf + lg, (size_t)taille - lg, "%s%s", *n > 0 ? "  ·  " : "", ip);
    (*n)++;
}

int reseauAdressesLocales(char *buf, int taille)
{
    buf[0] = '\0';
    int n = 0;
    if (!demarrerSockets()) return 0;
#ifdef _WIN32
    ULONG lg = 16 * 1024;
    IP_ADAPTER_ADDRESSES *liste = malloc(lg);
    if (liste == NULL) return 0;
    ULONG options = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET, options, NULL, liste, &lg) == ERROR_BUFFER_OVERFLOW) {
        free(liste);
        liste = malloc(lg);
        if (liste == NULL || GetAdaptersAddresses(AF_INET, options, NULL, liste, &lg) != NO_ERROR) {
            free(liste);
            return 0;
        }
    }
    for (IP_ADAPTER_ADDRESSES *a = liste; a != NULL; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        /* cartes virtuelles de WSL / Hyper-V : inutiles pour l'autre joueur */
        if (a->FriendlyName != NULL && wcsncmp(a->FriendlyName, L"vEthernet", 9) == 0) continue;
        for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u != NULL; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family == AF_INET) {
                ajouterAdresse(buf, taille, (const struct sockaddr_in *)u->Address.lpSockaddr, &n);
            }
        }
    }
    free(liste);
#else
    struct ifaddrs *liste = NULL;
    if (getifaddrs(&liste) != 0) return 0;
    for (struct ifaddrs *i = liste; i != NULL; i = i->ifa_next) {
        if (i->ifa_addr == NULL || i->ifa_addr->sa_family != AF_INET) continue;
        if ((i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP)) continue;
        /* ponts virtuels (Docker, machines virtuelles) : inutiles pour l'autre joueur */
        const char *nom = i->ifa_name;
        if (strncmp(nom, "docker", 6) == 0 || strncmp(nom, "br-", 3) == 0 || strncmp(nom, "veth", 4) == 0
            || strncmp(nom, "virbr", 5) == 0) {
            continue;
        }
        ajouterAdresse(buf, taille, (const struct sockaddr_in *)i->ifa_addr, &n);
    }
    freeifaddrs(liste);
#endif
    return n;
}
