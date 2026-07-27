# el1 Si446x / RFM26W Configurator

Frameworkfreie PHP-8-Webanwendung zum Erzeugen von Si4463/RFM26W-Konfigurationsstreams, die direkt von `el1::dev::spi::si446x::TConfiguration` geladen werden können.

## Start mit PHP Built-in Server

```sh
php -S 127.0.0.1:8080 -t tools/si446x-configurator
```

Danach `http://127.0.0.1:8080/` öffnen.

## Apache2

Es werden weder Rewrite-Regeln noch Framework-Routing benötigt. Das Verzeichnis kann direkt als DocumentRoot oder Alias veröffentlicht werden, sofern PHP 8+ für `.php` aktiviert ist. Beispiel:

```apache
Alias /si446x-configurator /path/to/el1/tools/si446x-configurator
<Directory /path/to/el1/tools/si446x-configurator>
	Require all granted
	DirectoryIndex index.php
</Directory>
```

## Sprache / i18n

Die Oberfläche ist vollständig auf Deutsch und Englisch verfügbar. Ohne explizite Auswahl wird die Sprache aus dem HTTP-Header `Accept-Language` des Browsers ermittelt. Unterstützt werden `de` und `en`; nicht unterstützte Browser-Sprachen fallen auf Englisch zurück.

Oben auf der Seite kann die Sprache jederzeit manuell umgestellt werden. Die Auswahl wird in einem `si446x_lang`-Cookie gespeichert und hat Vorrang vor `Accept-Language`. Beim Umschalten per Formular bleiben die bereits eingegebenen Konfigurationswerte erhalten.

## Parameter-Hilfe

Jeder einstellbare Parameter besitzt einen sprachabhängigen Hover-Tooltip. Die Erklärung beschreibt Bedeutung, Wirkung und wichtige Einschränkungen des jeweiligen Si446x-Parameters.

## Ausgabeformate

- `.cfg`: binärer, längenpräfixierter Si446x-Command-Stream mit abschließendem Nullbyte. Das ist das native Format von `TConfiguration`.
- `.json`: Commands plus berechnete Metadaten und Warnungen.
- `.hpp`: direkt einbindbares C++-Bytearray.

## WDS/EZConfigPro-Basis

Der Generator berechnet und überschreibt die Parameter, deren Abbildung auf Si4463-Properties eindeutig ist, insbesondere:

- `POWER_UP`: XO-Frequenz und XTAL/TCXO
- RF-Synthesizer: Center Frequency und Channel Step
- Modulation und Direct-/Packet-Quelle
- TX-NCO und TX-Datenrate
- TX Frequency Deviation
- Manchester-Mapping
- Preamble, Sync, CRC/Whitening sowie einfache Field-1-Länge/-Verarbeitung und FIFO-Schwellen
- PA power level
- GPIO-Pin-Konfiguration
- Interrupt-Masken

Die RX-Modemkette (`MODEM_CHFLT`, BCR, AFC und weitere voneinander abhängige DSP-Koeffizienten) wird nicht approximiert. Dafür kann eine von WDS/EZConfigPro erzeugte Basiskonfiguration importiert werden. Die Webapp bewahrt deren Commands und hängt die gewählten Overrides an. Alternativ lassen sich die entsprechenden Property-Gruppen im Expertenmodus direkt setzen.

Akzeptierte Basisformate:

- WDS-C-Header mit `RADIO_CONFIGURATION_DATA_ARRAY`
- binäre `.cfg`-Datei
- Hex-Bytestream
- JSON mit `commands`

## Raw Properties

Syntax je Zeile:

```text
GROUP:START: BYTE BYTE BYTE ...
```

Beispiel:

```text
21:00: 11 22 33 44 55 66 77 88
20:1C: 01 02 03
```

Lange Property-Blöcke werden automatisch auf die maximal 12 Property-Bytes pro `SET_PROPERTY`-Command aufgeteilt.

## Tests

```sh
php tools/si446x-configurator/tests/run.php
```
