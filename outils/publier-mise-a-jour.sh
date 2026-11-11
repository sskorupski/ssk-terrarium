#!/usr/bin/env bash
#
# Publie dans Skaron l'image d'une release, signée sur ce poste (ADR-mise-a-jour-distribuee-par-skaron,
# décision 05-signature-et-publication du cadrage 2026-010) :
#
#   1. télécharge firmware-<tag>.bin et son empreinte depuis la release GitHub, vérifie l'empreinte,
#      et que l'image annonce bien ce tag dans sa ligne de démarrage ;
#   2. la signe avec la clé minisign du trousseau, phrase de passe tapée au clavier, et le
#      commentaire de confiance « ssk-terrarium esp32-phase-<N> <version> » que le firmware exige ;
#   3. vérifie la signature avec outils/mise-a-jour.pub ;
#   4. après confirmation, la publie par `skaron release publish`, produit ssk-terrarium, cible
#      esp32-phase-<N>, canal stable.
#
# Le tag phase-N donne la version N.0.0, phase-N.M donne N.M.0 (RG-mise-a-jour-a-distance). Rien ne
# s'installe pour autant : chaque régulateur attend sa commande `installer`.
#
# Usage : outils/publier-mise-a-jour.sh <tag>        par exemple phase-1.3
# Variables : SKARON_S3_ENDPOINT, SKARON_S3_ACCESS_KEY, SKARON_S3_SECRET_KEY et les autres
# SKARON_* du CLI de Skaron (son README, « Émettre une licence ») ; facultatives : CANAL (stable),
# SERVICE (élément du trousseau, ssk-terrarium-mise-a-jour), COMPTE ($USER).
# Prérequis : macOS (security, shasum), gh, minisign, skaron.

set -euo pipefail
cd "$(dirname "$0")/.."

DEPOT="sskorupski/ssk-terrarium"
PRODUIT="ssk-terrarium"
CANAL="${CANAL:-stable}"
SERVICE="${SERVICE:-ssk-terrarium-mise-a-jour}"
COMPTE="${COMPTE:-${USER}}"
PUBLIQUE="outils/mise-a-jour.pub"

ok()  { echo "  ✓ $*"; }
ko()  { echo "  ✗ $*" >&2; exit 1; }
dit() { echo "  · $*"; }
confirmer() {
    local reponse
    read -r -p "  → $* [o/N] " reponse
    [ "${reponse}" = "o" ] || [ "${reponse}" = "O" ]
}

TAG="${1:-}"
[[ "${TAG}" =~ ^phase-([1-9][0-9]*)(\.(0|[1-9][0-9]*))?$ ]] \
    || ko "usage : $0 phase-N ou phase-N.M"
PHASE="${BASH_REMATCH[1]}"
VERSION="${PHASE}.${BASH_REMATCH[3]:-0}.0"
CIBLE="esp32-phase-${PHASE}"
COMMENTAIRE="${PRODUIT} ${CIBLE} ${VERSION}"
IMAGE="firmware-${TAG}.bin"

for outil in security shasum gh minisign skaron; do
    command -v "${outil}" >/dev/null || ko "${outil} introuvable"
done
for variable in SKARON_S3_ENDPOINT SKARON_S3_ACCESS_KEY SKARON_S3_SECRET_KEY; do
    [ -n "${!variable:-}" ] || ko "${variable} absente : voir le README de Skaron, « Émettre une licence »"
done
[ -r "${PUBLIQUE}" ] || ko "${PUBLIQUE} absente : outils/cle-mise-a-jour.sh"
security find-generic-password -s "${SERVICE}" -a "${COMPTE}" >/dev/null 2>&1 \
    || ko "aucune clé « ${SERVICE} » dans le trousseau : outils/cle-mise-a-jour.sh"

travail="$(mktemp -d)"
trap 'rm -rf "${travail}"' EXIT

echo "Mise à jour ${TAG} → Skaron : ${PRODUIT} ${VERSION}, cible ${CIBLE}, canal ${CANAL}"
echo

echo "1. Image de la release"
gh release download "${TAG}" -R "${DEPOT}" -p "${IMAGE}" -p "${IMAGE}.sha256" -D "${travail}" \
    || ko "release ${TAG} sans ${IMAGE} : publiée avant que publication.yml ne l'attache ?"
(cd "${travail}" && shasum -a 256 -c "${IMAGE}.sha256" >/dev/null) || ko "empreinte SHA-256 différente"
ok "${IMAGE} : empreinte vérifiée"
grep -aqF "=== ssk-terrarium ${TAG} : demarrage ===" "${travail}/${IMAGE}" \
    || ko "l'image n'annonce pas ${TAG} au démarrage : compilée depuis un autre commit ?"
ok "l'image annonce ${TAG}"
echo

echo "2. Signature"
dit "commentaire de confiance : ${COMMENTAIRE}"
dit "minisign demande la phrase de passe de la clé"
# La clé chiffrée passe du trousseau à minisign par un descripteur, jamais par un fichier.
minisign -S -s <(security find-generic-password -s "${SERVICE}" -a "${COMPTE}" -w | base64 -d) \
    -m "${travail}/${IMAGE}" -x "${travail}/${IMAGE}.minisig" -t "${COMMENTAIRE}" >/dev/null \
    || ko "signature refusée"
ok "image signée"
echo

echo "3. Vérification, avec la clé que le firmware compile"
relu="$(minisign -V -Q -p "${PUBLIQUE}" -m "${travail}/${IMAGE}" -x "${travail}/${IMAGE}.minisig")" \
    || ko "la signature ne se vérifie pas avec ${PUBLIQUE}"
[ "${relu}" = "${COMMENTAIRE}" ] || ko "commentaire de confiance inattendu : ${relu}"
ok "signature et commentaire vérifiés"
echo

echo "4. Publication"
artefacts="${travail}/artefacts/${CIBLE}"
mkdir -p "${artefacts}"
cp "${travail}/${IMAGE}" "${artefacts}/firmware.bin"
# Comme le .sig de Tauri : le fichier de signature en base64, sur une ligne.
base64 < "${travail}/${IMAGE}.minisig" | tr -d '\n' > "${artefacts}/firmware.bin.sig"
gh release view "${TAG}" -R "${DEPOT}" --json body -q .body > "${travail}/notes.md"
[ -s "${travail}/notes.md" ] || echo "${TAG}" > "${travail}/notes.md"
dit "skaron release publish --prd ${PRODUIT} --channel ${CANAL} --version ${VERSION}"
confirmer "publier ${VERSION} sur le canal ${CANAL} ? Les régulateurs se la verront proposer" \
    || { dit "rien publié"; exit 0; }
skaron release publish --prd "${PRODUIT}" --channel "${CANAL}" --version "${VERSION}" \
    --notes "@${travail}/notes.md" --artifacts "${travail}/artefacts"
ok "publiée"
echo
echo "Pour l'installer sur un régulateur, sans défaut en cours :"
echo "  {\"action\":\"verifier\"} puis {\"action\":\"installer\",\"version\":\"${VERSION}\"} sur ssk-terrarium/<id>/cmd/mise-a-jour"
