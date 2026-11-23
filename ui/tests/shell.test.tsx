import { readFileSync } from 'node:fs';
import { act, fireEvent, render, screen, within } from '@testing-library/react';
import { vi } from 'vitest';
import App from '../src/App';
import { useUi } from '../src/state/store';

// jsdom has no WebGL: the vtk.js viewer is replaced by a stub here (its geometry is tested in geometry.test.ts)
vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const sample = readFileSync('tests/data/m100_view.json', 'utf-8');
const load = (t: string, n: string) => act(() => { useUi.getState().loadText(t, n); });
const menu = () => within(screen.getByRole('navigation', { name: 'Main menu' }));
beforeEach(() => useUi.setState({ view: null, fileName: '', loadError: null, selection: null, search: '', bottomTab: 'Results' }));

describe('GUI shell', () => {
  it('shows the required top-level menus', () => {
    render(<App />);
    for (const m of ['File', 'Model', 'Properties', 'Boundary', 'Loads', 'Analysis', 'Results', 'Verification', 'Tools', 'Help']) expect(menu().getByRole('button', { name: m })).toBeTruthy();
  });
  it('analysis and verification commands stay disabled; only opening a model file is enabled', () => {
    render(<App />);
    fireEvent.click(menu().getByRole('button', { name: 'Analysis' }));
    for (const item of screen.getAllByRole('menuitem')) expect((item as HTMLButtonElement).disabled).toBe(true);
    fireEvent.click(menu().getByRole('button', { name: 'File' }));
    expect((screen.getByRole('menuitem', { name: /Open Model View JSON/ }) as HTMLButtonElement).disabled).toBe(false);
    expect((screen.getByRole('button', { name: 'Run Analysis' }) as HTMLButtonElement).disabled).toBe(true);
  });
  it('without a model: empty explorer, view tools disabled, no data pretended', () => {
    render(<App />);
    expect(screen.getAllByText(/No model loaded/).length).toBeGreaterThan(0);
    for (const n of ['Fit', 'Iso', 'Front', 'Top', 'Side', 'Solid', 'Nodes']) expect((screen.getByRole('button', { name: n }) as HTMLButtonElement).disabled).toBe(true);
    expect(screen.getByRole('toolbar').textContent).not.toMatch(/MOCK/);
  });
  it('loading a real model view fills explorer, properties and status bar', () => {
    render(<App />);
    load(sample, 'm100_view.json');
    expect(screen.getByText('Nodes', { selector: '.trow span' }).closest('.trow')!.textContent).toContain('106');
    expect(screen.getByText('Beam', { selector: '.trow span' }).closest('.trow')!.textContent).toContain('94');
    expect(screen.getByText('Shell').closest('.trow')!.classList.contains('dim')).toBe(true);
    expect(screen.getByText(/Elements: 94/)).toBeTruthy();
    fireEvent.click(screen.getByText('Beam', { selector: '.trow span' }));
    fireEvent.click(screen.getByText(/^E1 /));
    expect(useUi.getState().selection).toEqual({ kind: 'element', id: 1 });
    expect(screen.getByText('Length (m)')).toBeTruthy();
    expect((screen.getByRole('button', { name: 'Fit' }) as HTMLButtonElement).disabled).toBe(false);
  });
  it('rejects a wrong file with a readable error and keeps the previous model', () => {
    render(<App />);
    load(sample, 'a.json');
    load('{"schema":"kopruq-model","schema_version":1}', 'b.json');
    expect(screen.getByRole('alert').textContent).toMatch(/Not a kopruq-model-view/);
    expect(useUi.getState().fileName).toBe('a.json');
    load('not json', 'c.json');
    expect(screen.getByRole('alert').textContent).toMatch(/Not valid JSON/);
  });
  it('switches result tabs; results and verification stay honest placeholders', () => {
    render(<App />);
    fireEvent.click(screen.getByRole('tab', { name: 'Reactions' }));
    expect(screen.getByTestId('results-empty').textContent).toContain('no results');
    fireEvent.click(screen.getByRole('tab', { name: 'Verification' }));
    expect(screen.getByTestId('verification-empty').textContent).toContain('not implemented');
  });
  it('toggles are part of the shared state', () => {
    render(<App />);
    load(sample, 'a.json');
    fireEvent.click(screen.getByRole('button', { name: 'Solid' }));
    expect(useUi.getState().show.solid).toBe(false);
    fireEvent.click(screen.getByRole('button', { name: 'Iso' }));
    expect(useUi.getState().camera.preset).toBe('iso');
  });
});
