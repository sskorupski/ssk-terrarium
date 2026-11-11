# Invariants

Ces règles ne se négocient pas. Chacune se cite par son numéro, en revue, en message de commit ou en commentaire. Les quatre premières sont les règles de sécurité posées par le propriétaire ; le cadrage [ssk-terrarium-cadrage#2026-001](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-001) les détaille.

## INV-1 — Toutes les sorties sont coupées au démarrage

Chauffage, éclairage, pompe et ventilateur passent à l'état coupé avant toute autre opération de `setup()` (RG-sorties-coupees-au-demarrage), et une broche non pilotée laisse chaque relais ouvert (RG-relais-coupes-sans-commande). Cela vaut aussi au redémarrage qui suit une mise à jour à distance, et à celui qui revient à l'ancien firmware (INV-8).

- **Ce qu'il protège** : un redémarrage, un plantage ou une initialisation bloquée n'allume rien.
- **Sans lui** : un reset pendant que le chauffage chauffe le laisserait allumé, sans régulation, indéfiniment.

## INV-2 — Sans sonde intérieure acceptée, chauffage et pompe sont coupés, tant que la mise en sécurité est active

Une mesure est rejetée quand la sonde est muette, ou quand elle sort de −20 à 60 °C ou de 0 à 100 % d'humidité (RG-capteur-en-defaut). Une sonde rejetée plus de 2 minutes est en défaut, et ce défaut est signalé sur la liaison série et le courtier, avec sa cause ; pour une sonde intérieure, aussi par la LED d'état. Le défaut d'une sonde ne coupe rien à lui seul : tant qu'une autre sonde intérieure est acceptée, la régulation continue sur elle (RG-sondes-d-air, cadrage [ssk-terrarium-cadrage#2026-007](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-007)).

Quand aucune sonde intérieure n'est acceptée depuis plus de 2 minutes, l'air intérieur est en défaut : chauffage et pompe sont coupés, et le défaut est signalé par la LED, la liaison série et le courtier. Pendant ces 2 minutes, aucune décision n'est prise sans mesure de l'air : les sorties gardent leur état. Au moins une sonde reste toujours intérieure : un réglage qui n'en laisserait aucune est refusé (INV-5).

La mise en sécurité est active par défaut. Seul le propriétaire la désactive, par `cmd/config` (`mise_en_securite`), et ce choix est enregistré et publié : décision `03-lever-la-mise-en-securite` du cadrage [ssk-terrarium-cadrage#2026-006](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-006), prise le 9 novembre 2026 en connaissance du risque. Elle porte sur l'air intérieur, toutes sondes intérieures ensemble. Désactivée, la régulation se poursuit sur la mesure rejetée de la sonde intérieure prioritaire, ou garde sa dernière décision si toutes sont muettes ; la durée maximale de chauffe continue s'applique toujours.

- **Ce qu'il protège** : les animaux, d'une régulation qui chauffe ou brumise à l'aveugle.
- **Sans lui** : un fil du bus I2C qui se débranche, sans autre sonde intérieure pour prendre le relais, laisserait le chauffage dans son dernier état, indéfiniment. C'est exactement ce qu'accepte le propriétaire qui désactive la mise en sécurité.

## INV-3 — La régulation ne dépend jamais du réseau

Mesure, décision et pilotage des sorties tournent sur l'ESP32 seul. Le portail, MQTT et les alertes supervisent, depuis leur propre tâche ; rien de la boucle de régulation n'attend une réponse du réseau (RG-regulation-sans-reseau). Une commande reçue du réseau entre dans une file que la boucle lit sans attendre. Le téléchargement d'une mise à jour tourne dans sa propre tâche, sur le cœur 0 : pendant ce temps, la boucle mesure, régule et nourrit le chien de garde comme d'habitude (RG-regulation-pendant-la-mise-a-jour). Le mode `PLAGES` lit l'heure que le NTP a donnée, sans jamais l'attendre : tant qu'elle n'est pas connue, il chauffe sur la consigne comme `ACTIF`, si bien que l'absence de réseau ne peut ni couper le chauffage ni l'abaisser au plancher de 12 °C (RG-chauffage-par-plages).

- **Ce qu'il protège** : une box qui redémarre ou un Wi-Fi absent ne changent rien au climat du terrarium.
- **Sans lui** : un appel réseau bloquant suffit à figer la boucle, et le chien de garde à redémarrer la carte en boucle.

## INV-4 — Aucun élément sous 230 V dans le bac

Les câbles chauffants sont à l'extérieur, contre les vitres et sous l'isolant ; n'entrent dans le bac que les capteurs et des éléments en très basse tension (RG-230v-hors-du-bac).

- **Ce qu'il protège** : ni l'eau du bac ni une main qui y travaille n'approchent le 230 V.
- **Sans lui** : un câble abîmé ou une fuite de brumisation mettrait le 230 V au contact d'un bac humide.

Le limiteur matériel à 24 °C que cet invariant imposait jusqu'ici est reporté après la phase 1, par décision du propriétaire : décision `06-protection-contre-la-surchauffe` du cadrage, [#9](https://github.com/sskorupski/ssk-terrarium/issues/9). Tant qu'il manque, rien de matériel ne coupe le chauffage si le relais 1 reste collé ou si le firmware se trompe : la seule protection contre la surchauffe est le code, coupure par le substrat comprise (RG-coupure-par-le-substrat).

## INV-5 — Les limites de sécurité ne se règlent pas à l'exécution

Durée maximale d'une impulsion de brumisation, de chauffe continue, de silence d'un capteur, et bornes des réglages reçus du réseau (consigne de 15 à 22 °C, hystérésis de 0,2 à 2 °C, arrêt du chauffage sous 24 °C ; ventilation de 1 à 30 minutes toutes les 15 à 240 minutes, durée sous la période), plancher hors plage du mode `PLAGES` (marche sous 12 °C, arrêt à 14 °C), seuils de la coupure par le substrat (coupure à 24 °C, reprise sous 18,5 °C, RG-coupure-par-le-substrat), plages de rejet d'une mesure de l'air, écart minimal de 12 h et repli de 24 h de l'aération d'hibernation, seuil, durée et intervalle minimal de la chauffe de la SHT45 (95 %, 1 s, 5 minutes, RG-chauffe-de-la-sht45) : ce sont des `constexpr` de `include/config.h` ou de `lib/control`, jamais des réglages. Un réglage reçu par MQTT ou la liaison série, ou relu en NVS, est refusé hors de ces bornes (RG-reglages-bornes). Le rôle des sondes et leur combinaison se règlent (cadrage [ssk-terrarium-cadrage#2026-007](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-007)), mais jamais au point de ne laisser aucune sonde intérieure.

- **Ce qu'il protège** : une page web ou un message MQTT erroné ne peut pas lever une protection. Une seule exception, décidée par le propriétaire : la mise en sécurité d'une sonde (INV-2), qui se désactive par `cmd/config` ; les durées, plages et bornes, elles, ne se règlent toujours pas.
- **Sans lui** : une faute de frappe dans un réglage suffirait à noyer ou cuire le terrarium.

## INV-6 — La logique de régulation ne touche pas au matériel

`lib/control` ne contient que de la logique pure : ni `Arduino.h`, ni broche, ni allocation. Elle se teste sur l'ordinateur (`pio test -e native`).

- **Ce qu'il protège** : chaque règle de régulation et de protection a un test qui tourne en secondes, sans carte.
- **Sans lui** : une règle de sécurité ne s'éprouverait qu'en simulant une panne sur le vrai montage.

## INV-7 — Toute version est épinglée

Plateforme, bibliothèques et outils de CI sont fixés à une version exacte dans `platformio.ini` et `requirements-ci.txt`.

- **Ce qu'il protège** : une compilation ne change pas parce qu'un registre a publié quelque chose.
- **Sans lui** : le même commit pourrait produire deux firmwares différents, dont un jamais testé.

## INV-8 — Seule une image signée par le propriétaire s'installe à distance

Une mise à jour à distance n'installe qu'une image signée par la clé minisign du propriétaire, dont la clé publique est compilée dans le firmware (`outils/mise-a-jour.pub`). La signature couvre l'image et un commentaire de confiance qui nomme le produit, la phase et la version : une image d'une autre phase, ou qui n'est pas plus récente que celle en place, est refusée (RG-image-signee). La partition n'est rendue démarrable qu'après cette vérification. La clé secrète ne quitte pas le poste du propriétaire : ni la CI ni Skaron ne la détiennent. Un défaut en cours n'empêche pas une installation, mais il est nommé dans ses étapes (RG-mise-a-jour-pendant-un-defaut), et un nouveau firmware qui n'a pas, dans les 5 minutes, retenu une mesure de l'air et rejoint le courtier laisse le démarrage revenir à l'ancien (RG-retour-arriere-automatique). Cadrage [ssk-terrarium-cadrage#2026-010](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-010).

- **Ce qu'il protège** : les bornes, les coupures et le chien de garde ne peuvent être retirés par aucun compte MQTT, aucune fuite de licence, aucune compromission de Skaron, de son bucket ou de la CI. C'est ce qui garde INV-5 vraie à l'heure de la mise à jour.
- **Sans lui** : le compte qui publie sur `cmd/#` pourrait installer un firmware sans bornes, ou un firmware qui ne régule plus et ne se corrigerait qu'en ouvrant le boîtier.

## Changer un invariant

Un invariant change par une décision explicite du propriétaire, consignée dans le cadrage, puis ici, dans la même demande de fusion que le code qu'elle autorise. Jamais par contournement, même bien intentionné.
