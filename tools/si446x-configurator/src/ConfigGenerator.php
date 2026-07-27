<?php

declare(strict_types=1);

namespace El1\Si446xConfigurator;

use InvalidArgumentException;

final class ConfigGenerator
{
	private const GROUP_INT_CTL = 0x01;
	private const GROUP_PREAMBLE = 0x10;
	private const GROUP_SYNC = 0x11;
	private const GROUP_PKT = 0x12;
	private const GROUP_MODEM = 0x20;
	private const GROUP_MODEM_CHFLT = 0x21;
	private const GROUP_PA = 0x22;
	private const GROUP_FREQ_CONTROL = 0x40;

	private I18n $i18n;

	/** @var list<string> */
	private array $warnings = [];

	/** @var array<string,mixed> */
	private array $metadata = [];

	public function __construct(?I18n $i18n = null)
	{
		$this->i18n = $i18n ?? new I18n(I18n::DEFAULT_LANGUAGE);
	}

	/** @param array<string,mixed> $input */
	public function generate(array $input): GeneratedConfiguration
	{
		$this->warnings = [];
		$this->metadata = [];

		$base_text = trim((string)($input['base_config'] ?? ''));
		$stream = $base_text === '' ? new CommandStream() : CommandStream::fromText($base_text);
		$has_base = $base_text !== '';

		$xo_hz = $this->integer($input, 'xo_hz', 30_000_000, 25_000_000, 32_000_000);
		$clock_source = (string)($input['clock_source'] ?? 'xtal');
		if (!in_array($clock_source, ['xtal', 'tcxo'], true))
		{
			throw new InvalidArgumentException($this->i18n->t('generator.invalid_clock_source'));
		}
		$this->configurePowerUp($stream, $xo_hz, $clock_source === 'tcxo');

		$frequency_hz = $this->integer($input, 'frequency_hz', 433_920_000, 142_000_000, 1_050_000_000);
		$channel_spacing_hz = $this->integer($input, 'channel_spacing_hz', 0, 0, 2_000_000);
		$frequency = $this->calculateFrequency($frequency_hz, $xo_hz, $channel_spacing_hz);
		$stream->setProperty(self::GROUP_FREQ_CONTROL, 0x00, [
			$frequency['inte'],
			($frequency['frac'] >> 16) & 0x0f,
			($frequency['frac'] >> 8) & 0xff,
			$frequency['frac'] & 0xff,
			($frequency['channel_step'] >> 8) & 0xff,
			$frequency['channel_step'] & 0xff,
		]);
		$clkgen = $stream->getProperty(self::GROUP_MODEM, 0x51, 0x08) ?? 0x08;
		$stream->setProperty(self::GROUP_MODEM, 0x51, [($clkgen & 0xf8) | $frequency['band']]);

		$modulation = (string)($input['modulation'] ?? '2fsk');
		$modulation_values = ['cw' => 0, 'ook' => 1, '2fsk' => 2, '2gfsk' => 3, '4fsk' => 4, '4gfsk' => 5];
		if (!isset($modulation_values[$modulation]))
		{
			throw new InvalidArgumentException($this->i18n->t('generator.invalid_modulation'));
		}
		$mod_source = (string)($input['mod_source'] ?? 'packet');
		$source_values = ['packet' => 0, 'direct' => 1, 'pseudo' => 2];
		if (!isset($source_values[$mod_source]))
		{
			throw new InvalidArgumentException($this->i18n->t('generator.invalid_modulation_source'));
		}
		$direct_gpio = $this->integer($input, 'direct_gpio', 0, 0, 3);
		$direct_async = $this->boolean($input, 'direct_async');
		$mod_type = $modulation_values[$modulation] | ($source_values[$mod_source] << 3);
		if ($mod_source === 'direct')
		{
			$mod_type |= $direct_gpio << 5;
			if ($direct_async)
			{
				$mod_type |= 0x80;
			}
		}
		$stream->setProperty(self::GROUP_MODEM, 0x00, [$mod_type]);

		$bitrate = $this->integer($input, 'tx_bitrate', 10_000, 100, 1_000_000);
		$data_rate_raw = $bitrate * 10;
		if ($data_rate_raw > 0xffffff)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.bitrate_unrepresentable'));
		}
		$stream->setProperty(self::GROUP_MODEM, 0x03, [
			($data_rate_raw >> 16) & 0xff,
			($data_rate_raw >> 8) & 0xff,
			$data_rate_raw & 0xff,
		]);
		$stream->setProperty(self::GROUP_MODEM, 0x06, [
			($xo_hz >> 24) & 0x03,
			($xo_hz >> 16) & 0xff,
			($xo_hz >> 8) & 0xff,
			$xo_hz & 0xff,
		]);

