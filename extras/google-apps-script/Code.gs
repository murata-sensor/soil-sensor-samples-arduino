/**
 * SLT5006 M5Core2 / Python logger -> Google Sheets receiver.
 *
 * Deploy this file as a Google Apps Script web app bound to a spreadsheet.
 * Required Script Property:
 *   INGEST_TOKEN  A private, random shared token.
 * Optional Script Properties:
 *   TARGET_SHEET  Destination sheet name (default: soil_sensor).
 *   TIME_ZONE     Timestamp display zone (default: Asia/Tokyo).
 *   SPREADSHEET_ID  Use an unbound spreadsheet by ID instead.
 *
 * Request body (serial number and id are fictional examples):
 *   {
 *     "token": "shared secret",
 *     "rows": [{
 *       "id": "stable-id-for-retries",       // optional
 *       "ts": "2026-08-12T10:15:30+09:00",  // optional; server time if empty
 *       "serialNumber": "12345678",          // required
 *       "firmware": "1.7.6",                 // optional
 *       "battery_v": 4.97,
 *       "core2_battery_v": 4.08,
 *       "temperature_c": 24.5,
 *       "vwc_pct": 31.2,
 *       "vwc_coco_pct": 30.8,
 *       "vwc_rock_pct": 32.1,
 *       "ec_bulk_dsm": 0.431,
 *       "ec_pore_dsm": 1.105,
 *       "ec_pore_coco_dsm": 1.087,
 *       "rssi_dbm": -61,
 *       "error": ""
 *     }]
 *   }
 *
 * `id` should be stable when a logger retries a queued measurement. If it is
 * absent, this receiver derives one from serialNumber and the normalized ts.
 * Sheet column order is defined by SLT5006_HEADER, independently of JSON order.
 */

const SLT5006_SCHEMA_VERSION = 2;
const SLT5006_DEFAULT_SHEET = 'soil_sensor';
const SLT5006_DEFAULT_TIME_ZONE = 'Asia/Tokyo';
const SLT5006_MAX_ROWS_PER_REQUEST = 100;
const SLT5006_MAX_BODY_CHARS = 100000;
const SLT5006_LOCK_TIMEOUT_MS = 30000;

const SLT5006_HEADER = Object.freeze([
  'Date',
  'SerialNumber',
  'Firmware',
  'Battery(V)',
  'Temperature(degC)',
  'VWC(%)',
  'VWC rock(%)',
  'VWC coco(%)',
  'EC bulk(dS/m)',
  'EC pore(dS/m)',
  'EC pore coco(dS/m)',
  'Id',
  'Core2 Battery(V)',
  'WiFi RSSI(dBm)',
  'Error',
]);

const SLT5006_ID_COLUMN = SLT5006_HEADER.indexOf('Id') + 1;

/** A non-secret health response. Open the /exec URL in a browser to test it. */
function doGet() {
  return slt5006Json_({
    ok: true,
    service: 'slt5006-google-sheets-receiver',
    schemaVersion: SLT5006_SCHEMA_VERSION,
  });
}

