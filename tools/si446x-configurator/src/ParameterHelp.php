<?php

declare(strict_types=1);

namespace El1\Si446xConfigurator;

use InvalidArgumentException;

final class ParameterHelp
{
	/** @return array<string,string> */
	public static function all(string $language = 'de'): array
	{
		return $language === 'en' ? self::english() : self::german();
	}

	public static function get(string $name, string $language = 'de'): string
	{
		$all = self::all($language);
		if (!isset($all[$name]))
		{
			throw new InvalidArgumentException('Missing parameter help for: ' . $name);
		}
		return $all[$name];
	}

	/** @return array<string,string> */
	private static function german(): array
	{
		return [
			'preset' => 'Füllt mehrere Felder mit zusammenpassenden Startwerten. Das Preset sperrt keine Werte; alle Parameter können danach einzeln verändert werden.',
			'chip' => 'Wählt das Zielmodul bzw. die Si4463-Revision für Metadaten und Plausibilitätsprüfung. Für das HopeRF RFM26W ist Si4463 Rev. C2A vorgesehen.',
			'xo_hz' => 'Frequenz des Quarzes bzw. externen Referenztakts in Hz. Sie wird in POWER_UP eingetragen und für Frequenzsynthesizer, TX-NCO und Deviation-Berechnung verwendet; der Wert muss zur Hardware passen.',
			'clock_source' => 'Bestimmt, ob der Si446x einen Quarz an XTAL oder einen externen TCXO/Takt verwendet. Die Auswahl ändert die POWER_UP-Konfiguration.',
			'frequency_hz' => 'Mittenfrequenz von Kanal 0 in Hz. Daraus erzeugt der Konfigurator FREQ_CONTROL_INTE/FRAC und das passende Synthesizer-Band.',
			'channel_spacing_hz' => 'Frequenzabstand benachbarter Kanäle in Hz. Der Kanalindex aus START_TX/START_RX wird mit diesem Abstand auf die Basisfrequenz aufgeschlagen. 0 deaktiviert den Kanalabstand.',
			'modulation' => 'HF-Modulationsart. OOK schaltet den Träger ein/aus; 2-FSK/GFSK verwendet zwei Frequenzlagen, 4-FSK/GFSK vier. Diese Auswahl berechnet keine komplexen RX-Filterkoeffizienten neu.',
			'mod_source' => 'Quelle der Modulationsdaten: Packet Handler verwendet die internen FIFOs/Paketlogik, Direct Mode übernimmt einen externen Datenstrom, Pseudo verwendet die interne Pseudo-Datenquelle.',
			'tx_bitrate' => 'Nominale TX-Datenrate in bit/s, aus der MODEM_DATA_RATE und der TX-NCO-Takt abgeleitet werden. Bei importierten RX-Konfigurationen muss deren Clock-Recovery zu dieser Rate passen.',
			'deviation_hz' => 'Frequenzhub in Hz für FSK/GFSK, also die Abweichung einer Frequenzlage vom Träger. Für OOK/CW ist der Wert üblicherweise 0 bzw. ohne Bedeutung.',
			'rx_bandwidth_hz' => 'Gewünschte RX-Kanalbandbreite in Hz. Der aktuelle Generator speichert diesen Wert nur als Metadatum; RX-Filter, BCR und AFC müssen aus einer passenden WDS/EZConfigPro-Basis oder Raw-Properties kommen.',
			'manchester' => 'Aktiviert das Manchester-Mapping im MODEM_MAP_CONTROL. Dadurch wird die Nutzdatenabbildung Manchester-codiert; Sender und Empfänger müssen dieselbe Einstellung verwenden.',
			'direct_gpio' => 'Wählt GPIO0..GPIO3 als Datenquelle für Direct-TX. Die Ausgabe demodulierter RX-Daten wird separat über die GPIO-Funktion RX data bzw. RX raw data geroutet.',
			'direct_async' => 'Verwendet asynchronen Direct Mode ohne taktsynchrone Übergabe der externen Daten. Für Rohsignal-/Sniffer-Anwendungen ist dies oft der einfachere Startpunkt.',
			'preamble_tx_length' => 'Direkter 8-Bit-Wert für PREAMBLE_TX_LENGTH. Die genaue Zahl der ausgesendeten Präambelbits hängt zusätzlich von der übrigen PREAMBLE-Konfiguration einer importierten Basis ab.',
			'preamble_rx_threshold' => '7-Bit-Schwellwert des Präambeldetektors. Größere Werte verlangen mehr passende Präambel vor der Erkennung; 0..127 wird direkt in PREAMBLE_CONFIG_STD_1 geschrieben.',
			'sync_word' => 'Syncword als 1 bis 4 Hex-Bytes, z. B. "2D D4". Der Empfänger sucht nach diesem Muster hinter der Präambel; Sender und Empfänger müssen dasselbe Syncword verwenden.',
			'sync_errors' => 'Anzahl der beim Syncword tolerierten Bitfehler (0..7). Ein größerer Wert erhöht die Toleranz, kann aber auch die Wahrscheinlichkeit von Fehldetektionen erhöhen.',
			'field1_length' => 'Länge von Packet Field 1 in Bytes (13 Bit). 0 lässt die effektive Paketlänge über START_TX/START_RX bzw. eine vorhandene Basiskonfiguration bestimmen.',
			'pkt_tx_threshold' => 'Schwellwert der TX-FIFO-Paketlogik in Bytes. Er beeinflusst, wann der Packet Handler den TX-FIFO-Zustand als nachfüllbedürftig meldet.',
			'pkt_rx_threshold' => 'Schwellwert der RX-FIFO-Paketlogik in Bytes. Er beeinflusst, wann der Packet Handler meldet, dass genügend RX-Daten zum Auslesen bereitstehen.',
			'crc' => 'Wählt das vom Si446x Packet Handler verwendete CRC-Polynom. "kein CRC" deaktiviert die CRC-Berechnung; die Gegenstelle muss dasselbe Verfahren verwenden.',
			'crc_seed_all_ones' => 'Initialisiert das CRC-Schieberegister mit Einsen statt Nullen. Diese Einstellung ist Teil des Protokolls und muss auf Sender- und Empfängerseite übereinstimmen.',
			'sync_manchester' => 'Kennzeichnet das Syncword als Manchester-codiert. Diese Option betrifft das Sync-Feld und ist unabhängig vom Manchester-Mapping von Packet Field 1.',
			'field1_crc' => 'Aktiviert CRC-Start, CRC-Berechnung/Übertragung beim Senden und CRC-Prüfung beim Empfangen für Field 1. Hat keine Wirkung, wenn CRC auf "kein CRC" steht.',
			'field1_whiten' => 'Aktiviert Daten-Whitening für Packet Field 1. Whitening reduziert lange Folgen gleicher Bits; Polynom und Seed werden in den separaten Whitening-Feldern festgelegt.',
			'field1_manchester' => 'Aktiviert Manchester-Codierung speziell für Packet Field 1. Dies verändert die Bitdarstellung auf der Luft und muss zur Gegenstelle passen.',
			'field1_pn_start' => 'Startet bzw. setzt den PN-/Whitening-Zustand am Beginn von Field 1 zurück. Relevant, wenn Whitening für dieses Feld verwendet wird.',
			'whitening_poly' => '16-Bit-Whitening-Polynom als Hexwert ohne 0x-Präfix, z. B. 0108. Es wird nur wirksam, wenn Whitening für ein Paketfeld aktiviert ist.',
			'whitening_seed' => '16-Bit-Startwert des Whitening-PN-Generators als Hexwert, z. B. FFFF. Sender und Empfänger müssen denselben Seed verwenden.',
			'pa_power_raw' => 'Roher 7-Bit-Wert PA_PWR_LVL (0..127). Er ist kein dBm-Wert; die tatsächliche Ausgangsleistung hängt von Chip, Versorgung, Matching und Frequenz ab.',
			'drive_strength' => 'Ausgangstreiberstärke der digitalen Si446x-Pins. Niedrigere Treiberstärke kann Störungen und Flankensteilheit reduzieren; höhere ist für größere Lasten geeignet.',
			'gpio0_mode' => self::gpioModeDescription(0, 'de'),
			'gpio1_mode' => self::gpioModeDescription(1, 'de'),
			'gpio2_mode' => self::gpioModeDescription(2, 'de'),
			'gpio3_mode' => self::gpioModeDescription(3, 'de'),
			'gpio0_pull' => self::gpioPullDescription(0, 'de'),
			'gpio1_pull' => self::gpioPullDescription(1, 'de'),
			'gpio2_pull' => self::gpioPullDescription(2, 'de'),
			'gpio3_pull' => self::gpioPullDescription(3, 'de'),
			'nirq_mode' => 'Legt die Funktion des NIRQ-Pins über GPIO_PIN_CFG fest. Für die normale Interrupt-Anbindung sollte die Konfiguration zum Treiber und zur Verdrahtung passen; eine andere Funktion kann NIRQ als Interruptquelle unbrauchbar machen.',
			'sdo_mode' => 'Legt die alternative Funktion des SDO-Pins über GPIO_PIN_CFG fest. Bei regulärer SPI-Nutzung sollte diese Einstellung zur Hardwareverdrahtung passen.',
			'int_enable' => 'Hexadezimale 8-Bit-Maske INT_CTL_ENABLE. Sie schaltet die übergeordneten Interruptgruppen des Si446x frei; einzelne Quellen werden zusätzlich über PH/MODEM/CHIP_INT_ENABLE maskiert.',
			'ph_int_enable' => 'Hexadezimale 8-Bit-Maske der Packet-Handler-Interruptquellen, z. B. Paket gesendet/empfangen oder FIFO-bezogene Ereignisse. Nur aktiv, wenn die entsprechende Hauptgruppe freigeschaltet ist.',
			'modem_int_enable' => 'Hexadezimale 8-Bit-Maske der Modem-Interruptquellen, z. B. Präambel-/Sync-/RSSI-bezogene Ereignisse. Nur aktiv, wenn die MODEM-Hauptgruppe freigeschaltet ist.',
			'chip_int_enable' => 'Hexadezimale 8-Bit-Maske der Chip-Interruptquellen, z. B. Chip Ready, Command Error oder FIFO Under/Overflow. Nur aktiv, wenn die CHIP-Hauptgruppe freigeschaltet ist.',
			'base_config_file' => 'Importiert eine vorhandene WDS/EZConfigPro-, el1-, JSON- oder Hex-Konfiguration als Basis. Die im Formular gewählten Werte werden danach als Overrides angewendet; RX-DSP-Werte der Basis bleiben erhalten, sofern sie nicht überschrieben werden.',
			'base_config' => 'Textuelle Basiskonfiguration zum Einfügen. Unterstützt WDS RADIO_CONFIGURATION_DATA_ARRAY, JSON mit commands oder einen Hex-Bytestream. Formularwerte werden anschließend als Overrides angewendet.',
			'raw_properties' => 'Expertenmodus für direkte SET_PROPERTY-Overrides. Format pro Zeile: group:start: bytes..., z. B. 20:1C: 01 02. Der Generator splittet lange Werte automatisch in zulässige Befehle.',
			'raw_commands' => 'Expertenmodus für vollständige zusätzliche Si446x-Kommandos als Hex-Bytes, ein Command pro Zeile und maximal 16 Bytes. Diese Befehle werden am Ende des Streams angehängt.',
			'filename' => 'Basisname der heruntergeladenen Datei ohne notwendige Erweiterung. Unsichere Zeichen werden beim Download durch Bindestriche ersetzt.',
			'download_format' => 'Ausgabeformat: .cfg ist der binäre length+command-Stream für el1/WDS, .json enthält Commands und Metadaten, .hpp erzeugt ein direkt einbindbares C++-Bytearray.',
		];
	}

