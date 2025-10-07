
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

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (LittleFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                       // Use the compressed version
    File file = LittleFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}


void handleReboot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
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
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.sendContent("reset LittleFS<script>window.setTimeout(\"self.location.href='/'\",2000);</script>");
  server.client().stop();
  delay(1000);
}

void handleConfig() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.sendContent( String() + "<center>Firmware: " + FW_VERSION + "<br>Timestamp: " + String(now()) + "</center><br>");

  String content = "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'> <title>Blasenzähler</title><style> body { background-color: #f7f7f7; color: #333; font-family: 'Segoe UI', Verdana, sans-serif; margin: 0; padding: 0; } h2 { text-align: center; background-color: #0080ff; color: white; padding: 12px 0; margin: 0 0 15px 0; font-weight: 500; box-shadow: 0 2px 4px rgba(0,0,0,0.2); } table { width: 90%; max-width: 420px; margin: auto; border-collapse: collapse; font-size: 1.05em; background-color: white; border-radius: 10px; overflow: hidden; box-shadow: 0 0 8px rgba(0,0,0,0.1); } th, td { padding: 10px 8px; text-align: left; border-bottom: 1px solid #ddd; } th { background-color: #e8f0ff; font-weight: 600; } tr:last-child td { border-bottom: none; } td b { color: #0066cc; } .footer { text-align: center; margin: 15px 0; font-size: 0.9em; color: #666; } button { display: block; margin: 20px auto; background-color: #0080ff; color: white; border: none; border-radius: 8px; padding: 12px 24px; font-size: 1.1em; box-shadow: 0 3px 6px rgba(0,0,0,0.2); cursor: pointer; transition: background 0.2s ease; } button:hover { background-color: #0096ff; } </style></head><body><h2>Konfiguration</h2><form action='/configsave' method='post'><table>";
  content += "<tr><td>Name (Label)</td><td><input type=text name='website' value='" + website + "'></td></tr>";
  content += "<tr><td>MQTT-Server</td><td><input type=text name='mqtt_server' value='" + String(mqtt_server) + "'></td></tr>";
  content += "<tr><td>MQTT-Port</td><td><input type=number name='mqtt_port' value='" + String(mqtt_port) + "'></td></tr>";
  content += "<tr><td>Benutzername</td><td><input type=text name='username' value='" + String(username) + "'></td></tr>";
  content += "<tr><td>Passwort</td><td><input type=password name='password' value='" + String(password) + "'></td></tr>";
  content += "<tr><td>Basetopic</td><td><input type=text name='BASETOPIC' value='" + BASETOPIC + "'></td></tr>";
  content += "</table><br><center><input type='submit' value='Speichern'></form><br><br><a href='/'>Hauptseite</a></center>";

  server.sendContent(content);
  server.client().stop();
}

