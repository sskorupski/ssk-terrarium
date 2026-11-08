# Pratiques

## Couches

Le firmware sépare ce qui décide de ce qui touche au matériel, comme une architecture hexagonale ramenée à un microcontrôleur.

- **Le cœur, `lib/control`**, décide : quel mode, chauffer ou non, le capteur est-il en défaut, que veut dire une ligne tapée sur la liaison série. Il ne connaît ni broche, ni temps réel, ni bibliothèque Arduino : il reçoit des mesures et des instants, il rend des décisions (INV-6).
- **Les adaptateurs**, `lib/sensors`, `lib/outputs`, `lib/storage`, parlent au matériel, chacun pour un seul périphérique. Une erreur matérielle s'y traduit en donnée — `valid = false`, `false` en retour —, jamais en exception ni en blocage.
- **`src/main.cpp` assemble** : il lit les capteurs, passe les mesures au cœur, applique ses décisions aux sorties. La supervision (web, MQTT, alertes) se branche après, et rien de la régulation ne l'attend (INV-3) : elle tourne dans sa propre tâche (`lib/network`), et sa logique pure vit dans `lib/supervision`, testée en natif comme le cœur.
- **Une commande reçue du réseau est jugée par le cœur, dans la boucle de régulation** : la tâche de supervision ne fait qu'analyser sa forme. Les bornes d'un réglage (`isAcceptedHeatSettings`) ne se vérifient qu'à un seul endroit.

Le vocabulaire du cœur est celui du domaine — mode, consigne, hors-gel, défaut — et suit les règles du cadrage, citées par leur identifiant `RG-…`.

## Tests

- Toute règle du cœur a son test natif, écrit rouge d'abord. Le nom du test dit le comportement (`test_fault_once_silent_beyond_the_limit`), un commentaire cite la règle qu'il éprouve.
- Les cas aux bornes sont explicites : la limite elle-même, une milliseconde au-delà, le passage de `millis()` par zéro.
- Ce qui touche au matériel (capteurs, relais, NVS) ne se teste pas en natif. Il s'éprouve sur la carte, sans charge 230 V, avant chaque fin de phase, et le cadrage consigne ce qui a été mesuré.

## Erreurs

- Un capteur qui échoue rend une lecture invalide ; c'est au cœur d'en tirer la conséquence (INV-2).
- Une écriture NVS qui échoue se signale sur la liaison série, et la régulation continue avec l'état en mémoire.
- Aucune allocation dynamique dans `loop()` : objets statiques, tampons de taille fixe. Une fragmentation du tas sur des mois de fonctionnement ne se diagnostique pas. La tâche de supervision, elle, alloue : le TLS à chaque connexion, ArduinoJson à chaque commande, le portail à chaque page. Ces allocations sont rares, et un tas épuisé n'y coûte qu'une connexion manquée, jamais la régulation.
- Le chien de garde (10 s) redémarre une carte figée ; INV-1 garantit qu'elle repart sorties coupées.

## Messages de la liaison série

**Tout message que la carte écrit se comprend sans connaître le code** : il dit, en français et avec les mots de la notice, ce qui se passe et ce que la carte fait en conséquence. Qui suit la notice n'a que ces lignes pour savoir si son montage va bien ; un message obscur l'arrête, et un message qui ressemble à une panne lui fait défaire un montage correct.

- Un mot-clé en majuscules ouvre la ligne, puis vient la phrase : `MODE`, `MESURE`, `INFO`, `ABSENT`, `DEFAUT`, `REPRISE`, `PAUSE`, `ERREUR`, `WIFI`, `PORTAIL`, `MQTT`, `CONSIGNE`, `REFUS`. Le mot-clé se filtre, la phrase se lit.
- Des titres de section `=== … ===` découpent le journal : le premier, avec la version du firmware, sépare ses lignes des messages de démarrage de la puce, qu'il ne contrôle pas ; un autre dit quand l'initialisation est finie. Qui lit le journal sait où regarder, et quelle notice suit la carte.
- Une situation normale ne s'affiche jamais comme une erreur. Le premier démarrage écrit `INFO aucun mode enregistre, demarrage en MAINTENANCE`, et non le `nvs_open failed: NOT_FOUND` de la bibliothèque `Preferences` : un message de bibliothèque ou du cœur Arduino qui sort dans un cas normal s'évite à la source, et le firmware dit lui-même ce qu'il en est.
- Un `ERREUR` dit ce qui était attendu : `ERREUR commande inconnue (MODE ACTIF|PLAGES|HIBERNATION|MAINTENANCE, PLAGES, FUSEAU, WIFI CONFIGURER)`.
- Les messages publiés sur MQTT suivent la même règle : des clés et des valeurs françaises, sans accents (`{"evenement":"reglage","resultat":"refuse","raison":"hors bornes"}`).
- Les textes s'écrivent sans accents, pour qu'un terminal mal réglé les affiche intacts.
- Chaque ligne se termine par `\r\n`, comme le fait `Serial.println` : un `Serial.printf` finit donc par `\r\n`, jamais par `\n` seul. Sans le retour chariot, un terminal comme celui de la notice descend d'une ligne sans revenir à gauche, et le journal part en escalier.
- Chaque message figure dans la liste des sorties de `docs/developpement.md`, et dans la notice dès qu'une étape demande de l'observer.

