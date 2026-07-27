<?php

declare(strict_types=1);

namespace El1\Si446xConfigurator;

use InvalidArgumentException;

final class CommandStream
{
	/** @var list<list<int>> */
	private array $commands = [];

	/** @param list<list<int>> $commands */
	public function __construct(array $commands = [])
	{
		foreach ($commands as $command)
		{
			$this->appendCommand($command);
		}
	}

	public static function fromBinary(string $binary): self
	{
		$bytes = array_values(unpack('C*', $binary) ?: []);
		return self::fromBytes($bytes);
	}

	/** @param list<int> $bytes */
	public static function fromBytes(array $bytes): self
	{
		$self = new self();
		$offset = 0;
		$count = count($bytes);
		$terminated = false;

		while ($offset < $count)
		{
			$length = self::validateByte($bytes[$offset++]);
			if ($length === 0)
			{
				$terminated = true;
				break;
			}
			if ($length > 16)
			{
				throw new InvalidArgumentException('Command length exceeds the Si446x 16-byte command buffer.');
			}
			if ($offset + $length > $count)
			{
				throw new InvalidArgumentException('Configuration stream ends inside a command.');
			}
			$self->appendCommand(array_slice($bytes, $offset, $length));
			$offset += $length;
		}

		if (!$terminated)
		{
			throw new InvalidArgumentException('Configuration stream has no terminating zero byte.');
		}
		for (; $offset < $count; ++$offset)
		{
			if ($bytes[$offset] !== 0)
			{
				throw new InvalidArgumentException('Unexpected data after the terminating zero byte.');
			}
		}
		return $self;
	}

	public static function fromText(string $text): self
	{
		$text = trim($text);
		if ($text === '')
		{
			return new self();
		}

		if ($text[0] === '{' || $text[0] === '[')
		{
			$data = json_decode($text, true, flags: JSON_THROW_ON_ERROR);
			if (is_array($data) && isset($data['commands']))
			{
				$data = $data['commands'];
			}
			if (is_array($data) && $data !== [] && is_array(reset($data)))
			{
				return new self(array_values($data));
			}
		}

		$source = $text;
		$marker = stripos($source, 'RADIO_CONFIGURATION_DATA_ARRAY');
		if ($marker !== false)
		{
			$brace = strpos($source, '{', $marker);
			if ($brace !== false)
			{
				$end = self::findClosingBrace($source, $brace);
				if ($end !== null)
				{
					$source = substr($source, $brace + 1, $end - $brace - 1);
				}
			}
		}

		preg_match_all('/0x([0-9a-fA-F]{1,2})\b/', $source, $hex_matches);
		if (!empty($hex_matches[1]))
		{
			$bytes = array_map(static fn(string $value): int => hexdec($value), $hex_matches[1]);
			return self::fromBytes(array_values($bytes));
		}

		$tokens = preg_split('/[\s,;:]+/', $source, -1, PREG_SPLIT_NO_EMPTY) ?: [];
		$bytes = [];
		foreach ($tokens as $token)
		{
			if (preg_match('/^[0-9a-fA-F]{2}$/', $token) !== 1)
			{
				throw new InvalidArgumentException('Base configuration must be a WDS C array, JSON command list, or two-digit hex byte stream.');
			}
			$bytes[] = hexdec($token);
		}
		return self::fromBytes($bytes);
	}

	/** @param list<int> $command */
	public function appendCommand(array $command): void
	{
		$length = count($command);
		if ($length < 1 || $length > 16)
		{
			throw new InvalidArgumentException('Si446x commands must contain between 1 and 16 bytes.');
		}
		$this->commands[] = array_map([self::class, 'validateByte'], array_values($command));
	}

	/** @param list<int> $command */
	public function replaceFirstCommand(int $command_id, array $command): bool
	{
		$command_id = self::validateByte($command_id);
		$command = array_map([self::class, 'validateByte'], array_values($command));
		foreach ($this->commands as $index => $existing)
		{
			if ($existing[0] === $command_id)
			{
				$this->commands[$index] = $command;
				return true;
			}
		}
		return false;
	}

	/** @param list<int> $values */
	public function setProperty(int $group, int $start, array $values): void
	{
		$group = self::validateByte($group);
		$start = self::validateByte($start);
		$values = array_map([self::class, 'validateByte'], array_values($values));
		$offset = 0;
		while ($offset < count($values))
		{
			$chunk = array_slice($values, $offset, 12);
			$this->appendCommand(array_merge([0x11, $group, count($chunk), $start + $offset], $chunk));
			$offset += count($chunk);
		}
	}

	public function getProperty(int $group, int $index, ?int $default = null): ?int
	{
		$value = $default;
		foreach ($this->commands as $command)
		{
			if (count($command) < 5 || $command[0] !== 0x11 || $command[1] !== $group)
			{
				continue;
			}
			$count = $command[2];
			$start = $command[3];
			if ($index >= $start && $index < $start + $count)
			{
				$value = $command[4 + ($index - $start)] ?? $value;
			}
		}
		return $value;
	}

	public function hasCommand(int $command_id): bool
	{
		foreach ($this->commands as $command)
		{
			if ($command[0] === $command_id)
			{
				return true;
			}
		}
		return false;
	}

	/** @return list<list<int>> */
	public function commands(): array
	{
		return $this->commands;
	}

	/** @return list<int> */
	public function bytes(): array
	{
		$result = [];
		foreach ($this->commands as $command)
		{
			$result[] = count($command);
			array_push($result, ...$command);
		}
		$result[] = 0;
		return $result;
	}

	public function binary(): string
	{
		$bytes = $this->bytes();
		return $bytes === [] ? '' : pack('C*', ...$bytes);
	}

	public function hexDump(int $bytes_per_line = 16): string
	{
		$hex = array_map(static fn(int $byte): string => sprintf('%02X', $byte), $this->bytes());
		$lines = [];
		for ($offset = 0; $offset < count($hex); $offset += $bytes_per_line)
		{
			$lines[] = implode(' ', array_slice($hex, $offset, $bytes_per_line));
		}
		return implode("\n", $lines);
	}

	private static function validateByte(int $value): int
	{
		if ($value < 0 || $value > 0xff)
		{
			throw new InvalidArgumentException('Byte value outside 0..255.');
		}
		return $value;
	}

	private static function findClosingBrace(string $source, int $open): ?int
	{
		$depth = 0;
		for ($index = $open, $length = strlen($source); $index < $length; ++$index)
		{
			if ($source[$index] === '{')
			{
				++$depth;
			}
			elseif ($source[$index] === '}')
			{
				--$depth;
				if ($depth === 0)
				{
					return $index;
				}
			}
		}
		return null;
	}
}
