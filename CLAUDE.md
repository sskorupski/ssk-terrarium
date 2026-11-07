# CLAUDE.md

Ce fichier ne s'adresse qu'à un agent. Tout le reste est dans `docs/`.

## Lire d'abord

1. `docs/invariants.md` : les règles de sécurité ne se contournent jamais.
2. `docs/developpement.md` : commandes et pièges.
3. Le cadrage ssk-it/ssk-terrarium-cadrage 2026-001 : avant de changer un comportement, appliquer le skill `consulter-referentiel`.

## Règles de conduite

- Vérifier plutôt que supposer : lancer `pio test -e native` et `pio run -e esp32dev` avant d'annoncer quoi que ce soit.
- Test rouge d'abord pour toute règle de `lib/control`.
- Demander au propriétaire avant tout écart au brochage, aux bibliothèques prévues, ou à une règle de sécurité (INV-1 à INV-5).
- Ne jamais committer sur `main` : une copie liée et une branche par sujet, depuis `origin/main`.
- Ne jamais téléverser ni piloter une carte branchée sans que le propriétaire le demande : une sortie peut alimenter une charge 230 V.
- Tenir `docs/` à jour dans la même demande de fusion que le code qui le contredit.
