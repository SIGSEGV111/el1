'use strict';

const form = document.getElementById('config-form');
const preset = document.getElementById('preset');
const language = document.getElementById('language');
const languageSubmit = document.getElementById('language-submit');
const modSource = document.getElementById('mod-source');
const directOptions = document.getElementById('direct-options');
const parameterHelpElement = document.getElementById('parameter-help');
const parameterHelp = parameterHelpElement ? JSON.parse(parameterHelpElement.textContent) : {};

function addParameterHelp()
{
	for (const [name, description] of Object.entries(parameterHelp))
	{
		const fields = name === 'preset' ? [preset] : Array.from(form.elements).filter((field) => field.name === name);
		for (const field of fields)
		{
			if (!(field instanceof HTMLElement))
			{
				continue;
			}
			field.title = description;
			field.dataset.help = description;

			const label = field.closest('label');
			if (label && label.querySelectorAll('input[name], select[name], textarea[name]').length === 1)
			{
				label.title = description;
				label.dataset.help = description;
			}
		}
	}
}

function setField(name, value)
{
	const field = form.elements.namedItem(name);
	if (!field)
	{
		return;
	}
	if (field instanceof RadioNodeList)
	{
		field.value = value;
		return;
	}
	if (field.type === 'checkbox')
	{
		field.checked = Boolean(value);
		return;
	}
	field.value = String(value);
}

function updateDirectOptions()
{
	directOptions.hidden = modSource.value !== 'direct';
}

const presets = {
	'greensens-ook': {
		frequency_hz: 433920000,
		modulation: 'ook',
		mod_source: 'direct',
		direct_gpio: 0,
		direct_async: true,
		tx_bitrate: 10000,
		deviation_hz: 0,
		gpio0_mode: 21,
		gpio1_mode: 20,
		sync_word: '2D D4',
	},
	'greensens-fsk': {
		frequency_hz: 433920000,
		modulation: '2fsk',
		mod_source: 'direct',
		direct_gpio: 0,
		direct_async: true,
		tx_bitrate: 10000,
		deviation_hz: 20000,
		gpio0_mode: 20,
		gpio1_mode: 17,
	},
	'packet-433': {
		frequency_hz: 433920000,
		modulation: '2gfsk',
		mod_source: 'packet',
		tx_bitrate: 10000,
		deviation_hz: 20000,
		preamble_tx_length: 8,
		sync_word: '2D D4',
		crc: 'crc16_ccitt',
	},
	'packet-868': {
		frequency_hz: 868300000,
		modulation: '2gfsk',
		mod_source: 'packet',
		tx_bitrate: 38400,
		deviation_hz: 20000,
		preamble_tx_length: 8,
		sync_word: '2D D4',
		crc: 'crc16_ccitt',
	},
};

preset.addEventListener('change', () =>
{
	const values = presets[preset.value];
	if (!values)
	{
		return;
	}
	for (const [name, value] of Object.entries(values))
	{
		setField(name, value);
	}
	updateDirectOptions();
});

modSource.addEventListener('change', updateDirectOptions);
addParameterHelp();
updateDirectOptions();


if (language && languageSubmit)
{
	language.addEventListener('change', () =>
	{
		form.requestSubmit(languageSubmit);
	});
}
