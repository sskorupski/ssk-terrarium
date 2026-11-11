# Ventilation

Le ventilateur 5 V, sur le relais 4 (GPIO 33), renouvelle l'air confiné et saturé du bac sans le dessécher (RG-ventilation-periodique, cadrage [ssk-terrarium-cadrage#2026-013](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-013)). Il ne dépend d'aucune mesure : le défaut d'une sonde, la mise en sécurité et la coupure par le substrat n'y changent rien.

| Mode | Ventilateur |
| --- | --- |
| `ACTIF`, `PLAGES` | Il tourne les `duree` premières minutes de chaque `periode`, comptée depuis le démarrage ou le dernier changement de mode : un démarrage ou un changement de mode commence donc par une aération |
| `HIBERNATION` | Une aération de `duree` minutes par jour. Heure connue : elle commence à `heure`, dans le fuseau réglé. Heure inconnue : 24 h après le début de la précédente, ou après le démarrage ou l'entrée dans le mode. Jamais deux aérations à moins de 12 h d'intervalle |
| `MAINTENANCE` | Coupé, même au milieu d'une aération |

Par défaut : 5 minutes toutes les 60 minutes, et l'aération d'hibernation à 13:00. Un réglage accepté s'applique dès la mesure suivante, sans relancer le décompte : une aération en cours s'arrête dès qu'elle atteint la nouvelle durée.

## Régler

Bornes fixées dans `include/config.h` (RG-reglages-bornes) : durée de 1 à 30 minutes, période de 15 à 240 minutes, en minutes entières, la durée strictement sous la période, et une heure `HH:MM`. Le réglage est enregistré, et repris au démarrage s'il respecte les bornes du firmware en place (RG-mode-au-demarrage).

Sur la liaison série :

```
VENTILATION                 réglage en place
VENTILATION DUREE 10        durée d'une aération, en minutes
VENTILATION PERIODE 90      une aération toutes les 90 minutes en ACTIF et PLAGES
VENTILATION HEURE 14:30     heure de l'aération quotidienne d'HIBERNATION
```

Sur `cmd/config`, un objet partiel `ventilation` ([supervision.md](supervision.md)) :

```bash
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"ventilation": {"duree": 10, "heure": "14:30"}}'
```

La réponse arrive sur `event` : `{"evenement":"reglage","ventilation":{"duree":10,"periode":60,"heure":"14:30"},"resultat":"applique"}`. Un champ inconnu, une valeur non entière, une heure illisible, une valeur hors bornes ou une durée qui n'est pas sous la période refusent la commande en entier, avec la raison `ventilation invalide`.

`state` publie le réglage en place, `"ventilation":{"duree":5,"periode":60,"heure":"13:00"}`, ses bornes dans `"bornes":{…,"ventilation":{"duree":[1,30],"periode":[15,240]}}`, et ce que fait le ventilateur à l'instant dans `sorties.ventilateur`.

## Ce que la liaison série écrit

- `VENTILATION duree=5min periode=60min heure=13:00 au demarrage`, ou `INFO ventilation enregistree hors bornes, valeurs du firmware` avant elle ;
- `VENTILATION marche` et `VENTILATION arret` à chaque mise en marche et à chaque arrêt ;
- `VENTILATION duree=10min periode=60min heure=13:00 enregistree, conservee apres coupure` après un réglage accepté, ou `… appliquee mais non enregistree` si l'écriture en mémoire échoue ;
- `REFUS ventilation : duree 1-30 min, periode 15-240 min, duree sous la periode, heure HH:MM` pour un réglage hors bornes, et `REFUS ventilation : VENTILATION DUREE <minutes>, …` pour une ligne mal formée.
