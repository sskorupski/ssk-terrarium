# Développement

## Arborescence

```
include/config.h   brochage, valeurs par défaut, limites de sécurité (constexpr)
src/main.cpp       setup() et loop() : câble capteurs, régulation, sorties, supervision
lib/control/       logique pure, testée en natif : modes, hystérésis, défaut capteur, commandes série
lib/outputs/       les quatre relais : toute écriture d'une sortie passe par là
lib/sensors/       SHT45 (air, I2C) et DS18B20 (substrat, 1-Wire)
lib/storage/       NVS : le mode courant, puis les réglages (phase 2)
lib/web/           page web locale (phase 2)
lib/mqtt/          publication MQTT (phase 3)
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

Commandes de la liaison série : `MODE ACTIF`, `MODE HIBERNATION`, `MODE MAINTENANCE`. La carte répond `MODE <mode> enregistre, conserve apres coupure`, `MODE <mode> deja actif`, ou `ERREUR mode <mode> applique mais non enregistre, perdu a la prochaine coupure` si l'écriture en mémoire échoue ; au démarrage, elle écrit `MODE <mode> au demarrage`. Elle écrit aussi, toutes les 30 s, une ligne `MESURE air=19.2C hr=84% substrat=20.5C chauffage=1 mode=ACTIF` (`--` pour une mesure invalide), et à chaque changement d'état : `INFO aucun mode enregistre, demarrage en MAINTENANCE` au démarrage tant qu'aucun mode n'a été enregistré, `ABSENT capteur air` au démarrage, `DEFAUT capteur air` / `REPRISE capteur air`, `PAUSE chauffage apres 2 h continues` / `REPRISE chauffage`. La LED d'état est allumée tant qu'un défaut capteur ou une pause de chauffe dure.

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
- **`millis()` repasse par zéro au bout de 49,7 jours** : une durée se calcule toujours par soustraction non signée (`now - since > limit`), jamais par comparaison de deux instants.
- **OneWire 2.3.8 émet deux avertissements** `extra tokens at end of #undef` à la compilation ESP32 : ils viennent de la bibliothèque, pas du projet.
