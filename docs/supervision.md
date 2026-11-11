# Supervision à distance

Le régulateur rejoint le Wi-Fi du domicile, publie son état sur le courtier MQTT `mq.ssk-it.fr`, et accepte deux commandes : changer de mode, changer les réglages du chauffage (consigne, plages horaires et fuseau du mode `PLAGES`). Rien de tout cela n'entre dans la régulation : sans Wi-Fi ni courtier, le chauffage continue exactement de la même façon (INV-3). Seule exception, le Wi-Fi donne l'heure au mode `PLAGES` ; sans elle, ce mode chauffe sur la consigne ([mode-plages.md](mode-plages.md)). Le pourquoi est dans le cadrage [ssk-terrarium-cadrage#2026-002](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-002).

## Configurer le Wi-Fi

1. Brancher la carte en USB et ouvrir la liaison série (115 200 bauds). À chaque démarrage, elle écrit le nom et le mot de passe de son réseau de configuration :

   ```
   WIFI reseau de configuration ssk-terrarium-B2C3, mot de passe k7m2pqzr4hxa, page http://192.168.4.1
   ```

   Le mot de passe est tiré au premier démarrage et reste le même ensuite. Le noter : sans lui, on ne rejoint pas ce réseau.
2. Tant qu'aucun Wi-Fi n'est enregistré, ce réseau est ouvert (`PORTAIL ouvert`). Le rejoindre depuis un téléphone ; la page s'ouvre d'elle-même, sinon aller à `http://192.168.4.1`.
3. Saisir le nom et le mot de passe du Wi-Fi du domicile, puis le courtier : adresse `mq.ssk-it.fr`, port `443`, identifiant `terrarium` et son mot de passe (celui scellé dans `ssk-it-infra`). Enregistrer.
4. La carte rejoint le Wi-Fi (`WIFI connecte a …`), ferme son réseau (`PORTAIL ferme`), puis joint le courtier (`MQTT connecte a mq.ssk-it.fr:443`).

Si la carte ne rejoint pas le Wi-Fi, elle écrit pourquoi, une fois par cause : `WIFI echec de connexion a Noemie : reseau introuvable : absent, trop loin, ou en 5 GHz seulement (code 201)`, ou `mot de passe refuse`, ou `securite non prise en charge : box en WPA3 seul ?`. L'ESP32 ne capte que le 2,4 GHz. Le mode WPA3 seul d'une box n'a pas été essayé avec ce firmware : en cas de `securite non prise en charge`, passer la box en mode mixte WPA2/WPA3.

Le réseau de configuration se rouvre :

- tant que la carte n'a pas rejoint le Wi-Fi qu'on vient d'enregistrer, pour corriger un mot de passe ;
- quand le Wi-Fi enregistré reste injoignable 5 minutes (box changée) : la carte continue d'essayer l'ancien en même temps ;
- à la demande, par la commande série `WIFI CONFIGURER`, pour 15 minutes.

La page ne répond que sur ce réseau, jamais sur celui du domicile, et ne commande aucune sortie. Les mots de passe ne sont jamais réaffichés : on les ressaisit à chaque enregistrement.

**L'image fusionnée efface la mémoire** (`outils/firmware-fusionne.sh`, adresse `0x0`) : après l'avoir téléversée, la carte repart en `MAINTENANCE`, sans Wi-Fi, avec un nouveau mot de passe de configuration. On reprend à l'étape 1.

## Lire l'état

L'identifiant de la carte est son adresse MAC, écrite au démarrage : `MQTT identifiant ssk-terrarium/246f28a1b2c3`. Avec le compte `proprietaire` :

```bash
mosquitto_sub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/#' -v
```

Sous macOS, remplacer `--capath /etc/ssl/certs` par `--cafile` et le fichier `mosquitto/isrg-root-x1.pem` de `ssk-it-infra`. MQTT Explorer convient aussi : hôte `mq.ssk-it.fr`, port 443, TLS activé, validation du certificat activée.

| Sujet | Contenu |
| --- | --- |
| `ssk-terrarium/<id>/state` | toutes les 30 s, retenu : mesure de l'air retenue pour réguler (`air`) et la sonde qui la donne (`sonde_retenue`), chaque sonde d'air (`sondes`), mesure du substrat (`null` si invalides ou rejetées), sorties, mode, consigne, hystérésis, défaut capteur, cause du rejet de la mesure de l'air (`cause_mesure_air`), mise en sécurité de l'air intérieur (`mise_en_securite`), combinaison des sondes (`combinaison`), pause de chauffe, coupure par le substrat (`coupure_substrat`, RG-coupure-par-le-substrat), secondes depuis le démarrage, version, plages, fuseau, `heure_connue`, `en_plage` (`null` sans heure), réglage de la ventilation (`ventilation`), et `bornes` des réglages (ci-dessous) |
| `ssk-terrarium/<id>/event` | réponse à chaque commande, chaque changement de mode, série compris, et chaque chauffe de la SHT45 |
| `ssk-terrarium/<id>/status` | `en-ligne`, ou `hors-ligne` si la connexion tombe |

## Commander

```bash
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/mode' -m HIBERNATION
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"consigne": 20, "hysteresis": 0.5}'
```

- `cmd/mode` : `ACTIF`, `PLAGES`, `HIBERNATION` ou `MAINTENANCE`, exactement. Le mode s'enregistre comme par la liaison série.
- `cmd/config` : `consigne` et/ou `hysteresis`, en °C ; un champ absent garde sa valeur. Bornes fixées dans `include/config.h`, qu'aucune commande ne change (RG-reglages-bornes) : consigne de 15 à 22 °C, hystérésis de 0,2 à 2 °C, et consigne plus hystérésis sous 24 °C. Hors bornes, la commande est refusée en entier ; un JSON invalide ou un champ inconnu aussi. `plages` (liste de 0 à 4 `"HH:MM-HH:MM"`) et `fuseau` (chaîne POSIX TZ) s'y ajoutent : voir [mode-plages.md](mode-plages.md) ; `ventilation` (objet partiel `{"duree":…,"periode":…,"heure":"HH:MM"}`) aussi : voir [ventilation.md](ventilation.md).
- `state` publie ces bornes dans `bornes`, lues des constantes du firmware, pour qu'une application ne les recopie pas : `"bornes":{"consigne":[15.0,22.0],"hysteresis":[0.2,2.0],"somme_max":24.0,"plages_max":4,"fuseau_max":47,"combinaisons":["priorite-sht45","priorite-dht11","moyenne","plus-froide"],"ventilation":{"duree":[1,30],"periode":[15,240]}}`. `consigne` et `hysteresis` sont les intervalles admis, bornes comprises ; consigne plus hystérésis doit rester strictement sous `somme_max` ; `plages_max` est le nombre maximal de plages, `fuseau_max` la longueur maximale d'un fuseau POSIX ; `ventilation.duree` et `ventilation.periode` sont les intervalles admis, en minutes entières, la durée restant strictement sous la période.
- `state` dit pourquoi la mesure de l'air est rejetée, dès la première mesure refusée : `"cause_mesure_air"` vaut `null`, `"muet"` (le capteur ne répond plus), `"temperature-hors-plage"` (hors de −20 à 60 °C) ou `"humidite-hors-plage"` (hors de 0 à 100 %, souvent un capteur trempé de condensation). La mesure rejetée est publiée à `null`. `defaut_capteur` passe à `true` après 2 minutes de rejets, quand chauffage et pompe sont coupés (RG-capteur-en-defaut).
- La SHT45 se chauffe elle-même pour sortir de la condensation (RG-chauffe-de-la-sht45, cadrage [ssk-terrarium-cadrage#2026-008](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-008)) : juste après une mesure à 95 % ou plus, ou rejetée pour `humidite-hors-plage`, elle chauffe 1 seconde à 200 mW, au plus une fois toutes les 5 minutes. Chaque chauffe se publie sur `event` : `{"evenement":"chauffe-sonde","sonde":"sht45","motif":"humidite-haute"}`, ou `"motif":"humidite-hors-plage"`. `state` n'en change pas : la mesure de fin de chauffe, faussée par la chaleur de la puce, est jetée, et la mesure suivante arrive 30 secondes après, comme d'habitude. Des chauffes répétées toutes les 5 minutes disent que la chauffe ne suffit pas : sécher la sonde, ou l'éloigner de l'eau.
- `state` publie chaque sonde d'air sous son nom (RG-sondes-d-air, cadrage [ssk-terrarium-cadrage#2026-007](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-007)) : `"sondes":{"sht45":{"role":"interieure","temperature":19.2,"humidite":84,"cause":null,"defaut":false},"dht11":{"role":"exterieure",…}}`. Une mesure rejetée y est à `null`, avec sa `cause` ; `defaut` passe à `true` après 2 minutes de rejets. `air` est la mesure qui régule, calculée sur les seules sondes intérieures acceptées ; `sonde_retenue` dit d'où elle vient : `sht45`, `dht11`, `moyenne`, ou `null` quand aucune sonde intérieure n'est acceptée. `cause_mesure_air` est alors la cause de la sonde intérieure prioritaire. `defaut_capteur` ne passe à `true` que si aucune sonde intérieure n'est acceptée depuis 2 minutes : le défaut d'une seule sonde, quand l'autre prend le relais, ne coupe rien.
- `sondes` (`{"dht11": "interieure"}`, objet partiel) règle le rôle de chaque sonde, `interieure` (elle régule) ou `exterieure` (elle n'est que publiée) ; `combinaison` dit comment deux sondes intérieures acceptées font une seule mesure : `priorite-sht45` (par défaut), `priorite-dht11`, `moyenne` ou `plus-froide`. Une commande qui ne laisserait aucune sonde intérieure est refusée en entier : `{"evenement":"reglage","sondes":{"sht45":"exterieure","dht11":"exterieure"},"combinaison":"priorite-sht45","resultat":"refuse","raison":"aucune sonde interieure"}`. Le réglage est enregistré, et repris au démarrage ; sans réglage, la SHT45 est seule intérieure.
- `mise_en_securite` (`{"air": false}` ou `{"air": true}`) désactive ou réactive la mise en sécurité de l'air intérieur, toutes sondes intérieures ensemble, sur décision du propriétaire (INV-2) ; le choix est enregistré et publié dans `state`. **Désactivée, la carte chauffe sur la mesure rejetée de la sonde intérieure prioritaire, ou garde sa dernière décision si toutes sont muettes : rien de matériel n'arrête alors une surchauffe**, hormis la coupure de 15 minutes toutes les 2 heures de chauffe continue. La réponse : `{"evenement":"reglage","mise_en_securite":{"air":false},"resultat":"applique"}`.
- La réponse arrive sur `event`, par exemple `{"evenement":"reglage","consigne":30.0,"hysteresis":0.5,"resultat":"refuse","raison":"hors bornes"}`.
- **Ne jamais publier une commande en « retenu »** (`-r`). Une commande que le courtier garde serait rejouée à chaque reconnexion : la carte ignore toute commande reçue dans les 3 secondes qui suivent son abonnement, ce qui écarte les commandes retenues, et le dit sur `event`.

Le hors-gel de `HIBERNATION`, le plancher de 12-14 °C hors plage de `PLAGES`, la durée maximale de chauffe, et le chien de garde ne sont pas des réglages : aucune commande ne les touche. La coupure sur capteur en défaut ne se lève que par `mise_en_securite`, décision du propriétaire.
