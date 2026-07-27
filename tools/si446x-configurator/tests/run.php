<?php

declare(strict_types=1);

use El1\Si446xConfigurator\CommandStream;
use El1\Si446xConfigurator\ConfigGenerator;
use El1\Si446xConfigurator\I18n;
use El1\Si446xConfigurator\ParameterHelp;

require_once dirname(__DIR__) . '/src/CommandStream.php';
require_once dirname(__DIR__) . '/src/I18n.php';
require_once dirname(__DIR__) . '/src/ConfigGenerator.php';
require_once dirname(__DIR__) . '/src/ParameterHelp.php';

$tests = 0;

function expect(bool $condition, string $message): void
{
	global $tests;
	++$tests;
	if (!$condition)
	{
		fwrite(STDERR, "FAIL: {$message}\n");
		exit(1);
	}
}


expect(I18n::detect('de-DE,de;q=0.9,en;q=0.8') === 'de', 'Accept-Language detects German regional preference.');
expect(I18n::detect('fr-FR, en-GB;q=0.8, de;q=0.7') === 'en', 'Accept-Language skips unsupported languages and selects English.');
expect(I18n::detect('de;q=0.4,en;q=0.9') === 'en', 'Accept-Language honors quality values.');
expect(I18n::resolve('de', 'en', 'en-US') === 'de', 'Explicit language overrides cookie and browser language.');
expect(I18n::resolve(null, 'de', 'en-US') === 'de', 'Remembered language overrides browser language.');
expect((new I18n('de'))->t('download') === 'Download', 'German translation table is available.');
expect((new I18n('de'))->localizeError('Configuration stream has no terminating zero byte.') === 'Der Konfigurationsstream enthält kein abschließendes Nullbyte.', 'Command stream parser errors are localized for the UI.');
expect((new I18n('en'))->t('section.chip_clock') === 'Chip & Clock', 'English translation table is available.');

$i18n_reflection = new ReflectionClass(I18n::class);
$i18n_strings = $i18n_reflection->getReflectionConstant('STRINGS')?->getValue();
expect(is_array($i18n_strings), 'Translation table can be inspected for test coverage.');
expect(array_keys($i18n_strings['de'] ?? []) === array_keys($i18n_strings['en'] ?? []), 'German and English translation tables expose the same keys.');
$index_source = file_get_contents(dirname(__DIR__) . '/index.php') ?: '';
preg_match_all("/\bt\('([^']+)'/", $index_source, $ui_translation_matches);
foreach (array_unique($ui_translation_matches[1] ?? []) as $translation_key)
{
	expect((new I18n('de'))->t($translation_key) !== $translation_key, 'German UI translation exists for ' . $translation_key . '.');
	expect((new I18n('en'))->t($translation_key) !== $translation_key, 'English UI translation exists for ' . $translation_key . '.');
}

$stream = new CommandStream();
$stream->appendCommand([0x02, 0x01]);
$stream->setProperty(0x20, 0x00, range(0, 24));
expect(count($stream->commands()) === 4, 'SET_PROPERTY is chunked into max. 12 data bytes.');
foreach (array_slice($stream->commands(), 1) as $command)
{
	expect(count($command) <= 16, 'No command exceeds 16 bytes.');
}
$roundtrip = CommandStream::fromBinary($stream->binary());
expect($roundtrip->bytes() === $stream->bytes(), 'Binary command stream roundtrip.');
expect(substr($stream->binary(), -1) === "\0", 'Binary stream has terminating zero.');

$wds = <<<'WDS'
#define RADIO_CONFIGURATION_DATA_ARRAY { \
  0x07, 0x02, 0x01, 0x00, 0x01, 0xC9, 0xC3, 0x80, \
  0x00 \
}
#define SOME_UNRELATED_NUMBER 0xAA
WDS;
$parsed = CommandStream::fromText($wds);
expect(count($parsed->commands()) === 1, 'WDS RADIO_CONFIGURATION_DATA_ARRAY is parsed without unrelated hex constants.');
expect($parsed->commands()[0][0] === 0x02, 'WDS command ID parsed.');

