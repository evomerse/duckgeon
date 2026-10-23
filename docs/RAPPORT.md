# Rapport technique - Le Donjon / Duckgeon (Parties 1 et 2 + bonus graphique)

> Document de rendu (choix réalisés, questions de réflexion, tests). Pour
> installer et jouer, voir le [README](../README.md).

## Compilation et exécution

```bash
make            # compile les deux versions -> ./donjon et ./duckgeon
make console    # version console seule (aucune dépendance)
make gui        # version graphique seule (SDL2 + SDL2_ttf)
make run        # lance la version console
make run-gui    # lance la version graphique
make valgrind   # version console sous valgrind (fuites mémoire)
make clean
make reset      # make clean + supprime la sauvegarde donjon.sav
```

Dépendances de la version graphique : `SDL2` et `SDL2_ttf` (paquets
`SDL2-devel SDL2_ttf-devel` sous Fedora, `libsdl2-dev libsdl2-ttf-dev` sous
Debian/Ubuntu). La police est cherchée parmi les polices du système
(Montserrat, DejaVu, Liberation, Arial) ; on peut en imposer une avec
`assets/police.ttf` ou la variable `DONJON_POLICE=/chemin/police.ttf`.

## Organisation

```
include/main.h         prototypes du menu console
include/donjon.h       structure Donjon (tableau dynamique), partagée console/graphique
include/personnage.h   structure Personnage, enum Classe/Element, Apparence, prototypes
include/saisie.h       saisies clavier sécurisées
include/sauvegarde.h   sauvegarder / charger (fichier binaire)
include/objet.h        structure Objet (soin / arme), table de butin
include/inventaire.h   ajout / retrait / utilisation d'objets
include/combat.h       moteur de combat en équipe, éléments, bestiaire (sans SDL)
include/console_jeu.h  le jeu complet dans le terminal
include/gui*.h         interface SDL (rendu, sprites, écrans)
src/personnage.c       creerPersonnage, afficherPersonnage, attaquer, XP, 8 classes
src/donjon.c           malloc / realloc / free du tableau de personnages
src/objet.c            butin aléatoire, affichage d'un objet
src/inventaire.c       gestion du sac et de l'arme équipée
src/saisie.c           lireLigne / lireEntier (fgets + strtol)
src/sauvegarde.c       fwrite / fread du tableau, vérification du fichier
src/main.c             menu interactif console
src/console_jeu.c      exploration, duel en équipe, bestiaire, boutique... en texte
src/combat.c           tour par tour façon Honkai: Star Rail (sans SDL, testable seul)
src/gui.c              fenêtre, boucle SDL_PollEvent, texte (cache), widgets
src/gui_formes.c       formes vectorielles (SDL_RenderGeometry)
src/gui_sprite.c       dessin des héros (selon leur apparence) et des monstres
src/gui_main.c         main graphique, écran titre, transitions
src/gui_heros.c        écrans « Mes héros » et « Préparer l'expédition »
src/gui_editeur.c      éditeur de personnage
src/gui_combat.c       écran de combat
src/gui_bestiaire.c    bestiaire et table des éléments
src/gui_en_ligne.c     duel en ligne : héberger ou rejoindre par IP
src/reseau.c           sockets TCP non bloquants, messages du duel en ligne
```

La logique métier (`personnage`, `objet`, `inventaire`, `donjon`,
`sauvegarde`, `boutique`, `combat`) est compilée telle quelle dans les deux
exécutables : l'interface graphique ne fait que l'afficher et la piloter, et
le terminal peut jouer à tout le jeu avec le même moteur.

## Choix réalisés

- **Statistiques par classe** dans une table indexée par `enum Classe`
  (`STATS_CLASSE`) : l'équilibrage tient en un seul endroit.

  | Classe       | PV  | ATK | DEF | VIT | Particularité (console)                    |
  |--------------|-----|-----|-----|-----|--------------------------------------------|
  | Guerrier     | 120 | 14  | 8   | 95  | Gros PV, grosse défense                    |
  | Mage         | 80  | 20  | 3   | 100 | Ignore la moitié de la défense adverse     |
  | Voleur       | 95  | 16  | 5   | 115 | 25 % de chance de coup critique (x2)       |
  | Paladin      | 130 | 12  | 10  | 90  | Ajoute la moitié de sa DEF à ses coups     |
  | Archer       | 85  | 18  | 4   | 110 | 15 % de critique, 20 % de tir double       |
  | Prêtre       | 90  | 13  | 5   | 98  | Récupère 10 % de ses PV max à chaque coup  |
  | Nécromancien | 85  | 18  | 4   | 102 | Se soigne de la moitié des dégâts infligés |
  | Barde        | 90  | 14  | 5   | 108 | Dégâts aléatoires de 50 % à 160 %          |

  Les cinq dernières classes sont des ajouts. `enum Classe` se termine par
  `NB_CLASSES`, ce qui permet de boucler sur toutes les classes et de
  valider une saisie sans connaître leur nombre.

