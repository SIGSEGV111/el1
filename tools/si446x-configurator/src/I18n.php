<?php

declare(strict_types=1);

namespace El1\Si446xConfigurator;

final class I18n
{
	public const DEFAULT_LANGUAGE = 'en';

	/** @var array<string,string> */
	private const LANGUAGE_NAMES = [
		'de' => 'Deutsch',
		'en' => 'English',
	];

	/** @var array<string,string> */
	private const ERROR_KEYS = [
		'Command length exceeds the Si446x 16-byte command buffer.' => 'stream.command_too_long',
		'Configuration stream ends inside a command.' => 'stream.ends_inside_command',
		'Configuration stream has no terminating zero byte.' => 'stream.missing_terminator',
		'Unexpected data after the terminating zero byte.' => 'stream.trailing_data',
		'Base configuration must be a WDS C array, JSON command list, or two-digit hex byte stream.' => 'stream.invalid_base_format',
		'Si446x commands must contain between 1 and 16 bytes.' => 'stream.invalid_command_length',
		'Byte value outside 0..255.' => 'stream.byte_range',
	];

	/** @var array<string,array<string,string>> */
	private const STRINGS = [
		'de' => [
			'language' => 'Sprache',
			'apply_language' => 'Übernehmen',
			'intro' => 'Erzeugt direkt ladbare Si4463-Command-Streams für el1::dev::spi::si446x::TConfiguration.',
			'preset' => 'Preset',
			'preset.keep' => '– Werte beibehalten –',
			'preset.greensens_ook' => 'Greensens Sniffer: 433,92 MHz OOK Direct RX',
			'preset.greensens_fsk' => 'Greensens Sniffer: 433,92 MHz 2-FSK Direct RX',
			'preset.packet_433' => 'Packet Radio: 433,92 MHz / 10 kbit/s 2-GFSK',
			'preset.packet_868' => 'Packet Radio: 868,30 MHz / 38,4 kbit/s 2-GFSK',
			'error' => 'Fehler:',
			'warnings' => 'Hinweise zur erzeugten Konfiguration',
			'section.chip_clock' => 'Chip & Takt',
			'chip_module' => 'Chip / Modul',
			'xo_frequency' => 'XO-Frequenz [Hz]',
			'clock_source' => 'Taktquelle',
			'clock.xtal' => 'Quarz (XTAL)',
			'clock.tcxo' => 'TCXO / externer Takt',
			'section.rf_modem' => 'RF & Modem',
			'carrier_frequency' => 'Trägerfrequenz [Hz]',
			'channel_spacing' => 'Kanalabstand [Hz]',
			'modulation' => 'Modulation',
			'modulation_source' => 'Modulationsquelle',
			'mod_source.packet' => 'Packet Handler',
			'mod_source.direct' => 'Direct Mode',
			'mod_source.pseudo' => 'Pseudo',
			'tx_bitrate' => 'TX-Bitrate [bit/s]',
			'frequency_deviation' => 'Frequenzhub [Hz]',
			'rx_bandwidth' => 'RX-Bandbreite [Hz]',
			'metadata' => '(Metadatum)',
			'rx_bandwidth_placeholder' => 'aus WDS-Basis übernehmen',
			'manchester_mapping' => 'Manchester-Mapping',
			'direct_gpio' => 'Direct-Mode Daten-GPIO',
			'direct_async' => 'Asynchroner Direct Mode',
			'rf_modem_help' => 'Frequenz-Synthesizer, TX-NCO, TX-Datenrate und TX-Deviation werden berechnet. Die komplexen RX-Filter-, BCR- und AFC-Koeffizienten werden absichtlich nicht angenähert; dafür eine passende WDS/EZConfigPro-Basis importieren oder Raw-Properties setzen.',
			'section.packet' => 'Packet Handler',
			'preamble_tx_length' => 'Präambel-TX-Länge [raw]',
			'preamble_rx_threshold' => 'Präambel-RX-Schwellwert',
			'sync_word' => 'Syncword [1–4 Bytes]',
			'sync_errors' => 'Erlaubte Sync-Bitfehler',
			'field1_length' => 'Field-1-Länge [Bytes]',
			'field1_length_help' => '0 = Länge aus START_TX/START_RX verwenden',
			'tx_fifo_threshold' => 'TX-FIFO-Schwellwert [Bytes]',
			'rx_fifo_threshold' => 'RX-FIFO-Schwellwert [Bytes]',
			'crc.none' => 'kein CRC',
			'crc_seed_all_ones' => 'CRC seed = all ones',
			'sync_manchester' => 'Manchester-codiertes Syncword',
			'field1_crc' => 'CRC über Field 1 berechnen/senden/prüfen',
			'field1_whiten' => 'Whitening für Field 1',
			'field1_manchester' => 'Manchester für Field 1',
			'field1_pn_start' => 'PN/Whitening am Field-Start zurücksetzen',
			'whitening_polynomial' => 'Whitening-Polynom [hex]',
			'whitening_seed' => 'Whitening-Seed [hex]',
			'packet_help' => 'Field 1 kann direkt konfiguriert werden. Variable-Length-Layouts und getrennte Field-2..5/RX-Field-Strukturen bleiben aus einer importierten Basis erhalten oder können unten als Raw-Properties überschrieben werden.',
			'section.pa_gpio_interrupts' => 'PA, GPIO & Interrupts',
			'pa_power' => 'PA-Leistungswert [0..127]',
			'gpio_drive_strength' => 'GPIO-Treiberstärke',
			'drive.high' => 'Hoch',
			'drive.medium_high' => 'Mittelhoch',
			'drive.medium_low' => 'Mittelniedrig',
			'drive.low' => 'Niedrig',
			'gpio_function' => 'GPIO{gpio}-Funktion',
			'pull_up' => 'Pull-up',
			'nirq_function' => 'NIRQ-Funktion',
			'sdo_function' => 'SDO-Funktion',
			'interrupt_masks' => 'Interrupt-Masken',
			'hex' => '(hex)',
			'section.base_config' => 'WDS/EZConfigPro-Basiskonfiguration',
			'base_config_help' => 'Akzeptiert WDS-RADIO_CONFIGURATION_DATA_ARRAY, den el1-Binärstream als Upload, JSON mit commands oder einen Hex-Bytestream. Die oben gewählten Einstellungen werden anschließend als Overrides angehängt; POWER_UP wird in-place auf XO/TCXO angepasst.',
			'import_file' => 'Datei importieren',
			'paste_base' => 'Oder Basis einfügen',
			'advanced' => 'Expertenmodus: Raw Properties & Commands',
			'property_overrides' => 'Property-Overrides',
			'raw_properties_help' => 'pro Zeile. Werte werden automatisch in SET_PROPERTY-Blöcke ≤12 Bytes gesplittet.',
			'additional_commands' => 'Zusätzliche Commands',
			'raw_commands_help' => 'Ein vollständiger Si446x-Command pro Zeile, maximal 16 Bytes.',
			'filename' => 'Dateiname',
			'format' => 'Format',
			'format.cfg' => 'el1/WDS Command Stream (.cfg)',
			'format.json' => 'JSON + Metadaten (.json)',
			'format.hpp' => 'C++ Header (.hpp)',
			'update_preview' => 'Preview aktualisieren',
			'download' => 'Download',
			'preview' => 'Vorschau',
			'commands' => 'Befehle',
			'bytes' => 'Bytes',
			'length' => 'Länge',
			'command' => 'Befehl',
			'command_stream' => 'Command Stream',
			'footer' => 'Frameworkfrei · keine Datenbank · keine externen Assets',
			'upload.too_large' => 'Die Basiskonfiguration darf höchstens 256 KiB groß sein.',
			'upload.read_failed' => 'Die hochgeladene Basiskonfiguration konnte nicht gelesen werden.',
			'upload.failed' => 'Fehler beim Upload der Basiskonfiguration.',
			'generator.invalid_clock_source' => 'Ungültige Taktquelle.',
			'generator.invalid_modulation' => 'Ungültige Modulation.',
			'generator.invalid_modulation_source' => 'Ungültige Modulationsquelle.',
			'generator.bitrate_unrepresentable' => 'Die TX-Bitrate kann mit der gewählten TX-NCO-Konfiguration nicht dargestellt werden.',
			'generator.deviation_unrepresentable' => 'Der Frequenzhub kann für diese XO-Frequenz nicht dargestellt werden.',
			'generator.invalid_crc' => 'Ungültiges CRC-Polynom.',
			'generator.invalid_gpio' => 'Ungültiger GPIO-Modus.',
			'generator.invalid_nirq_sdo' => 'Ungültiger NIRQ/SDO-Modus.',
			'generator.invalid_raw_property' => 'Ungültiger Raw-Property-Override in Zeile {line}.',
			'generator.frequency_band' => 'Die Frequenz liegt außerhalb des unterstützten Synthesizer-Bands.',
			'generator.frequency_range' => 'Die Werte des Frequenzsynthesizers liegen außerhalb des darstellbaren Bereichs.',
			'generator.channel_step_range' => 'Der Kanalabstand überschreitet in diesem Band den 16-Bit-Bereich von CHANNEL_STEP_SIZE.',
			'generator.must_integer' => '{label} muss eine Ganzzahl sein.',
			'generator.must_between' => '{label} muss zwischen {minimum} und {maximum} liegen.',
			'generator.must_hex' => '{label} muss hexadezimal sein.',
			'generator.outside_range' => '{label} liegt außerhalb des erlaubten Bereichs.',
			'generator.byte_count' => '{label} muss {minimum}..{maximum} Bytes enthalten.',
			'generator.invalid_byte' => '{label} enthält ein ungültiges Byte.',
			'generator.warning_rx_metadata' => 'Die RX-Bandbreite wird nur als Metadatum gespeichert. Si446x-RX-Filter-, Clock-Recovery- und AFC-Koeffizienten müssen aus einer passenden WDS/EZConfigPro-Basiskonfiguration oder expliziten Raw-MODEM/MODEM_CHFLT-Overrides stammen.',
			'generator.warning_no_base' => 'Es wurde keine WDS/EZConfigPro-Basiskonfiguration angegeben. TX-/Frequenz-/Packet-/GPIO-Einstellungen werden erzeugt, aber die vollständige RX-Modem-/Filterkette verbleibt auf Reset-/Defaultwerten oder expliziten Raw-Overrides.',
			'generator.warning_base_mismatch' => 'Eine Änderung von Frequenz, Bitrate oder Frequenzhub berechnet WDS-generierte RX-Filter-/Clock-Recovery-/AFC-Koeffizienten nicht neu. Prüfe, dass die importierte Basiskonfiguration zu den resultierenden Modemparametern passt.',
			'stream.command_too_long' => 'Die Command-Länge überschreitet den 16-Byte-Command-Puffer des Si446x.',
			'stream.ends_inside_command' => 'Der Konfigurationsstream endet innerhalb eines Commands.',
			'stream.missing_terminator' => 'Der Konfigurationsstream enthält kein abschließendes Nullbyte.',
			'stream.trailing_data' => 'Nach dem abschließenden Nullbyte befinden sich unerwartete Daten.',
			'stream.invalid_base_format' => 'Die Basiskonfiguration muss ein WDS-C-Array, eine JSON-Command-Liste oder ein Hex-Bytestream mit zweistelligen Bytes sein.',
			'stream.invalid_command_length' => 'Si446x-Commands müssen zwischen 1 und 16 Bytes enthalten.',
			'stream.byte_range' => 'Ein Bytewert liegt außerhalb des Bereichs 0..255.',
			'label.rx_bandwidth' => 'RX-Bandbreite',
			'label.sync_word' => 'Syncword',
			'label.whitening_polynomial' => 'Whitening-Polynom',
			'label.whitening_seed' => 'Whitening-Seed',
			'label.raw_property_line' => 'Raw-Property-Zeile {line}',
			'label.raw_command_line' => 'Raw-Command-Zeile {line}',
		],
		'en' => [
			'language' => 'Language',
			'apply_language' => 'Apply',
			'intro' => 'Generates directly loadable Si4463 command streams for el1::dev::spi::si446x::TConfiguration.',
			'preset' => 'Preset',
			'preset.keep' => '– Keep current values –',
			'preset.greensens_ook' => 'Greensens sniffer: 433.92 MHz OOK Direct RX',
			'preset.greensens_fsk' => 'Greensens sniffer: 433.92 MHz 2-FSK Direct RX',
			'preset.packet_433' => 'Packet radio: 433.92 MHz / 10 kbit/s 2-GFSK',
			'preset.packet_868' => 'Packet radio: 868.30 MHz / 38.4 kbit/s 2-GFSK',
			'error' => 'Error:',
			'warnings' => 'Notes about the generated configuration',
			'section.chip_clock' => 'Chip & Clock',
			'chip_module' => 'Chip / Module',
			'xo_frequency' => 'XO frequency [Hz]',
			'clock_source' => 'Clock source',
			'clock.xtal' => 'Crystal (XTAL)',
			'clock.tcxo' => 'TCXO / external clock',
			'section.rf_modem' => 'RF & Modem',
			'carrier_frequency' => 'Carrier frequency [Hz]',
			'channel_spacing' => 'Channel spacing [Hz]',
			'modulation' => 'Modulation',
			'modulation_source' => 'Modulation source',
			'mod_source.packet' => 'Packet Handler',
			'mod_source.direct' => 'Direct Mode',
			'mod_source.pseudo' => 'Pseudo',
			'tx_bitrate' => 'TX bitrate [bit/s]',
			'frequency_deviation' => 'Frequency deviation [Hz]',
			'rx_bandwidth' => 'RX bandwidth [Hz]',
			'metadata' => '(metadata only)',
			'rx_bandwidth_placeholder' => 'keep from WDS base',
			'manchester_mapping' => 'Manchester mapping',
			'direct_gpio' => 'Direct-mode data GPIO',
			'direct_async' => 'Asynchronous Direct Mode',
			'rf_modem_help' => 'The frequency synthesizer, TX NCO, TX data rate and TX deviation are calculated. Complex RX filter, BCR and AFC coefficients are intentionally not approximated; import a matching WDS/EZConfigPro base or set raw properties instead.',
			'section.packet' => 'Packet Handler',
			'preamble_tx_length' => 'Preamble TX length [raw]',
			'preamble_rx_threshold' => 'Preamble RX threshold',
			'sync_word' => 'Sync word [1–4 bytes]',
			'sync_errors' => 'Allowed sync bit errors',
			'field1_length' => 'Field 1 length [bytes]',
			'field1_length_help' => '0 = use length from START_TX/START_RX',
			'tx_fifo_threshold' => 'TX FIFO threshold [bytes]',
			'rx_fifo_threshold' => 'RX FIFO threshold [bytes]',
			'crc.none' => 'no CRC',
			'crc_seed_all_ones' => 'CRC seed = all ones',
			'sync_manchester' => 'Manchester-encoded sync word',
			'field1_crc' => 'Calculate/send/check CRC for Field 1',
			'field1_whiten' => 'Whitening for Field 1',
			'field1_manchester' => 'Manchester for Field 1',
			'field1_pn_start' => 'Reset PN/whitening at field start',
			'whitening_polynomial' => 'Whitening polynomial [hex]',
			'whitening_seed' => 'Whitening seed [hex]',
			'packet_help' => 'Field 1 can be configured directly. Variable-length layouts and separate Field-2..5/RX-field structures are preserved from an imported base or can be overridden below with raw properties.',
			'section.pa_gpio_interrupts' => 'PA, GPIO & Interrupts',
			'pa_power' => 'PA power level [0..127]',
			'gpio_drive_strength' => 'GPIO drive strength',
			'drive.high' => 'High',
			'drive.medium_high' => 'Medium high',
			'drive.medium_low' => 'Medium low',
			'drive.low' => 'Low',
			'gpio_function' => 'GPIO{gpio} function',
			'pull_up' => 'Pull-up',
			'nirq_function' => 'NIRQ function',
			'sdo_function' => 'SDO function',
			'interrupt_masks' => 'Interrupt masks',
			'hex' => '(hex)',
			'section.base_config' => 'WDS/EZConfigPro base configuration',
			'base_config_help' => 'Accepts a WDS RADIO_CONFIGURATION_DATA_ARRAY, the el1 binary stream as an upload, JSON containing commands, or a hexadecimal byte stream. The settings above are appended as overrides; POWER_UP is adjusted in place for XO/TCXO.',
			'import_file' => 'Import file',
			'paste_base' => 'Or paste base configuration',
			'advanced' => 'Expert mode: Raw Properties & Commands',
			'property_overrides' => 'Property overrides',
			'raw_properties_help' => 'one per line. Values are automatically split into SET_PROPERTY blocks of at most 12 bytes.',
			'additional_commands' => 'Additional commands',
			'raw_commands_help' => 'One complete Si446x command per line, at most 16 bytes.',
			'filename' => 'File name',
			'format' => 'Format',
			'format.cfg' => 'el1/WDS command stream (.cfg)',
			'format.json' => 'JSON + metadata (.json)',
			'format.hpp' => 'C++ header (.hpp)',
			'update_preview' => 'Update preview',
			'download' => 'Download',
			'preview' => 'Preview',
			'commands' => 'Commands',
			'bytes' => 'Bytes',
			'length' => 'Length',
			'command' => 'Command',
			'command_stream' => 'Command Stream',
			'footer' => 'Framework-free · no database · no external assets',
			'upload.too_large' => 'The base configuration may not exceed 256 KiB.',
			'upload.read_failed' => 'The uploaded base configuration could not be read.',
			'upload.failed' => 'Failed to upload the base configuration.',
			'generator.invalid_clock_source' => 'Invalid clock source.',
			'generator.invalid_modulation' => 'Invalid modulation.',
			'generator.invalid_modulation_source' => 'Invalid modulation source.',
			'generator.bitrate_unrepresentable' => 'TX bitrate cannot be represented by the selected TX NCO configuration.',
			'generator.deviation_unrepresentable' => 'Frequency deviation cannot be represented for this XO frequency.',
			'generator.invalid_crc' => 'Invalid CRC polynomial.',
			'generator.invalid_gpio' => 'Invalid GPIO mode.',
			'generator.invalid_nirq_sdo' => 'Invalid NIRQ/SDO mode.',
			'generator.invalid_raw_property' => 'Invalid raw property override on line {line}.',
			'generator.frequency_band' => 'Frequency is outside the supported synthesizer band.',
			'generator.frequency_range' => 'Frequency synthesizer values are outside their representable range.',
			'generator.channel_step_range' => 'Channel spacing exceeds the 16-bit CHANNEL_STEP_SIZE range in this band.',
			'generator.must_integer' => '{label} must be an integer.',
			'generator.must_between' => '{label} must be between {minimum} and {maximum}.',
			'generator.must_hex' => '{label} must be hexadecimal.',
			'generator.outside_range' => '{label} is outside its allowed range.',
			'generator.byte_count' => '{label} must contain {minimum}..{maximum} bytes.',
			'generator.invalid_byte' => '{label} contains an invalid byte.',
			'generator.warning_rx_metadata' => 'RX bandwidth is recorded as metadata only. Si446x RX filter, clock-recovery and AFC coefficients must come from a matching WDS/EZConfigPro base configuration or explicit raw MODEM/MODEM_CHFLT overrides.',
			'generator.warning_no_base' => 'No WDS/EZConfigPro base configuration was supplied. TX/frequency/packet/GPIO settings are emitted, but the complete RX modem/filter chain remains at reset/default or explicit raw override values.',
			'generator.warning_base_mismatch' => 'Changing frequency, bitrate or deviation does not recalculate WDS-generated RX filter/clock-recovery/AFC coefficients. Verify that the imported base configuration matches the resulting modem parameters.',
			'stream.command_too_long' => 'Command length exceeds the Si446x 16-byte command buffer.',
			'stream.ends_inside_command' => 'Configuration stream ends inside a command.',
			'stream.missing_terminator' => 'Configuration stream has no terminating zero byte.',
			'stream.trailing_data' => 'Unexpected data follows the terminating zero byte.',
			'stream.invalid_base_format' => 'Base configuration must be a WDS C array, JSON command list, or two-digit hexadecimal byte stream.',
			'stream.invalid_command_length' => 'Si446x commands must contain between 1 and 16 bytes.',
			'stream.byte_range' => 'A byte value is outside the range 0..255.',
			'label.rx_bandwidth' => 'RX bandwidth',
			'label.sync_word' => 'sync word',
			'label.whitening_polynomial' => 'whitening polynomial',
			'label.whitening_seed' => 'whitening seed',
			'label.raw_property_line' => 'raw property line {line}',
			'label.raw_command_line' => 'raw command line {line}',
		],
	];

