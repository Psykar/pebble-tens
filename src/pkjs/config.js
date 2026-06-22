// Clay configuration page for Tens. Each item's messageKey matches the keys
// declared in package.json and read in src/c/settings.c. Mirrors UserConfig.

// The four bar slots each pick one metric from this list (mirrors the
// TENS_METRIC_* enum in src/c/settings.h).
var SLOT_OPTIONS = [
  { label: 'Off', value: 0 },
  { label: 'Week', value: 1 },
  { label: 'Month', value: 2 },
  { label: 'Year', value: 3 },
  { label: 'Life', value: 4 },
  { label: 'Battery', value: 5 },
];

function slotSelect(messageKey, label, defaultValue) {
  return {
    type: 'select',
    messageKey: messageKey,
    label: label,
    defaultValue: defaultValue,
    options: SLOT_OPTIONS,
  };
}

module.exports = [
  { type: 'heading', defaultValue: 'Tens' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Day grid' },
      {
        type: 'toggle',
        messageKey: 'RAINBOW',
        label: 'Rainbow (spectral grid)',
        defaultValue: false,
      },
      {
        type: 'toggle',
        messageKey: 'DARK_MODE',
        label: 'Dark mode (black background)',
        defaultValue: false,
      },
      {
        type: 'color',
        messageKey: 'GRID_COLOR_LIGHT',
        label: 'Box color (light mode)',
        defaultValue: '0x000000',
        sunlight: false,
      },
      {
        type: 'color',
        messageKey: 'GRID_COLOR_DARK',
        label: 'Box color (dark mode)',
        defaultValue: '0xFFFFFF',
        sunlight: false,
      },
      {
        type: 'toggle',
        messageKey: 'LAYOUT_4X6',
        label: 'Layout 4x6 (3x2 cells, vs 6x4)',
        defaultValue: false,
      },
      {
        type: 'toggle',
        messageKey: 'HOURS_HORIZONTAL',
        label: 'Hours fill horizontally (vs vertically)',
        defaultValue: true,
      },
      {
        type: 'toggle',
        messageKey: 'FILL_INVERT',
        label: 'Minute fill from far edge',
        defaultValue: false,
      },
      {
        type: 'toggle',
        messageKey: 'BARS_MISSING_STYLE',
        label: 'Bars: fill missing parts (vs outline)',
        defaultValue: true,
      },
      {
        type: 'toggle',
        messageKey: 'GRID_MISSING_STYLE',
        label: 'Current 10-min block: fill missing parts (vs outline)',
        defaultValue: false,
      },
    ],
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Bars' },
      slotSelect('SLOT_TL', 'Top-left', 2),
      slotSelect('SLOT_TR', 'Top-right', 3),
      slotSelect('SLOT_BL', 'Bottom-left', 4),
      slotSelect('SLOT_BR', 'Bottom-right', 5),
    ],
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Work day' },
      {
        type: 'toggle',
        messageKey: 'WORK_ENABLED',
        label: 'Highlight work-day hours',
        defaultValue: false,
      },
      {
        type: 'input',
        messageKey: 'WORK_START',
        label: 'Start time (HH:MM)',
        attributes: { placeholder: '09:00' },
        defaultValue: '09:00',
      },
      {
        type: 'input',
        messageKey: 'WORK_END',
        label: 'End time (HH:MM, exclusive)',
        attributes: { placeholder: '17:00' },
        defaultValue: '17:00',
      },
      {
        type: 'color',
        messageKey: 'WORK_COLOR',
        label: 'Work-day color',
        defaultValue: '0xFFAA00',
        sunlight: false,
      },
    ],
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'You' },
      {
        type: 'input',
        messageKey: 'BIRTH_YEAR',
        label: 'Birth year',
        attributes: { type: 'number', min: 1900, max: 2100 },
        defaultValue: '1990',
      },
      {
        type: 'input',
        messageKey: 'BIRTH_MONTH',
        label: 'Birth month (1-12)',
        attributes: { type: 'number', min: 1, max: 12 },
        defaultValue: '4',
      },
      {
        type: 'input',
        messageKey: 'BIRTH_DAY',
        label: 'Birth day (1-31)',
        attributes: { type: 'number', min: 1, max: 31 },
        defaultValue: '12',
      },
      {
        type: 'input',
        messageKey: 'LIFE_SPAN_YEARS',
        label: 'Life span (years)',
        attributes: { type: 'number', min: 1, max: 150 },
        defaultValue: '80',
      },
    ],
  },
  { type: 'submit', defaultValue: 'Save' },
];
