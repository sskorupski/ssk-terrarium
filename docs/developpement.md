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

Commandes de la liaison série : `MODE ACTIF`, `MODE HIBERNATION`, `MODE MAINTENANCE`. La carte répond `MODE <mode>` ; elle écrit aussi `DEFAUT capteur air` et `REPRISE capteur air`.

## Outils et versions

- PlatformIO Core 6.2.0 et clang-format 23.1.2 : les versions de la CI sont dans `requirements-ci.txt`. Pour s'y aligner : `pip install -r requirements-ci.txt` dans un environnement virtuel.
- La plateforme `espressif32@7.1.3` apporte le cœur Arduino-ESP32 2.0.17 (ESP-IDF 4.4). `main.cpp` sait aussi armer le chien de garde d'un cœur 3.x (ESP-IDF 5), pour le jour où la plateforme montera.

## Pièges

- **Dependabot ne connaît pas PlatformIO** : il surveille les actions GitHub et `requirements-ci.txt`, pas `platformio.ini`. Les bibliothèques se relisent à la main, avec `pio pkg outdated -e esp32dev`, à chaque phase.
- **Une suite native ne voit que `lib/`** : un fichier qui inclut `Arduino.h` n'a pas sa place dans `lib/control`, il ferait échouer la compilation native (INV-6).
- **`millis()` repasse par zéro au bout de 49,7 jours** : une durée se calcule toujours par soustraction non signée (`now - since > limit`), jamais par comparaison de deux instants.
- **OneWire 2.3.8 émet deux avertissements** `extra tokens at end of #undef` à la compilation ESP32 : ils viennent de la bibliothèque, pas du projet.
