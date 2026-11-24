// CSV and XLSX export of a result table. Pure functions (no DOM): the caller supplies the metadata, the headers and the rows already
// converted to the display unit. XLSX is written directly (OOXML parts zipped with fflate); only inline strings and numbers are used.
import { strToU8, zipSync } from 'fflate';

export type Cell = number | string;
export interface ExportDoc { meta: [string, string][]; headers: string[]; rows: Cell[][] }

const csvCell = (v: Cell) => { const s = typeof v === 'number' ? (Number.isFinite(v) ? String(v) : '') : v; return /[",\r\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s; };
/** UTF-8 CSV with a byte-order mark (spreadsheets then read non-ASCII text correctly); metadata block, blank line, table. */
export function toCsv(d: ExportDoc): string {
  const lines = [...d.meta.map(([k, v]) => [k, v].map(csvCell).join(',')), '', d.headers.map(csvCell).join(','), ...d.rows.map((r) => r.map(csvCell).join(','))];
  return '﻿' + lines.join('\r\n') + '\r\n';
}

const esc = (s: string) => s.replace(/[<>&"]/g, (c) => ({ '<': '&lt;', '>': '&gt;', '&': '&amp;', '"': '&quot;' })[c]!).replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F]/g, '');
export const colName = (i: number) => { let s = ''; for (let n = i + 1; n > 0; n = Math.floor((n - 1) / 26)) s = String.fromCharCode(65 + ((n - 1) % 26)) + s; return s; };
function sheetXml(d: ExportDoc): string {
  const all: Cell[][] = [...d.meta.map(([k, v]) => [k, v]), [], d.headers, ...d.rows];
  const rows = all.map((r, ri) => `<row r="${ri + 1}">${r.map((v, ci) => {
    const ref = `${colName(ci)}${ri + 1}`;
    if (typeof v === 'number') return Number.isFinite(v) ? `<c r="${ref}"><v>${v}</v></c>` : '';
    return v === '' ? '' : `<c r="${ref}" t="inlineStr"><is><t xml:space="preserve">${esc(v)}</t></is></c>`;
  }).join('')}</row>`).join('');
  return `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>${rows}</sheetData></worksheet>`;
}
export function toXlsx(d: ExportDoc, sheetName = 'Results'): Uint8Array {
  const name = esc(sheetName.replace(/[\/?*[\]:]/g, ' ').slice(0, 31) || 'Results');
  const xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>';
  return zipSync({
    '[Content_Types].xml': strToU8(`${xml}<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>`),
    '_rels/.rels': strToU8(`${xml}<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>`),
    'xl/workbook.xml': strToU8(`${xml}<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="${name}" sheetId="1" r:id="rId1"/></sheets></workbook>`),
    'xl/_rels/workbook.xml.rels': strToU8(`${xml}<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/></Relationships>`),
    'xl/worksheets/sheet1.xml': strToU8(sheetXml(d)),
  });
}

/** Browser download of a generated file (no server, no file-system access). */
export function download(fileName: string, data: string | Uint8Array, mime: string): void {
  const part: BlobPart = typeof data === 'string' ? data : (data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength) as ArrayBuffer);
  const url = URL.createObjectURL(new Blob([part], { type: mime })); const a = document.createElement('a');
  a.href = url; a.download = fileName; document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 1000);
}
export const safeFileName = (s: string) => s.replace(/[^\w.-]+/g, '_').replace(/^_+|_+$/g, '') || 'results';
