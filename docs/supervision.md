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
| `ssk-terrarium/<id>/state` | toutes les 30 s, retenu : mesures (`null` si invalides ou rejetées), sorties, mode, consigne, hystérésis, défaut capteur, cause du rejet de la mesure de l'air (`cause_mesure_air`), mise en sécurité de chaque sonde (`mise_en_securite`), pause de chauffe, secondes depuis le démarrage, version, plages, fuseau, `heure_connue`, `en_plage` (`null` sans heure), et `bornes` des réglages (ci-dessous) |
| `ssk-terrarium/<id>/event` | réponse à chaque commande, et chaque changement de mode, série compris |
| `ssk-terrarium/<id>/status` | `en-ligne`, ou `hors-ligne` si la connexion tombe |

## Commander

```bash
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/mode' -m HIBERNATION
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"consigne": 20, "hysteresis": 0.5}'
```

- `cmd/mode` : `ACTIF`, `PLAGES`, `HIBERNATION` ou `MAINTENANCE`, exactement. Le mode s'enregistre comme par la liaison série.
- `cmd/config` : `consigne` et/ou `hysteresis`, en °C ; un champ absent garde sa valeur. Bornes fixées dans `include/config.h`, qu'aucune commande ne change (RG-reglages-bornes) : consigne de 15 à 22 °C, hystérésis de 0,2 à 2 °C, et consigne plus hystérésis sous 24 °C. Hors bornes, la commande est refusée en entier ; un JSON invalide ou un champ inconnu aussi. `plages` (liste de 0 à 4 `"HH:MM-HH:MM"`) et `fuseau` (chaîne POSIX TZ) s'y ajoutent : voir [mode-plages.md](mode-plages.md).
- `state` publie ces bornes dans `bornes`, lues des constantes du firmware, pour qu'une application ne les recopie pas : `"bornes":{"consigne":[15.0,22.0],"hysteresis":[0.2,2.0],"somme_max":24.0,"plages_max":4,"fuseau_max":47}`. `consigne` et `hysteresis` sont les intervalles admis, bornes comprises ; consigne plus hystérésis doit rester strictement sous `somme_max` ; `plages_max` est le nombre maximal de plages, `fuseau_max` la longueur maximale d'un fuseau POSIX.
- `state` dit pourquoi la mesure de l'air est rejetée, dès la première mesure refusée : `"cause_mesure_air"` vaut `null`, `"muet"` (le capteur ne répond plus), `"temperature-hors-plage"` (hors de −20 à 60 °C) ou `"humidite-hors-plage"` (hors de 0 à 100 %, souvent un capteur trempé de condensation). La mesure rejetée est publiée à `null`. `defaut_capteur` passe à `true` après 2 minutes de rejets, quand chauffage et pompe sont coupés (RG-capteur-en-defaut).
- `mise_en_securite` (`{"air": false}` ou `{"air": true}`) désactive ou réactive la mise en sécurité de la sonde d'air, sur décision du propriétaire (INV-2) ; le choix est enregistré et publié dans `state`. **Désactivée, la carte chauffe sur la mesure rejetée, ou garde sa dernière décision si le capteur est muet : rien de matériel n'arrête alors une surchauffe**, hormis la coupure de 15 minutes toutes les 2 heures de chauffe continue. La réponse : `{"evenement":"reglage","mise_en_securite":{"air":false},"resultat":"applique"}`.
- La réponse arrive sur `event`, par exemple `{"evenement":"reglage","consigne":30.0,"hysteresis":0.5,"resultat":"refuse","raison":"hors bornes"}`.
- **Ne jamais publier une commande en « retenu »** (`-r`). Une commande que le courtier garde serait rejouée à chaque reconnexion : la carte ignore toute commande reçue dans les 3 secondes qui suivent son abonnement, ce qui écarte les commandes retenues, et le dit sur `event`.

Le hors-gel de `HIBERNATION`, le plancher de 12-14 °C hors plage de `PLAGES`, la durée maximale de chauffe, et le chien de garde ne sont pas des réglages : aucune commande ne les touche. La coupure sur capteur en défaut ne se lève que par `mise_en_securite`, décision du propriétaire.
