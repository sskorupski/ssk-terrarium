# ssk-terrarium

Firmware du régulateur de l'aquarium-terrarium à escargots : un ESP32 lit la température et l'humidité, et pilote le chauffage, l'éclairage, la brumisation et la ventilation, sans jamais dépendre du réseau pour réguler.

Le besoin, les règles de gestion, la nomenclature et le montage vivent dans le référentiel [ssk-terrarium-cadrage#2026-001](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-001). Ce dépôt n'en contient que le code.

## État

Phase 0, le socle : il compile pour l'ESP32 et ses tests passent sur un ordinateur. Chauffage et modes sont en place ; les capteurs, la supervision (page web, MQTT, alertes) et la brumisation restent des bouchons, à remplir phase par phase.

**Ne branchez aucune charge 230 V** avant d'avoir mesuré le niveau de déclenchement du module relais et confirmé `kOutputOffLevel` dans `include/config.h`.

## Démarrer

Il faut [PlatformIO Core](https://platformio.org/install/cli) 6.2.

```bash
pio test -e native      # tests de la logique de régulation, sur l'ordinateur
pio run -e esp32dev     # compilation pour l'ESP32
pio run -e esp32dev -t upload && pio device monitor
```

Sur la liaison série (115 200 bauds), `MODE ACTIF`, `MODE HIBERNATION` ou `MODE MAINTENANCE` change de mode ; le mode est conservé à travers les coupures de courant.

## Monter le régulateur

Chaque phase a sa notice pas à pas, avec ses schémas de branchement, et une version taguée `phase-N` dont la page de [Releases](https://github.com/sskorupski/ssk-terrarium/releases) porte le firmware prêt à téléverser et la notice en PDF : voir [docs/notice/README.md](docs/notice/README.md).

## Documentation

La documentation technique commence par [docs/README.md](docs/README.md).

## Licence

[MIT](LICENSE).
