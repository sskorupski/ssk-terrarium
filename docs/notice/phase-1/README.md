# Phase 1 — Le prototype d'hiver

À la fin de cette notice, le régulateur tient le bac à 19 °C en mode `ACTIF`, le protège du gel en mode `HIBERNATION`, et coupe le chauffage dès que le capteur d'air se tait. Les deux câbles chauffants passent par un relais puis par le limiteur Exo Terra réglé à 24 °C, qui coupe de lui-même si le firmware se trompe.

Durée : une demi-journée, en deux temps — le montage en très basse tension (étapes 1 à 8), puis le 230 V (étapes 9 et 10).

> **Le 230 V tue.** Les étapes 1 à 8 ne touchent qu'au 12 V, au 5 V et au 3,3 V. On ne passe à l'étape 9 qu'une fois toutes les précédentes validées, en particulier le niveau de déclenchement du module relais mesuré à l'étape 4. Qui n'a jamais câblé de 230 V fait faire l'étape 9 par quelqu'un qui en a l'habitude. Le circuit du garage doit être protégé par un différentiel 30 mA.

La notice part du montage de la [phase 0](../phase-0/README.md) terminé : la carte programmée, et sa LED d'état sur GPIO 2.

## Ce qu'il faut

| Élément | Référence |
| --- | --- |
| Carte ESP32 | Keyestudio KS5016 de la phase 0, avec sa LED d'état et sa résistance de 330 Ω (orange, orange, marron, or) sur GPIO 2 |
| Capteur d'air | Adafruit SHT45 n° 6174, avec son câble STEMMA QT vers prises femelles, prolongé de fils Dupont mâle/femelle |
| Sonde du substrat | DS18B20 étanche DFRobot, avec sa carte adaptatrice à bornier |
| Module relais | 4 relais 5 V optocouplé, Kiwi KW-1613, avec son cavalier JD-VCC |
| Abaisseur | LM2596 réglable à afficheur |
| Bloc 12 V | HN Power HNP42EU-120, 12 V 3,5 A, fiche 5,5 × 2,1 |
| Raccords 12 V | Embase jack femelle 5,5 × 2,1 à bornier ; cordon jack mâle 5,5 × 2,1 vers la carte ; fil souple |
| Répartiteur | Mini plaque d'essai (breadboard) : le 3,3 V et le GND de la carte alimentent trois éléments, et la carte n'a pas assez de broches pour les y brancher directement |
| Fils | Fils Dupont mâle/mâle et mâle/femelle |
| Multimètre | En voltmètre continu (V⎓) et en testeur de continuité |
| Câblage 230 V | Cordon secteur 3G 1 mm² avec fiche, porte-fusible 5 × 20 et fusible 1 A temporisé, socle de prise femelle, fil 1 mm² marron, borniers à levier, boîtier électrique et presse-étoupe |
| Chauffage | Limiteur Exo Terra 300 W Dimming & Pulse ; multiprise 2 prises ; câbles chauffants Reptil'us 50 W et 25 W, déjà collés sur les vitres |
| Pour l'essai | Une lampe de chevet, un sachet de glaçons |
| Firmware | Le fichier `.bin` de la version `phase-1`, sur sa page [Releases](https://github.com/sskorupski/ssk-terrarium/releases) |

## Étape 1 — Téléverser le firmware de la phase 1

Carte seule, sans autre fil que la LED d'état :

1. Téléverser le fichier `.bin` de la version `phase-1` à l'adresse `0x0`, comme à l'étape 2 de la [phase 0](../phase-0/README.md).
2. Ouvrir le terminal série comme à l'étape 3 de la phase 0, puis appuyer sur **RESET**.

Après les lignes de démarrage de la puce, la carte écrit :

```
=== ssk-terrarium phase-1 : demarrage ===
ABSENT capteur air
ABSENTE sonde substrat (optionnelle)
INFO aucun mode enregistre, demarrage en MAINTENANCE
MODE MAINTENANCE au demarrage
=== pret : regulation en cours ===
MESURE air=-- hr=-- substrat=-- chauffage=0 mode=MAINTENANCE
```

Les capteurs ne sont pas encore branchés : les deux lignes `ABSENT` sont attendues. Le téléversement efface le mode enregistré en phase 0 : la carte repart en `MAINTENANCE`. Une ligne `MESURE` arrive toutes les 30 secondes, et `DEFAUT capteur air` au bout de 2 minutes environ, LED d'état allumée, puisque le capteur manque.

Débrancher l'USB.

## Étape 2 — Vérifier la polarité du bloc 12 V

Un LM2596 ou une carte branchés à l'envers peuvent griller : la polarité se vérifie avant de leur brancher quoi que ce soit.

1. Bloc débranché du secteur, enfoncer sa fiche dans l'**embase jack femelle à bornier**. Ses deux bornes à vis sont marquées **+** et **−**.
2. Régler le multimètre en tension continue, calibre 20 V.
3. Brancher le bloc sur le secteur.
4. Pointe **rouge** sur la borne **+**, pointe **noire** sur la borne **−**.
5. Le multimètre lit entre **+12,0 et +12,6 V**. Une valeur **négative** veut dire une polarité inversée : s'arrêter là, et ne rien brancher sur ce bloc.
6. Débrancher le bloc du secteur.

## Étape 3 — Régler l'abaisseur LM2596 à 5,0 V

Le LM2596 alimente les bobines des relais. Il sort d'usine réglé au hasard, parfois bien au-delà de 5 V : il se règle **à vide**, avant de lui brancher le module relais.

1. Bloc débranché, relier par deux fils souples la borne **+** de l'embase à l'entrée **IN+** du LM2596, et la borne **−** à **IN−**. Rien ne se branche sur sa sortie.
2. Brancher le bloc sur le secteur. L'afficheur du LM2596 s'allume ; son bouton bascule entre la tension d'entrée et celle de sortie.
3. Multimètre sur la sortie : pointe rouge sur **OUT+**, pointe noire sur **OUT−**.
4. Tourner lentement la vis du petit potentiomètre bleu, qui demande de nombreux tours, jusqu'à lire **5,0 V** (entre 4,9 et 5,1 V) au multimètre. L'afficheur peut différer de quelques dixièmes : le multimètre fait foi.
5. Débrancher le bloc du secteur.

![Alimentation 12 V et 5 V](alimentation.svg)

Le schéma montre aussi le cordon jack vers la carte : il ne se branche qu'à l'étape 8.

## Étape 4 — Mesurer le niveau de déclenchement du module relais

Le firmware suppose un module qui colle ses relais quand son entrée est mise à la masse (déclenchement bas) : c'est ce qu'annonce sa fiche, et c'est ce que cette étape vérifie. **Si la mesure dit le contraire, s'arrêter** : le firmware allumerait tout au repos.

Le module a deux rangées de broches : `GND IN1 IN2 IN3 IN4 VCC` côté commandes, et `VCC JD-VCC GND` à côté, avec un cavalier entre `VCC` et `JD-VCC`.

1. Tout débranché, **retirer le cavalier** `JD-VCC` et le garder dans un sachet. Le côté commandes (`VCC`, optocoupleurs) sera alimenté en 3,3 V par la carte, le côté bobines (`JD-VCC`) en 5 V par le LM2596.
2. Relier la sortie du LM2596 au module : **OUT+** vers **JD-VCC**, **OUT−** vers le **GND** de la même rangée.
3. Sur la mini plaque d'essai, préparer deux lignes : une ligne **3,3 V**, reliée à la broche **3V3** de la carte, et une ligne **GND**, reliée à une broche **GND** de la carte.
4. Relier la ligne 3,3 V au **VCC** de la rangée des commandes, et la ligne GND à son **GND**. Ne rien brancher sur `IN1` à `IN4`.
5. Brancher la carte par l'USB, puis le bloc 12 V sur le secteur. Aucun relais ne colle, aucun voyant de relais ne s'allume : une entrée en l'air laisse le relais ouvert.
6. Avec un fil Dupont, relier **IN1** à la ligne **GND** : le relais 1 claque, et son voyant s'allume. Retirer le fil : il retombe.
7. Relier **IN1** à la ligne **3,3 V** : rien ne se passe.
8. Recommencer les gestes 6 et 7 pour `IN2`, `IN3` et `IN4`.
9. Multimètre en testeur de continuité, entre les bornes **COM** et **NO** du relais 1 : il ne sonne pas au repos, et sonne quand `IN1` est à la masse.
10. Débrancher le bloc, puis l'USB.

Le module se déclenche donc **au niveau bas**, et le firmware le coupe en tenant ses entrées au niveau haut. Noter le résultat : il se reporte dans le cadrage du projet. Si un relais a collé avec son entrée en l'air ou reliée au 3,3 V, s'arrêter là.

## Étape 5 — Brancher les commandes des relais

Tout débranché, relier les entrées du module aux broches de la carte :

| Entrée du module | Broche de la carte | Sortie |
| --- | --- | --- |
| IN1 | GPIO 26 | Chauffage |
| IN2 | GPIO 27 | Ruban LED (phase 2) |
| IN3 | GPIO 25 | Pompe de brumisation (phase 4) |
| IN4 | GPIO 33 | Ventilateur (phase 2) |

![Commande des relais](relais.svg)

Puis vérifier qu'aucun relais ne colle tant que le firmware n'a rien décidé :

1. Brancher la carte par l'USB, puis le bloc 12 V. Ouvrir le terminal série.
2. **Maintenir le bouton RESET enfoncé** au moins 5 secondes : la carte est arrêtée et ses broches en l'air. Aucun relais ne colle, aucun voyant de relais ne s'allume.
3. Relâcher **RESET** : la carte démarre. Aucun relais ne claque, ni pendant le démarrage, ni après `=== pret : regulation en cours ===`.
4. Taper `MODE ACTIF` puis **Entrée** : la carte répond `MODE ACTIF enregistre, conserve apres coupure`. Le capteur d'air n'étant pas branché, le chauffage reste coupé : le relais 1 ne colle pas.
5. Débrancher le bloc, puis l'USB.

Un relais qui claque, même brièvement, carte en reset ou au démarrage : s'arrêter avant le 230 V.

## Étape 6 — Brancher les capteurs

Tout débranché :

1. **Capteur d'air SHT45**, par son câble STEMMA QT prolongé de fils Dupont : fil **noir** (GND) sur la ligne GND, **rouge** (V+) sur la ligne 3,3 V, **bleu** (SDA) sur **GPIO 21**, **jaune** (SCL) sur **GPIO 22**.
2. **Sonde DS18B20** : ses trois fils dans le bornier de sa carte adaptatrice, selon les marquages du bornier. La carte adaptatrice porte la résistance de tirage de 4,7 kΩ ; s'il n'y en a pas, en ajouter une de 4,7 kΩ (jaune, violet, rouge, or — ou jaune, violet, noir, marron, marron sur une résistance à 5 anneaux) entre la broche de données et le 3,3 V.
3. **Carte adaptatrice** : **GND** (ou **−**) sur la ligne GND, **VCC** (ou **+**) sur la ligne 3,3 V, **D** (données) sur **GPIO 4**.

![Capteurs](capteurs.svg)

4. Brancher la carte par l'USB, puis le bloc 12 V. Ouvrir le terminal série et appuyer sur **RESET**.

Aucune ligne `ABSENT` au démarrage. Toutes les 30 secondes arrive une mesure, par exemple :

```
MESURE air=17.8C hr=62% substrat=17.5C chauffage=1 mode=ACTIF
```

Les valeurs ressemblent à la pièce : une température proche de celle d'un thermomètre posé à côté, une humidité entre 30 et 80 %.

## Étape 7 — Vérifier la régulation et le défaut capteur

En mode `ACTIF`, le chauffage s'allume sous 18,5 °C et s'éteint à 19,5 °C. Les mesures arrivent toutes les 30 secondes : chaque effet attendu peut prendre ce délai.

1. **Chauffe** : si l'air est au-dessus de 18,5 °C, approcher du capteur un sachet de glaçons, sans le mouiller. Dès qu'une mesure passe sous 18,5 °C, le relais 1 claque et la mesure affiche `chauffage=1`.
2. **Arrêt** : retirer le sachet et réchauffer le capteur dans la main. Dès qu'une mesure dépasse 19,5 °C, le relais 1 retombe et la mesure affiche `chauffage=0`.
3. **Défaut capteur** : relais 1 collé (refaire le geste 1 au besoin), débrancher le fil **bleu** (SDA) du capteur d'air. Au plus 2 minutes et demie plus tard, la carte écrit `DEFAUT capteur air`, le relais 1 retombe, et la LED d'état s'allume.
4. **Reprise** : rebrancher le fil bleu. À la mesure suivante, la carte écrit `REPRISE capteur air`, la LED d'état s'éteint, et la régulation reprend.
5. **Hors-gel** : taper `MODE HIBERNATION` puis **Entrée**. Le chauffage ne s'allume plus que sous 4 °C : le relais 1 reste ouvert, même avec le sachet de glaçons. Taper ensuite `MODE ACTIF` puis **Entrée**.

## Étape 8 — Alimenter la carte par le bloc 12 V

La carte quitte l'USB pour le bloc, qui l'alimente par son jack.

1. Bloc et USB débranchés, visser le **cordon jack mâle** dans l'embase à bornier, à côté des fils du LM2596 : son fil **+** sur la borne **+**, l'autre sur la borne **−**.
2. Bloc branché, vérifier au multimètre que le **centre** de la fiche du cordon est à **+12 V** par rapport à son manchon extérieur, puis débrancher le bloc.
3. Enfoncer la fiche du cordon dans le **jack** de la carte.
4. Brancher le bloc sur le secteur. La carte démarre : sa LED d'alimentation s'allume, aucun relais ne claque, et le relais 1 suit la température comme à l'étape 7.
5. Pour lire le journal, brancher l'USB en plus du jack.

Le montage en très basse tension est terminé. Le ranger dans le boîtier, capteurs dehors, avant de passer au 230 V.

## Étape 9 — Câbler le 230 V

**Toute cette étape se fait cordon secteur débranché**, sauf l'essai du geste 7 et la mise sous tension du geste 9, boîtier refermé.

Le relais 1 coupe la **phase** du circuit de chauffage. Le neutre et la terre vont directement au socle de prise.

1. Dans le boîtier, fixer le porte-fusible et le socle de prise femelle. Mettre le fusible **1 A temporisé** dans le porte-fusible.
2. Cordon secteur : son fil **marron** (phase) à l'entrée du porte-fusible ; son fil **bleu** (neutre) à la borne **N** du socle ; son fil **vert et jaune** (terre) à la borne de **terre** du socle. Les raccords se font par borniers à levier.
3. Sortie du porte-fusible vers la borne **COM** du relais 1, par un fil marron 1 mm².
4. Borne **NO** du relais 1 vers la borne **L** du socle, par un fil marron 1 mm². Rien ne se branche sur la borne **NC**.
5. Serrer chaque vis du relais et du socle, puis tirer sur chaque fil : aucun ne doit sortir.
6. Passer le cordon par un presse-étoupe, et refermer le boîtier.

![Câblage 230 V](secteur.svg)

7. **Essai à la lampe** : brancher une lampe de chevet, interrupteur allumé, sur le socle, puis le cordon secteur et le bloc 12 V. La lampe reste éteinte tant que le relais 1 est ouvert. Refaire le geste 1 de l'étape 7 (sachet de glaçons) : la lampe s'allume quand le relais claque, et s'éteint quand il retombe.
8. Débrancher le cordon secteur, retirer la lampe.
9. **Limiteur** : régler le limiteur Exo Terra en mode **Pulse** (celui des câbles et tapis chauffants), sur **24 °C**. Brancher le limiteur dans le socle, la multiprise dans le limiteur, et les deux câbles chauffants dans la multiprise. Placer la sonde du limiteur dans le bac, contre la vitre arrière côté intérieur, à hauteur des escargots. Brancher enfin le cordon secteur.

## Étape 10 — Mise en service

1. Capteur d'air au centre du bac, à l'écart de l'emplacement de la future buse ; sonde du substrat enfoncée dans le substrat.
2. Taper `MODE ACTIF` puis **Entrée**. Si l'air du bac est sous 18,5 °C, le relais 1 claque et le voyant du limiteur montre qu'il chauffe.
3. Suivre les mesures pendant quelques heures : la température monte vers 19 °C, puis le chauffage alterne autour de 19 °C. Une ligne `PAUSE chauffage apres 2 h continues` veut dire que les câbles ne suffisent pas à tenir la consigne : voir la section « Puissance de chauffe » du cadrage.
4. Une fois la consigne atteinte, mesurer la vitre côté intérieur, là où les escargots se posent, au thermomètre de contact ou infrarouge, et noter la valeur la plus chaude.
5. Débrancher le bloc 12 V pendant que le chauffage tourne : le relais retombe, les câbles chauffants s'arrêtent. Rebrancher le bloc : la carte redémarre dans le mode enregistré.

## Fin de la phase 1

La phase est terminée quand les dix étapes donnent ce qu'elles annoncent. Reporter dans le cadrage du projet le niveau de déclenchement mesuré à l'étape 4 et la température de vitre mesurée à l'étape 10.

L'hiver, laisser le régulateur en `ACTIF` pour l'élevage, ou en `HIBERNATION` pour la mise au repos : le mode survit aux coupures de courant.