/** Receives one batch and appends only rows whose id is not already present. */
function doPost(e) {
  try {
    const contents = e && e.postData ? String(e.postData.contents || '') : '';
    if (!contents) {
      slt5006RequestError_('empty_body', 'POST body must contain JSON.');
    }
    if (contents.length > SLT5006_MAX_BODY_CHARS) {
      slt5006RequestError_('body_too_large', 'POST body is too large.');
    }

    let body;
    try {
      body = JSON.parse(contents);
    } catch (parseError) {
      slt5006RequestError_('invalid_json', 'POST body is not valid JSON.');
    }
    if (!body || typeof body !== 'object' || Array.isArray(body)) {
      slt5006RequestError_('invalid_body', 'Top-level JSON value must be an object.');
    }

    const properties = PropertiesService.getScriptProperties();
    const expectedToken = properties.getProperty('INGEST_TOKEN');
    if (!expectedToken) {
      slt5006RequestError_(
        'server_not_configured',
        'The receiver owner must set the INGEST_TOKEN Script Property.'
      );
    }
    if (!slt5006TokensEqual_(body.token, expectedToken)) {
      slt5006RequestError_('unauthorized', 'The shared token is missing or incorrect.');
    }

    // Accept a single-row object too, which is convenient for manual tests and
    // remains compatible with older Python receiver examples.
    if (body.rows !== undefined && !Array.isArray(body.rows)) {
      slt5006RequestError_('invalid_body', 'rows must be an array when provided.');
    }
    const inputRows = Array.isArray(body.rows) ? body.rows : [body];
    if (inputRows.length === 0) {
      return slt5006Json_({ ok: true, received: 0, written: 0, duplicates: 0 });
    }
    if (inputRows.length > SLT5006_MAX_ROWS_PER_REQUEST) {
      slt5006RequestError_(
        'too_many_rows',
        'A request may contain at most ' + SLT5006_MAX_ROWS_PER_REQUEST + ' rows.'
      );
    }

    const timeZone = properties.getProperty('TIME_ZONE') || SLT5006_DEFAULT_TIME_ZONE;
    const normalizedRows = inputRows.map(function(row, index) {
      return slt5006NormalizeRow_(row, index, timeZone);
    });

    const lock = LockService.getScriptLock();
    if (!lock.tryLock(SLT5006_LOCK_TIMEOUT_MS)) {
      slt5006RequestError_('busy', 'The receiver is busy; retry this batch later.');
    }

    try {
      const sheet = slt5006GetSheet_(properties);
      slt5006EnsureHeader_(sheet);

      const knownIds = slt5006ReadIds_(sheet);
      const requestIds = new Set();
      const valuesToWrite = [];
      let duplicateCount = 0;

      normalizedRows.forEach(function(row) {
        if (knownIds.has(row.id) || requestIds.has(row.id)) {
          duplicateCount += 1;
          return;
        }
        requestIds.add(row.id);
        valuesToWrite.push(row.values);
      });

      if (valuesToWrite.length > 0) {
        sheet
          .getRange(
            sheet.getLastRow() + 1,
            1,
            valuesToWrite.length,
            SLT5006_HEADER.length
          )
          .setValues(valuesToWrite);
      }
      slt5006SortRows_(sheet);
      SpreadsheetApp.flush();

      return slt5006Json_({
        ok: true,
        received: normalizedRows.length,
        written: valuesToWrite.length,
        duplicates: duplicateCount,
      });
    } finally {
      lock.releaseLock();
    }
  } catch (error) {
    const publicCode = error && error.slt5006PublicCode;
    if (!publicCode) {
      console.error(error && error.stack ? error.stack : String(error));
    }
    return slt5006Json_({
      ok: false,
      error: publicCode || 'internal_error',
      message: publicCode ? String(error.message) : 'The receiver could not store this batch.',
    });
  }
}

/**
 * Creates/verifies the destination header. Run once in the editor before the
 * first deployment; doPost also calls the same check for safe first use.
 */
function setupSheet() {
  const properties = PropertiesService.getScriptProperties();
  const lock = LockService.getScriptLock();
  lock.waitLock(SLT5006_LOCK_TIMEOUT_MS);
  try {
    const sheet = slt5006GetSheet_(properties);
    slt5006EnsureHeader_(sheet);
    slt5006SortRows_(sheet);
    SpreadsheetApp.flush();
    console.log('Ready: ' + sheet.getName());
  } finally {
    lock.releaseLock();
  }
}