- **Dégâts** = `atk - def` (avec les bonus de classe), au minimum 1 : un
  combat finit toujours, même entre deux personnages très défensifs.
  Les PV ne descendent jamais sous 0.
- **Tableau dynamique** : `malloc` d'une capacité initiale de 4, puis
  doublement par `realloc` quand il est plein. Le résultat de `realloc`
  passe par un pointeur temporaire pour ne pas perdre l'ancien bloc en cas
  d'échec. Un seul `free` à la fin, puis le pointeur est remis à `NULL`.
- **Saisies robustes** : `fgets` + `strtol` au lieu de `scanf`, donc une
  saisie comme `abc` est refusée proprement au lieu de faire boucler le
  programme. La fin d'entrée (Ctrl+D) quitte proprement en libérant la mémoire.
- **Combat** : vérifie qu'il y a au moins 2 personnages vivants, interdit
  de s'attaquer soi-même ou d'utiliser un personnage K.O.
- **Fonctionnalités supplémentaires** : capacités spéciales par classe,
  mode « combat jusqu'au K.O. » tour par tour, barre de vie texte dans
  l'affichage.

## Bonus (exercices supplémentaires 1.9)

### Système d'expérience et de niveaux
- Mettre un adversaire K.O. rapporte `30 + 10 × niveau de la cible` XP.
- Il faut `50 × niveau actuel` XP pour monter de niveau. Chaque niveau donne
  +10 PV max, +2 ATK, +1 DEF et soigne entièrement. Un gros gain peut faire
  monter de plusieurs niveaux d'un coup (boucle `while`).
- Les PV max ne dépendent plus seulement de la classe : ils sont stockés dans
  le champ `pvMax` de la structure.

### Inventaire (structure `Objet`)
- `Objet` = nom + type (`SOIN` / `ARME`) + valeur (module `objet.h/.c`).
- Chaque personnage a un sac de 5 objets et une arme équipée. Il commence avec
  une potion et gagne un objet aléatoire à chaque victoire.
- Menu **6. Inventaire** : utiliser une potion (soigne, peut **ranimer** un
  personnage K.O.), équiper une arme (bonus d'ATK, l'ancienne retourne dans le
  sac), jeter un objet.
- Le sac est un **tableau de taille fixe** dans la structure, sans pointeur.
  Un `Personnage` reste donc un bloc contigu qu'on pourra écrire tel quel avec
  `fwrite` dans la Partie 2. Avec un pointeur, on écrirait une adresse mémoire
  sans valeur une fois le programme relancé.

## Partie 2 : sauvegarde (`sauvegarde.h/.c`)

- Menu console final : **4. Sauvegarder la partie**, **5. Charger une partie**
  (fichier `donjon.sav` dans le dossier courant).
- La version graphique charge `donjon.sav` au démarrage (héros de départ
  seulement s'il n'existe pas) et sauvegarde automatiquement à chaque
  changement d'écran et en quittant. Console et interface partagent le
  même fichier.
- Format binaire : signature `"DONJ"`, numéro de version, nombre `n`, puis
  les `n` structures `Personnage` écrites d'un seul `fwrite` (possible car la
  structure ne contient aucun pointeur).
- `sauvegarder` écrit dans `donjon.sav.tmp` puis le renomme : une écriture
  ratée (disque plein...) ne détruit pas l'ancienne sauvegarde.
- `charger` lit dans un **nouveau** bloc `malloc` ; ce n'est qu'une fois la
  lecture réussie que l'ancien tableau est libéré et remplacé. En cas
  d'erreur, le donjon reste intact.
- Gestion des erreurs : `perror` après `fopen`/`malloc`/`fwrite`/`rename`,
  signature, version et nombre vérifiés, fichier tronqué détecté, et chaque
  personnage lu est contrôlé (classe, élément, inventaire, nom terminé par
  `'\0'`) pour qu'un fichier abîmé ne fasse jamais sortir d'un tableau.

## Points de statistiques et foie gras

