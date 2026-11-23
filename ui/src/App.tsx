import { useEffect } from 'react';
import { Shell } from './app/Shell';
import { useUi } from './state/store';

export default function App() {
  // `?sample` opens the bundled sample model at start-up (handy for demos and screenshots).
  useEffect(() => { if (new URLSearchParams(window.location.search).has('sample')) void useUi.getState().loadSample(); }, []);
  return <Shell />;
}