function slt5006NormalizeRow_(row, index, timeZone) {
  const label = 'rows[' + index + ']';
  if (!row || typeof row !== 'object' || Array.isArray(row)) {
    slt5006RequestError_('invalid_row', label + ' must be an object.');
  }

  const serialNumber = slt5006Identifier_(row.serialNumber, label + '.serialNumber', 64);
  const timestampProvided = row.ts !== undefined && row.ts !== null && row.ts !== '';
  let timestamp;
  if (!timestampProvided) {
    if (row.id === undefined || row.id === null || row.id === '') {
      slt5006RequestError_('missing_field', label + '.id is required when .ts is empty.');
    }
    timestamp = new Date();
  } else {
    if (typeof row.ts !== 'string' || !slt5006IsIsoTimestamp_(row.ts)) {
      slt5006RequestError_(
        'invalid_timestamp',
        label + '.ts must be empty or an ISO 8601 timestamp with Z or a UTC offset.'
      );
    }
    timestamp = new Date(row.ts);
  }

  const canonicalTimestamp = timestamp.toISOString();
  const id = row.id === undefined || row.id === null || row.id === ''
    ? slt5006Sha256Hex_(serialNumber + '\n' + canonicalTimestamp)
    : slt5006Identifier_(row.id, label + '.id', 128);

  const firmware = slt5006OptionalText_(row.firmware, label + '.firmware', 32);
  const errorValue = row.error !== undefined ? row.error : row.error_flag;
  const errorText = slt5006OptionalText_(errorValue, label + '.error', 256);

  return {
    id: id,
    values: [
      Utilities.formatDate(timestamp, timeZone, 'yyyy-MM-dd HH:mm:ss'),
      serialNumber,
      firmware,
      slt5006OptionalNumber_(row.battery_v, label + '.battery_v'),
      slt5006OptionalNumber_(row.temperature_c, label + '.temperature_c'),
      slt5006OptionalNumber_(row.vwc_pct, label + '.vwc_pct'),
      slt5006OptionalNumber_(row.vwc_rock_pct, label + '.vwc_rock_pct'),
      slt5006OptionalNumber_(row.vwc_coco_pct, label + '.vwc_coco_pct'),
      slt5006OptionalNumber_(row.ec_bulk_dsm, label + '.ec_bulk_dsm'),
      slt5006OptionalNumber_(row.ec_pore_dsm, label + '.ec_pore_dsm'),
      slt5006OptionalNumber_(row.ec_pore_coco_dsm, label + '.ec_pore_coco_dsm'),
      id,
      slt5006OptionalNumber_(row.core2_battery_v, label + '.core2_battery_v'),
      slt5006OptionalNumber_(row.rssi_dbm, label + '.rssi_dbm'),
      errorText,
    ],
  };
}

function slt5006GetSheet_(properties) {
  const spreadsheetId = properties.getProperty('SPREADSHEET_ID');
  const spreadsheet = spreadsheetId
    ? SpreadsheetApp.openById(spreadsheetId)
    : SpreadsheetApp.getActiveSpreadsheet();
  if (!spreadsheet) {
    throw new Error(
      'No active spreadsheet. Bind this project to a sheet or set SPREADSHEET_ID.'
    );
  }

  const sheetName = properties.getProperty('TARGET_SHEET') || SLT5006_DEFAULT_SHEET;
  if (!sheetName || sheetName.length > 100) {
    throw new Error('TARGET_SHEET is empty or too long.');
  }
  return spreadsheet.getSheetByName(sheetName) || spreadsheet.insertSheet(sheetName);
}

function slt5006EnsureHeader_(sheet) {
  const range = sheet.getRange(1, 1, 1, SLT5006_HEADER.length);
  const existing = range.getDisplayValues()[0];
  let needsWrite = sheet.getLastRow() === 0;

  for (let i = 0; i < SLT5006_HEADER.length; i += 1) {
    if (existing[i] && existing[i] !== SLT5006_HEADER[i]) {
      throw new Error(
        'Header mismatch in column ' + (i + 1) + ': expected "' +
        SLT5006_HEADER[i] + '".'
      );
    }
    if (!existing[i]) needsWrite = true;
  }

  if (needsWrite) {
    range.setValues([SLT5006_HEADER]);
    range.setFontWeight('bold');
    sheet.setFrozenRows(1);
  }
}

function slt5006SortRows_(sheet) {
  const lastRow = sheet.getLastRow();
  if (lastRow < 3) return;
  sheet
    .getRange(2, 1, lastRow - 1, SLT5006_HEADER.length)
    .sort({ column: SLT5006_HEADER.indexOf('Date') + 1, ascending: true });
}

