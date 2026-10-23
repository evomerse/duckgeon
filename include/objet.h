/**
 * @file objet.h
 * @brief Objets ramassables par les personnages (soins et armes).
 */
#ifndef OBJET_H
#define OBJET_H

#define NOM_OBJET_MAX 30

/** Catégories d'objets. */
enum TypeObjet { SOIN, ARME };

/** Un objet de l'inventaire. */
typedef struct Objet {
    char nom[NOM_OBJET_MAX]; /**< Nom affiché ("" = emplacement vide)       */
    enum TypeObjet type;     /**< SOIN : rend des PV / ARME : bonus d'ATK    */
    int valeur;              /**< PV rendus (SOIN) ou bonus d'attaque (ARME) */
} Objet;

/** Retourne un objet tiré au hasard dans la table de butin. */
Objet objetAleatoire(void);

/** Retourne l'objet de départ donné à chaque nouveau personnage. */
Objet objetDepart(void);

/** Affiche un objet sur une ligne (sans retour à la ligne final). */
void afficherObjet(const Objet *o);

#endif /* OBJET_H */