		$deviation_hz = $this->integer($input, 'deviation_hz', 20_000, 0, 500_000);
		$deviation_raw = (int)round($deviation_hz * 1_048_576 / $xo_hz);
		if ($deviation_raw > 0x1ffff)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.deviation_unrepresentable'));
		}
		$stream->setProperty(self::GROUP_MODEM, 0x0a, [
			($deviation_raw >> 16) & 0x01,
			($deviation_raw >> 8) & 0xff,
			$deviation_raw & 0xff,
		]);

		$manchester = $this->boolean($input, 'manchester');
		$map_control = $stream->getProperty(self::GROUP_MODEM, 0x01, 0x00) ?? 0x00;
		$map_control = $manchester ? ($map_control | 0x80) : ($map_control & ~0x80);
		$stream->setProperty(self::GROUP_MODEM, 0x01, [$map_control]);

		$this->configurePacket($stream, $input);
		$this->configurePa($stream, $input);
		$this->configureInterrupts($stream, $input);
		$this->configureGpio($stream, $input);
		$this->configureRawOverrides($stream, (string)($input['raw_properties'] ?? ''));
		$this->configureRawCommands($stream, (string)($input['raw_commands'] ?? ''));

		$rx_bandwidth = trim((string)($input['rx_bandwidth_hz'] ?? ''));
		if ($rx_bandwidth !== '')
		{
			$rx_bandwidth_hz = $this->strictInteger($rx_bandwidth, 1_000, 850_000, $this->i18n->t('label.rx_bandwidth'));
			$this->metadata['rx_bandwidth_hz'] = $rx_bandwidth_hz;
			$this->warnings[] = $this->i18n->t('generator.warning_rx_metadata');
		}
		if (!$has_base)
		{
			$this->warnings[] = $this->i18n->t('generator.warning_no_base');
		}
		elseif ($frequency_hz !== 433_920_000 || $bitrate !== 10_000 || $deviation_hz !== 20_000)
		{
			$this->warnings[] = $this->i18n->t('generator.warning_base_mismatch');
		}

		$this->metadata += [
			'chip' => (string)($input['chip'] ?? 'Si4463-C2A'),
			'xo_hz' => $xo_hz,
			'clock_source' => $clock_source,
			'frequency_hz' => $frequency_hz,
			'channel_spacing_hz' => $channel_spacing_hz,
			'outdiv' => $frequency['outdiv'],
			'band' => $frequency['band'],
			'modulation' => $modulation,
			'mod_source' => $mod_source,
			'tx_bitrate' => $bitrate,
			'deviation_hz' => $deviation_hz,
			'manchester' => $manchester,
			'has_wds_base' => $has_base,
		];

		return new GeneratedConfiguration($stream, $this->metadata, $this->warnings);
	}

	private function configurePowerUp(CommandStream $stream, int $xo_hz, bool $tcxo): void
	{
		$existing_boot_options = 0x01;
		foreach ($stream->commands() as $command)
		{
			if ($command[0] === 0x02 && count($command) >= 7)
			{
				$existing_boot_options = $command[1];
				break;
			}
		}
		$command = [
			0x02,
			($existing_boot_options & 0x80) | 0x01,
			$tcxo ? 0x01 : 0x00,
			($xo_hz >> 24) & 0xff,
			($xo_hz >> 16) & 0xff,
			($xo_hz >> 8) & 0xff,
			$xo_hz & 0xff,
		];
		if (!$stream->replaceFirstCommand(0x02, $command))
		{
			$stream->appendCommand($command);
		}
	}

	/** @param array<string,mixed> $input */
	private function configurePacket(CommandStream $stream, array $input): void
	{
		$preamble_tx = $this->integer($input, 'preamble_tx_length', 8, 0, 255);
		$preamble_rx = $this->integer($input, 'preamble_rx_threshold', 20, 0, 127);
		$stream->setProperty(self::GROUP_PREAMBLE, 0x00, [$preamble_tx]);
		$preamble_std1 = $stream->getProperty(self::GROUP_PREAMBLE, 0x01, 0x14) ?? 0x14;
		$stream->setProperty(self::GROUP_PREAMBLE, 0x01, [($preamble_std1 & 0x80) | $preamble_rx]);

		$sync = $this->hexBytes((string)($input['sync_word'] ?? '2D D4'), 1, 4, $this->i18n->t('label.sync_word'));
		$sync_errors = $this->integer($input, 'sync_errors', 0, 0, 7);
		$sync_config = (($sync_errors & 0x07) << 4) | ((count($sync) - 1) & 0x03);
		if ($this->boolean($input, 'sync_manchester'))
		{
			$sync_config |= 0x04;
		}
		if (in_array((string)($input['modulation'] ?? ''), ['4fsk', '4gfsk'], true))
		{
			$sync_config |= 0x08;
		}
		$stream->setProperty(self::GROUP_SYNC, 0x00, [$sync_config]);
		$sync_padded = array_pad($sync, 4, 0x00);
		$stream->setProperty(self::GROUP_SYNC, 0x01, $sync_padded);

		$crc_map = [
			'none' => 0,
			'crc8_itu' => 1,
			'crc16_iec' => 2,
			'crc16_baicheva' => 3,
			'crc16_ibm' => 4,
			'crc16_ccitt' => 5,
			'crc16_koopman' => 6,
			'crc32_ieee' => 7,
			'crc32_castagnoli' => 8,
			'crc16_dnp' => 9,
		];
		$crc = (string)($input['crc'] ?? 'crc16_ccitt');
		if (!isset($crc_map[$crc]))
		{
			throw new InvalidArgumentException($this->i18n->t('generator.invalid_crc'));
		}
		$crc_config = $crc_map[$crc];
		if ($this->boolean($input, 'crc_seed_all_ones'))
		{
			$crc_config |= 0x80;
		}
		$stream->setProperty(self::GROUP_PKT, 0x00, [$crc_config]);

		$whitening_poly = $this->hexInteger((string)($input['whitening_poly'] ?? '0108'), 0, 0xffff, $this->i18n->t('label.whitening_polynomial'));
		$whitening_seed = $this->hexInteger((string)($input['whitening_seed'] ?? 'FFFF'), 0, 0xffff, $this->i18n->t('label.whitening_seed'));
		$stream->setProperty(self::GROUP_PKT, 0x01, [
			($whitening_poly >> 8) & 0xff,
			$whitening_poly & 0xff,
			($whitening_seed >> 8) & 0xff,
			$whitening_seed & 0xff,
		]);

		$tx_threshold = $this->integer($input, 'pkt_tx_threshold', 48, 0, 64);
		$rx_threshold = $this->integer($input, 'pkt_rx_threshold', 48, 0, 64);
		$stream->setProperty(self::GROUP_PKT, 0x0b, [$tx_threshold, $rx_threshold]);

		$field1_length = $this->integer($input, 'field1_length', 0, 0, 0x1fff);
		$field1_config = 0x00;
		if ($this->boolean($input, 'field1_manchester'))
		{
			$field1_config |= 0x01;
		}
		if ($this->boolean($input, 'field1_whiten'))
		{
			$field1_config |= 0x02;
		}
		if ($this->boolean($input, 'field1_pn_start'))
		{
			$field1_config |= 0x04;
		}
		if (in_array((string)($input['modulation'] ?? ''), ['4fsk', '4gfsk'], true))
		{
			$field1_config |= 0x10;
		}

		$field1_crc = 0x00;
		if ($crc !== 'none' && $this->boolean($input, 'field1_crc'))
		{
			// CRC_START | SEND_CRC | CHECK_CRC | CRC_ENABLE. TX/RX ignore the direction-specific bits.
			$field1_crc = 0xaa;
		}
		$stream->setProperty(self::GROUP_PKT, 0x0d, [
			($field1_length >> 8) & 0x1f,
			$field1_length & 0xff,
			$field1_config,
			$field1_crc,
		]);
	}

	/** @param array<string,mixed> $input */
	private function configurePa(CommandStream $stream, array $input): void
	{
		$power_raw = $this->integer($input, 'pa_power_raw', 0x7f, 0, 0x7f);
		$stream->setProperty(self::GROUP_PA, 0x01, [$power_raw]);
	}

	/** @param array<string,mixed> $input */
	private function configureInterrupts(CommandStream $stream, array $input): void
	{
		$stream->setProperty(self::GROUP_INT_CTL, 0x00, [
			$this->hexInteger((string)($input['int_enable'] ?? '00'), 0, 0xff, 'INT_CTL_ENABLE'),
			$this->hexInteger((string)($input['ph_int_enable'] ?? '00'), 0, 0xff, 'PH_INT_ENABLE'),
			$this->hexInteger((string)($input['modem_int_enable'] ?? '00'), 0, 0xff, 'MODEM_INT_ENABLE'),
			$this->hexInteger((string)($input['chip_int_enable'] ?? '00'), 0, 0xff, 'CHIP_INT_ENABLE'),
		]);
	}

	/** @param array<string,mixed> $input */
	private function configureGpio(CommandStream $stream, array $input): void
	{
		$valid_modes = array_merge(range(0, 28), range(31, 39));
		$command = [0x13];
		for ($gpio = 0; $gpio < 4; ++$gpio)
		{
			$mode = $this->integer($input, 'gpio' . $gpio . '_mode', $gpio === 1 ? 20 : 1, 0, 39);
			if (!in_array($mode, $valid_modes, true))
			{
				throw new InvalidArgumentException($this->i18n->t('generator.invalid_gpio'));
			}
			if ($this->boolean($input, 'gpio' . $gpio . '_pull'))
			{
				$mode |= 0x40;
			}
			$command[] = $mode;
		}
		$nirq_mode = $this->integer($input, 'nirq_mode', 0, 0, 39);
		$sdo_mode = $this->integer($input, 'sdo_mode', 0, 0, 39);
		if (!in_array($nirq_mode, $valid_modes, true) || !in_array($sdo_mode, $valid_modes, true))
		{
			throw new InvalidArgumentException($this->i18n->t('generator.invalid_nirq_sdo'));
		}
		$command[] = $nirq_mode;
		$command[] = $sdo_mode;
		$drive_strength = $this->integer($input, 'drive_strength', 1, 0, 3);
		$command[] = ($drive_strength & 0x03) << 5;
		$stream->appendCommand($command);
	}

	private function configureRawOverrides(CommandStream $stream, string $text): void
	{
		$lines = preg_split('/\R/', trim($text)) ?: [];
		foreach ($lines as $line_number => $line)
		{
			$line = trim(preg_replace('/\s*[#;].*$/', '', $line) ?? $line);
			if ($line === '')
			{
				continue;
			}
			if (preg_match('/^\s*(?:0x)?([0-9a-fA-F]{1,2})\s*:\s*(?:0x)?([0-9a-fA-F]{1,2})\s*:\s*(.+)$/', $line, $matches) !== 1)
			{
				throw new InvalidArgumentException($this->i18n->t('generator.invalid_raw_property', ['line' => $line_number + 1]));
			}
			$values = $this->hexBytes($matches[3], 1, 255, $this->i18n->t('label.raw_property_line', ['line' => $line_number + 1]));
			$stream->setProperty(hexdec($matches[1]), hexdec($matches[2]), $values);
		}
	}

	private function configureRawCommands(CommandStream $stream, string $text): void
	{
		$lines = preg_split('/\R/', trim($text)) ?: [];
		foreach ($lines as $line_number => $line)
		{
			$line = trim(preg_replace('/\s*[#;].*$/', '', $line) ?? $line);
			if ($line === '')
			{
				continue;
			}
			$command = $this->hexBytes($line, 1, 16, $this->i18n->t('label.raw_command_line', ['line' => $line_number + 1]));
			$stream->appendCommand($command);
		}
	}

	/** @return array{inte:int,frac:int,channel_step:int,band:int,outdiv:int} */
	private function calculateFrequency(int $frequency_hz, int $xo_hz, int $channel_spacing_hz): array
	{
		[$minimum, $maximum, $outdiv, $band] = match (true)
		{
			$frequency_hz >= 705_000_000 => [705_000_000, 1_050_000_000, 4, 0],
			$frequency_hz >= 525_000_000 => [525_000_000, 705_000_000, 6, 1],
			$frequency_hz >= 353_000_000 => [353_000_000, 525_000_000, 8, 2],
			$frequency_hz >= 239_000_000 => [239_000_000, 353_000_000, 12, 3],
			$frequency_hz >= 177_000_000 => [177_000_000, 239_000_000, 16, 4],
			default => [142_000_000, 177_000_000, 24, 5],
		};
		if ($frequency_hz < $minimum || $frequency_hz > $maximum)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.frequency_band'));
		}

		$f_pfd = 2.0 * $xo_hz / $outdiv;
		$n = $frequency_hz / $f_pfd;
		$inte = (int)floor($n) - 1;
		$frac = (int)round(($n - $inte) * 524_288.0);
		if ($frac > 0xfffff)
		{
			++$inte;
			$frac -= 524_288;
		}
		if ($inte < 0 || $inte > 0x7f || $frac < 0 || $frac > 0xfffff)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.frequency_range'));
		}
		$channel_step = (int)round($channel_spacing_hz / $f_pfd * 524_288.0);
		if ($channel_step > 0xffff)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.channel_step_range'));
		}
		return compact('inte', 'frac', 'channel_step', 'band', 'outdiv');
	}

	/** @param array<string,mixed> $input */
	private function integer(array $input, string $key, int $default, int $minimum, int $maximum): int
	{
		$value = trim((string)($input[$key] ?? (string)$default));
		return $this->strictInteger($value, $minimum, $maximum, $this->fieldLabel($key));
	}


	private function fieldLabel(string $key): string
	{
		$translation_key = match ($key)
		{
			'xo_hz' => 'xo_frequency',
			'frequency_hz' => 'carrier_frequency',
			'channel_spacing_hz' => 'channel_spacing',
			'direct_gpio' => 'direct_gpio',
			'tx_bitrate' => 'tx_bitrate',
			'deviation_hz' => 'frequency_deviation',
			'preamble_tx_length' => 'preamble_tx_length',
			'preamble_rx_threshold' => 'preamble_rx_threshold',
			'sync_errors' => 'sync_errors',
			'pkt_tx_threshold' => 'tx_fifo_threshold',
			'pkt_rx_threshold' => 'rx_fifo_threshold',
			'field1_length' => 'field1_length',
			'pa_power_raw' => 'pa_power',
			'drive_strength' => 'gpio_drive_strength',
			default => null,
		};
		return $translation_key === null ? $key : $this->i18n->t($translation_key);
	}

	private function strictInteger(string $value, int $minimum, int $maximum, string $label): int
	{
		if (preg_match('/^-?\d+$/', $value) !== 1)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.must_integer', ['label' => $label]));
		}
		$result = (int)$value;
		if ($result < $minimum || $result > $maximum)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.must_between', ['label' => $label, 'minimum' => $minimum, 'maximum' => $maximum]));
		}
		return $result;
	}

	private function hexInteger(string $value, int $minimum, int $maximum, string $label): int
	{
		$value = preg_replace('/^0x/i', '', trim($value)) ?? '';
		if (preg_match('/^[0-9a-fA-F]+$/', $value) !== 1)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.must_hex', ['label' => $label]));
		}
		$result = hexdec($value);
		if ($result < $minimum || $result > $maximum)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.outside_range', ['label' => $label]));
		}
		return (int)$result;
	}

	/** @return list<int> */
	private function hexBytes(string $value, int $minimum_count, int $maximum_count, string $label): array
	{
		$tokens = preg_split('/[\s,]+/', trim($value), -1, PREG_SPLIT_NO_EMPTY) ?: [];
		if (count($tokens) < $minimum_count || count($tokens) > $maximum_count)
		{
			throw new InvalidArgumentException($this->i18n->t('generator.byte_count', ['label' => $label, 'minimum' => $minimum_count, 'maximum' => $maximum_count]));
		}
		$result = [];
		foreach ($tokens as $token)
		{
			$token = preg_replace('/^0x/i', '', $token) ?? '';
			if (preg_match('/^[0-9a-fA-F]{1,2}$/', $token) !== 1)
			{
				throw new InvalidArgumentException($this->i18n->t('generator.invalid_byte', ['label' => $label]));
			}
			$result[] = hexdec($token);
		}
		return $result;
	}

	/** @param array<string,mixed> $input */
	private function boolean(array $input, string $key): bool
	{
		return isset($input[$key]) && in_array((string)$input[$key], ['1', 'true', 'on', 'yes'], true);
	}
}

