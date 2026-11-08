# Conventions

## Langue

Le code, ses commentaires et les messages de commit sont en anglais ; la documentation et les textes de la liaison série destinés au propriétaire sont en français.

## Nommage

- Fichiers en `snake_case` (`sensor_watch.cpp`), un couple `.h`/`.cpp` par classe ou groupe de fonctions.
- Espaces de noms `terrarium::<couche>` : `control`, `outputs`, `sensors`, `storage`, `web`, `mqtt`, `alerts`, `config`.
- Types en `PascalCase`, fonctions et variables en `camelCase`, membres suffixés `_`, constantes et valeurs d'énumération préfixées `k`.
- Les noms des modes sur les fils (liaison série, MQTT, page web) sont ceux du cadrage, en français et en majuscules : `ACTIF`, `PLAGES`, `HIBERNATION`, `MAINTENANCE`.

## Commentaires

- Un commentaire dit pourquoi, pas quoi. Il cite la règle du cadrage (`RG-…`) ou l'invariant (`INV-n`) qu'il applique.
- En tête de chaque en-tête, une ligne dit ce que l'unité représente, et si elle est de la logique pure.

## Commits

- [Conventional Commits](https://www.conventionalcommits.org/) : `feat(control): …`, `fix(outputs): …`, `docs: …`, `ci: …`, `chore: …`.
- Sujet à l'impératif, en anglais, 72 colonnes au plus ; le corps dit pourquoi, et cite les règles et invariants concernés.
- Une demande de fusion qui réalise une piste du cadrage la ferme par `Closes ssk-it/ssk-terrarium-cadrage#N` sur sa propre ligne.