- **Points de stats** (champ `pointsStats` de `Personnage`) : +2 à chaque
  montée de niveau (`gagnerXp`), +1 par victoire (K.O. en console, étage
  nettoyé en graphique), +2 de plus si l'étage avait un boss. Ils se
  dépensent avec `ameliorerStat` (+8 PV, +1 ATK, +1 DEF ou +3 VIT) : menu
  console **7. Ameliorer un personnage**, bouton « Améliorer » dans *Mes
  héros*. Le champ `victoires` compte les combats gagnés.
- **Foie gras** (module `boutique.h/.c`, sans SDL) : monnaie du joueur
  gagnée en nettoyant un étage (`4 + 3 × étage`, doublé avec un boss,
  +25 la première fois). Il sert à acheter 8 cosmétiques (5 coiffes, 3
  plumages) utilisables par tous les héros. Le foie gras, les cosmétiques
  achetés (masque de bits) et les étages déjà terminés sont dans la
  structure `Progression`, sauvegardée dans `progression.sav`.
- **Compatibilité des sauvegardes** : les nouveaux champs sont ajoutés en
  fin de structure et le format passe en version 2 (avec la taille d'un
  enregistrement dans l'en-tête). Un fichier de version 1 est relu
  enregistrement par enregistrement, les champs manquants valant 0.

## Bonus : interface graphique SDL (`./duckgeon`)

Dans la version graphique, **les héros sont des canards** : plumage,
houppette, bec, ailes qui tiennent l'arme, pattes palmées... avec tenue,
coiffe, cape et même barbe selon la personnalisation.

### Écrans
- **Titre** : les canards posent sur une estrade animée.
- **Mes héros** : liste défilante, fiche détaillée (statistiques, XP,
  capacités, arme, sac). Clic sur un objet = l'utiliser ou l'équiper
  (`utiliserObjet`, la même fonction que la console), clic droit = le
  jeter. « Se reposer au camp » soigne tout le monde.
- **Éditeur** (création ou personnalisation) :
  - *Identité* : nom (saisie texte UTF-8), 8 classes, élément parmi 7.
  - *Apparence* : plumage (8), houppette (10), couleur de la houppette,
    forme et couleur des yeux, pilosité, corpulence, tenue principale et secondaire,
    coiffe (10 : couronne, capuche, cornes, auréole, chapeau de mage...),
    marque (cicatrice, peinture de guerre...), cape, aura colorée.
    Aperçu en direct, bouton « Aléatoire », « Tourner », « Attaquer ».
  - *Statistiques* : 10 points à répartir à la création (PV, ATK, DEF, VIT).
- **Préparer le combat** : choix du mode, de la **taille des équipes (1 à
  4)** puis des héros.
  - *Contre les bots* : votre équipe contre des vagues de monstres pilotés
    par l'ordinateur, **autant de monstres que de héros** (équipe contre
    équipe) ; étages 1 à 10, chaque victoire débloque l'étage suivant
    (progression gardée dans `progression.sav`).
  - *Duel local* : joueur 1 contre joueur 2 sur le même PC, chacun son
    tour avec les mêmes touches. Les héros sont **copiés** à PV pleins :
    duel amical, sans XP ni K.O. durable, et un même héros peut jouer des
    deux côtés. Bouton « Revanche » à la fin.
  - *Niveau en duel* : niveaux actuels, ou tous les héros ramenés au même
    niveau (`imposerNiveau` : stats de la classe à ce niveau, sans arme ni
    points bonus) pour éviter les écarts trop grands.
  - *Duel en ligne* : de 1 contre 1 à 4 contre 4 entre deux PC (Linux,
    macOS ou Windows : sockets POSIX ou Winsock). L'un héberge et choisit
    le format (héros par équipe, niveau), son IP s'affiche ; l'autre tape
    cette IP. Le moteur a son propre générateur
    aléatoire (`combatGraine`) : avec la même graine et les mêmes actions,
    les deux machines calculent exactement le même combat, donc seules les
    actions circulent (protocole simple, entiers écrits octet par octet).
- **Combat** puis écran de fin (XP, montées de niveau, butin en mode bots ;
  équipe gagnante en duel).

Tous les personnages sont **dessinés en vectoriel** (`SDL_RenderGeometry`) à
partir de leur `Apparence` : aucune image à charger, et chaque option de
l'éditeur se voit immédiatement. La structure `Apparence` ne contient que
des indices : la logique métier ne dépend pas de l'affichage.

