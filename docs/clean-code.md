# Code propre

Ce document gouverne l'intérieur d'une unité ; les frontières entre couches sont dans [pratiques.md](pratiques.md).

## Lisibilité

- **Un nom dit l'unité et le sens.** `kSensorSilentMaxMs`, `kFrostHeatOffAtC`, `temperatureC` : la grandeur et son unité sont dans le nom, ce qui évite de confondre des secondes et des millisecondes à la relecture d'une comparaison.
- **Un booléen se lit comme une réponse** : `measurementValid`, `mistAllowed`, `isOn()`.
- **Une fonction fait une chose à un seul niveau.** `regulate()` décide et applique ; la traduction d'un état de capteur en état de chauffage est à part, dans `heaterFrom()`, et chaque branche y tient en une ligne.
- **Un `switch` sur une énumération couvre chaque valeur sans `default`** : le compilateur signale (`-Wswitch`) une valeur ajoutée et oubliée. La valeur sûre se retourne après le `switch`, comme dans `policyFor()`, qui retombe sur « tout coupé ».

## Sobriété

- Les objets vivent en statique, les chaînes de commande dans un tampon fixe (`serialLine[32]`) : rien n'alloue dans `loop()`.
- Une valeur constante est `constexpr`, et une contrainte entre constantes un `static_assert` dans `config.h` : une consigne qui atteindrait le seuil d'alerte haute ne compile pas.

## Conception

- Une classe du cœur a une seule raison de changer : `SensorWatch` ne sait que compter le silence d'un capteur, `HysteresisRegulator` que tenir une bande ; c'est `main.cpp` qui les combine.
- Le temps entre en paramètre (`update(nowMs, valid)`) au lieu d'être lu par `millis()` : c'est ce qui rend la règle testable aux bornes.

## Refactoring

- Un changement de forme ne se mêle jamais à un changement de comportement : deux commits, et les tests verts entre les deux.
- La dette se nomme : un `TODO(phase N)` dit dans quelle phase du cadrage la lacune se comble. Un `TODO` sans phase n'est pas accepté.

## Ce que l'outillage impose

- **Format** : clang-format, à la version de `requirements-ci.txt`, style Google, 100 colonnes (`.clang-format`), vérifié par la CI.
- **Avertissements** : `-Wall -Wextra` partout ; `-Werror` en plus pour la compilation native du cœur. La compilation ESP32 n'a pas `-Werror`, parce que les bibliothèques tierces y émettent des avertissements que le projet ne peut pas corriger.
- **Pas d'analyse statique** pour l'instant : `pio check` (cppcheck) n'est pas configuré, et rien ici ne prétend le contraire.
