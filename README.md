# Blasenzaehler
ESP8266-Sketch zum Erfassen von CO2-Blasen in Wein-Gärspunden

-- Zweck --

Dieses Projekt dient dazu, mithilfe eines ESP8266 und einer Reflexlichtschranke die in einem Gärröhrchen eines Weinballons aufsteigenden CO2-Blasen zu erfassen und daraus einen Rückschluss auf die Gäraktivität zu ziehen.
Es besteht auch die Möglichkeit, zu jeder erfassten Blase per MQTT ein publish abzusetzen, um dieses zentral zu speichern.

Weiter steht über den verbauten Webserver eine Möglichkeit zur Verfügung, die aktuellen Messwerte direkt einzusehen.


-- Aufbau --
Benötigt werden:

1x Wemos D1 mini (o.ä.)

1x TCRT5000 Reflexlichtschranke (Modul)


Zum platzsparenden Aufbau bietet sich eine Wäscheklammer aus Kunststoff an.
Es werden in die Klammer in eine Backe zwei Löcher à 3mm gebohrt. An der Lichtschranke werden beide Dioden tiefer gesetzt und die Kunststoffhalterung entfernt, so dass die Dioden in den zuvor gebohrten Löchern bündig anliegen.

Verdrahtung:

TCRT5000-Modul // Wemos D1 mini

VCC - 3v3

GND - GND

AO  - A0

