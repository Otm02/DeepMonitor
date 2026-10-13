import { useCallback, useEffect, useState } from "react";

interface State<T> {
  data: T | null;
  error: string | null;
  loading: boolean;
}

// Fetch-on-mount with optional polling and a manual refresh handle.
export function useFetch<T>(fn: () => Promise<T>, deps: unknown[] = [], pollMs = 0) {
  const [state, setState] = useState<State<T>>({ data: null, error: null, loading: true });

  // eslint-disable-next-line react-hooks/exhaustive-deps
  const memoFn = useCallback(fn, deps);

  const refresh = useCallback(() => {
    let cancelled = false;
    setState((s) => ({ ...s, loading: true }));
    memoFn()
      .then((data) => {
        if (!cancelled) setState({ data, error: null, loading: false });
      })
      .catch((err: Error) => {
        if (!cancelled) setState((s) => ({ ...s, error: err.message, loading: false }));
      });
    return () => {
      cancelled = true;
    };
  }, [memoFn]);

  useEffect(() => {
    const cancel = refresh();
    if (pollMs > 0) {
      const id = setInterval(refresh, pollMs);
      return () => {
        cancel();
        clearInterval(id);
      };
    }
    return cancel;
  }, [refresh, pollMs]);

  return { ...state, refresh };
}
