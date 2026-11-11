#!/usr/bin/env bash
#
# Génère la paire minisign qui signe les mises à jour du firmware (RG-image-signee), une fois :
#
#   1. minisign -G écrit la clé secrète, chiffrée par la phrase de passe tapée au clavier, dans
#      ~/.minisign/ssk-terrarium-mise-a-jour.key, et la clé publique dans outils/mise-a-jour.pub ;
#   2. la clé secrète est rangée dans le trousseau macOS, mot de passe générique
#      « ssk-terrarium-mise-a-jour », en base64 sur une ligne ;
#   3. en lecture seule : l'élément se relit, identique au fichier.
#
# La clé publique se commite : le firmware la compile, et sans elle il ne se compile pas. La clé
# secrète ne quitte pas ce poste (décision 05-signature-et-publication du cadrage 2026-010) : ni la
# CI ni Skaron ne la détiennent. Le fichier n'est jamais supprimé par ce script : en faire d'abord
# une copie hors de cette machine. Perdue, la clé ne se reconstitue pas, et plus aucune carte en
# service n'accepte d'image : il faudrait les reprendre toutes au câble.
#
# Une clé déjà rangée n'est jamais remplacée : changer de clé exige un firmware qui connaisse la
# nouvelle, installé avec l'ancienne.
#
# Usage : outils/cle-mise-a-jour.sh
# Variables facultatives : CLE (fichier de la clé secrète), SERVICE (élément du trousseau),
# COMPTE (défaut $USER).
# Prérequis : macOS (security, base64, shasum), minisign (brew install minisign).

set -euo pipefail
cd "$(dirname "$0")/.."

CLE="${CLE:-${HOME}/.minisign/ssk-terrarium-mise-a-jour.key}"
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
empreinte() { shasum -a 256 | cut -d' ' -f1; }

command -v security >/dev/null || ko "security introuvable : ce script est prévu pour macOS"
command -v minisign >/dev/null || ko "minisign introuvable : brew install minisign"
[[ "${SERVICE}" =~ ^[A-Za-z0-9._-]+$ && "${COMPTE}" =~ ^[A-Za-z0-9._@-]+$ ]] \
    || ko "service ou compte avec des caractères non prévus"

echo "Paire de signature des mises à jour (trousseau : service ${SERVICE}, compte ${COMPTE})"
echo

echo "1. Rien n'existe encore"
if security find-generic-password -s "${SERVICE}" -a "${COMPTE}" >/dev/null 2>&1; then
    ko "le trousseau a déjà une clé « ${SERVICE} » : elle n'est pas remplacée"
fi
[ ! -e "${CLE}" ] || ko "${CLE} existe déjà : il n'est pas écrasé"
if [ -e "${PUBLIQUE}" ]; then
    dit "${PUBLIQUE} existe déjà, sans clé secrète au trousseau : elle sera remplacée"
fi
ok "aucune clé dans le trousseau, aucun fichier à écraser"
echo

echo "2. Génération"
dit "minisign demande la phrase de passe deux fois ; elle chiffre la clé secrète, et chaque"
dit "publication la redemandera. Ne jamais générer sans phrase de passe."
confirmer "générer la paire ?" || { dit "rien écrit"; exit 0; }
mkdir -p "$(dirname "${CLE}")"
temporaire="$(mktemp -d)"
trap 'rm -rf "${temporaire}"' EXIT
minisign -G -p "${temporaire}/mise-a-jour.pub" -s "${CLE}"
chmod 600 "${CLE}"
ok "clé secrète : ${CLE}"
echo

echo "3. Trousseau"
ATTENDU="$(base64 < "${CLE}" | tr -d '\n' | empreinte)"
dit "mot de passe générique « ${SERVICE} », valeur : la clé chiffrée en base64, jamais affichée"
# `security -i` lit ses commandes sur l'entrée standard : la valeur n'est jamais en argument.
printf 'add-generic-password -a "%s" -s "%s" -l "ssk-terrarium — clé de signature des mises à jour" -w "%s"\n' \
    "${COMPTE}" "${SERVICE}" "$(base64 < "${CLE}" | tr -d '\n')" | security -i >/dev/null \
    || ko "écriture refusée par le trousseau"
RELU="$(security find-generic-password -s "${SERVICE}" -a "${COMPTE}" -w 2>/dev/null || true)"
[ -n "${RELU}" ] || ko "l'élément ne se relit pas"
[ "$(printf '%s' "${RELU}" | tr -d '\n' | empreinte)" = "${ATTENDU}" ] || ko "l'élément relu diffère du fichier"
unset RELU
ok "élément écrit, relu identique au fichier"
echo

echo "4. Clé publique"
mv "${temporaire}/mise-a-jour.pub" "${PUBLIQUE}"
ok "${PUBLIQUE} :"
sed -n 2p "${PUBLIQUE}"
echo
echo "Terminé. À faire :"
echo "  - commiter ${PUBLIQUE} : le firmware la compile ;"
echo "  - copier ${CLE} hors de cette machine (la clé est chiffrée par la phrase de passe) ;"
echo "  - publier une version : outils/publier-mise-a-jour.sh <tag>."