final class GeneratedConfiguration
{
	/**
	 * @param array<string,mixed> $metadata
	 * @param list<string> $warnings
	 */
	public function __construct(
		public readonly CommandStream $stream,
		public readonly array $metadata,
		public readonly array $warnings,
	)
	{
	}

	public function json(): string
	{
		return json_encode([
			'format' => 'el1-si446x-config-v1',
			'metadata' => $this->metadata,
			'warnings' => $this->warnings,
			'commands' => $this->stream->commands(),
			'bytes_hex' => $this->stream->hexDump(),
		], JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR) . "\n";
	}

	public function cppHeader(string $symbol = 'SI446X_CONFIGURATION'): string
	{
		$symbol = preg_replace('/[^A-Za-z0-9_]/', '_', strtoupper($symbol)) ?: 'SI446X_CONFIGURATION';
		$bytes = $this->stream->bytes();
		$lines = [];
		for ($offset = 0; $offset < count($bytes); $offset += 12)
		{
			$chunk = array_slice($bytes, $offset, 12);
			$lines[] = "\t" . implode(', ', array_map(static fn(int $byte): string => sprintf('0x%02x', $byte), $chunk)) . ',';
		}
		return "#pragma once\n\n#include <el1/io_types.hpp>\n\nstatic constexpr el1::io::types::byte_t {$symbol}[] =\n{\n" . implode("\n", $lines) . "\n};\n";
	}
}
