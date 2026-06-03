/*
"backgroundColor": 0,
        "hourColor": 1,
        "handColor": 2,
        "dotColor": 3,
        "handOutlineColor": 4,

        "hourFormat": 6,
        "dateToggle": 9,
        "digTimeToggle": 10,
*/

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Macro Clock Settings'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Colors'
      },
      {
        type: 'select',
        messageKey: 'backgroundColor',
        label: 'Background Color',
        defaultValue: 'wht',
        options: [
          { label: 'Black', value: 'blk' },
          { label: 'White', value: 'wht' },
          { label: 'Red', value: 'red' },
          { label: 'Orange', value: 'org' },
          { label: 'Yellow', value: 'ylw' },
          { label: 'Green', value: 'grn' },
          { label: 'Blue', value: 'ble' },
          { label: 'Purple', value: 'prp' },
          { label: 'Pink', value: 'pnk' },
          { label: 'Gray', value: 'gry' },
        ]
      },
      {
        type: 'select',
        messageKey: 'hourColor',
        label: 'Hour Digit Color',
        defaultValue: 'blk',
        options: [
          { label: 'Black', value: 'blk' },
          { label: 'White', value: 'wht' },
          { label: 'Red', value: 'red' },
          { label: 'Orange', value: 'org' },
          { label: 'Yellow', value: 'ylw' },
          { label: 'Green', value: 'grn' },
          { label: 'Blue', value: 'ble' },
          { label: 'Purple', value: 'prp' },
          { label: 'Pink', value: 'pnk' },
          { label: 'Gray', value: 'gry' },
        ]
      },
      {
        type: 'select',
        messageKey: 'handColor',
        label: 'Clock Hand Color',
        defaultValue: 'red',
        options: [
          { label: 'Black', value: 'blk' },
          { label: 'White', value: 'wht' },
          { label: 'Red', value: 'red' },
          { label: 'Orange', value: 'org' },
          { label: 'Yellow', value: 'ylw' },
          { label: 'Green', value: 'grn' },
          { label: 'Blue', value: 'ble' },
          { label: 'Purple', value: 'prp' },
          { label: 'Pink', value: 'pnk' },
          { label: 'Gray', value: 'gry' },
        ]
      },
      {
        type: 'select',
        messageKey: 'dotColor',
        label: 'Dot Color',
        defaultValue: 'gry',
        options: [
          { label: 'Black', value: 'blk' },
          { label: 'White', value: 'wht' },
          { label: 'Red', value: 'red' },
          { label: 'Orange', value: 'org' },
          { label: 'Yellow', value: 'ylw' },
          { label: 'Green', value: 'grn' },
          { label: 'Blue', value: 'ble' },
          { label: 'Purple', value: 'prp' },
          { label: 'Pink', value: 'pnk' },
          { label: 'Gray', value: 'gry' },
        ]
      },
      {
        type: 'select',
        messageKey: 'handOutlineColor',
        label: 'Clock Hand Outline Color',
        defaultValue: 'gry',
        options: [
          { label: 'None', value: 'nob' },
          { label: 'Black', value: 'blk' },
          { label: 'White', value: 'wht' },
          { label: 'Red', value: 'red' },
          { label: 'Orange', value: 'org' },
          { label: 'Yellow', value: 'ylw' },
          { label: 'Green', value: 'grn' },
          { label: 'Blue', value: 'ble' },
          { label: 'Purple', value: 'prp' },
          { label: 'Pink', value: 'pnk' },
          { label: 'Gray', value: 'gry' },
        ]
      },
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Other Settings'
      },
      {
        type: 'radiogroup',
        messageKey: 'hourFormat',
        label: '24-Hour Format',
        defaultValue: '24h',
        options: [
          { label: '12-Hour', value: '12h' },
          { label: '24-Hour', value: '24h' },
        ]
      },
      {
        type: 'select',
        messageKey: 'dateToggle',
        label: 'Show Date',
        defaultValue: 'flick',
        options: [
          { label: 'No', value: 'off' },
          { label: 'Show on Flick', value: 'flick' },
          { label: 'Always show', value: 'on' },
        ]
      },
      {
        type: 'select',
        messageKey: 'digTimeToggle',
        label: 'Show Digital Time',
        defaultValue: 'off',
        options: [
          { label: 'No', value: 'off' },
          { label: 'Show on Flick', value: 'flick' },
          { label: 'Always show', value: 'on' },
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save Settings'
  }
]
