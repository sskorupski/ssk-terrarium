# Invariants

Ces règles ne se négocient pas. Chacune se cite par son numéro, en revue, en message de commit ou en commentaire. Les quatre premières sont les règles de sécurité posées par le propriétaire ; le cadrage [ssk-terrarium-cadrage#2026-001](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-001) les détaille.

## INV-1 — Toutes les sorties sont coupées au démarrage

Chauffage, éclairage, pompe et ventilateur passent à l'état coupé avant toute autre opération de `setup()` (RG-sorties-coupees-au-demarrage), et une broche non pilotée laisse chaque relais ouvert (RG-relais-coupes-sans-commande).

- **Ce qu'il protège** : un redémarrage, un plantage ou une initialisation bloquée n'allume rien.
- **Sans lui** : un reset pendant que le chauffage chauffe le laisserait allumé, sans régulation, indéfiniment.

## INV-2 — Un capteur d'air en défaut coupe le chauffage et la pompe

Un capteur muet ou aberrant plus de 2 minutes est en défaut : chauffage et pompe sont coupés, le défaut est signalé par la LED d'état et la liaison série (RG-capteur-en-defaut). Pendant ces 2 minutes, aucune décision n'est prise sur une mesure invalide : les sorties gardent leur état.

- **Ce qu'il protège** : les animaux, d'une régulation qui chauffe ou brumise à l'aveugle.
- **Sans lui** : un fil du bus I2C qui se débranche laisserait le chauffage dans son dernier état, indéfiniment.

## INV-3 — La régulation ne dépend jamais du réseau

Mesure, décision et pilotage des sorties tournent sur l'ESP32 seul. Le portail, MQTT et les alertes supervisent, depuis leur propre tâche ; rien de la boucle de régulation n'attend une réponse du réseau (RG-regulation-sans-reseau). Une commande reçue du réseau entre dans une file que la boucle lit sans attendre.

- **Ce qu'il protège** : une box qui redémarre ou un Wi-Fi absent ne changent rien au climat du terrarium.
- **Sans lui** : un appel réseau bloquant suffit à figer la boucle, et le chien de garde à redémarrer la carte en boucle.

## INV-4 — Aucun élément sous 230 V dans le bac

Les câbles chauffants sont à l'extérieur, contre les vitres et sous l'isolant ; n'entrent dans le bac que les capteurs et des éléments en très basse tension (RG-230v-hors-du-bac).

- **Ce qu'il protège** : ni l'eau du bac ni une main qui y travaille n'approchent le 230 V.
- **Sans lui** : un câble abîmé ou une fuite de brumisation mettrait le 230 V au contact d'un bac humide.

Le limiteur matériel à 24 °C que cet invariant imposait jusqu'ici est reporté après la phase 1, par décision du propriétaire : décision `06-protection-contre-la-surchauffe` du cadrage, [#9](https://github.com/sskorupski/ssk-terrarium/issues/9). Tant qu'il manque, rien de matériel ne coupe le chauffage si le relais 1 reste collé ou si le firmware se trompe : la seule protection contre la surchauffe est le code.

## INV-5 — Les limites de sécurité ne se règlent pas à l'exécution

Durée maximale d'une impulsion de brumisation, de chauffe continue, de silence d'un capteur, et bornes des réglages reçus du réseau (consigne de 15 à 22 °C, hystérésis de 0,2 à 2 °C, arrêt du chauffage sous 24 °C) : ce sont des `constexpr` de `include/config.h`, jamais des réglages. Un réglage reçu par MQTT ou relu en NVS est refusé hors de ces bornes (RG-reglages-bornes).

- **Ce qu'il protège** : une page web ou un message MQTT erroné ne peut pas lever une protection.
- **Sans lui** : une faute de frappe dans un réglage suffirait à noyer ou cuire le terrarium.

## INV-6 — La logique de régulation ne touche pas au matériel

`lib/control` ne contient que de la logique pure : ni `Arduino.h`, ni broche, ni allocation. Elle se teste sur l'ordinateur (`pio test -e native`).

- **Ce qu'il protège** : chaque règle de régulation et de protection a un test qui tourne en secondes, sans carte.
- **Sans lui** : une règle de sécurité ne s'éprouverait qu'en simulant une panne sur le vrai montage.

## INV-7 — Toute version est épinglée

Plateforme, bibliothèques et outils de CI sont fixés à une version exacte dans `platformio.ini` et `requirements-ci.txt`.

- **Ce qu'il protège** : une compilation ne change pas parce qu'un registre a publié quelque chose.
- **Sans lui** : le même commit pourrait produire deux firmwares différents, dont un jamais testé.

## Changer un invariant

Un invariant change par une décision explicite du propriétaire, consignée dans le cadrage, puis ici, dans la même demande de fusion que le code qu'elle autorise. Jamais par contournement, même bien intentionné.
