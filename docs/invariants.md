# Invariants

Ces règles ne se négocient pas. Chacune se cite par son numéro, en revue, en message de commit ou en commentaire. Les quatre premières sont les règles de sécurité posées par le propriétaire ; le cadrage [ssk-terrarium-cadrage#2026-001](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-001) les détaille.

## INV-1 — Toutes les sorties sont coupées au démarrage

Chauffage, éclairage, pompe et ventilateur passent à l'état coupé avant toute autre opération de `setup()` (RG-sorties-coupees-au-demarrage), et une broche non pilotée laisse chaque relais ouvert (RG-relais-coupes-sans-commande).

- **Ce qu'il protège** : un redémarrage, un plantage ou une initialisation bloquée n'allume rien.
- **Sans lui** : un reset pendant que le chauffage chauffe le laisserait allumé, sans régulation, jusqu'au limiteur.

## INV-2 — Un capteur d'air en défaut coupe le chauffage et la pompe

Un capteur muet ou aberrant plus de 2 minutes est en défaut : chauffage et pompe sont coupés, le défaut est signalé par la LED d'état et la liaison série (RG-capteur-en-defaut). Pendant ces 2 minutes, aucune décision n'est prise sur une mesure invalide : les sorties gardent leur état.

- **Ce qu'il protège** : les animaux, d'une régulation qui chauffe ou brumise à l'aveugle.
- **Sans lui** : un fil du bus I2C qui se débranche laisserait le chauffage dans son dernier état, indéfiniment.

## INV-3 — La régulation ne dépend jamais du réseau

Mesure, décision et pilotage des sorties tournent sur l'ESP32 seul. La page web, MQTT et les alertes supervisent ; rien de la boucle de régulation n'attend une réponse du réseau (RG-regulation-sans-reseau).

- **Ce qu'il protège** : une box qui redémarre ou un Wi-Fi absent ne changent rien au climat du terrarium.
- **Sans lui** : un appel réseau bloquant suffit à figer la boucle, et le chien de garde à redémarrer la carte en boucle.

## INV-4 — Le limiteur matériel à 24 °C reste en place

Le thermostat Exo Terra, réglé à 24 °C, est en série des câbles chauffants, hors du firmware (RG-limiteur-materiel). Aucune évolution du code ne justifie de le retirer.

- **Ce qu'il protège** : les animaux, d'un relais collé ou d'un défaut du firmware.
- **Sans lui** : la seule protection contre la surchauffe serait le code qu'elle doit justement rattraper.

## INV-5 — Les limites de sécurité ne se règlent pas à l'exécution

Durée maximale d'une impulsion de brumisation, de chauffe continue, de silence d'un capteur : ce sont des `constexpr` de `include/config.h`, jamais des réglages. Les réglages en NVS sont bornés par elles avant d'être acceptés.

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