void handleSaveConfig() {
  // Eingaben aus dem Formular holen
  strlcpy(mqtt_server, server.arg("mqtt_server").c_str(), sizeof(mqtt_server));
  mqtt_port = server.arg("mqtt_port").toInt();
  strlcpy(username, server.arg("username").c_str(), sizeof(username));
  strlcpy(password, server.arg("password").c_str(), sizeof(password));
  BASETOPIC = server.arg("BASETOPIC");
  website = server.arg("website");

  saveConfig();  // JSON speichern

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
  if (!handleFileRead(server.uri()))     {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for ( uint8_t i = 0; i < server.args(); i++ ) {
      message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( 404, "text/plain", message );
  }
}


void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.sendContent(
    "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'> <title>Blasenzaehler</title><style>#timestamp { text-align: center; font-size: 0.95em; color: #444; margin-top: 6px; } body { background-color: #f7f7f7; color: #333; font-family: 'Segoe UI', Verdana, sans-serif; margin: 0; padding: 0; } h2 { text-align: center; background-color: #0080ff; color: white; padding: 12px 0; margin: 0 0 15px 0; font-weight: 500; box-shadow: 0 2px 4px rgba(0,0,0,0.2); } table { width: 90%; max-width: 420px; margin: auto; border-collapse: collapse; font-size: 1.05em; background-color: white; border-radius: 10px; overflow: hidden; box-shadow: 0 0 8px rgba(0,0,0,0.1); } th, td { padding: 10px 8px; text-align: left; border-bottom: 1px solid #ddd; } th { background-color: #e8f0ff; font-weight: 600; } tr:last-child td { border-bottom: none; } td b { color: #0066cc; } .footer { text-align: center; margin: 15px 0; font-size: 0.9em; color: #666; } button { display: block; margin: 20px auto; background-color: #0080ff; color: white; border: none; border-radius: 8px; padding: 12px 24px; font-size: 1.1em; box-shadow: 0 3px 6px rgba(0,0,0,0.2); cursor: pointer; transition: background 0.2s ease; } button:hover { background-color: #0096ff; } </style></head><body>"
  );
  String content = "<h2>"+(String)website+"</h2><center>Alter der Blasen zur Folgeblase in s</center><table>";
  //  content += "<tr><td align=center><b>#</b></td><td align=right align=center><b>Blasenalter</b></td><td align=center valign=bottom><b>max.<br>Messwert</b></td></tr>";
  // Zeitspanne über 5 Blasen (von letzter bis 5. letzter)

  content += R"rawliteral(<table id="dataTable">
    <tr><th>Blase</th><th>Zeit (s)</th><th>Wert</th></tr>
    <tr><td>5</td><td id="b5"></td><td id="v5"></td></tr>
    <tr><td>4</td><td id="b4"></td><td id="v4"></td></tr>
    <tr><td>3</td><td id="b3"></td><td id="v3"></td></tr>
    <tr><td>2</td><td id="b2"></td><td id="v2"></td></tr>
    <tr><td>1</td><td id="b1"></td><td id="v1"></td></tr>
    <tr><td><b>Schnitt</b></td><td id="schnitt"></td><td></td></tr>
    <tr><td><b>Blasen/min</b></td><td id="bpm"></td><td></td></tr>
    <tr><td><b>Blasen/h</b></td><td id="bps"></td><td></td></tr>
  </table><div id="timestamp">Letztes Update: -</div><br>
<center><a href='/config'>Konfiguration</a></center><br>
  <script>
  function formatDate(d) { const pad = n => n < 10 ? '0' + n : n; return pad(d.getDate()) + '.' + pad(d.getMonth()+1) + '.' + d.getFullYear() + ' ' + pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds()); }
    async function updateData() {
      try {
        const response = await fetch('/data.json');
        if (!response.ok) return;
        const data = await response.json();

        const a = data.abstaende;
        const v = data.werte;

        document.getElementById('b5').textContent = a[0].toFixed(1);
        document.getElementById('v5').textContent = '(' + v[0] + ')';
        document.getElementById('b4').textContent = a[1].toFixed(1);
        document.getElementById('v4').textContent = '(' + v[1] + ')';
        document.getElementById('b3').textContent = a[2].toFixed(1);
        document.getElementById('v3').textContent = '(' + v[2] + ')';
        document.getElementById('b2').textContent = a[3].toFixed(1);
        document.getElementById('v2').textContent = '(' + v[3] + ')';
        document.getElementById('b1').textContent = a[4].toFixed(1);
        document.getElementById('v1').textContent = '(' + v[4] + ')';

        document.getElementById('schnitt').textContent = data.schnitt.toFixed(1);
        document.getElementById('bpm').textContent = data.bpm.toFixed(1);
        document.getElementById('bps').textContent = data.bps.toFixed(1);
        document.getElementById('ar').textContent = data.ar.toFixed(0);
        document.getElementById('minimum').textContent = data.minimum.toFixed(0);
        document.getElementById('minimumsw').textContent = data.minimumsw.toFixed(0);
        document.getElementById('timestamp').textContent = 'Letztes Update: ' + formatDate(new Date());
      } catch(e) {
        console.log("Fehler beim Abrufen:", e);
      }
    }

    setInterval(updateData, 2000);
    updateData();
  </script>)rawliteral";


  content = content + "<br><br>";
  content = content + "<h2>Analoge Messwerte</h2>";
  content = content + "<center>Aktueller Messwert: <span id='ar' style='display:inline'></span><br>";
  content = content + (String)sekunden + "s-Schwellwert: <span id='minimumsw' style='display:inline'></span>";
  content = content + "<br>" + (String)sekunden + "s-Minimum: <span id='minimum' style='display:inline'></span></center>";
  server.sendContent( String() + content);
  /*
    Dir dir = LittleFS.openDir("/data");
    server.sendContent( String() + "<br><br>-- LittleFS directory ------------------");
    while (dir.next()) {
      server.sendContent( String() + "<br><b>" + dir.fileName() + "</b>");
      server.sendContent( String() + "  ");

      File f = dir.openFile("r");
      server.sendContent( String(f.size()) + " Bytes");
      server.sendContent( String() + "<br><font color=red>");
      while (f.available()) {
        String line = f.readStringUntil('\n');
        server.sendContent( String() + line + "<br>");
      }
      server.sendContent( String() + "<br></font>");
      f.close();
    }
    server.sendContent( String() + "<br>-- LittleFS -----------------------------");
  */

  server.sendContent( String() + "</body></html>"                    );
  server.client().stop(); // Stop is needed because we sent no content length
}

/*++++++++++++++++++++++++++++++++
   Blasen als JSON bereitstellen
  +++++++++++++++++++++++++++++++*/
void handleDataJson() {
  unsigned long intervall = blas[0] - blas[4];
  float schnitt = (intervall / 4.0) / 1000.0;
  float blasenProMin = 60000.0 / (intervall / 4.0);
  float blasenProStd = 3600000.0 / (intervall / 4.0);

  DynamicJsonDocument doc(256);
  JsonArray abstaende = doc.createNestedArray("abstaende");

  // 5 Einzelabstände
  abstaende.add((blas[3] - blas[4]) / 1000.0);
  abstaende.add((blas[2] - blas[3]) / 1000.0);
  abstaende.add((blas[1] - blas[2]) / 1000.0);
  abstaende.add((blas[0] - blas[1]) / 1000.0);
  abstaende.add((millis() - blas[0]) / 1000.0);

  JsonArray werte = doc.createNestedArray("werte");
  werte.add(blasval[4]);
  werte.add(blasval[3]);
  werte.add(blasval[2]);
  werte.add(blasval[1]);
  werte.add(blasval[0]);

  doc["schnitt"] = schnitt;
  doc["bpm"] = blasenProMin;
  doc["bps"] = blasenProStd;
  doc["ar"] = ar;
  doc["minimumsw"] = getsecMin();
  doc["minimum"] = getsecMin() / faktor;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}
