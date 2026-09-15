# Logbook

`el1::system::logbook` provides structured per-thread logging backed by each
thread's flight recorder. Applications normally write records with `WriteLog<>`.

A process-wide console sink is enabled by default. It writes matching records to
`stderr` immediately and includes the local wall-clock time with 100 ms resolution, log verbosity,
category, thread name and formatted message. The default console filter is
`OPERATIONAL` and above. The recorder itself keeps monotonic timestamps; the
console maps them to wall-clock time when records are rendered.

Example:

```cpp
using namespace el1::system::logbook;

WriteLog<ECategory::STATE_CHANGE, EVerbosity::OPERATIONAL, U"server started on port %d">(8080);
WriteLog<ECategory::LIVENESS, EVerbosity::DEBUG, U"accepted client %s">(client_name);
```

Typical output:

```text
[10:59:42.0] OPERATIONAL/STATE_CHANGE [main] server started on port 8080
```

## Console configuration

The default console sink can be controlled at runtime:

```cpp
TLogBook::SetConsoleEnabled(false);
TLogBook::SetConsoleEnabled(true);
TLogBook::SetConsoleFilter(TLogFilter::AtLeastVerbosity(EVerbosity::DEBUG));
```

It can also be configured before the application starts:

```bash
EL1_LOG_CONSOLE=0 ./application
EL1_LOG_CONSOLE_VERBOSITY=debug ./application
```

`EL1_LOG_CONSOLE` is enabled unless its value is `0`, `false`, `off` or `no`.
`EL1_LOG_CONSOLE_VERBOSITY` accepts `trace`, `debug`, `diag`, `verbose`,
`operational` (or `info`), `terse`, and `off`/`none`. The older/general
`EL1_LOG_VERBOSITY` name is accepted as a fallback.

The console sink has two output paths:

- records matching the console filter are printed immediately through the
  pass-through path;
- when a thread commits its flight recorder, a replay marker is printed and the
  complete retained recorder is printed again in per-thread recorder order,
  including records below the live console filter.

The replay is intentionally complete. A partial replay that omitted records
already printed live would destroy the chronological flow and make the recorder
harder to read.

Console text is emitted through `io::text::terminal::term`, not directly through
libc stdio. Human-facing terminal output uses `stderr`; `stdout` remains reserved
for pipeline/data output.

## Flight-recorder lifetime

A normal thread return or an el1 `shutdown_t` termination discards that thread's
retained recorder. Normal process termination likewise discards the main-thread
recorder. This avoids replaying already visible console records during controlled
shutdown. Explicit `Commit()` still replays the retained history, and a worker
thread that terminates with an exception keeps the previous implicit replay
behavior when its recorder is destroyed.