	/** @return array<string,string> */
	private static function english(): array
	{
		return [
			'preset' => 'Fills several fields with a coherent set of starting values. A preset does not lock any values; every parameter can still be changed individually.',
			'chip' => 'Selects the target module or Si4463 revision for metadata and plausibility checks. HopeRF RFM26W modules use the Si4463 Rev. C2A target here.',
			'xo_hz' => 'Frequency of the crystal or external reference clock in Hz. It is written to POWER_UP and used to calculate the frequency synthesizer, TX NCO and deviation; it must match the hardware.',
			'clock_source' => 'Selects whether the Si446x uses a crystal on XTAL or an external TCXO/clock. This changes the POWER_UP configuration.',
			'frequency_hz' => 'Center frequency of channel 0 in Hz. The configurator derives FREQ_CONTROL_INTE/FRAC and the appropriate synthesizer band from it.',
			'channel_spacing_hz' => 'Frequency spacing between adjacent channels in Hz. The channel index passed to START_TX/START_RX is multiplied by this value and added to the base frequency. 0 disables channel spacing.',
			'modulation' => 'RF modulation type. OOK switches the carrier on and off; 2-FSK/GFSK uses two frequency states and 4-FSK/GFSK uses four. This selection does not recalculate complex RX filter coefficients.',
			'mod_source' => 'Source of modulation data: Packet Handler uses the internal FIFOs/packet logic, Direct Mode accepts an external data stream, and Pseudo uses the internal pseudo-data source.',
			'tx_bitrate' => 'Nominal TX data rate in bit/s, used to derive MODEM_DATA_RATE and the TX NCO clock. For imported RX configurations, their clock recovery settings must match this rate.',
			'deviation_hz' => 'Frequency deviation in Hz for FSK/GFSK, i.e. the offset of a frequency state from the carrier. For OOK/CW this is normally 0 or irrelevant.',
			'rx_bandwidth_hz' => 'Desired RX channel bandwidth in Hz. The current generator stores this value as metadata only; RX filters, BCR and AFC must come from a matching WDS/EZConfigPro base or raw properties.',
			'manchester' => 'Enables Manchester mapping in MODEM_MAP_CONTROL. This changes the payload bit mapping; transmitter and receiver must use the same setting.',
			'direct_gpio' => 'Selects GPIO0..GPIO3 as the data source for Direct TX. Demodulated RX data is routed separately using the GPIO functions RX data or RX raw data.',
			'direct_async' => 'Uses asynchronous Direct Mode without clock-synchronous handoff of external data. This is often the simpler starting point for raw-signal/sniffer applications.',
			'preamble_tx_length' => 'Direct 8-bit value for PREAMBLE_TX_LENGTH. The exact number of transmitted preamble bits also depends on the remaining PREAMBLE configuration of an imported base.',
			'preamble_rx_threshold' => '7-bit threshold of the preamble detector. Higher values require more matching preamble before detection; 0..127 is written directly to PREAMBLE_CONFIG_STD_1.',
			'sync_word' => 'Sync word as 1 to 4 hexadecimal bytes, e.g. "2D D4". The receiver searches for this pattern after the preamble; transmitter and receiver must use the same sync word.',
			'sync_errors' => 'Number of tolerated bit errors in the sync word (0..7). Higher values increase tolerance but can also increase false detections.',
			'field1_length' => 'Length of Packet Field 1 in bytes (13 bit). A value of 0 lets START_TX/START_RX or an existing base configuration determine the effective packet length.',
			'pkt_tx_threshold' => 'TX FIFO packet-logic threshold in bytes. It affects when the Packet Handler reports that the TX FIFO needs refilling.',
			'pkt_rx_threshold' => 'RX FIFO packet-logic threshold in bytes. It affects when the Packet Handler reports that enough RX data is ready to read.',
			'crc' => 'Selects the CRC polynomial used by the Si446x Packet Handler. "no CRC" disables CRC calculation; the peer must use the same method.',
			'crc_seed_all_ones' => 'Initializes the CRC shift register with ones instead of zeros. This is part of the protocol and must match on transmitter and receiver.',
			'sync_manchester' => 'Marks the sync word as Manchester encoded. This option applies to the sync field and is independent of Manchester mapping for Packet Field 1.',
			'field1_crc' => 'Enables CRC start, CRC calculation/transmission on TX and CRC checking on RX for Field 1. It has no effect when CRC is set to "no CRC".',
			'field1_whiten' => 'Enables data whitening for Packet Field 1. Whitening reduces long runs of equal bits; polynomial and seed are configured separately.',
			'field1_manchester' => 'Enables Manchester encoding specifically for Packet Field 1. This changes the over-the-air bit representation and must match the peer.',
			'field1_pn_start' => 'Starts or resets the PN/whitening state at the beginning of Field 1. Relevant when whitening is enabled for this field.',
			'whitening_poly' => '16-bit whitening polynomial as a hexadecimal value without a 0x prefix, e.g. 0108. It only has an effect when whitening is enabled for a packet field.',
			'whitening_seed' => '16-bit initial value of the whitening PN generator as a hexadecimal value, e.g. FFFF. Transmitter and receiver must use the same seed.',
			'pa_power_raw' => 'Raw 7-bit PA_PWR_LVL value (0..127). This is not a dBm value; actual output power depends on the chip, supply, matching network and frequency.',
			'drive_strength' => 'Output drive strength of the digital Si446x pins. Lower drive strength can reduce interference and edge rate; higher settings are useful for larger loads.',
			'gpio0_mode' => self::gpioModeDescription(0, 'en'),
			'gpio1_mode' => self::gpioModeDescription(1, 'en'),
			'gpio2_mode' => self::gpioModeDescription(2, 'en'),
			'gpio3_mode' => self::gpioModeDescription(3, 'en'),
			'gpio0_pull' => self::gpioPullDescription(0, 'en'),
			'gpio1_pull' => self::gpioPullDescription(1, 'en'),
			'gpio2_pull' => self::gpioPullDescription(2, 'en'),
			'gpio3_pull' => self::gpioPullDescription(3, 'en'),
			'nirq_mode' => 'Selects the NIRQ pin function through GPIO_PIN_CFG. For normal interrupt use this must match the driver and wiring; another function can make NIRQ unusable as an interrupt source.',
			'sdo_mode' => 'Selects the alternate SDO pin function through GPIO_PIN_CFG. For normal SPI operation this setting must match the hardware wiring.',
			'int_enable' => 'Hexadecimal 8-bit INT_CTL_ENABLE mask. It enables the top-level Si446x interrupt groups; individual sources are additionally masked by PH/MODEM/CHIP_INT_ENABLE.',
			'ph_int_enable' => 'Hexadecimal 8-bit mask of Packet Handler interrupt sources, such as packet sent/received or FIFO events. Only effective when the corresponding top-level group is enabled.',
			'modem_int_enable' => 'Hexadecimal 8-bit mask of modem interrupt sources, such as preamble, sync and RSSI events. Only effective when the MODEM top-level group is enabled.',
			'chip_int_enable' => 'Hexadecimal 8-bit mask of chip interrupt sources, such as Chip Ready, Command Error or FIFO under/overflow. Only effective when the CHIP top-level group is enabled.',
			'base_config_file' => 'Imports an existing WDS/EZConfigPro, el1, JSON or hexadecimal configuration as the base. Values selected in the form are applied afterwards as overrides; RX DSP values from the base are preserved unless explicitly overwritten.',
			'base_config' => 'Text base configuration to paste. Supports WDS RADIO_CONFIGURATION_DATA_ARRAY, JSON containing commands, or a hexadecimal byte stream. Form values are applied afterwards as overrides.',
			'raw_properties' => 'Expert mode for direct SET_PROPERTY overrides. One line uses group:start: bytes..., e.g. 20:1C: 01 02. Long values are automatically split into valid commands.',
			'raw_commands' => 'Expert mode for complete additional Si446x commands as hexadecimal bytes, one command per line and at most 16 bytes. Commands are appended to the end of the stream.',
			'filename' => 'Base name of the downloaded file without a required extension. Unsafe characters are replaced with hyphens when downloading.',
			'download_format' => 'Output format: .cfg is the binary length+command stream for el1/WDS, .json contains commands and metadata, and .hpp creates a directly includable C++ byte array.',
		];
	}

	private static function gpioModeDescription(int $gpio, string $language): string
	{
		if ($language === 'en')
		{
			return sprintf('Selects the internal Si446x function routed to GPIO%d. For Direct RX, "RX data" and "RX raw data" are especially useful; the electrical pull-up option is configured separately.', $gpio);
		}
		return sprintf('Wählt die interne Si446x-Funktion, die auf GPIO%d geroutet wird. Für Direct-RX sind insbesondere "RX data" und "RX raw data" interessant; die elektrische Pull-up-Option wird separat gesetzt.', $gpio);
	}

	private static function gpioPullDescription(int $gpio, string $language): string
	{
		if ($language === 'en')
		{
			return sprintf('Enables the internal pull-up for GPIO%d in addition to the selected GPIO function. Use only when the electrical wiring and intended signal direction are compatible with it.', $gpio);
		}
		return sprintf('Aktiviert den internen Pull-up für GPIO%d zusätzlich zur gewählten GPIO-Funktion. Nur verwenden, wenn die elektrische Beschaltung und gewünschte Signalrichtung dazu passen.', $gpio);
	}
}
