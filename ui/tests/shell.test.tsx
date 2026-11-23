import { fireEvent, render, screen, within } from '@testing-library/react';
import App from '../src/App';
import { useUi } from '../src/state/store';

beforeEach(() => useUi.setState({ selection: null, search: '', bottomTab: 'Results' }));

describe('GUI foundation shell', () => {
  it('shows the required top-level menus', () => {
    render(<App />);
    for (const m of ['File', 'Model', 'Properties', 'Boundary', 'Loads', 'Analysis', 'Results', 'Verification', 'Tools', 'Help'])
      expect(within(screen.getByRole('navigation', { name: 'Main menu' })).getByRole('button', { name: m })).toBeTruthy();
  });
  it('never offers an unimplemented command as enabled', () => {
    render(<App />);
    fireEvent.click(within(screen.getByRole('navigation', { name: 'Main menu' })).getByRole('button', { name: 'Analysis' }));
    for (const item of screen.getAllByRole('menuitem')) expect((item as HTMLButtonElement).disabled).toBe(true);
    expect((screen.getByRole('button', { name: 'Run Analysis' }) as HTMLButtonElement).disabled).toBe(true);
    for (const b of screen.getByRole('toolbar').querySelectorAll('button')) expect((b as HTMLButtonElement).disabled).toBe(true);
  });
  it('marks placeholder data as MOCK and unsupported element types as empty', () => {
    render(<App />);
    expect(screen.getAllByText(/MOCK/).length).toBeGreaterThan(0);
    expect(screen.getByText('Shell').closest('.trow')!.classList.contains('dim')).toBe(true);
    expect(screen.getByText('Solid').closest('.trow')!.classList.contains('dim')).toBe(true);
  });
  it('synchronises the selection with the properties inspector', () => {
    render(<App />);
    fireEvent.click(screen.getByText('Nodes', { selector: '.trow span' }));
    fireEvent.click(screen.getByText('N3'));
    expect(screen.getByText('20.000')).toBeTruthy(); // X of mock node 3
  });
  it('switches result tabs and shows the verification placeholder, no fake results', () => {
    render(<App />);
    fireEvent.click(screen.getByRole('tab', { name: 'Reactions' }));
    expect(screen.getByTestId('results-empty').textContent).toContain('no results');
    fireEvent.click(screen.getByRole('tab', { name: 'Verification' }));
    expect(screen.getByTestId('verification-empty').textContent).toContain('not implemented');
  });
  it('filters the explorer with the search box', () => {
    render(<App />);
    fireEvent.click(screen.getByText('Beam', { selector: '.trow span' }));
    expect(screen.getAllByText(/^E\d+ /).length).toBe(10);
    fireEvent.change(screen.getByLabelText('Search model'), { target: { value: 'GIRDER-2' } });
    expect(screen.getAllByText(/^E\d+ /).length).toBe(5);
  });
});
