# Coupure par le substrat

La sonde DS18B20, côté chaud du substrat, coupe le chauffage avant que le substrat ne cuise les escargots (RG-coupure-par-le-substrat). Ses seuils, sa zone d'approche et son comportement quand la sonde se tait se règlent depuis le réseau ou la liaison série, dans des bornes que rien ne déplace, et elle peut se désactiver, toujours visiblement. Le pourquoi est dans les cadrages [ssk-terrarium-cadrage#2026-005](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-005) et [ssk-terrarium-cadrage#2026-018](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-018).

Sans réglage, rien ne change : coupure active à 24 °C, reprise strictement sous 18,5 °C, le jour comme la nuit, sans approche, et une sonde muette pendant une coupure la lève au bout de 2 minutes.

## Ce que fait la coupure

- **Coupure** : active, dès qu'une mesure valide du substrat atteint le seuil de coupure du moment, le chauffage est coupé, dans tous les modes. Il le reste jusqu'à une mesure valide strictement sous le seuil de reprise du moment ; il suit alors de nouveau son mode, sur l'air.
- **Jour et nuit** : les seuils du moment (RG-jour-et-nuit) valent en `ACTIF`, `PLAGES` et `HIBERNATION` ; tant que l'heure n'est pas connue, ce sont ceux du jour. Au passage du jour à la nuit ou l'inverse, la mesure suivante se juge sur les nouveaux seuils : une coupure se lève si la mesure est déjà sous la nouvelle reprise, et une mesure qui atteint la nouvelle coupure coupe.
- **Zone d'approche** : de `coupure − approche` inclus à `coupure` exclu, quand le mode demande de chauffer, le chauffage ne tourne que le début de chaque tranche de 10 minutes : `(coupure − mesure) / approche` de la tranche, arrondi à la minute, calculé au début de la tranche sur la dernière mesure. 10 minutes à l'entrée de la zone, de moins en moins jusqu'au seuil. Une tranche commence quand le substrat est dans la zone et que le mode demande de chauffer, la suivante à la fin de la précédente ; sortir de la zone ou couper y met fin. Une approche de 0, par défaut, garde la coupure franche. Une tranche où le chauffage s'arrête ne compte pas comme une chauffe continue (RG-chauffe-continue-bornee).
- **Sonde muette pendant une coupure** : `reprise-air` (par défaut) lève la coupure au bout de `delai` minutes, et l'air décide seul jusqu'au retour d'une mesure valide ; `maintien` garde la coupure jusqu'à une mesure valide sous la reprise, mais la lève dès que l'air retenu pour réguler descend sous 4 °C, le bas du hors-gel. Ce plancher n'est pas un réglage.
- **Désactivée**, la coupure ne s'applique jamais : une coupure en cours se lève à la mesure suivante, et le chauffage suit son mode. La sonde reste relevée et publiée. La désactivation est enregistrée et survit à un redémarrage.
- Une mesure est valide entre −20 et 60 °C, hors 85 °C de mise sous tension. Sans sonde, la coupure ne s'applique jamais. La coupure en cours n'est pas gardée après un redémarrage. Elle n'est pas un défaut : la LED d'état reste éteinte.

## Régler

Par `cmd/config`, ou sur la liaison série par `CONFIG` suivi du même JSON. `securite_substrat` est un objet partiel à deux niveaux : un champ absent garde sa valeur.

```json
{"securite_substrat":{"active":true,"approche":2.0,"jour":{"coupure":24.0,"reprise":22.0},"nuit":{"coupure":22.0,"reprise":18.0},"sonde_muette":{"comportement":"maintien","delai":2}}}
```

| Champ | Valeurs admises |
| --- | --- |
| `active` | `true` ou `false` |
| `approche` | de 0 à 4 °C, au dixième de degré |
| `jour.coupure`, `nuit.coupure` | de 20 à 28 °C, au dixième de degré |
| `jour.reprise`, `nuit.reprise` | de 15 °C à la coupure du même moment moins 2 °C, au dixième de degré |
| `sonde_muette.comportement` | `reprise-air` ou `maintien` |
| `sonde_muette.delai` | de 1 à 30 minutes entières |

Ces bornes sont des constantes de `include/config.h` (INV-5). Une commande mal formée, hors bornes, plus fine que le dixième de degré, ou qui laisserait une reprise à moins de 2 °C sous sa coupure, est refusée en entier : `{"evenement":"reglage","champ":"securite_substrat","resultat":"refuse","raison":"securite_substrat invalide"}`. Acceptée, elle répond `{"evenement":"reglage","champ":"securite_substrat","resultat":"applique"}` (`applique_non_enregistre` si la mémoire refuse l'écriture), s'enregistre, et vaut dès la mesure suivante. Une activation ou une désactivation publie en plus `{"evenement":"securite-substrat","active":false}`.

Au démarrage, un réglage illisible ou hors des bornes du firmware remet la coupure active, sur ses seuils d'origine.

## Lire

Dans `state` (RG-topics-mqtt) :

```json
"coupure_substrat":false,
"securite_substrat":{"active":true,"approche":2.0,"jour":{"coupure":24.0,"reprise":22.0},"nuit":{"coupure":22.0,"reprise":18.0},"sonde_muette":{"comportement":"maintien","delai":2}},
"seuils_substrat":{"coupure":24.0,"reprise":22.0},
"part_substrat":0.50
```

- `coupure_substrat` : le chauffage est coupé par le substrat ; toujours `false` désactivée.
- `seuils_substrat` : les seuils du moment, `null` quand la coupure est désactivée. C'est la seule valeur à lire pour savoir ce qui s'applique.
- `part_substrat` : la part de chaque tranche de 10 minutes où le substrat laisse chauffer, en dixièmes publiés au centième (`0.50` pour 5 minutes) : `1.00` hors de la zone, `0.00` pendant une coupure, la part de la tranche en cours, ou celle qu'une tranche prendrait sur la dernière mesure si le mode ne demande pas de chauffer ; `null` désactivée ou sans mesure valide. `sorties.chauffage` dit ce qui se passe.

Les bornes sont dans `bornes.substrat` : `{"coupure":[20.0,28.0],"reprise_min":15.0,"ecart_min":2.0,"delai":[1,30],"approche":[0.0,4.0],"comportements":["reprise-air","maintien"]}`.

## Liaison série

- `SUBSTRAT` affiche le réglage : `SUBSTRAT coupure active, jour 24.0/18.5C, nuit 24.0/18.5C, approche 0.0C, sonde muette reprise-air 2min actuelle`. La même ligne s'écrit au démarrage (`au demarrage`) et à chaque réglage (`: enregistree`, ou `: appliquee mais non enregistree`) ; `coupure DESACTIVEE` quand elle l'est.
- `ATTENTION coupure par le substrat DESACTIVEE` au démarrage et à la désactivation, `SECURITE coupure par le substrat activee` à la réactivation.
- `COUPURE chauffage : substrat a 24.1 C`, puis `REPRISE chauffage : substrat a 18.4 C`, `REPRISE chauffage : sonde substrat muette`, `REPRISE chauffage : sonde substrat muette, air a 3.9 C sous 4.0 C` ou `REPRISE chauffage : coupure par le substrat desactivee`.
- `APPROCHE substrat : chauffe 5 min sur 10` quand la part change dans la zone, `APPROCHE substrat : sortie de la zone` quand elle revient à 10 minutes.
- `REFUS securite_substrat : hors bornes, ou une reprise a moins de 2.0C sous sa coupure`, ou `REFUS CONFIG : securite_substrat invalide` pour un JSON mal formé.