### Combat tour par tour (inspiré de Honkai: Star Rail)
- **Ligne de temps** (à gauche) : chaque combattant a une valeur d'action
  `10000 / vitesse` ; le plus bas joue, et ce temps est retiré à tous.
  Les personnages rapides jouent donc plus souvent.
- **Points de compétence** partagés par l'équipe (max 5, une réserve par
  joueur en duel) : l'attaque de base (Q) en rapporte 1, la compétence (E)
  en coûte 1.
- **Énergie et ultimes** : chaque action et chaque coup reçu remplit la
  jauge ; pleine, l'ultime (touches 1 à 4 ou clic sur la jauge) peut être
  lancé à tout moment pendant le tour du joueur, avec une cinématique.
- **Faiblesses et rupture** : chaque monstre a des éléments faibles
  (icônes) et une barre de résistance blanche. Frapper avec un élément
  faible la vide ; à zéro le monstre est **brisé** : dégâts bonus, action
  retardée et effet selon l'élément (brûlure, gel, saignement...). Voir
  aussi la section « Éléments » plus bas.
- **Ciblage** : un ennemi, une zone (cible + voisins), tous, un allié ou
  toute l'équipe selon la capacité (← → ou clic, Espace pour valider).
- 3 capacités par classe (24 au total), boucliers, soins, réanimation,
  bonus d'attaque, avance de tour, poison, malus de défense.
- 9 monstres (Gelée, Squelette, Loup sombre, Golem, Spectre, Champignon,
  Diablotin, Gargouille) et 4 boss, adaptés à l'étage, à la taille et au
  niveau moyen de l'équipe, en 2 à 4 vagues.
- **10 étages** décrits par une table `ETAGES` (`combat.c`) : nom, ambiance,
  nombre de vagues, monstres possibles et boss éventuel. Chaque étage a son
  décor (champignons lumineux, flaques, os et tombes, lave, remparts, vide).

  | Étage | Nom                    | Boss            | Mécanique du boss                          |
  |-------|------------------------|-----------------|--------------------------------------------|
  | 3     | Le Trône visqueux      | Roi des Gelées  | Invoque des gelées en renfort              |
  | 6     | La Crypte de la Liche  | Liche           | Frappe toute l'équipe et se soigne         |
  | 8     | Le Nid du Dragon       | Dragon ancien   | Souffle sur toute l'équipe + brûlure       |
  | 10    | Le Cœur du Néant       | Canard Noir     | DEF -30 % et vol d'énergie sur l'équipe    |

- **Phase 2** : sous 50 % de PV, chaque boss se transforme une fois (garde
  restaurée, altérations purgées, ATK +30 %, attaques spéciales plus
  fréquentes, rejoue vite).
- Le Canard Noir est dessiné avec le même code que les héros
  (`dessinerHeros`) et une apparence corrompue.
- En duel, chaque héros a aussi une barre de résistance et deux faiblesses
  : les deux éléments forts contre le sien. Au-delà de 30 actions, la **fatigue**
  augmente les dégâts de 5 % par action pour qu'un duel entre soigneurs
  finisse toujours.
- Les PV des héros sont ceux du donjon (pointeurs vers le tableau
  dynamique) : un héros K.O. le reste jusqu'au repos ou à une potion.

Commandes : **Q** basique · **E** compétence · **1-4** ultimes · **← →**
cible · **Espace/Entrée** valider · **Échap** retour/annuler ·
**F12** capture d'écran (fichier `capture-*.bmp`).

### Éléments
- **Cycle** (`CYCLE_ELEMENTS`, `personnage.c`) : Feu, Glace, Vent, Foudre,
  Physique, Quantique, Imaginaire ; chaque élément est **fort** contre les
  deux suivants et **faible** contre les deux précédents. Aucun élément
  n'est avantagé, et une seule table suffit à calculer les deux relations
  (`elementsForts`, `elementsFaibles`, en masques de bits).
- **Dégâts** : `multiplicateurElement` donne 125 % (fort), 80 % (faible) ou
  100 %. Il s'applique dans les deux sens (héros et monstres ont un
  élément), en combat d'équipe comme dans le duel classique de la console
  (`attaquer`).
- **Les attaques changent avec l'élément** : `nomCapacite` ajoute l'élément
  au nom des capacités offensives (*Fendoir de feu*, *Orbe de givre*...) et
  chaque attaque d'un héros applique l'effet de son élément, une fois par
  cible et par action (`Combat.touches`) : brûlure (Feu), retard d'action
  (Glace), garde entamée plus vite (Foudre), tour avancé (Vent), DEF
  ignorée (Quantique), DEF réduite (Imaginaire), dégâts bonus (Physique).
