'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const crypto = require('node:crypto');

const source = fs.readFileSync(
  path.join(__dirname, '..', 'extras', 'google-apps-script', 'Code.gs'),
  'utf8'
);
const context = {
  console,
  Utilities: {
    DigestAlgorithm: { SHA_256: 'sha256' },
    Charset: { UTF_8: 'utf8' },
    computeDigest(algorithm, value, charset) {
      return Array.from(crypto.createHash(algorithm).update(value, charset).digest())
        .map((byte) => byte >= 128 ? byte - 256 : byte);
    },
    formatDate(date) {
      return date.toISOString();
    },
  },
};
vm.createContext(context);
vm.runInContext(source, context, { filename: 'Code.gs' });

assert.equal(context.slt5006TokensEqual_('secret', 'secret'), true);
assert.equal(context.slt5006TokensEqual_('secret', 'Secret'), false);
assert.equal(context.slt5006TokensEqual_('secret', 'secret2'), false);
assert.equal(context.slt5006TokensEqual_(null, 'secret'), false);

assert.equal(context.slt5006IsIsoTimestamp_('2026-09-04T12:34:56Z'), true);
assert.equal(
  context.slt5006IsIsoTimestamp_('2026-09-04T21:34:56+09:00'),
  true
);
assert.equal(context.slt5006IsIsoTimestamp_('2026-09-04 12:34:56'), false);
for (const value of [
  '2024-02-29T12:34:56Z',
  '2000-02-29T12:34:56Z',
  '2026-11-06T12:34:56.123456Z',
  '2026-11-06T12:34:56.1234567+09:00',
  '2026-11-06T24:00:00Z',
]) {
  assert.equal(context.slt5006IsIsoTimestamp_(value), true, value);
}
for (const value of [
  '2026-02-29T12:34:56Z',
  '2026-02-30T12:34:56Z',
  '1900-02-29T12:34:56Z',
  '2100-02-29T12:34:56Z',
  '2026-04-31T12:34:56Z',
  '2026-00-01T12:34:56Z',
  '2026-13-01T12:34:56Z',
  '2026-11-00T12:34:56Z',
  '2026-11-06T12:60:00Z',
  '2026-11-06T12:34:60Z',
  '2026-11-06T12:34:56+24:00',
]) {
  assert.equal(context.slt5006IsIsoTimestamp_(value), false, value);
}

assert.equal(context.slt5006OptionalNumber_('12.5', 'value'), 12.5);
for (const value of [undefined, null, ''])
  assert.equal(context.slt5006OptionalNumber_(value, 'value'), '');
for (const value of [' ', '\t', '\r\n', true, [], {}, Infinity, NaN]) {
  assert.throws(
    () => context.slt5006OptionalNumber_(value, 'value'),
    (error) => error.slt5006PublicCode === 'invalid_number'
  );
}
assert.throws(
  () => context.slt5006OptionalNumber_('not-a-number', 'value'),
  (error) => error.slt5006PublicCode === 'invalid_number'
);
assert.equal(context.slt5006OptionalText_('=SUM(A1:A2)', 'error', 256), "'=SUM(A1:A2)");
assert.equal(context.slt5006Identifier_(0, 'serialNumber', 64), '0');
for (const value of [true, false, {}, [], Infinity, NaN, null]) {
  assert.throws(
    () => context.slt5006Identifier_(value, 'serialNumber', 64),
    (error) => error.slt5006PublicCode === 'invalid_identifier'
  );
}

const normalized = context.slt5006NormalizeRow_(
  {
    id: 'device-1-20260904',
    ts: '2026-09-04T12:34:56Z',
    serialNumber: '12345',
    firmware: '1.7.6',
    temperature_c: 24.5,
    battery_v: 5.01,
    vwc_pct: 10,
    vwc_rock_pct: 20,
    vwc_coco_pct: 30,
    ec_bulk_dsm: 0.1,
    ec_pore_dsm: 0.2,
    ec_pore_coco_dsm: 0.3,
    core2_battery_v: 4.1,
    rssi_dbm: -70,
    error: '@formula',
  },
  0,
  'Asia/Tokyo'
);
assert.equal(normalized.id, 'device-1-20260904');
assert.equal(normalized.values[1], '12345');
const expectedHeader = [
  'Date', 'SerialNumber', 'Firmware', 'Battery(V)', 'Temperature(degC)',
  'VWC(%)', 'VWC rock(%)', 'VWC coco(%)', 'EC bulk(dS/m)', 'EC pore(dS/m)',
  'EC pore coco(dS/m)', 'Id', 'Core2 Battery(V)', 'WiFi RSSI(dBm)', 'Error',
];
assert.deepEqual(
  Array.from(vm.runInContext('SLT5006_HEADER', context)), expectedHeader
);
assert.deepEqual(Array.from(normalized.values), [
  '2026-09-04T12:34:56.000Z', '12345', '1.7.6', 5.01, 24.5, 10, 20, 30,
  0.1, 0.2, 0.3, 'device-1-20260904', 4.1, -70, "'@formula",
]);
assert.equal(vm.runInContext('SLT5006_ID_COLUMN', context), 12);
const derivedUtc = context.slt5006NormalizeRow_(
  { ts: '2026-11-06T03:34:56.123Z', serialNumber: '12345' }, 0, 'UTC'
);
const derivedOffset = context.slt5006NormalizeRow_(
  { ts: '2026-11-06T12:34:56.1234567+09:00', serialNumber: '12345' }, 0, 'UTC'
);
assert.equal(derivedUtc.id, derivedOffset.id);
assert.match(derivedUtc.id, /^[0-9a-f]{64}$/);

