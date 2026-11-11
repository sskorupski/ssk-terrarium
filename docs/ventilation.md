# Ventilation, jour et nuit

Le ventilateur 5 V, sur le relais 4 (GPIO 33), renouvelle l'air confiné et saturé du bac sans le dessécher (RG-ventilation-periodique, cadrages [ssk-terrarium-cadrage#2026-013](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-013) et [ssk-terrarium-cadrage#2026-015](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-015)). Le régulateur distingue le jour et la nuit (RG-jour-et-nuit) : la ventilation, et la consigne de chauffage en `ACTIF` ([ssk-terrarium-cadrage#2026-016](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-016)), ont chacune leurs réglages de jour et de nuit.

## Le jour et la nuit

Deux modes :

- `metronome` : le même jour tous les jours, `"08:00-21:00"` par défaut ;
- `saisons` : de 1 à 6 saisons, chacune avec sa date de début `MM-JJ` et son jour `HH:MM-HH:MM`. Entre le début d'une saison et celui de la suivante, le début et la fin du jour glissent jour après jour, en ligne droite, arrondis à la minute ; les saisons bouclent sur l'année. Les jours se comptent sur 365 jours : le 29 février prend le jour du 28.

Un jour ne passe pas minuit, dure au moins une heure et laisse une nuit d'au moins une heure. Tant que l'heure n'est pas connue, c'est le jour.

## Ce que fait le ventilateur

| Mode | Ventilateur |
| --- | --- |
| `ACTIF`, `PLAGES` | Le rythme du moment, `jour` ou `nuit` |
| `HIBERNATION` | Une aération par jour, de jour comme de nuit : à `hibernation.heure` si l'heure est connue, sinon 24 h après la précédente ; jamais deux à moins de 12 h |
| `MAINTENANCE` | Coupé, même au milieu d'une aération, option saturation comprise |

Chaque rythme, `jour` et `nuit`, a un mode :

- `metronome` : chaque aération dure `duree` minutes, la suivante commence `periode` minutes après le début de la précédente ;
- `rubato` : la durée et l'intervalle sont tirés au hasard dans ±`ecart` % autour de `duree` et `periode`, arrondis à la minute, puis ramenés dans les bornes, la durée restant sous l'intervalle.

La première aération commence au démarrage et à chaque changement de mode. La durée d'une aération et l'intervalle jusqu'à la suivante se fixent à son début : un nouveau réglage, ou le passage du jour à la nuit, vaut à partir de l'aération suivante.

**L'option saturation**, inactive par défaut, vaut en `ACTIF`, `PLAGES` et `HIBERNATION`. Quand toutes les mesures de l'air depuis `attente` minutes sont à `humidite` % HR ou plus, elle fait tourner le ventilateur, selon `comportement` : `aeration`, pendant `duree` minutes ; `continu`, tant que l'air reste saturé, au plus `duree` minutes. Puis rien pendant `pause` minutes. Une mesure de l'air manquante la fait taire.

Par défaut : jour et nuit en `metronome`, 5 minutes toutes les 60, écart 30 % ; hibernation à 13:00 pendant 5 minutes ; saturation inactive, `aeration`, 95 %, 10, 5 et 60 minutes.

## La consigne de nuit

En `ACTIF`, le chauffage suit la nuit `chauffage_nuit`, et le jour `consigne` et `hysteresis`, qui gardent leur sens ; sans heure, celle du jour. `PLAGES` chauffe sur celle du jour en plage. Par défaut, la nuit vaut 19 °C ± 0,5, comme le jour.

## Régler

Bornes fixées dans `include/config.h` (RG-reglages-bornes) : durée de 1 à 30 minutes, période de 15 à 240, durée sous la période, écart de 10 à 50 %, humidité de 80 à 100 %, attente de 1 à 60 minutes, pause de 15 à 240 ; consigne de nuit bornée comme celle du jour. Les réglages s'enregistrent et se reprennent au démarrage s'ils respectent les bornes du firmware en place (RG-mode-au-demarrage) ; un réglage de ventilation enregistré par un firmware de 2026-013 n'est pas repris, et prend les valeurs par défaut, qui lui sont égales.

Sur `cmd/config` ([supervision.md](supervision.md)), et sur la liaison série par `CONFIG` suivi du même JSON :

```bash
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"ventilation": {"nuit": {"mode": "rubato", "periode": 120}, "saturation": {"active": true}}}'
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"jour_nuit": {"mode": "saisons", "saisons": [{"debut": "03-20", "jour": "07:00-20:00"}, {"debut": "06-21", "jour": "06:00-22:00"}, {"debut": "09-22", "jour": "07:30-19:30"}, {"debut": "12-21", "jour": "08:30-17:30"}]}}'
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"chauffage_nuit": {"consigne": 17}}'
```

```
CONFIG {"ventilation":{"jour":{"duree":1}}}
```

`ventilation` est partiel à deux niveaux : `jour`, `nuit` (`mode`, `duree`, `periode`, `ecart`), `hibernation` (`heure`, `duree`), `saturation` (`active`, `comportement`, `humidite`, `attente`, `duree`, `pause`). `jour_nuit` se donne en entier et remplace celui en place. `chauffage_nuit` est partiel. La réponse arrive sur `event` : `{"evenement":"reglage","champ":"ventilation","resultat":"applique"}`, ou `"resultat":"refuse","raison":"ventilation invalide"` ; `jour_nuit invalide` et `chauffage_nuit invalide` pour une commande mal formée ; `{"evenement":"reglage","chauffage_nuit":{…},"resultat":"refuse","raison":"hors bornes"}` pour une consigne de nuit hors bornes.

`state` publie `jour_nuit`, `jour_aujourdhui` (`null` sans heure), `moment` (`jour` ou `nuit`), `ventilation`, `aeration_saturation`, `chauffage_nuit`, et dans `bornes` : `saisons_max` et `ventilation` (`duree`, `periode`, `ecart`, `humidite`, `attente`, `pause`). Ce que fait le ventilateur à l'instant se lit dans `sorties.ventilateur`.

## Ce que la liaison série écrit

- au démarrage, `VENTILATION jour metronome duree=5min periode=60min ecart=30%`, la même ligne pour `nuit`, `VENTILATION hibernation heure=13:00 duree=5min`, `VENTILATION saturation inactive aeration humidite=95% attente=10min duree=5min pause=60min au demarrage`, `JOUR metronome 08:00-21:00 au demarrage`, `JOUR heure inconnue : jour` et `CONSIGNE nuit 19.0C hysteresis 0.5C au demarrage` ;
- `VENTILATION marche` et `VENTILATION arret` à chaque mise en marche et chaque arrêt, suivis de `(saturation)` quand c'est l'option qui les cause ;
- `VENTILATION` et `JOUR` seuls affichent les réglages en place, et `JOUR aujourd'hui 06:30-20:59, jour` une fois l'heure connue ;
- après un réglage : les mêmes lignes suivies de `: enregistree` (ou `: enregistre`), et `CONSIGNE nuit 17.0C hysteresis 0.5C enregistree` ;
- `REFUS ventilation : hors bornes, ou une duree pas sous sa periode`, `REFUS consigne nuit … hors bornes`, `REFUS CONFIG : <raison>` pour un JSON mal formé ;
- `INFO ventilation enregistree illisible ou hors bornes, valeurs du firmware`, `INFO jour et nuit enregistres illisibles, ceux du firmware`, `INFO consigne de nuit enregistree hors bornes, valeurs du firmware` au démarrage.
