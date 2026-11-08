#include "web_ui.h"

#include <WiFi.h>

namespace terrarium::web {

namespace {

constexpr char kPortalAddress[] = "http://192.168.4.1/";

// Text placed in an HTML attribute or element: SSIDs may hold any character.
String escapeHtml(const char* text) {
  String out;
  for (; *text != '\0'; ++text) {
    switch (*text) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += *text;
    }
  }
  return out;
}

const char* postedField(AsyncWebServerRequest* request, const char* name) {
  const AsyncWebParameter* parameter = request->getParam(name, /*post=*/true);
  return parameter == nullptr ? nullptr : parameter->value().c_str();
}

// The portal answers on the access point only, never on the home network.
bool fromAccessPoint(AsyncWebServerRequest* request) {
  return request->client()->localIP() == WiFi.softAPIP();
}

}  // namespace

void WebUi::begin() {
  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) { handleForm(request); });
  server_.on("/", HTTP_POST, [this](AsyncWebServerRequest* request) { handleSave(request); });
  // Captive portal: every other address leads to the form, which is what
  // phones probe for to open the page by themselves.
  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (!fromAccessPoint(request)) {
      request->send(404);
      return;
    }
    request->redirect(kPortalAddress);
  });
  server_.begin();
}

void WebUi::showCurrent(const supervision::NetworkSettings& settings) {
  portENTER_CRITICAL(&lock_);
  current_ = settings;
  portEXIT_CRITICAL(&lock_);
}

bool WebUi::takePosted(supervision::NetworkSettings& out) {
  portENTER_CRITICAL(&lock_);
  const bool has = hasPosted_;
  if (has) out = posted_;
  hasPosted_ = false;
  portEXIT_CRITICAL(&lock_);
  return has;
}

void WebUi::handleForm(AsyncWebServerRequest* request) {
  if (!fromAccessPoint(request)) {
    request->send(404);
    return;
  }
  sendForm(request, 200, nullptr, false);
}

void WebUi::handleSave(AsyncWebServerRequest* request) {
  if (!fromAccessPoint(request)) {
    request->send(404);
    return;
  }
  supervision::NetworkSettings settings{};
  const supervision::SettingsError error = supervision::fillFromForm(
      postedField(request, "ssid"), postedField(request, "wifi_mdp"), postedField(request, "hote"),
      postedField(request, "port"), postedField(request, "mqtt_id"),
      postedField(request, "mqtt_mdp"), settings);
  if (error != supervision::SettingsError::kNone) {
    sendForm(request, 400, supervision::describe(error), true);
    return;
  }
  portENTER_CRITICAL(&lock_);
  posted_ = settings;
  hasPosted_ = true;
  current_ = settings;
  portEXIT_CRITICAL(&lock_);

  String html =
      "<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Terrarium : r&eacute;glages enregistr&eacute;s</title></head><body><main>"
      "<h1>R&eacute;glages enregistr&eacute;s</h1><p role=\"status\">Le r&eacute;gulateur "
      "rejoint le Wi-Fi <strong>";
  html += escapeHtml(settings.wifiSsid);
  html +=
      "</strong>. Ce r&eacute;seau de configuration se fermera d&egrave;s qu'il sera "
      "connect&eacute;. "
      "S'il n'y parvient pas, ce r&eacute;seau reste ouvert : revenez sur cette page pour "
      "corriger.</p></main></body></html>";
  request->send(200, "text/html; charset=utf-8", html);
}

void WebUi::sendForm(AsyncWebServerRequest* request, int status, const char* message,
                     bool isError) {
  supervision::NetworkSettings current{};
  portENTER_CRITICAL(&lock_);
  current = current_;
  portEXIT_CRITICAL(&lock_);
  char port[6] = "443";
  if (current.brokerPort != 0) snprintf(port, sizeof(port), "%u", current.brokerPort);

  String html =
      "<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Terrarium : configuration du r&eacute;seau</title><style>"
      "body{font-family:system-ui,sans-serif;max-width:32rem;margin:0 auto;padding:1rem;"
      "color:#111;background:#fff}label{display:block;margin-top:1rem;font-weight:600}"
      "input{display:block;width:100%;box-sizing:border-box;padding:.6rem;font-size:1rem;"
      "border:1px solid #555}button{margin-top:1.5rem;padding:.7rem 1.2rem;font-size:1rem}"
      "input:focus,button:focus{outline:3px solid #1a5fb4;outline-offset:2px}"
      ".erreur{color:#a00;font-weight:600}fieldset{border:1px solid #888;margin-top:1rem}"
      "</style></head><body><main><h1>Configuration du r&eacute;seau</h1>";
  if (message != nullptr) {
    html += isError ? "<p class=\"erreur\" role=\"alert\">" : "<p role=\"status\">";
    html += escapeHtml(message);
    html += "</p>";
  }
  html +=
      "<form method=\"post\" action=\"/\"><fieldset><legend>Wi-Fi du domicile</legend>"
      "<label for=\"ssid\">Nom du Wi-Fi</label>"
      "<input id=\"ssid\" name=\"ssid\" required maxlength=\"32\" autocomplete=\"off\" value=\"";
  html += escapeHtml(current.wifiSsid);
  html +=
      "\"><label for=\"wifi_mdp\">Mot de passe du Wi-Fi</label>"
      "<input id=\"wifi_mdp\" name=\"wifi_mdp\" type=\"password\" maxlength=\"63\" "
      "autocomplete=\"off\"></fieldset><fieldset><legend>Courtier MQTT</legend>"
      "<label for=\"hote\">Adresse du courtier</label>"
      "<input id=\"hote\" name=\"hote\" required maxlength=\"63\" autocomplete=\"off\" "
      "autocapitalize=\"none\" value=\"";
  html += escapeHtml(current.brokerHost);
  html +=
      "\"><label for=\"port\">Port</label>"
      "<input id=\"port\" name=\"port\" required inputmode=\"numeric\" pattern=\"[0-9]{1,5}\" "
      "value=\"";
  html += port;
  html +=
      "\"><label for=\"mqtt_id\">Identifiant MQTT</label>"
      "<input id=\"mqtt_id\" name=\"mqtt_id\" required maxlength=\"32\" autocomplete=\"off\" "
      "autocapitalize=\"none\" value=\"";
  html += escapeHtml(current.mqttUser);
  html +=
      "\"><label for=\"mqtt_mdp\">Mot de passe MQTT</label>"
      "<input id=\"mqtt_mdp\" name=\"mqtt_mdp\" type=\"password\" required maxlength=\"64\" "
      "autocomplete=\"off\"></fieldset><button type=\"submit\">Enregistrer</button></form>"
      "<p>Les mots de passe ne sont jamais r&eacute;affich&eacute;s : saisissez-les &agrave; "
      "chaque enregistrement.</p></main></body></html>";
  request->send(status, "text/html; charset=utf-8", html);
}

}  // namespace terrarium::web
