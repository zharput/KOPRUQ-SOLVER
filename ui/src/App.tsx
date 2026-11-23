import { useEffect } from 'react';
import { Shell } from './app/Shell';
import { isTauri, tauriHost } from './platform/TauriHost';
import * as runner from './state/runner';
import { connectHost } from './state/runner';
import { useUi } from './state/store';

// Development builds expose the store and runner for end-to-end scripts (not present in production builds).
if (import.meta.env.DEV) (window as unknown as { __kopruq?: unknown }).__kopruq = { useUi, runner };

export default function App() {
  // `?sample` opens the bundled sample model at start-up (handy for demos and screenshots).
  useEffect(() => { if (isTauri()) useUi.getState().setHost(tauriHost); void connectHost(); }, []);
  useEffect(() => { if (new URLSearchParams(window.location.search).has('sample')) void useUi.getState().loadSample(); }, []);
  return <Shell />;
}
