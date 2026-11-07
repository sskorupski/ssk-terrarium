# Phase 1 — Le prototype d'hiver (en préparation)

Cette notice s'écrit avec la phase 1 et se fige avec le tag `phase-1`. Tant que ce tag n'existe pas, **rien de ce qui suit ne se réalise** : le niveau de déclenchement du module relais n'est pas encore mesuré, et c'est de lui que dépend la sûreté des sorties.

Les étapes prévues, d'après le cadrage 2026-001, dans l'ordre où elles se feront :

1. Régler l'abaisseur LM2596 à 5,0 V, à vide, avant de lui brancher quoi que ce soit.
2. Vérifier au multimètre la polarité du bloc 12 V (centre +) avant de le brancher à la carte.
3. Retirer le cavalier JD-VCC du module relais ; VCC sur le 3,3 V de la carte, JD-VCC sur le 5 V du LM2596.
4. Mesurer le niveau de déclenchement du module, et vérifier qu'aucun relais ne colle carte en reset, au démarrage et sans firmware.
5. Brancher le capteur d'air SHT45 (I²C) et la sonde du substrat DS18B20 (1-Wire).
6. Brancher les commandes des quatre relais : chauffage, LED, pompe, ventilateur.
7. Câbler le 230 V : relais de chauffage, limiteur Exo Terra réglé à 24 °C, multiprise, deux câbles chauffants.
8. Mise en service : modes, hors-gel, défaut capteur, mesure de la vitre côté intérieur.

Chaque étape aura son schéma WireViz, sa procédure et ce qu'on doit observer.
