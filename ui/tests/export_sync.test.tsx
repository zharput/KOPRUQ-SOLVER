import { readFileSync } from 'node:fs';
import { act, fireEvent, render, screen, within } from '@testing-library/react';
import { strFromU8, unzipSync } from 'fflate';
import { vi } from 'vitest';
import App from '../src/App';
import { colName, safeFileName, toCsv, toXlsx } from '../src/results/exportTable';
import { parseResults } from '../src/results/parse';
import { defaultResultView, useUi } from '../src/state/store';

vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const viewText = readFileSync('tests/data/cantilever_view.json', 'utf-8'), resText = readFileSync('tests/data/cantilever_results.json', 'utf-8');

describe('export documents (pure)', () => {
  const doc = { meta: [['Model Name', 'm, "x".json'], ['Unit System', 'kN.m']] as [string, string][], headers: ['ID', 'My (kN·m)'], rows: [[1, -4000.5], [2, NaN], [3, 'a,b']] };
  it('CSV: BOM, metadata block, quoting, NaN left empty, CRLF', () => {
    const t = toCsv(doc); expect(t.charCodeAt(0)).toBe(0xfeff);
    expect(t.slice(1).split('\r\n')).toEqual(['Model Name,"m, ""x"".json"', 'Unit System,kN.m', '', 'ID,My (kN·m)', '1,-4000.5', '2,', '3,"a,b"', '']);
  });
  it('XLSX: valid OOXML package with the metadata, headers and exact numbers', () => {
    const z = unzipSync(toXlsx(doc, 'My: sheet/1')); expect(Object.keys(z).sort()).toEqual(['[Content_Types].xml', '_rels/.rels', 'xl/_rels/workbook.xml.rels', 'xl/workbook.xml', 'xl/worksheets/sheet1.xml']);
    const sheet = strFromU8(z['xl/worksheets/sheet1.xml']); expect(sheet).toContain('<c r="A1" t="inlineStr"><is><t xml:space="preserve">Model Name</t></is></c>');
    expect(sheet).toContain('m, &quot;x&quot;.json'); expect(sheet).toContain('<c r="B5"><v>-4000.5</v></c>'); expect(sheet).not.toContain('NaN');
    expect(strFromU8(z['xl/workbook.xml'])).toContain('name="My  sheet 1"');   // characters Excel forbids in sheet names are replaced
  });
  it('helpers', () => { expect([0, 25, 26, 27, 701, 702].map(colName)).toEqual(['A', 'Z', 'AA', 'AB', 'ZZ', 'AAA']); expect(safeFileName('a b/ç.json')).toBe('a_b_.json'); });
});

describe('export and synchronisation in the Tables workspace', () => {
  let blobs: Blob[] = []; let names: string[] = [];
  beforeEach(() => {
    blobs = []; names = [];
    Object.assign(URL, { createObjectURL: (b: Blob) => { blobs.push(b); return 'blob:x'; }, revokeObjectURL: () => {} });
    vi.spyOn(HTMLAnchorElement.prototype, 'click').mockImplementation(function (this: HTMLAnchorElement) { names.push(this.download); });
    useUi.setState({ view: null, results: null, stations: null, resultView: defaultResultView(), selection: null, tableUi: {}, unitSystem: 'kN.m', centerTab: 'tables' });
    act(() => useUi.getState().loadText(viewText, 'cantilever.json')); act(() => useUi.setState({ results: parseResults(resText), tableTab: 'Element Forces', centerTab: 'tables' }));
  });
  const text = (b: Blob) => new Promise<string>((res) => { const r = new FileReader(); r.onload = () => res(String(r.result)); r.readAsText(b); });
  it('CSV export carries the metadata and the values in the selected unit', async () => {
    act(() => useUi.setState({ unitSystem: 'Ton.m' })); render(<App />);
    fireEvent.click(screen.getByRole('button', { name: 'CSV' })); expect(names[0]).toBe('cantilever_Element_Forces_tip_Ton.m.csv');
    const t = await text(blobs[0]); const lines = t.replace(/^﻿/, '').split('\r\n');
    expect(lines.slice(0, 5)).toEqual(['Model Name,cantilever.json', 'Analysis Case,Linear Static', 'Load Case / Combination,tip', 'Result Type,Element Forces', 'Unit System,Ton.m']);
    expect(lines[5]).toBe('Rows,1'); expect(lines[6]).toBe(''); const h = lines[7].split(','), row = lines[8].split(','); expect(h).toContain('End-I My (tf·m)'); expect(Number(row[h.indexOf('End-I My (tf·m)')])).toBeCloseTo(-4000 / 9.80665, 9); expect(row[0]).toBe('7');
  });
  it('Excel export is an xlsx zip with the same table', async () => {
    render(<App />); fireEvent.click(screen.getByRole('button', { name: 'Excel' })); expect(names[0]).toMatch(/\.xlsx$/);
    const z = unzipSync(new Uint8Array(await new Promise<ArrayBuffer>((res) => { const r = new FileReader(); r.onload = () => res(r.result as ArrayBuffer); r.readAsArrayBuffer(blobs[0]); })));
    expect(strFromU8(z['xl/worksheets/sheet1.xml'])).toContain('<v>-4000</v>');
  });
  it('export uses the selected rows only, when there are some', async () => {
    act(() => useUi.setState({ view: { ...useUi.getState().view!, model: { ...useUi.getState().view!.model } } })); render(<App />);
    fireEvent.click(screen.getByLabelText('Select 7')); fireEvent.click(screen.getByRole('button', { name: 'CSV' })); const t = await text(blobs[0]); expect(t).toContain('Rows,1 (selected)');
  });
  it('clicking a table row selects the element for the viewer, explorer and properties; a selection made elsewhere marks the row', () => {
    render(<App />);
    fireEvent.click(within(screen.getByTestId('tbl-forces')).getByText('Beam')); expect(useUi.getState().selection).toEqual({ kind: 'element', id: 7 });
    expect(document.querySelector('tr.active')?.getAttribute('data-row-id')).toBe('7');
    act(() => useUi.getState().select(null)); expect(document.querySelector('tr.active')).toBeNull();
    act(() => useUi.getState().select({ kind: 'element', id: 7 })); expect(document.querySelector('tr.active')?.getAttribute('data-row-id')).toBe('7');
    act(() => useUi.getState().select({ kind: 'node', id: 2 })); expect(document.querySelector('tr.active')).toBeNull();   // a node is not a row of the member table
    expect(screen.getByTestId('hidden-selection')).toBeTruthy();
  });
  it('node tables follow node selections; a row hidden by search is reported, not faked', () => {
    act(() => useUi.setState({ tableTab: 'Displacements' })); render(<App />);
    act(() => useUi.getState().select({ kind: 'node', id: 2 })); expect(document.querySelector('tr.active')?.getAttribute('data-row-id')).toBe('2');
    fireEvent.change(screen.getByLabelText('Search by ID'), { target: { value: '1' } }); expect(document.querySelector('tr.active')).toBeNull(); expect(screen.getByTestId('hidden-selection').textContent).toMatch(/hidden by search/);
  });
});
