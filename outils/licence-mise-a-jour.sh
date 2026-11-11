#!/usr/bin/env bash
#
# Émet la licence Skaron d'un régulateur et la lui transmet (décision 06-licence-du-regulateur du
# cadrage 2026-010), une fois par régulateur :
#
#   1. demande au clavier le mot de passe MQTT du compte propriétaire, jamais affiché ;
#   2. après confirmation, émet la licence par `skaron license issue`, produit ssk-terrarium, abonné
#      l'identifiant du régulateur, avec la clé de licence du trousseau ;
#   3. la publie sur ssk-terrarium/<id>/cmd/mise-a-jour, par l'entrée standard de mosquitto_pub ;
#   4. attend la réponse du régulateur sur event : {"etape":"licence","resultat":"applique"}.
#
# Le jeton ne s'affiche jamais, ne passe ni en argument ni dans un fichier : d'une variable du shell
# à l'entrée standard. Le mot de passe MQTT va dans un fichier de configuration de mosquitto, lisible
# du seul utilisateur et effacé à la sortie. Réémettre une licence remplace la précédente.
#
# Usage : outils/licence-mise-a-jour.sh <id>      l'adresse MAC du régulateur, 12 chiffres hexadécimaux
# Variables : SKARON_S3_ENDPOINT, SKARON_S3_ACCESS_KEY, SKARON_S3_SECRET_KEY (README de Skaron) ;
# facultatives : KID (clé de licence, k2026), CANAL (stable), EXP (expiration, un an par défaut),
# MQTT_COMPTE (proprietaire), MQTT_HOTE (mq.ssk-it.fr), MQTT_PORT (443).
# Prérequis : macOS (security), skaron, mosquitto_pub et mosquitto_sub (brew install mosquitto).

set -euo pipefail
cd "$(dirname "$0")/.."

PRODUIT="ssk-terrarium"
KID="${KID:-k2026}"
CANAL="${CANAL:-stable}"
MQTT_COMPTE="${MQTT_COMPTE:-proprietaire}"
MQTT_HOTE="${MQTT_HOTE:-mq.ssk-it.fr}"
MQTT_PORT="${MQTT_PORT:-443}"
SERVICE_LICENCE="skaron-licence-${KID}"
ATTENTE_S=30

ok()  { echo "  ✓ $*"; }
ko()  { echo "  ✗ $*" >&2; exit 1; }
dit() { echo "  · $*"; }
confirmer() {
    local reponse
    read -r -p "  → $* [o/N] " reponse
    [ "${reponse}" = "o" ] || [ "${reponse}" = "O" ]
}

ID="${1:-}"
[[ "${ID}" =~ ^[0-9a-f]{12}$ ]] \
    || ko "usage : $0 <id>, l'identifiant écrit au démarrage : MQTT identifiant ssk-terrarium/<id>"
[[ -z "${EXP:-}" || "${EXP}" =~ ^[0-9T:+.Z-]+$ ]] || ko "EXP : AAAA-MM-JJ ou RFC 3339"
for outil in security skaron mosquitto_pub mosquitto_sub; do
    command -v "${outil}" >/dev/null || ko "${outil} introuvable"
done
for variable in SKARON_S3_ENDPOINT SKARON_S3_ACCESS_KEY SKARON_S3_SECRET_KEY; do
    [ -n "${!variable:-}" ] || ko "${variable} absente : voir le README de Skaron, « Émettre une licence »"
done
security find-generic-password -s "${SERVICE_LICENCE}" -a "${USER}" >/dev/null 2>&1 \
    || ko "aucune clé de licence « ${SERVICE_LICENCE} » dans le trousseau (Skaron, outillage/cle-licence)"

travail="$(mktemp -d)"
abonne=""
trap '[ -n "${abonne}" ] && kill "${abonne}" 2>/dev/null; rm -rf "${travail}"' EXIT
SUJET="ssk-terrarium/${ID}/cmd/mise-a-jour"
EVENEMENTS="ssk-terrarium/${ID}/event"

echo "Licence de mise à jour du régulateur ${ID} : ${PRODUIT}, canal ${CANAL}"
echo

echo "1. Courtier"
# Les racines que le firmware embarque valident aussi le courtier depuis ce poste.
sed -n '/-----BEGIN CERTIFICATE-----/,/-----END CERTIFICATE-----/p' lib/mqtt/root_certificates.h \
    > "${travail}/racines.pem"
grep -q 'BEGIN CERTIFICATE' "${travail}/racines.pem" || ko "racines introuvables dans lib/mqtt/root_certificates.h"
read -r -s -p "  → mot de passe MQTT du compte ${MQTT_COMPTE} : " motdepasse
echo
[ -n "${motdepasse}" ] || ko "mot de passe vide"
[[ "${motdepasse}" != *" "* ]] || ko "mot de passe avec une espace : le fichier de configuration de mosquitto ne le lit pas"
# mosquitto_pub et mosquitto_sub lisent leurs options dans $XDG_CONFIG_HOME, une par ligne.
umask 077
for client in mosquitto_pub mosquitto_sub; do
    printf -- '-h %s\n-p %s\n--cafile %s\n-u %s\n-P %s\n' "${MQTT_HOTE}" "${MQTT_PORT}" \
        "${travail}/racines.pem" "${MQTT_COMPTE}" "${motdepasse}" > "${travail}/${client}"
done
unset motdepasse
ok "configuration prête, lisible de vous seul"
echo

echo "2. Licence"
dit "skaron license issue --prd ${PRODUIT} --sub ${ID} --ed regulateur --channel ${CANAL}"
dit "puis publication sur ${SUJET}, jeton jamais affiché"
confirmer "émettre la licence et la transmettre au régulateur ?" || { dit "rien émis"; exit 0; }

XDG_CONFIG_HOME="${travail}" mosquitto_sub -t "${EVENEMENTS}" -W "${ATTENTE_S}" \
    > "${travail}/reponses" 2>"${travail}/abonnement" &
abonne=$!
sleep 2  # L'abonnement doit précéder la commande.
kill -0 "${abonne}" 2>/dev/null || ko "abonnement à ${EVENEMENTS} refusé : $(cat "${travail}/abonnement")"

jeton="$(skaron license issue --kid "${KID}" \
    --key <(security find-generic-password -s "${SERVICE_LICENCE}" -a "${USER}" -w | base64 -d) \
    --prd "${PRODUIT}" --sub "${ID}" --ed regulateur --channel "${CANAL}" ${EXP:+--exp "${EXP}"})" \
    || ko "licence non émise"
[[ "${jeton}" =~ ^[A-Za-z0-9._-]+$ ]] || ko "jeton inattendu : il n'est pas transmis"
ok "licence inscrite dans Skaron"
printf '{"licence":"%s"}' "${jeton}" \
    | XDG_CONFIG_HOME="${travail}" mosquitto_pub -t "${SUJET}" -s \
    || ko "publication refusée par le courtier"
unset jeton
ok "licence publiée sur ${SUJET}"
echo

echo "3. Réponse du régulateur"
for _ in $(seq "${ATTENTE_S}"); do
    if reponse="$(grep -m1 '"etape":"licence"' "${travail}/reponses")"; then
        echo "  ${reponse}"
        [[ "${reponse}" == *'"resultat":"applique"'* ]] && ok "licence enregistrée par le régulateur" && exit 0
        ko "le régulateur n'a pas enregistré la licence"
    fi
    sleep 1
done
ko "aucune réponse en ${ATTENTE_S} s : régulateur hors ligne ? La licence est émise : relancer le script la remplace"
