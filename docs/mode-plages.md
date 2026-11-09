# Le mode PLAGES

`PLAGES` chauffe sur la consigne pendant des plages horaires, et ne chauffe en dehors que pour tenir l'air entre 12 et 14 °C. Il s'ajoute à `ACTIF` (consigne jour et nuit), `HIBERNATION` (hors-gel 4-6 °C) et `MAINTENANCE` (tout coupé). Le pourquoi est dans le cadrage [ssk-terrarium-cadrage#2026-003](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-003).

## Ce que fait le chauffage

| Situation | Chauffage |
| --- | --- |
| Dans une plage | Comme `ACTIF` : marche sous consigne − hystérésis, arrêt au-dessus de consigne + hystérésis (19 °C ± 0,5 par défaut) |
| Hors de toute plage | Marche sous 12 °C, arrêt à 14 °C. Ces deux seuils ne se règlent pas |
| Heure pas encore connue depuis le démarrage | Comme dans une plage : sans réseau, le bac est chauffé sur la consigne, jamais laissé au plancher |

Le capteur d'air en défaut coupe le chauffage, et la chauffe continue reste bornée à 2 h suivies de 15 min de coupure, en plage comme hors plage. L'éclairage et la ventilation suivent ce qu'ils font en `ACTIF`.

L'état est réévalué à chaque mesure, toutes les 30 s : une entrée ou une sortie de plage prend effet dans les 30 s qui suivent l'heure dite.

## L'heure

La carte n'a pas d'horloge sauvegardée : elle demande l'heure par NTP (`fr.pool.ntp.org`, puis `pool.ntp.org`) dès qu'elle a rejoint le Wi-Fi après un démarrage, puis une fois par jour. Entre deux synchronisations, et si le réseau tombe, l'horloge interne continue. L'heure se perd à chaque coupure de courant ou redémarrage.

Le fuseau est une chaîne POSIX TZ ; par défaut celui de Paris, heure d'été comprise : `CET-1CEST,M3.5.0,M10.5.0/3`. Quelques autres : `UTC0`, `GMT0BST,M3.5.0/1,M10.5.0` (Londres), `EST5EDT,M3.2.0,M11.1.0` (New York). Une chaîne que la carte ne sait pas lire est refusée plutôt qu'interprétée comme UTC. Une heure d'été doit donner ses deux dates, au format `Mm.s.j`.

## Les plages

Jusqu'à 4 plages, au format `HH:MM-HH:MM`, dans le fuseau réglé. Le début est compris, la fin non : `14:05-17:05` chauffe de 14 h 05 à 17 h 04. Une plage peut passer minuit (`22:00-02:00`). Des plages peuvent se recouvrir. Une plage dont le début égale la fin est refusée. Sans aucune plage, la carte est toujours hors plage, une fois l'heure connue.

Le firmware part avec deux plages : `01:35-06:30` et `14:05-17:05`.

## Régler par la liaison série

| Commande | Effet |
| --- | --- |
| `MODE PLAGES` | Passe en mode `PLAGES`, enregistré comme les autres modes |
| `PLAGES` | Affiche les plages, le fuseau et l'heure |
| `PLAGES 01:35-06:30 14:05-17:05` | Remplace les plages, séparées par une seule espace |
| `PLAGES AUCUNE` | Retire toutes les plages |
| `FUSEAU` | Affiche les plages, le fuseau et l'heure |
| `FUSEAU CET-1CEST,M3.5.0,M10.5.0/3` | Remplace le fuseau |

Réponses :

```
PLAGES 01:35-06:30 14:05-17:05 enregistrees, conservees apres coupure
PLAGES actuelles 01:35-06:30 14:05-17:05, fuseau CET-1CEST,M3.5.0,M10.5.0/3, heure 14:12
REFUS plages : 4 au plus, au format HH:MM-HH:MM separees par une espace, debut different de la fin, ou AUCUNE
FUSEAU UTC0 enregistre, conserve apres coupure
REFUS fuseau : chaine POSIX TZ attendue, par exemple CET-1CEST,M3.5.0,M10.5.0/3
```

## Régler par MQTT

Sur `cmd/config`, à côté de `consigne` et `hysteresis` ([supervision.md](supervision.md)) :

```bash
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"plages": ["01:35-06:30", "14:05-17:05"]}'
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"fuseau": "CET-1CEST,M3.5.0,M10.5.0/3"}'
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/mode' -m PLAGES
```

`plages` remplace toute la liste ; `[]` les retire toutes. Une plage invalide, plus de 4 plages ou un fuseau illisible refusent la commande en entier, tout comme une consigne hors bornes envoyée avec elle. La réponse arrive sur `event` : `{"evenement":"reglage","plages":["01:35-06:30","14:05-17:05"],"resultat":"applique"}`, ou `{"evenement":"commande","sujet":"cmd/config","resultat":"refusee","raison":"plages invalides"}`.

`state` publie en plus `plages`, `fuseau`, `heure_connue`, et `en_plage` (`null` tant que l'heure n'est pas connue) ; ses `bornes` donnent `plages_max` et `fuseau_max` ([supervision.md](supervision.md)).

## Suivre dans le journal

```
PLAGES 01:35-06:30 14:05-17:05 au demarrage, fuseau CET-1CEST,M3.5.0,M10.5.0/3
PLAGES heure inconnue : chauffage sur la consigne 19.0C jusqu'a l'heure NTP
HEURE demande NTP a fr.pool.ntp.org, puis une fois par jour
HEURE connue par NTP : 2026-11-08 14:12:03, fuseau CET-1CEST,M3.5.0,M10.5.0/3
PLAGES en plage a 14:12 : chauffage sur la consigne 19.0C
MESURE air=18.2C hr=84% substrat=19.0C chauffage=1 mode=PLAGES heure=14:12
PLAGES hors plage a 17:05 : chauffage seulement sous 12C, arret a 14C
HEURE resynchronisee par NTP : 2026-11-09 14:12:04, fuseau CET-1CEST,M3.5.0,M10.5.0/3
```

Les lignes `PLAGES en plage` et `PLAGES hors plage` ne s'écrivent qu'en mode `PLAGES`, à chaque changement, et après un changement de plages ou de fuseau. La ligne `MESURE` porte l'heure locale dans tous les modes, `heure=--` tant qu'elle n'est pas connue.

## Essayer sans attendre

Pour voir les deux régimes en quelques minutes, régler une plage courte qui commence deux minutes plus tard, par exemple à 14 h 10 : `PLAGES 14:12-14:15`, puis `MODE PLAGES`. La carte annonce `hors plage`, puis `en plage` à 14 h 12, puis `hors plage` à 14 h 15. Remettre ensuite les plages voulues.
