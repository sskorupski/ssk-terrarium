# Phase 2 — Le régulateur complet en local

La phase 2 apporte trois choses : la ventilation, l'éclairage à l'heure et la page web locale. **Cette notice ne couvre pour l'instant que la ventilation**, la seule des trois que le firmware réalise ; l'éclairage et la page web s'y ajouteront, et le tag `phase-2` ne se posera qu'une fois les trois montées et validées.

À la fin de cette notice, un ventilateur de 80 mm, fixé dans le couvercle, renouvelle l'air du bac : 5 minutes toutes les heures en `ACTIF` et en `PLAGES`, 5 minutes par jour en `HIBERNATION`, jamais en `MAINTENANCE`. La durée, la période et l'heure se règlent par la liaison série ou à distance ([ventilation.md](../../ventilation.md), cadrage [ssk-terrarium-cadrage#2026-013](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-013)).

Durée : une heure, essais compris. Tout se passe en 5 V : aucune étape ne touche au 230 V de la phase 1, qui reste en place. On débranche quand même le cordon secteur (**N**) avant d'ouvrir le boîtier (**T**).

La notice part du montage de la [phase 1](../phase-1/README.md) terminé : capteurs, relais 1 au chauffage, carte ESP32 alimentée par le bloc 12 V.

## Ce qu'il faut

Les repères sont ceux de la phase 1 pour les pièces déjà montées ; les nouvelles pièces prennent les lettres suivantes, sans I ni O. Les pièces **X** et **Y** n'ont pas d'illustration : aucune image sous licence libre ne les montrait fidèlement.

| Repère | Pièce | Ce que c'est, et sa référence d'achat |
| --- | --- | --- |
| **A** | Carte ESP32 | Montée à la phase 1 |
| **E** | Module relais | Monté à la phase 1 ; le relais 4 (`K4`), commandé par `IN4` depuis la broche 33, est encore libre |
| **F** | Bloc 12 V | Monté à la phase 1 |
| **H** | Abaisseur 5 V | Monté à la phase 1, réglé à 5,0 V ; il alimente déjà les bobines des relais, il alimentera aussi le ventilateur |
| **M** | Fil souple | Un fil rouge, assez long pour aller de l'abaisseur (**H**) aux vis du relais 4 |
| **N** | Cordon secteur | Monté à la phase 1 : on le débranche avant d'ouvrir le boîtier |
| **T** | Boîtier électrique | Celui de la phase 1 |
| **X** | Ventilateur | Carré de 80 × 80 mm, 10,8 mm d'épaisseur, 5 V, 0,18 A, avec un câble de 10 cm terminé par une petite fiche à deux fils, rouge et noir, au pas de 1,25 mm. Une flèche moulée sur le cadre montre le sens de l'air. Kubii « Ventilateur 80x80x10.8mm » |
| **Y** | Rallonge du ventilateur | Câble à deux fils, rouge et noir, avec d'un côté une fiche qui s'accouple à celle du ventilateur (**X**), au pas de 1,25 mm (type Molex PicoBlade), et de l'autre des fils nus. Assez long pour aller du couvercle du bac au boîtier (**T**) |

Outils et accessoires, sans repère :

- le multimètre de la phase 1, en testeur de continuité et en voltmètre continu ;
- une grille fine, de la moustiquaire par exemple, pour que les escargots n'atteignent pas les pales, et de quoi fixer le ventilateur au couvercle (vis, colliers) ;
- le firmware : le fichier `.bin` de la version `phase-2`, sur sa page [Releases](https://github.com/sskorupski/ssk-terrarium/releases). Tant que le tag n'est pas posé, celui de `main`, compilé et téléversé par `pio run -e esp32dev -t upload` ([developpement.md](../../developpement.md)).

## Étape 1 — Téléverser le firmware de la phase 2

*Pièces : A.*

Cordon secteur (**N**) et bloc 12 V (**F**) débranchés de la prise murale, carte ESP32 (**A**) reliée à l'ordinateur par l'USB :

1. Téléverser le firmware à l'adresse `0x0`, comme à l'étape 1 de la phase 1.
2. Ouvrir le terminal série, puis appuyer sur le bouton **RESET** de la carte ESP32.

Parmi ses lignes de démarrage, la carte ESP32 écrit :

```
MODE MAINTENANCE au demarrage
…
VENTILATION duree=5min periode=60min heure=13:00 au demarrage
…
=== pret : regulation en cours ===
```

`VENTILATION` donne les réglages du ventilateur : 5 minutes d'aération toutes les 60 minutes, et l'aération quotidienne de l'hibernation à 13:00. Comme à la phase 1, le téléversement efface le mode enregistré : la carte repart en `MAINTENANCE`, où le ventilateur reste coupé. Les réglages de la phase 1 sont effacés de même, Wi-Fi compris : on les refera à l'étape 6.

Débrancher l'USB.

## Étape 2 — Vérifier la voie du ventilateur, à vide

*Pièces : A, E, F, H.*

**Ce qu'on vérifie, et pourquoi.** Le ventilateur sera branché sur le relais 4, qui n'a encore rien commandé. On vérifie, sans ventilateur, que la carte ESP32 l'enclenche et le relâche comme le firmware le prévoit, et l'on repère ses vis.

1. Cordon secteur (**N**) et bloc 12 V (**F**) débranchés, ouvrir le boîtier (**T**). **Repérer les vis du relais 4** comme celles du relais 1 à l'étape 4 de la phase 1 : sur le bord du module, les trois vis à côté du nom `K4`. Celle du milieu est **COM**. Multimètre en testeur de continuité, une pointe sur COM, l'autre sur chaque vis extérieure : celle qui sonne, relais relâché, est **NC** ; l'autre est **NO**. Marquer la vis **NO** au feutre.
2. Brancher la carte ESP32 par l'USB, puis le bloc 12 V sur la prise murale ; le cordon secteur reste débranché. Ouvrir le terminal série. Aucun relais ne s'enclenche.
3. Taper `VENTILATION DUREE 1` puis **Entrée** : la carte répond `VENTILATION duree=1min periode=60min heure=13:00 enregistree, conservee apres coupure`. Une minute d'aération suffit pour l'essai.
4. Taper `MODE ACTIF` puis **Entrée**. Un changement de mode commence par une aération : à la mesure suivante, dans les 30 secondes, **le relais 4 s'enclenche** et la carte écrit `VENTILATION marche`.
5. Environ une minute plus tard, le relais 4 se relâche, et la carte écrit `VENTILATION arret`.
6. Taper `MODE ACTIF` une seconde fois : la carte répond `MODE ACTIF deja actif`, et rien ne se passe. Taper `MODE HIBERNATION`, puis aussitôt `MODE ACTIF` : le changement de mode relance une aération, `VENTILATION marche`. Taper alors `MODE MAINTENANCE` : dans les 30 secondes, le relais 4 se relâche et la carte écrit `VENTILATION arret`.
7. Taper `VENTILATION DUREE 5` pour revenir aux 5 minutes, puis débrancher le bloc 12 V et l'USB.

**Le résultat attendu** : le relais 4 ne s'enclenche qu'avec `VENTILATION marche`, et se relâche avec `VENTILATION arret`. Un relais 4 qui s'enclenche à un autre moment, ou un autre relais qui s'enclenche avec lui : s'arrêter là, et vérifier le fil de `IN4` (étape 5 de la phase 1).

## Étape 3 — Préparer le ventilateur

*Pièces : X, Y.*

1. Enfoncer la fiche du ventilateur (**X**) dans celle de la rallonge (**Y**), jusqu'au clic.
2. Repérer, au bout nu de la rallonge, le fil qui prolonge le **rouge** du ventilateur : multimètre en testeur de continuité, une pointe sur la petite languette métallique du fil rouge, au dos de la fiche du ventilateur, l'autre sur chaque fil nu tour à tour. Celui qui sonne est le **+**. Le marquer au feutre s'il n'est pas rouge. L'autre est le **−**. Un ventilateur branché à l'envers ne tourne pas, et peut griller.
3. Fixer le ventilateur au couvercle du bac, devant une ouverture de sa taille, **la flèche du sens de l'air tournée vers l'extérieur** : il aspire l'air humide du bac et le rejette dehors. Poser la grille fine côté bac, entre les escargots et les pales.
4. Faire passer la rallonge du couvercle au boîtier (**T**), par le même chemin que les câbles des capteurs. Seule de la très basse tension entre dans le bac ou touche son couvercle (INV-4).

## Étape 4 — Brancher le ventilateur

*Pièces : E, H, M, Y.*

Cordon secteur (**N**) et bloc 12 V (**F**) débranchés de la prise murale, câble USB débranché. Les fils de l'abaisseur (**H**) posés à l'étape 4 de la phase 1 restent en place : le Dupont rouge de **VOUT+** vers `JD-VCC`, le noir de **VOUT−** vers `GND`. Les nouveaux fils se serrent sous les mêmes vis, à côté d'eux :

1. Un fil souple rouge (**M**) de la vis **VOUT+** de l'abaisseur à la vis **COM** du relais 4 (celle du milieu).
2. Le fil **+** de la rallonge (**Y**) sur la vis **NO** du relais 4, celle marquée à l'étape 2.
3. Le fil **−** de la rallonge sur la vis **VOUT−** de l'abaisseur.
4. La vis **NC** du relais 4 reste libre.
5. Tirer doucement sur chaque fil, nouveaux comme anciens : aucun ne doit sortir de sous sa vis.
6. **Vérifier avant d'alimenter**, multimètre en testeur de continuité : entre **VOUT+** et la vis **NO** du relais 4, rien ne sonne. Le ventilateur ne reçoit donc rien tant que le relais est relâché, même si la carte ESP32 s'arrête (RG-relais-coupes-sans-commande).

![Ventilateur 5 V sur le relais 4](ventilateur.svg)

Le ventilateur tire 0,18 A sous 5 V. L'abaisseur, qui débite environ 0,3 A pour les bobines des quatre relais, les fournit sans peine.

## Étape 5 — Essayer le ventilateur

*Pièces : A, F, H, X.*

1. Brancher la carte ESP32 (**A**) par l'USB, puis le bloc 12 V (**F**) ; le cordon secteur reste débranché. Ouvrir le terminal série. La carte est en `MAINTENANCE` depuis l'étape 2 : le ventilateur (**X**) ne tourne pas.
2. Taper `MODE ACTIF` puis **Entrée**. Dans les 30 secondes, `VENTILATION marche` : le relais 4 s'enclenche et **le ventilateur tourne**. Une main posée au-dessus du couvercle sent l'air sortir.
3. Pendant qu'il tourne, l'afficheur de l'abaisseur (**H**), basculé sur sa tension de sortie, indique toujours 5,0 V environ. Noter la valeur.
4. Cinq minutes plus tard, `VENTILATION arret` : le ventilateur s'arrête. Le suivant tournera une heure après le premier.
5. Taper `MODE MAINTENANCE` pendant une aération : le ventilateur s'arrête dans les 30 secondes.
6. **Hibernation** : taper `MODE HIBERNATION`. Le ventilateur ne tourne pas à l'entrée dans le mode : il tournera une fois par jour, à 13:00 si la carte connaît l'heure, sinon 24 heures après l'entrée dans le mode. Pour le voir sans attendre, une fois le Wi-Fi configuré et l'heure connue (`PLAGES` l'affiche), taper `VENTILATION HEURE` suivi de l'heure qu'il est, par exemple `VENTILATION HEURE 15:42` : le ventilateur tourne à la mesure suivante. Remettre ensuite `VENTILATION HEURE 13:00`.

## Étape 6 — Remettre en service

*Pièces : A, F, N, T.*

1. Débrancher le bloc 12 V et l'USB, refermer le boîtier (**T**), puis rebrancher le cordon secteur (**N**) et le bloc 12 V.
2. Taper le mode voulu, `MODE ACTIF` pour l'élevage ou `MODE HIBERNATION` pour la mise au repos. Si le Wi-Fi servait, le reconfigurer ([supervision.md](../../supervision.md)) ; refaire de même les plages et le fuseau du mode `PLAGES` s'ils avaient été changés ([mode-plages.md](../../mode-plages.md)).
3. Taper `VENTILATION` : la carte répond `VENTILATION duree=5min periode=60min heure=13:00 actuelle`, ou les réglages choisis.

## Fin de la ventilation

La ventilation est montée quand les six étapes donnent ce qu'elles annoncent. Reporter dans le cadrage [ssk-terrarium-cadrage#2026-013](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-013) la tension de l'abaisseur mesurée à l'étape 5 avec le ventilateur en marche.

Les valeurs de 5 minutes par heure restent à éprouver : les lignes `MESURE` montrent l'humidité du bac avant et après chaque aération. Une humidité qui chute trop appelle une aération plus courte ou plus rare ; un air qui reste saturé, l'inverse. Les réglages se changent sans rien rebrancher, par `VENTILATION DUREE`, `VENTILATION PERIODE` et `VENTILATION HEURE`, ou à distance ([ventilation.md](../../ventilation.md)), dans les bornes du firmware : 1 à 30 minutes toutes les 15 à 240 minutes.