	public function __construct(public readonly string $language)
	{
		if (!isset(self::LANGUAGE_NAMES[$language]))
		{
			throw new \InvalidArgumentException('Unsupported language: ' . $language);
		}
	}

	/** @return array<string,string> */
	public static function languages(): array
	{
		return self::LANGUAGE_NAMES;
	}

	public static function isSupported(?string $language): bool
	{
		return $language !== null && isset(self::LANGUAGE_NAMES[strtolower($language)]);
	}

	public static function detect(string $accept_language, string $fallback = self::DEFAULT_LANGUAGE): string
	{
		$candidates = [];
		foreach (explode(',', $accept_language) as $position => $entry)
		{
			$parts = array_map('trim', explode(';', $entry));
			$tag = strtolower($parts[0] ?? '');
			if ($tag === '' || $tag === '*')
			{
				continue;
			}
			$quality = 1.0;
			foreach (array_slice($parts, 1) as $parameter)
			{
				if (preg_match('/^q\s*=\s*(0(?:\.\d+)?|1(?:\.0+)?)$/i', $parameter, $matches) === 1)
				{
					$quality = (float)$matches[1];
				}
			}
			$language = explode('-', $tag, 2)[0];
			if (isset(self::LANGUAGE_NAMES[$language]))
			{
				$candidates[] = ['language' => $language, 'quality' => $quality, 'position' => $position];
			}
		}
		usort($candidates, static fn(array $a, array $b): int => $b['quality'] <=> $a['quality'] ?: $a['position'] <=> $b['position']);
		return $candidates[0]['language'] ?? (self::isSupported($fallback) ? strtolower($fallback) : self::DEFAULT_LANGUAGE);
	}

	public static function resolve(?string $explicit, ?string $cookie, string $accept_language): string
	{
		if (self::isSupported($explicit))
		{
			return strtolower((string)$explicit);
		}
		if (self::isSupported($cookie))
		{
			return strtolower((string)$cookie);
		}
		return self::detect($accept_language);
	}

	public function localizeError(string $message): string
	{
		$key = self::ERROR_KEYS[$message] ?? null;
		return $key === null ? $message : $this->t($key);
	}

	/** @param array<string,string|int|float> $parameters */
	public function t(string $key, array $parameters = []): string
	{
		$text = self::STRINGS[$this->language][$key] ?? self::STRINGS[self::DEFAULT_LANGUAGE][$key] ?? $key;
		foreach ($parameters as $name => $value)
		{
			$text = str_replace('{' . $name . '}', (string)$value, $text);
		}
		return $text;
	}
}