## Interface web

Le portail de configuration, puis la page web locale de la phase 2, visent WCAG 2.2 AA : HTML sémantique d'abord, ARIA seulement pour un composant que HTML n'offre pas, avec le comportement clavier que décrit l'[APG](https://www.w3.org/WAI/ARIA/apg/). La table des ressources de la WAI à ouvrir selon le geste sera ajoutée ici avec la première vue.

## Livraison

- Une branche par sujet, fusionnée dans `main` par une demande de fusion dont la CI est verte ; la branche est rebasée sur `main` et la CI repasse avant la fusion.
- Une phase se termine sur la carte : téléversement, essais sans charge, puis avec charge sous surveillance.

### Livrer une phase

Chaque phase se livre de sorte qu'une autre personne puisse refaire le montage seule, avec la notice et sans l'historique des discussions.

1. **La notice s'écrit avec le code**, dans `docs/notice/phase-N/README.md`, dans les mêmes demandes de fusion : ce qu'il faut, les étapes dans l'ordre où on les fait, et pour chacune ce qu'on doit observer. Une étape sans résultat observable n'est pas vérifiable par qui la suit.
2. **Chaque branchement a son schéma WireViz** : `docs/notice/phase-N/<sujet>.yml`, et `outils/schemas.sh` en produit le `.gv` et le `.svg`, commités tous les trois. La CI régénère les `.gv` et refuse une demande de fusion dont le schéma ne suit plus sa source.
3. **La phase se valide en suivant sa propre notice**, sur le vrai montage, de la première étape à la dernière ; ce qui a été mesuré (niveau de déclenchement, tensions) se reporte dans le cadrage.
4. **Le tag `phase-N` se pose sur `main`** une fois la notice validée. Un correctif qui ne change pas le montage prend `phase-N.1`, `phase-N.2`… ; un changement de montage attend la phase suivante.
5. **Le workflow de publication** réagit au tag : il refait les contrôles, construit l'image fusionnée du firmware et le PDF de la notice, et crée la version GitHub avec ces deux fichiers.

Le tag est le seul repère d'une personne qui reprend le projet : la notice qu'elle lit et le firmware qu'elle téléverse viennent du même commit.

### Rédiger une notice

**Toute valeur de résistance s'écrit avec son code couleur entre parenthèses**, dans la notice comme dans son schéma WireViz : « 330 Ω (orange, orange, marron, or) ». Quand le kit fourni risque de contenir des résistances à 5 anneaux (1 %, corps souvent bleu), la variante s'ajoute : « ou orange, orange, noir, noir, marron sur une résistance à 5 anneaux ». Qui monte trie ses résistances à l'œil, dans un sachet mélangé ; une valeur seule l'oblige à décoder les anneaux ou à sortir le multimètre, et une erreur d'un facteur dix — 330 Ω pour 3,3 kΩ — ne se voit qu'une fois le montage sous tension.

**Un élément facultatif le dit**, avec ce qui le remplace pour vérifier l'étape, et la phase à partir de laquelle il devient nécessaire.

**Une pièce se nomme par ce qu'elle est et ce qu'elle fait, pas par sa référence d'achat.** « L'adaptateur jack à vis », « l'abaisseur 5 V », « la prise murale » — non « l'embase à bornier », « le LM2596 », « le secteur ». La liste du matériel décrit chaque pièce assez pour la reconnaître sur la table (forme, repères imprimés), et garde la référence pour l'acheter ; la notice reprend ensuite toujours le même nom. Un repère imprimé se cite tel qu'il figure sur la pièce, avec ses variantes connues (`VIN+` ou `IN+`).

**Un état se décrit par ce qu'on débranche, jamais par « tout débranché »** : « bloc 12 V débranché de la prise murale et câble USB débranché », avec ce qui reste en place des étapes précédentes. « Tout débranché » laisse croire qu'il faut défaire le montage.

**Chaque pièce porte un repère, comme dans une notice de meuble** : une lettre majuscule dans la liste du matériel, sans I ni O, qu'on confondrait avec 1 et 0. Chaque étape commence par la liste des repères qu'elle utilise ; le texte cite le repère entre parenthèses à la première mention de la pièce dans l'étape, « l'abaisseur 5 V (**H**) », et les schémas WireViz le portent en tête du nom de la pièce. Un repère reste attaché à sa pièce d'une phase à l'autre.

**Une illustration de pièce vient d'une source sous licence libre**, CC0, domaine public, CC BY ou CC BY-SA — Wikimedia Commons, les bibliothèques Fritzing —, jamais d'une page de vendeur : le dépôt est public. Elle se range dans `illustrations/<repère>` à côté de la notice, réduite à 320 px de large, et se crédite en fin de notice, auteur, licence et lien vers la source. Quand elle ne montre pas exactement le modèle acheté, la description le dit ; une pièce sans image libre fidèle reste sans illustration.
