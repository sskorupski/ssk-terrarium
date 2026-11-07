# Pratiques

## Couches

Le firmware sépare ce qui décide de ce qui touche au matériel, comme une architecture hexagonale ramenée à un microcontrôleur.

- **Le cœur, `lib/control`**, décide : quel mode, chauffer ou non, le capteur est-il en défaut, que veut dire une ligne tapée sur la liaison série. Il ne connaît ni broche, ni temps réel, ni bibliothèque Arduino : il reçoit des mesures et des instants, il rend des décisions (INV-6).
- **Les adaptateurs**, `lib/sensors`, `lib/outputs`, `lib/storage`, parlent au matériel, chacun pour un seul périphérique. Une erreur matérielle s'y traduit en donnée — `valid = false`, `false` en retour —, jamais en exception ni en blocage.
- **`src/main.cpp` assemble** : il lit les capteurs, passe les mesures au cœur, applique ses décisions aux sorties. La supervision (web, MQTT, alertes) se branche après, et rien de la régulation ne l'attend (INV-3).

Le vocabulaire du cœur est celui du domaine — mode, consigne, hors-gel, défaut — et suit les règles du cadrage, citées par leur identifiant `RG-…`.

## Tests

- Toute règle du cœur a son test natif, écrit rouge d'abord. Le nom du test dit le comportement (`test_fault_once_silent_beyond_the_limit`), un commentaire cite la règle qu'il éprouve.
- Les cas aux bornes sont explicites : la limite elle-même, une milliseconde au-delà, le passage de `millis()` par zéro.
- Ce qui touche au matériel (capteurs, relais, NVS) ne se teste pas en natif. Il s'éprouve sur la carte, sans charge 230 V, avant chaque fin de phase, et le cadrage consigne ce qui a été mesuré.

## Erreurs

- Un capteur qui échoue rend une lecture invalide ; c'est au cœur d'en tirer la conséquence (INV-2).
- Une écriture NVS qui échoue se signale sur la liaison série, et la régulation continue avec l'état en mémoire.
- Aucune allocation dynamique une fois `loop()` lancé : objets statiques, tampons de taille fixe. Une fragmentation du tas sur des mois de fonctionnement ne se diagnostique pas.
- Le chien de garde (10 s) redémarre une carte figée ; INV-1 garantit qu'elle repart sorties coupées.

## Interface web (à partir de la phase 2)

La page web locale vise WCAG 2.2 AA : HTML sémantique d'abord, ARIA seulement pour un composant que HTML n'offre pas, avec le comportement clavier que décrit l'[APG](https://www.w3.org/WAI/ARIA/apg/). La table des ressources de la WAI à ouvrir selon le geste sera ajoutée ici avec la première vue.

## Livraison

- Une branche par sujet, fusionnée dans `main` par une demande de fusion dont la CI est verte ; la branche est rebasée sur `main` et la CI repasse avant la fusion.
- Une phase se termine sur la carte : téléversement, essais sans charge, puis avec charge sous surveillance.
- Le dépôt ne publie pas de versions GitHub : le firmware se téléverse depuis `main`.
