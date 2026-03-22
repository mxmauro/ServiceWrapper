# ServiceWrapper

Windows service wrapper for running a child console application under the Service Control Manager.

## What It Does

- Detects whether it was started by the Service Control Manager or from a console.
- Supports `install` and `uninstall` commands when run from the command line.
- Starts the configured child console application when the service starts.
- Sends `CTRL+C` to the child process on service stop.
- Waits 10 seconds before forcefully terminating the child process.
- Stops the wrapper service when the child process exits.
- Logs child startup failures to the Windows Application event log.

## Build Output

The Visual Studio project currently builds the executable as:

```text
bin\sw.exe
```

## Command Line Usage

```text
sw install --name <service-name> --target <child-exe> [options]
sw uninstall --name <service-name>
```

## Install Parameters

- `--name <service-name>`
  Service name to create in the Service Control Manager.

- `--target <child-exe>`
  Full path to the child console executable to launch when the service starts.

- `--child-args <raw arguments>`
  Raw command line passed to the child process after the executable path.

- `--display-name <display name>`
  Optional display name shown by Windows Services. Defaults to the service name.

- `--account <user account>`
  Service logon account. Defaults to `LocalSystem`.

- `--password <password>`
  Password for `--account` when required by the selected Windows account.

- `--startup manual|auto|delayed-auto`
  Service startup mode. Defaults to `manual`.

- `--restart-count <n>`
  Number of restart attempts after a crash. Defaults to `3`.

- `--restart-delay-ms <ms>`
  Delay between restart attempts in milliseconds. Defaults to `5000`.

- `--restart-reset-seconds <s>`
  Failure count reset period in seconds. Defaults to `86400`.

## Examples

Install a service with manual startup:

```powershell
sw install --name MyWorker --target "C:\Apps\Worker\worker.exe"
```

Install a delayed auto-start service with child arguments:

```powershell
sw install --name MyWorker `
  --display-name "My Worker Service" `
  --target "C:\Apps\Worker\worker.exe" `
  --child-args "--port 8080 --config C:\Apps\Worker\worker.json" `
  --startup delayed-auto
```

Install using a custom account:

```powershell
sw install --name MyWorker `
  --target "C:\Apps\Worker\worker.exe" `
  --account ".\svc-worker" `
  --password "secret"
```

Uninstall the service:

```powershell
sw uninstall --name MyWorker
```

## Runtime Behavior

When the service is installed, the wrapper registers itself as the service binary and stores the child executable path and optional child arguments in its own service command line.

At service start:

1. The wrapper starts the configured child process in a new process group.
2. The service reports `SERVICE_RUNNING` after the child starts successfully.

At service stop:

1. The wrapper sends `CTRL+C` to the child console process.
2. It waits up to 10 seconds for the child to exit cleanly.
3. If the child is still running, it terminates it.

If the child process exits on its own, the wrapper service also stops.

## Event Log

If the wrapper fails to start the child process, it writes an error entry to the Windows Application log using the event source:

```text
ServiceWrapper
```

## Notes

- This wrapper is intended for console child processes that can handle `CTRL+C`.
- The `--child-args` value is passed as raw text to `CreateProcessW`.
- Running the internal `--service` mode directly from a console is not supported; it is intended to be launched only by the Service Control Manager.
