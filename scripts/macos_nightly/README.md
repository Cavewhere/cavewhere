# Nightly Flaky Test Runner

Automated overnight test runner that clones CaveWhere fresh, builds in Release mode, and runs both test suites multiple times to detect flaky tests.

## What it does

1. Cleans up runs older than 7 days
2. Fresh `git clone --recursive` of `origin/dev`
3. Conan install with system Qt 6.11, Release build
4. Builds `cavewhere-test` and `cavewhere-qml-test`
5. Runs both suites 16 times in parallel (4 cpp + 2 qml concurrent)
6. Each test run has a 22-minute timeout (kills hung tests)
7. Generates `summary.txt` with per-run pass/fail and failing test frequency
8. Sends a macOS notification (click to open logs directory)

Logs and builds go to `/var/tmp/cavewhere-nightly/<timestamp>/`.

## Prerequisites

```bash
brew install terminal-notifier   # clickable macOS notifications
brew install coreutils           # gtimeout for test timeouts
```

Conan 2, CMake, and Qt 6.11 (`~/Qt/6.11.1/macos`) must be installed.

## Running manually

```bash
./scripts/macos_nightly/nightly-test.sh
```

Override defaults with environment variables:

```bash
NUM_RUNS=4 CPP_PARALLEL=2 QML_PARALLEL=1 ./scripts/macos_nightly/nightly-test.sh
```

## Scheduling with launchd (macOS)

The launchd job must not point directly into a dev checkout — branch switches
or uncommitted edits in that checkout will silently change (or break) what
runs at 1 AM. Instead it points at `nightly-launcher.sh`, which lives at a
stable path outside any repo and fetches the latest `nightly-test.sh` from
`origin/dev` on GitHub each run.

### 1. Install the launcher

Copy the launcher to a stable location outside any git checkout:

```bash
mkdir -p ~/.local/bin
cp scripts/macos_nightly/nightly-launcher.sh ~/.local/bin/
chmod +x ~/.local/bin/nightly-launcher.sh
```

Updating the launcher itself is rare (it only changes if you modify how the
nightly is fetched). When you do change it, re-run the `cp` above.

### 2. Install the plist

```bash
cp scripts/macos_nightly/com.cavewhere.nightly-test.plist ~/Library/LaunchAgents/
```

**Before loading**, edit the plist to match your system:
- Update the launcher path in `ProgramArguments` if your home directory is not `/Users/cave`
- Update the `PATH` in `EnvironmentVariables` to include the directory containing `conan` (find it with `which conan`)
- Update `HOME` to your home directory

### 3. Grant permissions

macOS requires Full Disk Access for scheduled scripts. Go to:

**System Settings > Privacy & Security > Full Disk Access**

Add `/bin/zsh` (press `Cmd+Shift+G` to type the path).

### 4. Load and test

```bash
# Load the agent
launchctl load ~/Library/LaunchAgents/com.cavewhere.nightly-test.plist

# Test it immediately
launchctl start com.cavewhere.nightly-test

# Check the log
tail -f /var/tmp/cavewhere-nightly/launchd.log
```

The agent runs at 1:00 AM daily. It persists across reboots.

### Uninstalling

```bash
launchctl unload ~/Library/LaunchAgents/com.cavewhere.nightly-test.plist
rm ~/Library/LaunchAgents/com.cavewhere.nightly-test.plist
rm ~/.local/bin/nightly-launcher.sh
```

## How a nightly run flows

1. launchd fires `~/.local/bin/nightly-launcher.sh` at 1 AM (deployed copy of `scripts/macos_nightly/nightly-launcher.sh`).
2. The launcher curls the latest `scripts/macos_nightly/nightly-test.sh` from `origin/dev` on GitHub into `/var/tmp/cavewhere-nightly/nightly-test.fetched.sh` and `exec`s it.
3. `nightly-test.sh` does its own `git clone --recursive` of `origin/dev` into `/var/tmp/cavewhere-nightly/<timestamp>/cavewhere`, builds it, and runs the test suites.

This means changes to `nightly-test.sh` on `dev` take effect on the next run with no further action. Changes to `nightly-launcher.sh` itself require re-running step 1 in the setup section.

## Configuration reference

| Variable | Default | Description |
|----------|---------|-------------|
| `NUM_RUNS` | 16 | Number of test iterations per suite |
| `CPP_PARALLEL` | 4 | Concurrent `cavewhere-test` instances |
| `QML_PARALLEL` | 2 | Concurrent `cavewhere-qml-test` instances (uses ~8GB RAM each) |
| `TEST_TIMEOUT` | 22m | Max time per individual test run before kill |
| `KEEP_DAYS` | 90 | Delete run directories older than this |