$generator = new ConfigGenerator();
$result = $generator->generate([
	'xo_hz' => '30000000',
	'clock_source' => 'xtal',
	'frequency_hz' => '915000000',
	'channel_spacing_hz' => '0',
	'modulation' => '2fsk',
	'mod_source' => 'packet',
	'direct_gpio' => '0',
	'tx_bitrate' => '100000',
	'deviation_hz' => '50000',
	'preamble_tx_length' => '8',
	'preamble_rx_threshold' => '20',
	'field1_length' => '64', 'field1_crc' => '1', 'field1_whiten' => '1',
	'pkt_tx_threshold' => '48', 'pkt_rx_threshold' => '48',
	'sync_word' => '2D D4',
	'sync_errors' => '0',
	'crc' => 'crc16_ccitt',
	'crc_seed_all_ones' => '1',
	'whitening_poly' => '0108',
	'whitening_seed' => 'FFFF',
	'pa_power_raw' => '127',
	'gpio0_mode' => '1', 'gpio1_mode' => '20', 'gpio2_mode' => '1', 'gpio3_mode' => '1',
	'nirq_mode' => '0', 'sdo_mode' => '0', 'drive_strength' => '1',
	'int_enable' => '00', 'ph_int_enable' => '00', 'modem_int_enable' => '00', 'chip_int_enable' => '00',
]);
expect($result->metadata['outdiv'] === 4, '915 MHz selects OUTDIV 4.');
expect($result->metadata['band'] === 0, '915 MHz selects band 0.');
expect($result->stream->hasCommand(0x02), 'Generated stream contains POWER_UP.');
expect($result->stream->getProperty(0x10, 0x01) === 0x14, 'Preamble RX threshold is written to PREAMBLE_CONFIG_STD_1.');
expect($result->stream->getProperty(0x40, 0x00) === 0x3c, '915 MHz / 30 MHz XO yields FREQ_CONTROL_INTE = 0x3c.');
expect($result->stream->getProperty(0x40, 0x01) === 0x08, '915 MHz / 30 MHz XO yields FREQ_CONTROL_FRAC high nibble 0x08.');
expect($result->stream->getProperty(0x40, 0x02) === 0x00, '915 MHz / 30 MHz XO yields default FRAC mid byte.');
expect($result->stream->getProperty(0x40, 0x03) === 0x00, '915 MHz / 30 MHz XO yields default FRAC low byte.');
expect($result->stream->getProperty(0x20, 0x03) === 0x0f, '100 kbit/s TX data rate high byte matches reset reference.');
expect($result->stream->getProperty(0x20, 0x04) === 0x42, '100 kbit/s TX data rate middle byte matches reset reference.');
expect($result->stream->getProperty(0x20, 0x05) === 0x40, '100 kbit/s TX data rate low byte matches reset reference.');
expect($result->stream->getProperty(0x12, 0x0d) === 0x00 && $result->stream->getProperty(0x12, 0x0e) === 0x40, 'Field 1 length is encoded as a 13-bit value.');
expect($result->stream->getProperty(0x12, 0x0f) === 0x02, 'Field 1 whitening flag is encoded.');
expect($result->stream->getProperty(0x12, 0x10) === 0xaa, 'Field 1 CRC is enabled for TX and RX processing.');
expect($result->stream->getProperty(0x20, 0x0a) === 0x00, '50 kHz deviation high byte matches reset reference.');
expect($result->stream->getProperty(0x20, 0x0b) === 0x06, '50 kHz deviation middle byte matches reset reference.');
expect(abs(($result->stream->getProperty(0x20, 0x0c) ?? 0) - 0xd3) <= 1, '50 kHz deviation low byte matches reset reference within rounding.');

