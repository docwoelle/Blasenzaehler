String getContentType(String filename) {
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";

  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";

  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) {
    if (LittleFS.exists(pathWithGz)) path += ".gz";

    File file = LittleFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();

    Serial.println(String("\tSent file: ") + path);
    return true;
  }

  Serial.println(String("\tFile Not Found: ") + path);
  return false;
}

void handleReboot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent("rebooting<script>window.setTimeout(\"self.location.href='/'\",2000);</script>");
  server.client().stop();
  delay(1000);
  ESP.restart();
}

void handleReset() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent("reset LittleFS<script>window.setTimeout(\"self.location.href='/'\",2000);</script>");
  server.client().stop();
  delay(1000);
}

void handleConfig() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent(String() + "<center>Firmware: " + FW_VERSION +
                     "<br>Timestamp: " + String(now()) + "</center><br>");

  String content =
    "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Blasenzähler</title><style>"
    "body{background-color:#f7f7f7;color:#333;font-family:'Segoe UI',Verdana,sans-serif;margin:0;padding:0;}"
    "h2{text-align:center;background-color:#0080ff;color:white;padding:12px 0;margin:0 0 15px 0;font-weight:500;box-shadow:0 2px 4px rgba(0,0,0,0.2);}"
    "table{width:90%;max-width:420px;margin:auto;border-collapse:collapse;font-size:1.05em;background-color:white;border-radius:10px;overflow:hidden;box-shadow:0 0 8px rgba(0,0,0,0.1);}"
    "th,td{padding:10px 8px;text-align:left;border-bottom:1px solid #ddd;}"
    "th{background-color:#e8f0ff;font-weight:600;}tr:last-child td{border-bottom:none;}"
    "button{display:block;margin:20px auto;background-color:#0080ff;color:white;border:none;border-radius:8px;padding:12px 24px;font-size:1.1em;}"
    "</style></head><body><h2>Konfiguration</h2>"
    "<form action='/configsave' method='post'><table>";

  content += "<tr><td>Name (Label)</td><td><input type=text name='website' value='" + website + "'></td></tr>";
  content += "<tr><td>MQTT-Server</td><td><input type=text name='mqtt_server' value='" + String(mqtt_server) + "'></td></tr>";
  content += "<tr><td>MQTT-Port</td><td><input type=number min=1 max=65535 name='mqtt_port' value='" + String(mqtt_port) + "'></td></tr>";
  content += "<tr><td>Benutzername</td><td><input type=text name='username' value='" + String(username) + "'></td></tr>";
  content += "<tr><td>Passwort</td><td><input type=password name='password' value='" + String(password) + "'></td></tr>";
  content += "<tr><td>Basetopic</td><td><input type=text name='BASETOPIC' value='" + BASETOPIC + "'></td></tr>";
  content += "</table><br><center><input type='submit' value='Speichern'></form>"
             "<br><br><a href='/'>Hauptseite</a></center>";

  server.sendContent(content);
  server.client().stop();
}

void handleSaveConfig() {
  strlcpy(mqtt_server, server.arg("mqtt_server").c_str(), sizeof(mqtt_server));
  mqtt_port = server.arg("mqtt_port").toInt();

  if (mqtt_port < 1 || mqtt_port > 65535) {
    mqtt_port = 1883;
  }

  strlcpy(username, server.arg("username").c_str(), sizeof(username));
  strlcpy(password, server.arg("password").c_str(), sizeof(password));
  BASETOPIC = server.arg("BASETOPIC");
  website = server.arg("website");

  saveConfig();

  server.send(200, "text/html",
              "<html><body><h3>Konfiguration gespeichert!</h3>"
              "<p>Neustart in 1 Sekunde...</p>"
              "<script>setTimeout(()=>{location.href='/'},5000)</script>"
              "</body></html>");

  delay(1000);
  ESP.restart();
}

void saveConfig() {
  DynamicJsonDocument doc(1024);
  doc["mqtt_server"] = mqtt_server;
  doc["mqtt_port"] = mqtt_port;
  doc["username"] = username;
  doc["password"] = password;
  doc["BASETOPIC"] = BASETOPIC;
  doc["website"] = website;

  File file = LittleFS.open("/config.json", "w");

  if (!file) {
    Serial.println("Fehler beim Öffnen von config.json zum Schreiben!");
    return;
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("Fehler beim Schreiben der JSON-Datei!");
  } else {
    Serial.println("Konfiguration erfolgreich gespeichert.");
  }

  file.close();
}