- **Faiblesses des monstres** = éléments forts contre le leur, plus
  quelques faiblesses bonus (`faiblessesMonstre`). Le joueur peut donc les
  déduire de la table.
- **Affichage** : fiche du héros (fort / faible contre), éditeur, panneau
  d'action en combat (« Foudre contre Glace : peu efficace (−20 %) »),
  bestiaire et table des éléments (avec le schéma du cycle).

### Équilibrage
- **Bouclier du paladin** : l'*Égide sacrée* protégeait toute l'équipe à
  chaque tour, si bien qu'aucun dégât ne passait. Elle a maintenant un
  **temps de recharge de 5 tours du paladin** (`RECHARGE_COMPETENCE`,
  champ `Combattant.recharge`) et vaut 8 + 2 × DEF au lieu de 12 + 3 × DEF.
- **Défense contre les bots** : dégâts = `ATK × 100 / (100 + k × DEF)` avec
  k = 6 entre héros (duel, inchangé) mais **k = 3 quand un monstre frappe
  un héros**. L'ATK des monstres commence plus haut (×1,3 à l'étage 1) et
  augmente moins vite (+11 % par étage au lieu de +18 %), pour que les
  premiers étages fassent mal sans rendre les derniers impossibles.
- Réglages vérifiés par une campagne simulée (4 héros niveau 1 qui montent
  les 10 étages en se reposant entre chaque) : les étages 1 et 2 coûtent
  maintenant 25 à 40 % des PV de l'équipe (contre presque rien avant), les
  boss gardent des défaites possibles.

### Bestiaire
- Fiche publique `InfoMonstre` (`combat.h`) : description, statistiques de
  base, élément, attaques normale et spéciale avec leur effet.
- `statsMonstre` calcule les vraies statistiques d'un monstre à un étage
  donné (la même fonction sert à créer les monstres en combat) et
  `etagesMonstre` liste les étages où il apparaît.
- Écran *Bestiaire* de Duckgeon (onglets Monstres / Table des éléments) et
  menus 9 et 10 du terminal.

### Choix techniques
- Boucle principale **non bloquante** : `SDL_PollEvent` traite les
  événements en attente puis rend la main au dessin (60 images/s avec la
  synchronisation verticale). Un `scanf` bloquerait l'affichage et les
  animations.
- **Barres de vie** : largeur = `largeurMax * pv / pvMax` ; la valeur
  affichée rattrape progressivement la vraie valeur pour animer la perte.
- **Moteur de combat séparé** (`combat.c`) : il applique les règles et
  décrit chaque action par une liste d'effets ; l'interface les anime.
  Les cases 0..3 forment le camp 0 et les cases 4..8 le camp 1 (monstres
  ou joueur 2) : les mêmes règles servent aux deux modes, seules les
  cibles « alliées » et « ennemies » sont relatives au camp du lanceur.
  On peut ainsi le tester sans fenêtre (des milliers de combats simulés
  sous valgrind sans erreur).
- **Widgets en mode immédiat** : un bouton est une fonction appelée à chaque
  image qui retourne 1 s'il est cliqué ; aucun objet d'interface à libérer.
- **Cache de textes** : les textures produites par SDL_ttf sont conservées
  (512 entrées, éviction de la moins récemment utilisée) au lieu d'être
  recréées à chaque image.
- **Nettoyage** : `guiQuitter` détruit les textures, polices, curseurs,
  renderer et fenêtre, puis `TTF_Quit` / `SDL_Quit` ; le combat et l'état
  de l'application sont alloués avec `calloc` et libérés en sortie.
- Logique de taille fixe (1280×720) mise à l'échelle par
  `SDL_RenderSetLogicalSize` : la fenêtre est redimensionnable.

### Non retenus
- **SDL_image** : remplacé par des sprites vectoriels, plus faciles à
  personnaliser qu'une planche d'images par combinaison d'options.
- **SDL_mixer** : non utilisé (pas de fichiers audio à fournir).
- **Liste chaînée** : elle remplacerait le tableau `malloc` évalué en Partie 1
  et ne correspond plus à la signature imposée en Partie 2
  (`sauvegarder(..., Personnage *liste, int n)`).

## Le jeu complet dans le terminal (`console_jeu.c`)

- Aucune dépendance graphique : le module n'inclut que `combat.h`,
  `boutique.h`, `sauvegarde.h`... et se compile dans `./donjon`.
