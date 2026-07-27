<?php

declare(strict_types=1);

use El1\Si446xConfigurator\ConfigGenerator;
use El1\Si446xConfigurator\GeneratedConfiguration;
use El1\Si446xConfigurator\I18n;
use El1\Si446xConfigurator\ParameterHelp;

require_once __DIR__ . '/src/CommandStream.php';
require_once __DIR__ . '/src/I18n.php';
require_once __DIR__ . '/src/ConfigGenerator.php';
require_once __DIR__ . '/src/ParameterHelp.php';

function escape(string $value): string
{
	return htmlspecialchars($value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}


$explicit_language = isset($_POST['lang']) ? (string)$_POST['lang'] : (isset($_GET['lang']) ? (string)$_GET['lang'] : null);
$language = I18n::resolve($explicit_language, isset($_COOKIE['si446x_lang']) ? (string)$_COOKIE['si446x_lang'] : null, (string)($_SERVER['HTTP_ACCEPT_LANGUAGE'] ?? ''));
$i18n = new I18n($language);
if (I18n::isSupported($explicit_language))
{
	setcookie('si446x_lang', $language, [
		'expires' => time() + 31536000,
		'path' => '/',
		'samesite' => 'Lax',
	]);
}

/** @param array<string,string|int|float> $parameters */
function t(string $key, array $parameters = []): string
{
	global $i18n;
	return escape($i18n->t($key, $parameters));
}

/** @param array<string,mixed> $values */
function value(array $values, string $name, string $default = ''): string
{
	return escape((string)($values[$name] ?? $default));
}

/** @param array<string,mixed> $values */
function checked(array $values, string $name, bool $default = false): string
{
	$is_checked = array_key_exists($name, $values) ? in_array((string)$values[$name], ['1', 'true', 'on', 'yes'], true) : $default;
	return $is_checked ? ' checked' : '';
}

/** @param array<string,mixed> $values */
function selected(array $values, string $name, string $option, string $default = ''): string
{
	$current = (string)($values[$name] ?? $default);
	return $current === $option ? ' selected' : '';
}

/** @return array<int,string> */
function gpio_modes(string $language): array
{
	if ($language === 'de')
	{
		return [
			0 => 'Nichts', 1 => 'Hochohmig', 2 => '0 treiben', 3 => '1 treiben', 4 => 'Eingang',
			5 => '32-kHz-Takt', 6 => 'Boot-Takt', 7 => 'Geteilter Takt', 8 => 'CTS', 9 => 'Invertiertes CTS',
			10 => 'Command overlap', 11 => 'SDO', 12 => 'POR', 13 => 'CAL_WUT', 14 => 'WUT', 15 => 'EN_PA',
			16 => 'TX-Datentakt', 17 => 'RX-Datentakt', 18 => 'EN_LNA', 19 => 'TX-Daten', 20 => 'RX-Daten',
			21 => 'RX-Rohdaten', 22 => 'Antennenschalter 1', 23 => 'Antennenschalter 2', 24 => 'Gültige Präambel',
			25 => 'Ungültige Präambel', 26 => 'Syncword erkannt', 27 => 'CCA', 28 => 'Im Sleep', 31 => 'TX/RX-Datentakt',
			32 => 'TX-Zustand', 33 => 'RX-Zustand', 34 => 'RX FIFO voll', 35 => 'TX FIFO leer', 36 => 'Niedrige Batteriespannung',
			37 => 'CCA latch', 38 => 'Hopped', 39 => 'Hop table wrap',
		];
	}
	return [
		0 => 'Do nothing', 1 => 'Tri-state', 2 => 'Drive 0', 3 => 'Drive 1', 4 => 'Input',
		5 => '32 kHz clock', 6 => 'Boot clock', 7 => 'Divided clock', 8 => 'CTS', 9 => 'Inverted CTS',
		10 => 'Command overlap', 11 => 'SDO', 12 => 'POR', 13 => 'CAL_WUT', 14 => 'WUT', 15 => 'EN_PA',
		16 => 'TX data clock', 17 => 'RX data clock', 18 => 'EN_LNA', 19 => 'TX data', 20 => 'RX data',
		21 => 'RX raw data', 22 => 'Antenna switch 1', 23 => 'Antenna switch 2', 24 => 'Valid preamble',
		25 => 'Invalid preamble', 26 => 'Sync word detect', 27 => 'CCA', 28 => 'In sleep', 31 => 'TX/RX data clock',
		32 => 'TX state', 33 => 'RX state', 34 => 'RX FIFO full', 35 => 'TX FIFO empty', 36 => 'Low battery',
		37 => 'CCA latch', 38 => 'Hopped', 39 => 'Hop table wrap',
	];
}

$defaults = [
	'chip' => 'RFM26W-Si4463-C2A',
	'xo_hz' => '30000000',
	'clock_source' => 'xtal',
	'frequency_hz' => '433920000',
	'channel_spacing_hz' => '0',
	'modulation' => '2fsk',
	'mod_source' => 'packet',
	'direct_gpio' => '0',
	'tx_bitrate' => '10000',
	'deviation_hz' => '20000',
	'rx_bandwidth_hz' => '',
	'preamble_tx_length' => '8',
	'preamble_rx_threshold' => '20',
	'field1_length' => '0',
	'field1_crc' => '1',
	'pkt_tx_threshold' => '48',
	'pkt_rx_threshold' => '48',
	'sync_word' => '2D D4',
	'sync_errors' => '0',
	'crc' => 'crc16_ccitt',
	'crc_seed_all_ones' => '1',
	'whitening_poly' => '0108',
	'whitening_seed' => 'FFFF',
	'pa_power_raw' => '127',
	'gpio0_mode' => '1',
	'gpio1_mode' => '20',
	'gpio2_mode' => '1',
	'gpio3_mode' => '1',
	'nirq_mode' => '0',
	'sdo_mode' => '0',
	'drive_strength' => '1',
	'int_enable' => '00',
	'ph_int_enable' => '00',
	'modem_int_enable' => '00',
	'chip_int_enable' => '00',
	'base_config' => '',
	'raw_properties' => '',
	'raw_commands' => '',
	'filename' => 'si4463-config',
	'download_format' => 'cfg',
];

$input = $defaults;
$error = null;
$result = null;

if ($_SERVER['REQUEST_METHOD'] === 'POST')
{
	$checkboxes = ['direct_async', 'manchester', 'sync_manchester', 'crc_seed_all_ones', 'field1_crc', 'field1_whiten', 'field1_manchester', 'field1_pn_start', 'gpio0_pull', 'gpio1_pull', 'gpio2_pull', 'gpio3_pull'];
	$normalized_post = $_POST;
	foreach ($checkboxes as $checkbox)
	{
		if (!array_key_exists($checkbox, $normalized_post))
		{
			$normalized_post[$checkbox] = '0';
		}
	}
	$input = array_merge($defaults, $normalized_post);
	if (isset($_FILES['base_config_file']) && is_array($_FILES['base_config_file']))
	{
		$error_code = (int)($_FILES['base_config_file']['error'] ?? UPLOAD_ERR_NO_FILE);
		if ($error_code === UPLOAD_ERR_OK)
		{
			$size = (int)($_FILES['base_config_file']['size'] ?? 0);
			if ($size > 262144)
			{
				$error = $i18n->t('upload.too_large');
			}
			else
			{
				$uploaded = file_get_contents((string)$_FILES['base_config_file']['tmp_name']);
				if ($uploaded === false)
				{
					$error = $i18n->t('upload.read_failed');
				}
				elseif (str_contains($uploaded, "\0"))
				{
					$bytes = array_values(unpack('C*', $uploaded) ?: []);
					$input['base_config'] = implode(' ', array_map(static fn(int $byte): string => sprintf('%02X', $byte), $bytes));
				}
				else
				{
					$input['base_config'] = $uploaded;
				}
			}
		}
		elseif ($error_code !== UPLOAD_ERR_NO_FILE)
		{
			$error = $i18n->t('upload.failed');
		}
	}

	if ($error === null && !isset($_POST['language_change']))
	{
		try
		{
			$result = (new ConfigGenerator($i18n))->generate($input);
		}
		catch (Throwable $exception)
		{
			$error = $i18n->localizeError($exception->getMessage());
		}
	}

	if ($result instanceof GeneratedConfiguration && isset($_POST['download']))
	{
		$format = (string)($input['download_format'] ?? 'cfg');
		$filename = preg_replace('/[^A-Za-z0-9._-]+/', '-', (string)($input['filename'] ?? 'si4463-config')) ?: 'si4463-config';
		$filename = trim($filename, '.-');
		if ($filename === '')
		{
			$filename = 'si4463-config';
		}
		$content = '';
		$content_type = 'application/octet-stream';
		$extension = 'cfg';
		switch ($format)
		{
			case 'json':
				$content = $result->json();
				$content_type = 'application/json; charset=utf-8';
				$extension = 'json';
				break;
			case 'hpp':
				$content = $result->cppHeader($filename);
				$content_type = 'text/plain; charset=utf-8';
				$extension = 'hpp';
				break;
			case 'cfg':
			default:
				$content = $result->stream->binary();
				break;
		}
		header('Content-Type: ' . $content_type);
		header('Content-Length: ' . strlen($content));
		header('Content-Disposition: attachment; filename="' . $filename . '.' . $extension . '"');
		header('X-Content-Type-Options: nosniff');
		echo $content;
		exit;
	}
}

$gpio_modes = gpio_modes($language);
$parameter_help = ParameterHelp::all($language);
?>
<!doctype html>
<html lang="<?= escape($language) ?>">
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title>el1 Si446x / RFM26W Configurator</title>
	<link rel="stylesheet" href="assets/app.css">
	<script id="parameter-help" type="application/json"><?= json_encode($parameter_help, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT | JSON_THROW_ON_ERROR) ?></script>
	<script src="assets/app.js" defer></script>
</head>
<body>
<header class="page-header">
	<div>
		<h1>Si446x / RFM26W Configurator</h1>
		<p><?= t('intro') ?></p>
	</div>
	<div class="header-controls">
		<div class="language-control">
			<label class="language-label"><?= t('language') ?>
				<select name="lang" id="language" form="config-form">
					<?php foreach (I18n::languages() as $language_code => $language_name): ?>
						<option value="<?= escape($language_code) ?>"<?= $language === $language_code ? ' selected' : '' ?>><?= escape($language_name) ?></option>
					<?php endforeach; ?>
				</select>
			</label>
			<button type="submit" name="language_change" value="1" form="config-form" id="language-submit" class="language-submit secondary"><?= t('apply_language') ?></button>
		</div>
		<label class="preset-label"><?= t('preset') ?>
			<select id="preset">
				<option value=""><?= t('preset.keep') ?></option>
				<option value="greensens-ook"><?= t('preset.greensens_ook') ?></option>
				<option value="greensens-fsk"><?= t('preset.greensens_fsk') ?></option>
				<option value="packet-433"><?= t('preset.packet_433') ?></option>
				<option value="packet-868"><?= t('preset.packet_868') ?></option>
			</select>
		</label>
	</div>
</header>

<main>
	<?php if ($error !== null): ?>
		<div class="message error"><strong><?= t('error') ?></strong> <?= escape($error) ?></div>
	<?php endif; ?>
	<?php if ($result instanceof GeneratedConfiguration && $result->warnings !== []): ?>
		<div class="message warning">
			<strong><?= t('warnings') ?></strong>
			<ul>
				<?php foreach ($result->warnings as $warning): ?><li><?= escape($warning) ?></li><?php endforeach; ?>
			</ul>
		</div>
	<?php endif; ?>

	<form method="post" enctype="multipart/form-data" id="config-form">
		<section class="card">
			<h2><?= t('section.chip_clock') ?></h2>
			<div class="grid">
				<label><?= t('chip_module') ?>
					<select name="chip">
						<option value="RFM26W-Si4463-C2A"<?= selected($input, 'chip', 'RFM26W-Si4463-C2A', 'RFM26W-Si4463-C2A') ?>>HopeRF RFM26W / Si4463 Rev. C2A</option>
						<option value="Si4463-C2A"<?= selected($input, 'chip', 'Si4463-C2A') ?>>Si4463 Rev. C2A</option>
					</select>
				</label>
				<label><?= t('xo_frequency') ?>
					<input type="number" name="xo_hz" min="25000000" max="32000000" step="1" value="<?= value($input, 'xo_hz') ?>" required>
				</label>
				<label><?= t('clock_source') ?>
					<select name="clock_source">
						<option value="xtal"<?= selected($input, 'clock_source', 'xtal', 'xtal') ?>><?= t('clock.xtal') ?></option>
						<option value="tcxo"<?= selected($input, 'clock_source', 'tcxo') ?>><?= t('clock.tcxo') ?></option>
					</select>
				</label>
			</div>
		</section>

		<section class="card">
			<h2><?= t('section.rf_modem') ?></h2>
			<div class="grid">
				<label><?= t('carrier_frequency') ?>
					<input type="number" name="frequency_hz" min="142000000" max="1050000000" step="1" value="<?= value($input, 'frequency_hz') ?>" required>
				</label>
				<label><?= t('channel_spacing') ?>
					<input type="number" name="channel_spacing_hz" min="0" max="2000000" step="1" value="<?= value($input, 'channel_spacing_hz') ?>">
				</label>
				<label><?= t('modulation') ?>
					<select name="modulation">
						<?php foreach (['cw' => 'CW', 'ook' => 'OOK', '2fsk' => '2-FSK', '2gfsk' => '2-GFSK', '4fsk' => '4-FSK', '4gfsk' => '4-GFSK'] as $key => $label): ?>
							<option value="<?= $key ?>"<?= selected($input, 'modulation', $key, '2fsk') ?>><?= $label ?></option>
						<?php endforeach; ?>
					</select>
				</label>
				<label><?= t('modulation_source') ?>
					<select name="mod_source" id="mod-source">
						<option value="packet"<?= selected($input, 'mod_source', 'packet', 'packet') ?>><?= t('mod_source.packet') ?></option>
						<option value="direct"<?= selected($input, 'mod_source', 'direct') ?>><?= t('mod_source.direct') ?></option>
						<option value="pseudo"<?= selected($input, 'mod_source', 'pseudo') ?>><?= t('mod_source.pseudo') ?></option>
					</select>
				</label>
				<label><?= t('tx_bitrate') ?>
					<input type="number" name="tx_bitrate" min="100" max="1000000" step="1" value="<?= value($input, 'tx_bitrate') ?>" required>
				</label>
				<label><?= t('frequency_deviation') ?>
					<input type="number" name="deviation_hz" min="0" max="500000" step="1" value="<?= value($input, 'deviation_hz') ?>" required>
				</label>
				<label><?= t('rx_bandwidth') ?> <span class="muted"><?= t('metadata') ?></span>
					<input type="number" name="rx_bandwidth_hz" min="1000" max="850000" step="1" value="<?= value($input, 'rx_bandwidth_hz') ?>" placeholder="<?= t('rx_bandwidth_placeholder') ?>">
				</label>
				<label class="checkbox"><input type="checkbox" name="manchester" value="1"<?= checked($input, 'manchester') ?>> <?= t('manchester_mapping') ?></label>
			</div>
			<div id="direct-options" class="subgrid">
				<label><?= t('direct_gpio') ?>
					<select name="direct_gpio">
						<?php for ($gpio = 0; $gpio < 4; ++$gpio): ?><option value="<?= $gpio ?>"<?= selected($input, 'direct_gpio', (string)$gpio, '0') ?>>GPIO<?= $gpio ?></option><?php endfor; ?>
					</select>
				</label>
				<label class="checkbox"><input type="checkbox" name="direct_async" value="1"<?= checked($input, 'direct_async') ?>> <?= t('direct_async') ?></label>
			</div>
			<p class="help"><?= t('rf_modem_help') ?></p>
		</section>

		<section class="card">
			<h2><?= t('section.packet') ?></h2>
			<div class="grid">
				<label><?= t('preamble_tx_length') ?>
					<input type="number" name="preamble_tx_length" min="0" max="255" value="<?= value($input, 'preamble_tx_length') ?>">
				</label>
				<label><?= t('preamble_rx_threshold') ?>
					<input type="number" name="preamble_rx_threshold" min="0" max="127" value="<?= value($input, 'preamble_rx_threshold') ?>">
				</label>
				<label><?= t('sync_word') ?>
					<input type="text" name="sync_word" value="<?= value($input, 'sync_word') ?>" placeholder="2D D4">
				</label>
				<label><?= t('sync_errors') ?>
					<input type="number" name="sync_errors" min="0" max="7" value="<?= value($input, 'sync_errors') ?>">
				</label>
				<label><?= t('field1_length') ?>
					<input type="number" name="field1_length" min="0" max="8191" value="<?= value($input, 'field1_length') ?>">
					<span class="help"><?= t('field1_length_help') ?></span>
				</label>
				<label><?= t('tx_fifo_threshold') ?>
					<input type="number" name="pkt_tx_threshold" min="0" max="64" value="<?= value($input, 'pkt_tx_threshold') ?>">
				</label>
				<label><?= t('rx_fifo_threshold') ?>
					<input type="number" name="pkt_rx_threshold" min="0" max="64" value="<?= value($input, 'pkt_rx_threshold') ?>">
				</label>
				<label>CRC
					<select name="crc">
						<?php foreach (['none'=>$i18n->t('crc.none'),'crc8_itu'=>'CRC-8 ITU-T','crc16_iec'=>'CRC-16 IEC','crc16_baicheva'=>'CRC-16 Baicheva','crc16_ibm'=>'CRC-16 IBM','crc16_ccitt'=>'CRC-16 CCITT','crc16_koopman'=>'CRC-16 Koopman','crc32_ieee'=>'CRC-32 IEEE 802.3','crc32_castagnoli'=>'CRC-32 Castagnoli','crc16_dnp'=>'CRC-16 DNP'] as $key=>$label): ?>
							<option value="<?= $key ?>"<?= selected($input, 'crc', $key, 'crc16_ccitt') ?>><?= $label ?></option>
						<?php endforeach; ?>
					</select>
				</label>
				<label class="checkbox"><input type="checkbox" name="crc_seed_all_ones" value="1"<?= checked($input, 'crc_seed_all_ones', true) ?>> <?= t('crc_seed_all_ones') ?></label>
				<label class="checkbox"><input type="checkbox" name="sync_manchester" value="1"<?= checked($input, 'sync_manchester') ?>> <?= t('sync_manchester') ?></label>
				<label class="checkbox"><input type="checkbox" name="field1_crc" value="1"<?= checked($input, 'field1_crc', true) ?>> <?= t('field1_crc') ?></label>
				<label class="checkbox"><input type="checkbox" name="field1_whiten" value="1"<?= checked($input, 'field1_whiten') ?>> <?= t('field1_whiten') ?></label>
				<label class="checkbox"><input type="checkbox" name="field1_manchester" value="1"<?= checked($input, 'field1_manchester') ?>> <?= t('field1_manchester') ?></label>
				<label class="checkbox"><input type="checkbox" name="field1_pn_start" value="1"<?= checked($input, 'field1_pn_start') ?>> <?= t('field1_pn_start') ?></label>
				<label><?= t('whitening_polynomial') ?>
					<input type="text" name="whitening_poly" value="<?= value($input, 'whitening_poly') ?>">
				</label>
				<label><?= t('whitening_seed') ?>
					<input type="text" name="whitening_seed" value="<?= value($input, 'whitening_seed') ?>">
				</label>
			</div>
			<p class="help"><?= t('packet_help') ?></p>
		</section>

		<section class="card">
			<h2><?= t('section.pa_gpio_interrupts') ?></h2>
			<div class="grid">
				<label><?= t('pa_power') ?>
					<input type="number" name="pa_power_raw" min="0" max="127" value="<?= value($input, 'pa_power_raw') ?>">
				</label>
				<label><?= t('gpio_drive_strength') ?>
					<select name="drive_strength">
						<option value="0"<?= selected($input, 'drive_strength', '0') ?>><?= t('drive.high') ?></option>
						<option value="1"<?= selected($input, 'drive_strength', '1', '1') ?>><?= t('drive.medium_high') ?></option>
						<option value="2"<?= selected($input, 'drive_strength', '2') ?>><?= t('drive.medium_low') ?></option>
						<option value="3"<?= selected($input, 'drive_strength', '3') ?>><?= t('drive.low') ?></option>
					</select>
				</label>
				<?php for ($gpio = 0; $gpio < 4; ++$gpio): ?>
					<label><?= t('gpio_function', ['gpio' => $gpio]) ?>
						<select name="gpio<?= $gpio ?>_mode">
							<?php foreach ($gpio_modes as $number => $label): ?><option value="<?= $number ?>"<?= selected($input, 'gpio' . $gpio . '_mode', (string)$number, $gpio === 1 ? '20' : '1') ?>><?= $number ?> – <?= escape($label) ?></option><?php endforeach; ?>
						</select>
						<span class="inline-check"><input type="checkbox" name="gpio<?= $gpio ?>_pull" value="1"<?= checked($input, 'gpio' . $gpio . '_pull') ?>> <?= t('pull_up') ?></span>
					</label>
				<?php endfor; ?>
				<label><?= t('nirq_function') ?>
					<select name="nirq_mode"><?php foreach ($gpio_modes as $number => $label): ?><option value="<?= $number ?>"<?= selected($input, 'nirq_mode', (string)$number, '0') ?>><?= $number ?> – <?= escape($label) ?></option><?php endforeach; ?></select>
				</label>
				<label><?= t('sdo_function') ?>
					<select name="sdo_mode"><?php foreach ($gpio_modes as $number => $label): ?><option value="<?= $number ?>"<?= selected($input, 'sdo_mode', (string)$number, '0') ?>><?= $number ?> – <?= escape($label) ?></option><?php endforeach; ?></select>
				</label>
			</div>
			<h3><?= t('interrupt_masks') ?> <span class="muted"><?= t('hex') ?></span></h3>
			<div class="grid four">
				<label>INT_CTL_ENABLE <input type="text" name="int_enable" maxlength="4" value="<?= value($input, 'int_enable') ?>"></label>
				<label>PH_INT_ENABLE <input type="text" name="ph_int_enable" maxlength="4" value="<?= value($input, 'ph_int_enable') ?>"></label>
				<label>MODEM_INT_ENABLE <input type="text" name="modem_int_enable" maxlength="4" value="<?= value($input, 'modem_int_enable') ?>"></label>
				<label>CHIP_INT_ENABLE <input type="text" name="chip_int_enable" maxlength="4" value="<?= value($input, 'chip_int_enable') ?>"></label>
			</div>
		</section>

		<section class="card">
			<h2><?= t('section.base_config') ?></h2>
			<p class="help"><?= t('base_config_help') ?></p>
			<label><?= t('import_file') ?> <input type="file" name="base_config_file" accept=".h,.hpp,.c,.cfg,.bin,.json,.txt"></label>
			<label><?= t('paste_base') ?>
				<textarea name="base_config" rows="8" spellcheck="false" placeholder="#define RADIO_CONFIGURATION_DATA_ARRAY { 0x07, 0x02, ... 0x00 }"><?= value($input, 'base_config') ?></textarea>
			</label>
		</section>

		<details class="card">
			<summary><strong><?= t('advanced') ?></strong></summary>
			<div class="advanced-grid">
				<label><?= t('property_overrides') ?>
					<textarea name="raw_properties" rows="9" spellcheck="false" placeholder="21:00: 11 22 33 ...&#10;20:1C: 01 02 03"><?= value($input, 'raw_properties') ?></textarea>
					<span class="help"><code>group:start: bytes...</code>, <?= t('raw_properties_help') ?></span>
				</label>
				<label><?= t('additional_commands') ?>
					<textarea name="raw_commands" rows="9" spellcheck="false" placeholder="13 01 14 01 01 00 00 20"><?= value($input, 'raw_commands') ?></textarea>
					<span class="help"><?= t('raw_commands_help') ?></span>
				</label>
			</div>
		</details>

		<section class="card actions-card">
			<div class="download-row">
				<label><?= t('filename') ?> <input type="text" name="filename" value="<?= value($input, 'filename') ?>"></label>
				<label><?= t('format') ?>
					<select name="download_format">
						<option value="cfg"<?= selected($input, 'download_format', 'cfg', 'cfg') ?>><?= t('format.cfg') ?></option>
						<option value="json"<?= selected($input, 'download_format', 'json') ?>><?= t('format.json') ?></option>
						<option value="hpp"<?= selected($input, 'download_format', 'hpp') ?>><?= t('format.hpp') ?></option>
					</select>
				</label>
				<button type="submit" name="preview" value="1" class="secondary"><?= t('update_preview') ?></button>
				<button type="submit" name="download" value="1"><?= t('download') ?></button>
			</div>
		</section>
	</form>

	<?php if ($result instanceof GeneratedConfiguration): ?>
		<section class="card preview">
			<h2><?= t('preview') ?></h2>
			<div class="stats">
				<span><strong><?= count($result->stream->commands()) ?></strong> <?= t('commands') ?></span>
				<span><strong><?= strlen($result->stream->binary()) ?></strong> <?= t('bytes') ?></span>
				<span><strong><?= escape(number_format((int)$result->metadata['frequency_hz'] / 1_000_000, 6, '.', '')) ?></strong> MHz</span>
				<span><strong><?= escape((string)$result->metadata['outdiv']) ?></strong> OUTDIV</span>
			</div>
			<div class="preview-grid">
				<div>
					<h3><?= t('command_stream') ?></h3>
					<pre><?= escape($result->stream->hexDump()) ?></pre>
				</div>
				<div>
					<h3><?= t('commands') ?></h3>
					<div class="command-table-wrap"><table><thead><tr><th>#</th><th><?= t('length') ?></th><th><?= t('command') ?></th><th><?= t('bytes') ?></th></tr></thead><tbody>
					<?php foreach ($result->stream->commands() as $index => $command): ?>
						<tr><td><?= $index ?></td><td><?= count($command) ?></td><td><code>0x<?= sprintf('%02X', $command[0]) ?></code></td><td><code><?= implode(' ', array_map(static fn(int $byte): string => sprintf('%02X', $byte), $command)) ?></code></td></tr>
					<?php endforeach; ?>
					</tbody></table></div>
				</div>
			</div>
		</section>
	<?php endif; ?>
</main>
<footer><?= t('footer') ?></footer>
</body>
</html>
