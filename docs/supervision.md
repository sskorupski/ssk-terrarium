# Supervision à distance

Le régulateur rejoint le Wi-Fi du domicile, publie son état sur le courtier MQTT `mq.ssk-it.fr`, et accepte trois commandes : changer de mode, changer les réglages du chauffage (consigne, plages horaires et fuseau du mode `PLAGES`), et mettre à jour son firmware ([plus bas](#mettre-à-jour-le-firmware-à-distance)). Rien de tout cela n'entre dans la régulation : sans Wi-Fi ni courtier, le chauffage continue exactement de la même façon (INV-3). Seule exception, le Wi-Fi donne l'heure au mode `PLAGES` ; sans elle, ce mode chauffe sur la consigne ([mode-plages.md](mode-plages.md)). Le pourquoi est dans le cadrage [ssk-terrarium-cadrage#2026-002](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-002).

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

**L'image fusionnée efface la mémoire** (`outils/firmware-fusionne.sh`, adresse `0x0`) : après l'avoir téléversée, la carte repart en `MAINTENANCE`, sans Wi-Fi, avec un nouveau mot de passe de configuration. On reprend à l'étape 1. Une mise à jour à distance, elle, garde tous les réglages ([plus bas](#mettre-à-jour-le-firmware-à-distance)).

## Lire l'état

L'identifiant de la carte est son adresse MAC, écrite au démarrage : `MQTT identifiant ssk-terrarium/246f28a1b2c3`. Avec le compte `proprietaire` :

```bash
mosquitto_sub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/#' -v
```

Sous macOS, remplacer `--capath /etc/ssl/certs` par `--cafile` et le fichier `mosquitto/isrg-root-x1.pem` de `ssk-it-infra`. MQTT Explorer convient aussi : hôte `mq.ssk-it.fr`, port 443, TLS activé, validation du certificat activée.

| Sujet | Contenu |
| --- | --- |
| `ssk-terrarium/<id>/state` | toutes les 30 s, retenu : mesure de l'air retenue pour réguler (`air`) et la sonde qui la donne (`sonde_retenue`), chaque sonde d'air (`sondes`), mesure du substrat (`null` si invalides ou rejetées), sorties, mode, consigne, hystérésis, défaut capteur, cause du rejet de la mesure de l'air (`cause_mesure_air`), mise en sécurité de l'air intérieur (`mise_en_securite`), combinaison des sondes (`combinaison`), pause de chauffe, coupure par le substrat (`coupure_substrat`, `securite_substrat`, `seuils_substrat`, `part_substrat`, RG-coupure-par-le-substrat, [coupure-substrat.md](coupure-substrat.md)), secondes depuis le démarrage, version, plages, fuseau, `heure_connue`, `en_plage` (`null` sans heure), jour et nuit (`jour_nuit`, `jour_aujourdhui`, `moment`), réglages de la ventilation (`ventilation`, `aeration_saturation`), consigne de nuit (`chauffage_nuit`), et `bornes` des réglages (ci-dessous) |
| `ssk-terrarium/<id>/event` | réponse à chaque commande, chaque changement de mode, série compris, chaque chauffe de la SHT45, chaque activation ou désactivation de la coupure par le substrat (`securite-substrat`), et chaque étape d'une mise à jour |
| `ssk-terrarium/<id>/status` | `en-ligne`, ou `hors-ligne` si la connexion tombe |

## Commander

```bash
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/mode' -m HIBERNATION
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/config' -m '{"consigne": 20, "hysteresis": 0.5}'
```

- `cmd/mode` : `ACTIF`, `PLAGES`, `HIBERNATION` ou `MAINTENANCE`, exactement. Le mode s'enregistre comme par la liaison série.
- `cmd/config` : `consigne` et/ou `hysteresis`, en °C ; un champ absent garde sa valeur. Bornes fixées dans `include/config.h`, qu'aucune commande ne change (RG-reglages-bornes) : consigne de 15 à 22 °C, hystérésis de 0,2 à 2 °C, et consigne plus hystérésis sous 24 °C. Hors bornes, la commande est refusée en entier ; un JSON invalide ou un champ inconnu aussi. `plages` (liste de 0 à 4 `"HH:MM-HH:MM"`) et `fuseau` (chaîne POSIX TZ) s'y ajoutent : voir [mode-plages.md](mode-plages.md) ; `ventilation` (partiel à deux niveaux), `jour_nuit` et `chauffage_nuit` aussi : voir [ventilation.md](ventilation.md) ; `securite_substrat` (partiel à deux niveaux) : voir [coupure-substrat.md](coupure-substrat.md). Sur la liaison série, `CONFIG` suivi du même JSON règle de même.
- `state` publie ces bornes dans `bornes`, lues des constantes du firmware, pour qu'une application ne les recopie pas : `"bornes":{"consigne":[15.0,22.0],"hysteresis":[0.2,2.0],"somme_max":24.0,"plages_max":4,"fuseau_max":47,"combinaisons":["priorite-sht45","priorite-dht11","moyenne","plus-froide"],"saisons_max":6,"ventilation":{"duree":[1,30],"periode":[15,240],"ecart":[10,50],"humidite":[80,100],"attente":[1,60],"pause":[15,240]},"substrat":{"coupure":[20.0,28.0],"reprise_min":15.0,"ecart_min":2.0,"delai":[1,30],"approche":[0.0,4.0],"comportements":["reprise-air","maintien"]}}`. `consigne` et `hysteresis` sont les intervalles admis, bornes comprises ; consigne plus hystérésis doit rester strictement sous `somme_max` ; `plages_max` est le nombre maximal de plages, `fuseau_max` la longueur maximale d'un fuseau POSIX ; `saisons_max` est le nombre maximal de saisons du jour et de la nuit ; les intervalles de `ventilation` sont admis bornes comprises, en minutes ou en pour cent entiers, une durée restant strictement sous sa période ; `substrat` : voir [coupure-substrat.md](coupure-substrat.md).
- `state` dit pourquoi la mesure de l'air est rejetée, dès la première mesure refusée : `"cause_mesure_air"` vaut `null`, `"muet"` (le capteur ne répond plus), `"temperature-hors-plage"` (hors de −20 à 60 °C) ou `"humidite-hors-plage"` (hors de 0 à 100 %, souvent un capteur trempé de condensation). La mesure rejetée est publiée à `null`. `defaut_capteur` passe à `true` après 2 minutes de rejets, quand chauffage et pompe sont coupés (RG-capteur-en-defaut).
- La SHT45 se chauffe elle-même pour sortir de la condensation (RG-chauffe-de-la-sht45, cadrage [ssk-terrarium-cadrage#2026-008](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-008)) : juste après une mesure à 95 % ou plus, ou rejetée pour `humidite-hors-plage`, elle chauffe 1 seconde à 200 mW, au plus une fois toutes les 5 minutes. Chaque chauffe se publie sur `event` : `{"evenement":"chauffe-sonde","sonde":"sht45","motif":"humidite-haute"}`, ou `"motif":"humidite-hors-plage"`. `state` n'en change pas : la mesure de fin de chauffe, faussée par la chaleur de la puce, est jetée, et la mesure suivante arrive 30 secondes après, comme d'habitude. Des chauffes répétées toutes les 5 minutes disent que la chauffe ne suffit pas : sécher la sonde, ou l'éloigner de l'eau.
- `state` publie chaque sonde d'air sous son nom (RG-sondes-d-air, cadrage [ssk-terrarium-cadrage#2026-007](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-007)) : `"sondes":{"sht45":{"role":"interieure","temperature":19.2,"humidite":84,"cause":null,"defaut":false},"dht11":{"role":"exterieure",…}}`. Une mesure rejetée y est à `null`, avec sa `cause` ; `defaut` passe à `true` après 2 minutes de rejets. `air` est la mesure qui régule, calculée sur les seules sondes intérieures acceptées ; `sonde_retenue` dit d'où elle vient : `sht45`, `dht11`, `moyenne`, ou `null` quand aucune sonde intérieure n'est acceptée. `cause_mesure_air` est alors la cause de la sonde intérieure prioritaire. `defaut_capteur` ne passe à `true` que si aucune sonde intérieure n'est acceptée depuis 2 minutes : le défaut d'une seule sonde, quand l'autre prend le relais, ne coupe rien.
- `sondes` (`{"dht11": "interieure"}`, objet partiel) règle le rôle de chaque sonde, `interieure` (elle régule) ou `exterieure` (elle n'est que publiée) ; `combinaison` dit comment deux sondes intérieures acceptées font une seule mesure : `priorite-sht45` (par défaut), `priorite-dht11`, `moyenne` ou `plus-froide`. Une commande qui ne laisserait aucune sonde intérieure est refusée en entier : `{"evenement":"reglage","sondes":{"sht45":"exterieure","dht11":"exterieure"},"combinaison":"priorite-sht45","resultat":"refuse","raison":"aucune sonde interieure"}`. Le réglage est enregistré, et repris au démarrage ; sans réglage, la SHT45 est seule intérieure.
- `mise_en_securite` (`{"air": false}` ou `{"air": true}`) désactive ou réactive la mise en sécurité de l'air intérieur, toutes sondes intérieures ensemble, sur décision du propriétaire (INV-2) ; le choix est enregistré et publié dans `state`. **Désactivée, la carte chauffe sur la mesure rejetée de la sonde intérieure prioritaire, ou garde sa dernière décision si toutes sont muettes : rien de matériel n'arrête alors une surchauffe**, hormis la coupure de 15 minutes toutes les 2 heures de chauffe continue. La réponse : `{"evenement":"reglage","mise_en_securite":{"air":false},"resultat":"applique"}`.
- La réponse arrive sur `event`, par exemple `{"evenement":"reglage","consigne":30.0,"hysteresis":0.5,"resultat":"refuse","raison":"hors bornes"}`.
- **Ne jamais publier une commande en « retenu »** (`-r`). Une commande que le courtier garde serait rejouée à chaque reconnexion : la carte ignore toute commande reçue dans les 3 secondes qui suivent son abonnement, ce qui écarte les commandes retenues, et le dit sur `event`.

Le hors-gel de `HIBERNATION`, le plancher de 12-14 °C hors plage de `PLAGES`, la durée maximale de chauffe, les bornes de la coupure par le substrat, et le chien de garde ne sont pas des réglages : aucune commande ne les touche. La coupure sur capteur en défaut ne se lève que par `mise_en_securite`, décision du propriétaire.

## Mettre à jour le firmware à distance

Une nouvelle version s'installe sans câble ni passage au garage, et seulement sur commande : rien ne s'installe de soi-même, rien ne se relance après un échec. Le pourquoi est dans le cadrage [ssk-terrarium-cadrage#2026-010](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-010).

- **L'image vient de Skaron** (`api.skaron.ssk-it.fr`), l'hôte fixé à la compilation, en HTTPS vérifié contre les racines déjà embarquées. Une commande ne porte qu'un numéro de version : jamais une adresse ni une image.
- **Seule une image signée par le propriétaire s'installe** (INV-8). La carte vérifie la signature minisign avec la clé publique compilée, `outils/mise-a-jour.pub`, et le commentaire de confiance signé avec elle, `ssk-terrarium esp32-phase-<N> <version>` : une image d'une autre phase, ou plus ancienne que celle en place, est refusée. La partition n'est rendue démarrable qu'après.
- **La régulation continue pendant le téléchargement**, qui tourne dans sa propre tâche. La seule interruption est le redémarrage, sorties coupées, quelques secondes. Les réglages, le Wi-Fi et le courtier sont conservés.
- **Un nouveau firmware qui ne marche pas se défait seul** : voir « Retour arrière », plus bas.

Publier une version dans Skaron se fait sur le poste du propriétaire : [pratiques.md](pratiques.md#publier-une-mise-à-jour).

### Une fois par régulateur : sa licence

Skaron ne répond qu'à un régulateur muni de sa licence. Depuis le poste du propriétaire, avec l'identifiant de la carte :

```bash
outils/licence-mise-a-jour.sh 246f28a1b2c3
```

Le script émet la licence par le CLI de Skaron, la publie sur `cmd/mise-a-jour` sans jamais l'afficher, et attend la réponse : `{"evenement":"mise-a-jour","etape":"licence","resultat":"applique"}`. Elle est enregistrée en mémoire non volatile, et ne se publie ni ne s'écrit jamais sur la liaison série. Faute de réseau, elle se tape aussi sur la liaison série : `licence <jeton>`, réponse `MISE-A-JOUR licence enregistree, conservee apres coupure`. Un jeton vide, de plus de 768 caractères ou avec autre chose que des lettres, des chiffres, `-`, `_` et `.` est refusé : `"resultat":"refuse"`.

### Vérifier, puis installer

```bash
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/mise-a-jour' -m '{"action":"verifier"}'
mosquitto_pub -h mq.ssk-it.fr -p 443 --capath /etc/ssl/certs -u proprietaire -P '<mot de passe>' -t 'ssk-terrarium/<id>/cmd/mise-a-jour' -m '{"action":"installer","version":"1.3.0"}'
```

`verifier` demande à Skaron la version proposée : `"etape":"disponible","version":"1.3.0"`, ou `"etape":"a-jour"` avec la version en place. `installer` n'installe que la version que Skaron propose encore au moment de la commande : on installe exactement ce qu'on a vu. La version se lit au sens semver : le tag `phase-1.3` donne `1.3.0`, et une compilation entre deux tags porte en plus le nombre de commits et le hash (`1.2.0+3.g1a2b3c4`), sans dépasser son tag. `state` publie toujours `version` telle que `git describe` la donne.

Une installation acceptée se suit sur `event`, au format `{"evenement":"mise-a-jour","etape":…}` :

| `etape` | Quand | Champs |
| --- | --- | --- |
| `licence` | licence reçue | `resultat` : `applique`, `applique_non_enregistre` ou `refuse` |
| `disponible` | réponse à `verifier` | `version` proposée par Skaron |
| `a-jour` | réponse à `verifier` | `version` en place |
| `refusee` | commande refusée | `raison` |
| `acceptee` | Skaron propose la version demandée | `version`, et `defauts` si un défaut est en cours |
| `avertissement` | un défaut apparaît pendant le téléchargement | `version`, `defauts` |
| `telechargement` | tous les 10 % | `version`, `progression` (10 à 100) |
| `echec` | installation abandonnée, le firmware en place continue | `version`, `raison` |
| `redemarrage` | image vérifiée, juste avant de redémarrer | `version` |
| `validee` | nouveau firmware déclaré valide | `version` |
| `annulee` | ancien firmware restauré par le démarrage | `version` refusée |

`refusee` donne l'une de ces raisons, et rien ne se télécharge :

- `mise a jour en cours` : une vérification ou une installation tourne déjà, ou le firmware qui vient d'être installé ne s'est pas encore déclaré valide.
- `sans licence`, `licence refusee` (Skaron répond 401 ou 403 : licence révoquée ou expirée), `quota depasse` (429 : dix adresses d'image par heure), `service injoignable` (pas de réseau, Skaron en panne, ou réponse illisible).
- `version non proposee` : Skaron propose une autre version, ou aucune. Relancer `verifier`.
- `version inconnue` : le firmware n'a pas été compilé depuis un tag `phase-*`, et ne sait pas se situer.

`echec` donne l'une de ces raisons : `telechargement interrompu` (aucun octet reçu pendant 60 s, connexion perdue, ou adresse refusée), `signature invalide`, `image invalide`, `image trop grande`, `version non plus recente` ou `phase differente`. Une commande illisible reçoit la réponse commune : `{"evenement":"commande","sujet":"cmd/mise-a-jour","resultat":"refusee","raison":"JSON invalide"}`, ou `champ inconnu`, `action inconnue (verifier, installer)`, `version invalide (x.y.z)`.

**Un défaut n'empêche ni n'interrompt une installation : il est nommé** (RG-mise-a-jour-pendant-un-defaut, décision du propriétaire du 11 novembre 2026). `acceptee` porte alors `defauts`, la liste des défauts en cours, et chaque défaut qui apparaît pendant le téléchargement publie `avertissement` avec la liste complète : `{"evenement":"mise-a-jour","etape":"acceptee","version":"1.3.0","defauts":["sonde dht11","substrat rejete"]}`. Les noms : `defaut capteur` (l'air intérieur), `sonde sht45`, `sonde dht11`, `substrat rejete`, `coupure substrat`, `pause chauffe`. Une installation pendant un défaut ajoute à une situation dégradée un redémarrage, sorties coupées, et efface les délais de détection en cours, qui repartent de zéro. Et un nouveau firmware qui ne retient aucune mesure de l'air dans les 5 minutes est défait : installé pendant un défaut de l'air intérieur, il risque de l'être.

Comme toute commande, une commande reçue dans les 3 secondes qui suivent l'abonnement est ignorée : **ne jamais publier `cmd/mise-a-jour` en retenu**.

### Retour arrière

Le nouveau firmware démarre « à vérifier ». Il se déclare valide, et publie `validee`, dès qu'il a retenu une mesure de l'air **et** rejoint le courtier. S'il n'y parvient pas dans les 5 minutes, s'il plante, ou si le chien de garde le redémarre avant, le démarrage revient à l'ancien firmware. Celui-ci publie `annulee` avec la version refusée dès qu'il rejoint le courtier, et ne retente rien.

Sur la liaison série, les lignes de la mise à jour commencent par `MISE-A-JOUR` : `licence enregistree`, `aucune licence enregistree : mise a jour a distance impossible`, `refusee : <raison>`, `<version> acceptee : telechargement dans la partition inactive`, `telechargement 40 % : tas libre … octets, minimum …`, `<version> abandonnee : <raison>, le firmware en place continue`, `<version> verifiee : redemarrage, sorties coupees le temps du demarrage`, puis au démarrage du nouveau firmware `firmware <version> a verifier : …`, `firmware <version> valide : retour arriere annule`, ou `firmware <version> non valide apres 5 minutes : retour a l'ancien`. L'ancien firmware restauré écrit `<version> refusee par le demarrage : retour au firmware <version>`.
