# Développement

## Arborescence

```
include/config.h   brochage, valeurs par défaut, limites de sécurité (constexpr)
src/main.cpp       setup() et loop() : câble capteurs, régulation, sorties, supervision
lib/control/       logique pure, testée en natif : modes, hystérésis, plages horaires, fuseau, bornes des réglages, défaut capteur, commandes série
lib/supervision/   logique pure de la supervision, testée en natif : portail, identité, état et commandes MQTT
lib/outputs/       les quatre relais : toute écriture d'une sortie passe par là
lib/sensors/       SHT45 (air, I2C) et DS18B20 (substrat, 1-Wire)
lib/storage/       NVS : le mode, la consigne, les plages et le fuseau, le Wi-Fi et le courtier, le mot de passe du portail
lib/network/       tâche de supervision sur le cœur 0 : Wi-Fi, portail, MQTT, NTP
lib/web/           portail de configuration ; page web locale (phase 2)
lib/mqtt/          MQTT sur TLS, racines ISRG Root X1 et Root YR embarquées
lib/alerts/        alertes ntfy (phase 3)
test/test_*/       un dossier par suite Unity, compilées pour l'environnement native
docs/notice/       notices de montage pas à pas, une par phase, avec leurs schémas WireViz
outils/            schémas, PDF des notices, image fusionnée du firmware
```

## Gestes courants

```bash
pio test -e native                  # toutes les suites natives
pio test -e native -f test_sensor_watch
pio run -e esp32dev                 # compilation ESP32
pio run -e esp32dev -t upload       # téléversement par USB
pio device monitor                  # liaison série, 115 200 bauds
clang-format -i src/*.cpp lib/*/*.{h,cpp} include/*.h test/*/*.cpp
```

Commandes de la liaison série : `MODE ACTIF`, `MODE PLAGES`, `MODE HIBERNATION`, `MODE MAINTENANCE`, `PLAGES [<plages>|AUCUNE]`, `FUSEAU [<POSIX TZ>]` ([mode-plages.md](mode-plages.md)), `WIFI CONFIGURER` (rouvre le réseau de configuration 15 minutes). La carte répond `MODE <mode> enregistre, conserve apres coupure`, `MODE <mode> deja actif`, ou `ERREUR mode <mode> applique mais non enregistre, perdu a la prochaine coupure` si l'écriture en mémoire échoue ; au démarrage, elle écrit `MODE <mode> au demarrage`. Après les lignes de démarrage de la puce (`rst:`, `load:`, `entry`), que le firmware ne contrôle pas, ses propres lignes s'ouvrent sur `=== ssk-terrarium <version> : demarrage ===` — la version vient de `git describe` : le tag `phase-N` pour une version publiée — et l'initialisation se clôt sur `=== pret : regulation en cours ===`. Elle écrit aussi, toutes les 30 s, une ligne `MESURE air=19.2C hr=84% substrat=20.5C chauffage=1 mode=ACTIF heure=14:12` (`--` pour une mesure invalide ou une heure inconnue), et à chaque changement d'état : `INFO aucun mode enregistre, demarrage en MAINTENANCE` au démarrage tant qu'aucun mode n'a été enregistré, `ABSENT capteur air` au démarrage, `DEFAUT capteur air` / `REPRISE capteur air`, `PAUSE chauffage apres 2 h continues` / `REPRISE chauffage`. Les lignes du mode `PLAGES` et de l'heure (`PLAGES …`, `FUSEAU …`, `HEURE …`) sont décrites dans [mode-plages.md](mode-plages.md). La LED d'état est allumée tant qu'un défaut capteur ou une pause de chauffe dure.

