import { useCallback, useRef, useState } from 'react';

/** Minimal pointer-drag splitter: returns the size (px) and a pointer-down handler for the handle. */
export function useSplit(initial: number, min: number, max: number, axis: 'x' | 'y', invert = false) {
  const [size, setSize] = useState(initial);
  const start = useRef<{ p: number; s: number } | null>(null);
  const onPointerDown = useCallback((e: React.PointerEvent) => {
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
    start.current = { p: axis === 'x' ? e.clientX : e.clientY, s: size };
    const move = (ev: PointerEvent) => {
      if (!start.current) return;
      const d = (axis === 'x' ? ev.clientX : ev.clientY) - start.current.p;
      setSize(Math.min(max, Math.max(min, start.current.s + (invert ? -d : d))));
    };
    const up = () => { start.current = null; window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', up); };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }, [axis, invert, max, min, size]);
  return { size, onPointerDown };
}