let writtenHeader;
const emptySheet = {
  getLastRow() { return 0; },
  getRange() {
    return {
      getDisplayValues() { return [Array(15).fill('')]; },
      setValues(values) { writtenHeader = Array.from(values[0]); },
      setFontWeight() {},
    };
  },
  setFrozenRows() {},
};
context.slt5006EnsureHeader_(emptySheet);
assert.deepEqual(writtenHeader, expectedHeader);
assert.throws(() => context.slt5006EnsureHeader_({
  getLastRow() { return 2; },
  getRange() {
    return { getDisplayValues() { return [[
      'Date', 'SerialNumber', 'Battery(V)',
    ]]; } };
  },
}), /Header mismatch/);

// Exercise real doPost with out-of-order backlog, new data, and duplicate IDs.
const storedRows = [];
let sorts = 0;
let locked = false;
const sheet = {
  getName() { return 'soil_sensor'; },
  getLastRow() { return storedRows.length + 1; },
  getRange(row, column, count, width) {
    return {
      getDisplayValues() {
        if (row === 1) return [expectedHeader];
        return storedRows.slice(row - 2, row - 2 + count).map(
          (values) => values.slice(column - 1, column - 1 + width)
        );
      },
      setValues(values) {
        assert.equal(locked, true);
        storedRows.splice(row - 2, 0, ...values.map((value) => Array.from(value)));
      },
      sort(options) {
        assert.equal(locked, true);
        assert.equal(row, 2);
        assert.equal(column, 1);
        assert.equal(count, storedRows.length);
        assert.equal(width, expectedHeader.length);
        assert.equal(options.column, 1);
        assert.equal(options.ascending, true);
        sorts += 1;
        storedRows.sort((a, b) => a[0].localeCompare(b[0]));
      },
    };
  },
};
context.PropertiesService = {
  getScriptProperties() {
    return { getProperty(key) { return key === 'INGEST_TOKEN' ? 'secret' : null; } };
  },
};
context.LockService = {
  getScriptLock() {
    return {
      tryLock() { locked = true; return true; },
      waitLock() { locked = true; },
      releaseLock() { locked = false; },
    };
  },
};
context.SpreadsheetApp = {
  getActiveSpreadsheet() { return { getSheetByName() { return sheet; } }; },
  flush() { assert.equal(locked, true); },
};
context.ContentService = {
  MimeType: { JSON: 'json' },
  createTextOutput(text) { return { setMimeType() { return JSON.parse(text); } }; },
};
const post = (rows) => context.doPost({
  postData: { contents: JSON.stringify({ token: 'secret', rows }) },
});
// Serial numbers in these fixtures are fictional.
const newer = {
  id: 'newer', ts: '2026-11-06T00:15:00Z', serialNumber: '12345678',
  temperature_c: 21.12,
};
const oldest = {
  id: 'old-error', ts: '2026-11-05T14:02:00Z', serialNumber: 0,
  error: 'sensor response timed out',
};
const middle = {
  id: 'middle', ts: '2026-11-05T14:20:00Z', serialNumber: '12345678',
  temperature_c: 24.19,
};
assert.deepEqual(post([newer]), {
  ok: true, received: 1, written: 1, duplicates: 0,
});
assert.equal(sorts, 0);
assert.equal(post([middle, oldest, newer]).written, 2);
assert.deepEqual(storedRows.map((row) => row[11]), ['old-error', 'middle', 'newer']);
assert.equal(storedRows[0][14], 'sensor response timed out');
assert.equal(storedRows[1][4], 24.19);
assert.equal(storedRows[2][4], 21.12);
assert.equal(locked, false);
storedRows.reverse();
const duplicate = post([newer]);
assert.equal(duplicate.written, 0);
assert.equal(duplicate.duplicates, 1);
assert.deepEqual(storedRows.map((row) => row[11]), ['old-error', 'middle', 'newer']);
assert.equal(sorts, 2);
const beforeInvalidBatch = JSON.stringify(storedRows);
for (const invalid of [
  { id: 'bad-date', ts: '2026-02-30T00:00:00Z', serialNumber: '12345' },
  { ...newer, id: 'blank-number', temperature_c: '   ' },
  { ...newer, id: 'boolean-serial', serialNumber: true },
]) {
  const result = post([{ ...newer, id: 'must-not-write' }, invalid]);
  assert.equal(result.ok, false);
  assert.match(result.error, /^invalid_(timestamp|number|identifier)$/);
  assert.equal(JSON.stringify(storedRows), beforeInvalidBatch);
  assert.equal(locked, false);
}
for (const rows of [null, {}, 'invalid', 1]) {
  const result = context.doPost({
    postData: { contents: JSON.stringify({
      ...newer, id: 'must-not-write', token: 'secret', rows,
    }) },
  });
  assert.equal(result.ok, false);
  assert.equal(result.error, 'invalid_body');
  assert.equal(JSON.stringify(storedRows), beforeInvalidBatch);
}
assert.deepEqual(context.doPost({
  postData: { contents: JSON.stringify({ ...newer, token: 'secret' }) },
}), { ok: true, received: 1, written: 0, duplicates: 1 });
storedRows.reverse();
context.setupSheet();
assert.deepEqual(storedRows.map((row) => row[11]), ['old-error', 'middle', 'newer']);
assert.equal(locked, false);
context.slt5006SortRows_({
  getLastRow() { return 1; },
  getRange() { assert.fail('Empty data must not be sorted'); },
});

console.log('Google Apps Script validation tests passed.');