- Menu du sujet conservé (1 à 5), puis inventaire, amélioration, élément,
  bestiaire, table des éléments, boutique et repos. *3. Lancer un combat*
  propose l'exploration des étages, le duel en équipe et le duel classique.
- Le combat texte pilote le **même moteur** que Duckgeon : affichage des
  deux camps (barres de PV, énergie, garde, faiblesses, altérations, temps
  de recharge), ordre des prochains tours, choix de l'action et de la cible
  (avec l'efficacité de l'élément), puis récit de l'action à partir du
  journal (`nbJournalTotal`) et de la liste d'effets du moteur.
- La progression (étages, foie gras, cosmétiques) est partagée avec
  Duckgeon via `progression.sav`.
- Sous Windows, `SetConsoleOutputCP(CP_UTF8)` pour les accents des textes
  du moteur.

## Questions de réflexion

**Pourquoi `malloc` pour le tableau de personnages plutôt qu'un tableau
statique de taille fixe ? Que faut-il faire à la fin avec `free` ?**

Le nombre de personnages n'est pas connu à la compilation : il dépend de
l'utilisateur. Un tableau statique impose une limite arbitraire (trop petite
on bloque l'utilisateur, trop grande on gaspille de la mémoire) et vit sur la
pile, dont la taille est limitée. Avec `malloc`, la mémoire est prise sur le
tas à la taille voulue, et peut être agrandie avec `realloc` au fur et à
mesure. En contrepartie, cette mémoire n'est jamais libérée automatiquement :
il faut impérativement appeler `free` une fois (et une seule) sur le pointeur
retourné avant la fin du programme, sinon il y a fuite mémoire. Après le
`free`, on remet le pointeur à `NULL` pour ne pas réutiliser une zone libérée.

**Pourquoi `attaquer` prend-elle des pointeurs et non les structures
directement ?**

En C, les arguments sont passés par valeur : si `attaquer` recevait des
`Personnage`, elle travaillerait sur des copies, et la perte de PV de la
cible serait perdue au retour de la fonction. Avec un pointeur, la fonction
accède à l'original et `cible->pv -= degats` modifie réellement le personnage
stocké dans le tableau. Cela évite aussi de copier toute la structure
(≈ 46 octets) à chaque appel. Pour `afficherPersonnage`, on utilise
`const Personnage *` : on garde l'efficacité du pointeur tout en garantissant
au compilateur que la fonction ne modifie pas le personnage.

## Tests réalisés

- Création de 5 personnages (déclenche un `realloc` de 4 à 8 cases).
- Attaque simple et combat complet jusqu'au K.O.
- Saisies invalides (`abc`, `9`, ligne vide, nom trop long) : refusées.
- Tentative d'auto-attaque et d'attaque avec un personnage K.O. : refusées.
- Victoire : gain d'XP + objet ramassé ; équipement d'une arme (ATK augmentée).
- Potion utilisée sur un personnage K.O. : il revient au combat.
- Gain de 240 XP d'un coup : passage du niveau 1 au niveau 3.
- Sauvegarde de 3 personnages, relance, chargement : les 3 sont retrouvés ;
  création de 2 personnages après chargement (le tableau réalloué grandit).
- Chargement d'un fichier invalide : refusé, donjon inchangé.
- 300 combats automatiques (étages 1 à 10, équipes de 1 à 4) sous
  valgrind : aucun blocage ni erreur mémoire ; invocations et phases 2
  déclenchées.
- Fichier de version 1 relu en version 2 (points à 0), aller-retour v2,
  achats de cosmétiques et progression sous valgrind : aucune erreur.
- Console : victoire → +1 point, menu 7 → statistique augmentée.
- Terminal sous valgrind (entrées scriptées) : création avec choix de
  l'élément, exploration de l'étage 1 jusqu'à la victoire (étage 2
  débloqué, foie gras gagné), exploration à 3 héros jusqu'à la défaite avec
  le temps de recharge du paladin (5, 4, 3, 2, 1), duel en équipe jusqu'à
  la victoire, bestiaire, table des éléments, achat et équipement d'un
  cosmétique, sauvegarde et chargement : aucune erreur, aucune fuite.
- Campagne simulée des 10 étages (équipes de 2 et 4 héros) pour régler
  les dégâts des monstres.
- Rendus hors écran de Duckgeon (combat, fiche du héros, éditeur,
  bestiaire, table des éléments) pour vérifier la mise en page.
- Valgrind : `All heap blocks were freed -- no leaks are possible`,
  `ERROR SUMMARY: 0 errors`.
