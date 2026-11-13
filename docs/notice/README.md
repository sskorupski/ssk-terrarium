# Notices de montage

Le régulateur se construit phase par phase. Chaque phase ajoute du code et des branchements, et se livre avec une notice pas à pas : ce qu'il faut, dans quel ordre brancher, comment téléverser le firmware, et ce qu'on doit observer pour savoir que tout va bien.

## Quelle phase suivre

| Phase | Ce qu'elle apporte | Version | Notice |
| --- | --- | --- | --- |
| 0 | La carte ESP32 seule, programmée, et sa LED d'état | `phase-0` | [phase-0](phase-0/README.md) |
| 1 | Capteurs, chauffage, hors-gel et protections : le prototype d'hiver | `phase-1` | [phase-1](phase-1/README.md) (en validation sur le montage) |
| 2 | Ventilation, câble chauffant de l'air, page web locale | `phase-2` | [phase-2](phase-2/README.md) (en cours : ventilation et câble de l'air) |
| 3 | Supervision à distance : MQTT, alertes, mise à jour par le réseau | `phase-3` | à venir |
| 4 | Brumisation automatique | `phase-4` | à venir |
| 5 | Historique et courbes | `phase-5` | à venir |

Les phases se suivent dans l'ordre : la notice d'une phase part du montage terminé de la précédente. Le pourquoi de chaque choix est dans le cadrage du projet, [ssk-terrarium-cadrage](https://github.com/ssk-it/ssk-terrarium-cadrage).

## Une version par phase

Chaque phase terminée porte un tag Git, `phase-N`. Un correctif qui ne change pas le montage porte `phase-N.1`, `phase-N.2`… Le tag fige ensemble le firmware et la notice de la phase : on lit toujours la notice **du tag qu'on téléverse**, jamais celle de `main`, qui décrit peut-être une phase pas encore terminée.

Chaque tag a sa page sur [Releases](https://github.com/sskorupski/ssk-terrarium/releases), avec deux fichiers :

- `ssk-terrarium-phase-N.bin` (ou `phase-N.1`…) : le firmware prêt à téléverser, en une seule image à l'adresse `0x0` ;
- `notice-phase-N.pdf` : la notice, schémas compris, à imprimer pour l'atelier.

## Sécurité, à chaque phase

- Rien ne se branche sous tension : on débranche le bloc 12 V et le cordon secteur avant de toucher un fil.
- Le 230 V n'arrive qu'à la phase 1, et seulement après les vérifications au multimètre que sa notice décrit.
- Aucun élément sous 230 V n'entre dans le bac : les câbles chauffants restent à l'extérieur, contre les vitres et sous l'isolant.
- Le limiteur matériel à 24 °C et le fusible du chauffage sont reportés après la phase 1 ([#9](https://github.com/sskorupski/ssk-terrarium/issues/9), [#10](https://github.com/sskorupski/ssk-terrarium/issues/10)) : tant qu'ils manquent, rien de matériel ne coupe le chauffage si le relais 1 reste collé.