void handleNotFound() {
  if (!handleFileRead(server.uri())) {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(404, "text/plain", message);
  }
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent(
    "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Blasenzähler</title><style>"
    "#timestamp{text-align:center;font-size:.95em;color:#444;margin-top:6px;}"
    "body{background-color:#f7f7f7;color:#333;font-family:'Segoe UI',Verdana,sans-serif;margin:0;padding:0;}"
    "h2{text-align:center;background-color:#0080ff;color:white;padding:12px 0;margin:0 0 15px 0;font-weight:500;box-shadow:0 2px 4px rgba(0,0,0,.2);}"
    "table{width:90%;max-width:420px;margin:auto;border-collapse:collapse;font-size:1.05em;background-color:white;border-radius:10px;overflow:hidden;box-shadow:0 0 8px rgba(0,0,0,.1);}"
    "th,td{padding:10px 8px;text-align:left;border-bottom:1px solid #ddd;}"
    "th{background-color:#e8f0ff;font-weight:600;}tr:last-child td{border-bottom:none;}"
    "td b{color:#0066cc;}.footer{text-align:center;margin:15px 0;font-size:.9em;color:#666;}"
    "</style></head><body>"
  );

  String content = "<h2>" + website + "</h2>"
                   "<center>Alter der Blasen zur Folgeblase in s</center>"
                   "<table id='dataTable'>"
                   "<tr><th>Blase</th><th>Zeit (s)</th><th>Wert</th></tr>"
                   "<tr><td>5</td><td id='b5'>-</td><td id='v5'>-</td></tr>"
                   "<tr><td>4</td><td id='b4'>-</td><td id='v4'>-</td></tr>"
                   "<tr><td>3</td><td id='b3'>-</td><td id='v3'>-</td></tr>"
                   "<tr><td>2</td><td id='b2'>-</td><td id='v2'>-</td></tr>"
                   "<tr><td>1</td><td id='b1'>-</td><td id='v1'>-</td></tr>"
                   "<tr><td><b>Schnitt</b></td><td id='schnitt'>-</td><td></td></tr>"
                   "<tr><td><b>Blasen/min</b></td><td id='bpm'>-</td><td></td></tr>"
                   "<tr><td><b>Blasen/h</b></td><td id='bps'>-</td><td></td></tr>"
                   "<tr><td><b>Temperatur</b></td><td id='temp'>-</td><td></td></tr>"
                   "</table>"
                   "<div id='timestamp'>Letztes Update: -</div><br>"
                   "<center><a href='/config'>Konfiguration</a></center><br>";

  content += R"rawliteral(
<script>
function formatDate(d) {
  const pad = n => n < 10 ? '0' + n : n;
  return pad(d.getDate()) + '.' + pad(d.getMonth()+1) + '.' +
         d.getFullYear() + ' ' + pad(d.getHours()) + ':' +
         pad(d.getMinutes()) + ':' + pad(d.getSeconds());
}

function setCell(id, value) {
  const e = document.getElementById(id);
  if (e) e.textContent = value;
}

async function updateData() {
  try {
    const response = await fetch('/data.json', {cache: 'no-store'});
    if (!response.ok) return;

    const data = await response.json();
    const a = data.abstaende || [];
    const v = data.werte || [];

    for (let i = 0; i < 5; i++) {
      const row = 5 - i;
      setCell('b' + row, a[i] == null ? '-' : Number(a[i]).toFixed(1));
      setCell('v' + row, v[i] == null ? '-' : '(' + v[i] + ')');
    }

    setCell('schnitt', data.schnitt == null ? '-' : Number(data.schnitt).toFixed(1));
    setCell('ar', data.ar == null ? '-' : Number(data.ar).toFixed(0));
    setCell('temp', data.temp == null ? '-' : Number(data.temp).toFixed(1) + '°C');
    setCell('minimum', data.minimum == null ? '-' : Number(data.minimum).toFixed(0));
    setCell('minimumsw', data.minimumsw == null ? '-' : Number(data.minimumsw).toFixed(0));

    setCell('timestamp', 'Letztes Update: ' + formatDate(new Date()));

    setCell('bpm', data.bpm == null ? '-' : Number(data.bpm).toFixed(1));
    setCell('bps', data.bps == null ? '-' : Number(data.bps).toFixed(1));
  } catch (e) {
    console.log("Fehler beim Abrufen:", e);
  }
}

setInterval(updateData, 2000);
updateData();
</script>
)rawliteral";

  content += "<h2>Analoge Messwerte</h2>"
             "<center>Aktueller Messwert: <span id='ar'>-</span><br>"
             "60s-Schwellwert: <span id='minimumsw'>-</span><br>"
             "60s-Minimum: <span id='minimum'>-</span></center>";

  server.sendContent(content);
  server.sendContent("</body></html>");
  server.client().stop();
}

void handleDataJson() {
  DynamicJsonDocument doc(768);
  JsonArray abstaende = doc.createNestedArray("abstaende");
  JsonArray werte = doc.createNestedArray("werte");

  // Die fünf historischen Werte. Noch nicht vorhandene Werte werden als null
  // ausgegeben; dadurch entsteht im Browser kein NaN/Infinity-Problem.
  for (int i = 0; i < 4; i++) {
    int idx = 4 - i;
    if (bubbleHistoryCount >= (uint8_t)(i + 2)) {
      abstaende.add((blas[idx - 1] - blas[idx]) / 1000.0f);
    } else {
      abstaende.add(nullptr);
    }
  }

  if (bubbleHistoryCount >= 1) {
    abstaende.add((millis() - blas[0]) / 1000.0f);
  } else {
    abstaende.add(nullptr);
  }

  for (int i = 4; i >= 0; i--) {
    int historyIndex = i;
    int needed = 5 - i;

    if (bubbleHistoryCount >= needed) {
      werte.add(blasval[historyIndex]);
    } else {
      werte.add(nullptr);
    }
  }

  float schnitt = 0.0f;
  float blasenProMin = 0.0f;
  float blasenProStd = 0.0f;

  if (bubbleHistoryCount >= 5) {
    uint32_t intervall = blas[0] - blas[4];
    if (intervall > 0) {
      schnitt = (intervall / 4.0f) / 1000.0f;
      blasenProMin = 60000.0f / (intervall / 4.0f);
      blasenProStd = 3600000.0f / (intervall / 4.0f);
    }
  }

  doc["ar"] = ar;
  doc["temp"] = tempbz;
  doc["minimumsw"] = getsecMin();
  doc["minimum"] = minw == 65535 ? 0 : minw;
  doc["schnitt"] = bubbleHistoryCount >= 5 ? schnitt : nullptr;
  doc["bpm"] = bubbleHistoryCount >= 5 ? blasenProMin : nullptr;
  doc["bps"] = bubbleHistoryCount >= 5 ? blasenProStd : nullptr;
  doc["blasen"] = bubbleHistoryCount;
  doc["mqtt_pending"] = mqtt_bla;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}