Lignes de la supervision ([supervision.md](supervision.md)) : au démarrage, `WIFI reseau de configuration ssk-terrarium-XXXX, mot de passe …, page http://192.168.4.1`, `MQTT identifiant ssk-terrarium/<id>`, puis `WIFI enregistre : <ssid>, courtier <hote>:<port>` ou `WIFI aucun reseau enregistre : reseau de configuration ouvert`, et `CONSIGNE 20.0C hysteresis 0.5C au demarrage` si une consigne est enregistrée (`INFO consigne enregistree hors bornes, valeurs du firmware` sinon). En marche : `PORTAIL ouvert : reseau …` / `PORTAIL ferme`, `WIFI connecte a <ssid>, adresse <ip>` / `WIFI perdu, nouvel essai automatique`, `WIFI reglages enregistres : …`, `WIFI ouverture du reseau de configuration demandee`, `MQTT connecte a <hote>:<port>`, `MQTT echec de connexion a <hote>:<port> (code N), nouvel essai plus tard` (code négatif : réseau ou TLS ; 4 ou 5 : compte refusé), `MQTT deconnecte`, `MQTT commande cmd/… refusee : <raison>`, `MQTT commande cmd/… ignoree : recue a la connexion`, `CONSIGNE 20.0C hysteresis 0.5C enregistree`, `REFUS consigne … hors bornes (…)`. Un changement de mode reçu par MQTT s'écrit comme un changement série : `MODE HIBERNATION enregistre, conserve apres coupure`.

## Outils et versions

- PlatformIO Core et clang-format : leurs versions sont épinglées dans `requirements-ci.txt`, que Dependabot fait monter. Pour s'y aligner : `pip install -r requirements-ci.txt` dans un environnement virtuel.
- La plateforme `espressif32@7.1.3` apporte le cœur Arduino-ESP32 2.0.17 (ESP-IDF 4.4). `main.cpp` sait aussi armer le chien de garde d'un cœur 3.x (ESP-IDF 5), pour le jour où la plateforme montera.

## Notices et publication

```bash
pip install -r requirements-notice.txt   # WireViz, pandoc, WeasyPrint (environnement virtuel)
brew install graphviz pango              # dépendances système de WireViz et WeasyPrint
outils/schemas.sh                        # régénère les schémas des notices
python outils/notice_pdf.py docs/notice/phase-0 notice-phase-0.pdf
outils/firmware-fusionne.sh ssk-terrarium-phase-0.bin
```

Le `.gv` d'un schéma ne dépend que de WireViz, alors que le `.svg` dépend aussi de la version de Graphviz : c'est pourquoi la CI compare les `.gv`, pas les `.svg`.

## Pièges

- **Dependabot ne connaît pas PlatformIO** : il surveille les actions GitHub et `requirements-ci.txt`, pas `platformio.ini`. Les bibliothèques se relisent à la main, avec `pio pkg outdated -e esp32dev`, à chaque phase.
- **Une suite native ne voit que `lib/`** : un fichier qui inclut `Arduino.h` n'a pas sa place dans `lib/control`, il ferait échouer la compilation native (INV-6).
- **La supervision tourne dans sa propre tâche, sur le cœur 0** (`lib/network/supervisor.cpp`) : une connexion TLS prend une à deux secondes, et la boucle de régulation, sur le cœur 1, ne doit jamais l'attendre (INV-3). Les deux tâches n'échangent que par copie : un instantané de l'état sous verrou, une file de commandes, une file d'événements. Seule la tâche de régulation est surveillée par le chien de garde.
- **Une suite native qui emploie `config.h` l'inclut elle-même** : `include/` n'est pas dans le chemin des bibliothèques de `lib/`. Les bornes et limites passent donc en paramètre au cœur (`HeatGuard`, `isAcceptedHeatSettings`).
- **Seule la boucle de régulation lit l'heure locale et fixe `TZ`** (`setenv`, `tzset`, `localtime_r`) : `tzset` n'est pas sûr entre tâches. La tâche de supervision démarre seulement le client SNTP, qui compte les synchronisations dans un atomique.
- **`millis()` repasse par zéro au bout de 49,7 jours** : une durée se calcule toujours par soustraction non signée (`now - since > limit`), jamais par comparaison de deux instants.
- **OneWire 2.3.8 émet deux avertissements** `extra tokens at end of #undef` à la compilation ESP32 : ils viennent de la bibliothèque, pas du projet.