$result_433 = $generator->generate([
	'frequency_hz' => '433920000',
	'xo_hz' => '30000000',
	'clock_source' => 'xtal',
	'channel_spacing_hz' => '0',
	'modulation' => 'ook', 'mod_source' => 'direct', 'direct_gpio' => '0', 'direct_async' => '1',
	'tx_bitrate' => '10000', 'deviation_hz' => '0',
	'preamble_tx_length' => '8', 'preamble_rx_threshold' => '20', 'field1_length' => '0', 'pkt_tx_threshold' => '48', 'pkt_rx_threshold' => '48', 'sync_word' => '2D D4', 'sync_errors' => '0',
	'crc' => 'none', 'whitening_poly' => '0108', 'whitening_seed' => 'FFFF', 'pa_power_raw' => '127',
	'gpio0_mode' => '21', 'gpio1_mode' => '20', 'gpio2_mode' => '1', 'gpio3_mode' => '1',
	'nirq_mode' => '0', 'sdo_mode' => '0', 'drive_strength' => '1',
	'int_enable' => '00', 'ph_int_enable' => '00', 'modem_int_enable' => '00', 'chip_int_enable' => '00',
]);
expect($result_433->metadata['outdiv'] === 8, '433.92 MHz selects OUTDIV 8.');
expect($result_433->metadata['band'] === 2, '433.92 MHz selects band 2.');
expect(($result_433->stream->getProperty(0x20, 0x00) ?? 0) === 0x89, 'OOK asynchronous direct mode is encoded in MODEM_MOD_TYPE.');
expect($result_433->warnings !== [], 'Configuration without WDS base carries an RX modem warning.');
expect(str_contains($result_433->json(), 'el1-si446x-config-v1'), 'JSON output carries format marker.');
expect(str_contains($result_433->cppHeader('test-config'), 'TEST_CONFIG'), 'C++ header sanitizes symbol name.');


$expected_parameter_help = [
	'preset', 'chip', 'xo_hz', 'clock_source', 'frequency_hz', 'channel_spacing_hz', 'modulation', 'mod_source',
	'tx_bitrate', 'deviation_hz', 'rx_bandwidth_hz', 'manchester', 'direct_gpio', 'direct_async',
	'preamble_tx_length', 'preamble_rx_threshold', 'sync_word', 'sync_errors', 'field1_length', 'pkt_tx_threshold',
	'pkt_rx_threshold', 'crc', 'crc_seed_all_ones', 'sync_manchester', 'field1_crc', 'field1_whiten',
	'field1_manchester', 'field1_pn_start', 'whitening_poly', 'whitening_seed', 'pa_power_raw', 'drive_strength',
	'gpio0_mode', 'gpio1_mode', 'gpio2_mode', 'gpio3_mode', 'gpio0_pull', 'gpio1_pull', 'gpio2_pull', 'gpio3_pull',
	'nirq_mode', 'sdo_mode', 'int_enable', 'ph_int_enable', 'modem_int_enable', 'chip_int_enable', 'base_config_file',
	'base_config', 'raw_properties', 'raw_commands', 'filename', 'download_format',
];
$help = ParameterHelp::all();
expect(array_keys($help) === $expected_parameter_help, 'Parameter help covers every configurable UI field in a stable order.');
foreach ($expected_parameter_help as $name)
{
	expect(trim(ParameterHelp::get($name)) !== '', 'German parameter help for ' . $name . ' is non-empty.');
	expect(trim(ParameterHelp::get($name, 'en')) !== '', 'English parameter help for ' . $name . ' is non-empty.');
}
expect(array_keys(ParameterHelp::all('en')) === $expected_parameter_help, 'English parameter help covers the same fields in the same order.');
expect(ParameterHelp::get('frequency_hz', 'de') !== ParameterHelp::get('frequency_hz', 'en'), 'Parameter help changes with the selected language.');

$german_generator = new ConfigGenerator(new I18n('de'));
$german_result = $german_generator->generate([
	'xo_hz' => '30000000', 'clock_source' => 'xtal', 'frequency_hz' => '433920000', 'channel_spacing_hz' => '0',
	'modulation' => 'ook', 'mod_source' => 'direct', 'direct_gpio' => '0', 'direct_async' => '1',
	'tx_bitrate' => '10000', 'deviation_hz' => '0', 'preamble_tx_length' => '8', 'preamble_rx_threshold' => '20',
	'field1_length' => '0', 'pkt_tx_threshold' => '48', 'pkt_rx_threshold' => '48', 'sync_word' => '2D D4', 'sync_errors' => '0',
	'crc' => 'none', 'whitening_poly' => '0108', 'whitening_seed' => 'FFFF', 'pa_power_raw' => '127',
	'gpio0_mode' => '21', 'gpio1_mode' => '20', 'gpio2_mode' => '1', 'gpio3_mode' => '1',
	'nirq_mode' => '0', 'sdo_mode' => '0', 'drive_strength' => '1', 'int_enable' => '00', 'ph_int_enable' => '00',
	'modem_int_enable' => '00', 'chip_int_enable' => '00',
]);
expect(str_contains($german_result->warnings[0] ?? '', 'Basiskonfiguration'), 'Generator warnings are localized to German.');

printf("OK: %d tests\n", $tests);
