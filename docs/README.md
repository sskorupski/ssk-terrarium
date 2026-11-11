# Documentation technique

Ordre de lecture pour qui arrive sur le projet :

1. [invariants.md](invariants.md) : ce qui ne se transgresse jamais, et pourquoi.
2. [developpement.md](developpement.md) : l'arborescence, les commandes, les pièges d'outillage.
3. [pratiques.md](pratiques.md) : couches, tests, erreurs, livraison.
4. [clean-code.md](clean-code.md) : lisibilité et ce que l'outillage impose.
5. [conventions.md](conventions.md) : nommage, commentaires, commits.

Pour monter le régulateur plutôt que le développer : [notice/README.md](notice/README.md), une notice pas à pas par phase. Pour le relier au Wi-Fi, lire ses mesures et le commander à distance : [supervision.md](supervision.md). Pour chauffer sur des plages horaires : [mode-plages.md](mode-plages.md). Pour aérer le bac, régler le jour et la nuit et la consigne de nuit : [ventilation.md](ventilation.md).

Le pourquoi fonctionnel (modes, consignes, protections, matériel) est dans les cadrages [ssk-terrarium-cadrage#2026-001](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-001) pour le Wi-Fi et MQTT, [ssk-terrarium-cadrage#2026-002](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-002), et pour le mode `PLAGES` et l'heure NTP, [ssk-terrarium-cadrage#2026-003](https://ssk-canon.s3-website.fr-par.scw.cloud/ssk-it/ssk-terrarium-cadrage/cadrages/2026-003) : le code y renvoie par le nom des règles, `RG-…`.