function slt5006ReadIds_(sheet) {
  const ids = new Set();
  const lastRow = sheet.getLastRow();
  if (lastRow < 2) return ids;

  sheet
    .getRange(2, SLT5006_ID_COLUMN, lastRow - 1, 1)
    .getDisplayValues()
    .forEach(function(value) {
      const id = String(value[0] || '').trim();
      if (id) ids.add(id);
    });
  return ids;
}

function slt5006OptionalNumber_(value, fieldName) {
  if (value === undefined || value === null || value === '') return '';
  if (typeof value === 'boolean' || (typeof value !== 'number' && typeof value !== 'string')) {
    slt5006RequestError_('invalid_number', fieldName + ' must be a finite number.');
  }
  if (typeof value === 'string' && !value.trim()) {
    slt5006RequestError_('invalid_number', fieldName + ' must be a finite number.');
  }
  const numberValue = Number(value);
  if (!isFinite(numberValue)) {
    slt5006RequestError_('invalid_number', fieldName + ' must be a finite number.');
  }
  return numberValue;
}

function slt5006Identifier_(value, fieldName, maxLength) {
  const validType = typeof value === 'string' ||
    (typeof value === 'number' && isFinite(value));
  const text = validType ? String(value).trim() : '';
  const pattern = /^[A-Za-z0-9][A-Za-z0-9._:@+\-]*$/;
  if (!text || text.length > maxLength || !pattern.test(text)) {
    slt5006RequestError_(
      'invalid_identifier',
      fieldName + ' must start with an ASCII letter or digit and contain only ' +
      'letters, digits, dot, underscore, colon, at-sign, plus, or hyphen.'
    );
  }
  return text;
}

function slt5006OptionalText_(value, fieldName, maxLength) {
  if (value === undefined || value === null || value === '') return '';
  if (typeof value === 'object') {
    slt5006RequestError_('invalid_text', fieldName + ' must be text or a number.');
  }
  const text = String(value).trim();
  if (text.length > maxLength || /[\u0000-\u001F\u007F]/.test(text)) {
    slt5006RequestError_('invalid_text', fieldName + ' is too long or has control characters.');
  }
  // A leading apostrophe makes Sheets store formula-like input as inert text.
  return /^[=+\-@]/.test(text) ? "'" + text : text;
}

function slt5006IsIsoTimestamp_(value) {
  const match = /^(\d{4})-(\d{2})-(\d{2})T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+\-]\d{2}:\d{2})$/.exec(value);
  if (!match) return false;
  const year = Number(match[1]);
  const month = Number(match[2]);
  const day = Number(match[3]);
  const leapYear = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
  const days = [31, leapYear ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  return month >= 1 && month <= 12 && day >= 1 && day <= days[month - 1] &&
    !isNaN(new Date(value).getTime());
}

function slt5006Sha256Hex_(value) {
  return Utilities.computeDigest(
    Utilities.DigestAlgorithm.SHA_256,
    value,
    Utilities.Charset.UTF_8
  ).map(function(byte) {
    return ('0' + ((byte + 256) % 256).toString(16)).slice(-2);
  }).join('');
}

/** Compares the token without returning on the first differing character. */
function slt5006TokensEqual_(actual, expected) {
  if (typeof actual !== 'string' || typeof expected !== 'string') return false;
  const length = Math.max(actual.length, expected.length);
  let difference = actual.length ^ expected.length;
  for (let i = 0; i < length; i += 1) {
    const left = i < actual.length ? actual.charCodeAt(i) : 0;
    const right = i < expected.length ? expected.charCodeAt(i) : 0;
    difference |= left ^ right;
  }
  return difference === 0;
}

function slt5006RequestError_(code, message) {
  const error = new Error(message);
  error.slt5006PublicCode = code;
  throw error;
}

function slt5006Json_(object) {
  return ContentService
    .createTextOutput(JSON.stringify(object))
    .setMimeType(ContentService.MimeType.JSON);
}
