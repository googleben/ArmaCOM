# ArmaCOM

ArmaCOM is an Arma 3 extension which allows communication between SQF code in-game and a serial (COM) port in Windows, intended for use with an Arduino. As of now the extension does not work with BattlEye enabled.

## Installation

Simply copy `ArmaCOM_x64.dll` to your game directory (e.g. `C:\Program Files\Steam\steamapps\common\arma3\`).

## Use

Every interaction with the extension except for recieving data is handled through SQF's [callExtension](https://community.bistudio.com/wiki/callExtension) command. Functions and arguments are listed under the Commands heading. There are just a few steps to get started writing data to a serial port:

1. Find out which serial port you'll be connecting to, and use its name (e.g. `COM1`) as the `portName` argument
2. If applicable, set any or all of baud rate, parity bits, and stop bits
3. Run the `connect` command
4. Run the `write` command

Here's steps 3 and 4 in SQF writing to `COM1` to show how the syntax looks:

```
    diag_log ("ArmaCOM" callExtension ["connect", ["COM1"]]);
    diag_log ("ArmaCOM" callExtension ["write", ["COM1", "Hello, World!"]]);
```

If all goes well, the serial port should be opened and written to. Reading data takes a little more boilerplate, since ArmaCOM uses a callback to send receiveed data to SQF (as opposed to SQF having to poll the extension). The game will receive an `ExtensionCallback` event for every line (terminated by `\n`) the extension reads from a serial port, with `name = "ArmaCOM"` and `function = "data_read"` `_data` will be an array where the first element is the name of the port andd the second element is the received string (remember, Arma extensions can only communicate using strings, so you have to use [parseSimpleArray](https://community.bistudio.com/wiki/parseSimpleArray) to get the array *as* an array). Here's some example SQF for setting up an event handler:

```
    addMissionEventHandler ["ExtensionCallback", {
	    params ["_name", "_function", "_data"];
        if ((_name isEqualTo "ArmaCOM") && (_function isEqualTo "data_read")) then {
            private _resArr = parseSimpleArray _data;
            systemChat (format ["Received message from serial port %1: %2", _resArr select 0, _resArr select 1]);
        };
    }];
```

## Commands

Commands are sent to the extension via the `callExtension` operator in SQF. If the command takes arguments, it must be called in this form:
```
"ArmaCOM" callExtension [commandName, [argument(s)]]
```
If the command takes no arguments, it may optionally be called with this alternative `callExtension` syntax:
```
"ArmaCOM" callExtension commandName
```

Note that extensions are only allowed to send strings to Arma, so even if the command returns a boolean, that boolean will still be in string form. Unless otherwise noted, commands return a human-readable string with either a success message describing what was successfully done, or an error message describing what the extension failed to do and, if applicable, a Windows error message describing why it failed (e.g. "File not found").
Additionally, `portName` in the arguments of the commands must be the name of the *port* (e.g. `COM1`), not the name of the *DOS device* (e.g. `\Device\SerialPort0`).

| Command | Arguments | Description |
| --- | --- | --- |
| `listPorts` | None | Returns a list of open COM ports and their DOS device names |
| `listBaudRates` | None | Returns a list of available baud rate setting values and their indices |
| `listStopBits` | None | Returns a list of available stop bits setting values and their indicies |
| `listParities` | None | Returns a list of available parity bit setting values and their indicies |
| `listDataBits` | None | Returns a list of available data bits setting values and their indicies |
| `getBaudRate` | `[portName]` | Returns the index of the currently set baud rate |
| `getParity` | `[portName]` | Returns the index of the currently set parity bit(s) |
| `getStopBits` | `[portName]` | Returns the index of the currently set stop bit(s) |
| `getDataBits` | `[portName]` | Returns the index of the currently set data bits |
| `isUsingWriteThread` | `[portName]` | Returns `true` or `false` based on whether the extension is currently using a write thread for the given port |
| `useWriteThread` | `[portName]` | EXPERIMENTAL! Tells the extension to use a separate thread to make writes to the serial port. As of now, may not be undone without a game restart. |
| `isConnected` | [`portName`] | Returns `true` or `false` based on whether the extension is currently connected to the specified serial port. |
| `connect` | `[portName]` | Connects to the serial port at `portName`. |
| `disconnect` | `portName` | Disconnects from the given serial port |
| `write` | `[portName, data]` | Writes `data` to the serial port. Note: `data` does not have to be a string, but if it's not, you may not get what you expect |
| `getFParity` | `[portName]` | Returns `true` or `false` based on the currently set `fParity` |
| `getFOutxCtsFlow` | `[portName]` | Returns `true` or `false` based on the currently set `fOutxCtsFlow` |
| `getFOutxDsrFlow` | `[portName]` | Returns `true` or `false` based on the currently set `fOutxDsrFlow` |
| `getFDtrControl` | `[portName]` | Returns `true` or `false` based on the currently set `fDtrControl` |
| `getFDsrSensitivity` | `[portName]` | Returns `true` or `false` based on the currently set `fDsrSensitivity` |
| `getFTXContinueOnXoff` | `[portName]` | Returns `true` or `false` based on the currently set `fTXContinueOnXoff` |
| `getFOutX` | `[portName]` | Returns `true` or `false` based on the currently set `fOutX` |
| `getFInX` | `[portName]` | Returns `true` or `false` based on the currently set `fInX` |
| `getFErrorChar` | `[portName]` | Returns `true` or `false` based on the currently set `fErrorChar` |
| `getFNull` | `[portName]` | Returns `true` or `false` based on the currently set `fNull` |
| `getFRtsControl` | `[portName]` | Returns `true` or `false` based on the currently set `fRtsControl` |
| `getFAbortOnError` | `[portName]` | Returns `true` or `false` based on the currently set `fAbortOnError` |
| `getXonLim` | `[portName]` | Returns `true` or `false` based on the currently set `XonLim` |
| `getXoffLim` | `[portName]` | Returns `true` or `false` based on the currently set `XoffLim` |
| `getXonChar` | `[portName]` | Returns `true` or `false` based on the currently set `XonChar` |
| `getXoffChar` | `[portName]` | Returns `true` or `false` based on the currently set `XoffChar` |
| `getErrorChar` | `[portName]` | Returns `true` or `false` based on the currently set `ErrorChar` |
| `getEofChar` | `[portName]` | Returns `true` or `false` based on the currently set `EofChar` |
| `getEvtChar` | `[portName]` | Returns `true` or `false` based on the currently set `EvtChar` |
| `setBaudRate` | `[portName, baudRateIndex]` | Sets the baud rate. `baudRateIndex` is the index of the desired baud rate (from `listBaudRates`) |
| `setStopBits` | `[portName, stopBitsIndex]` | Sets the stop bit(s). `stopBitsIndex` is the index of the desired stop bit(s) (from `listStopBits`) |
| `setDataBits` | `[portName, dataBitsIndex]` | Sets the data bits. `dataBitsIndex` is the index of the desired baud rate (from `listDataBits`) |
| `setParity` | `[portName, parityIndex]` | Sets the parity. `parityIndex` is the index of the desired parity (from `listParities`) |
| `setFParity` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, parity checking is performed. Default: `false`. |
| `setFOutxCtsFlow` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, the Clear-To-Send signal is monitored, and when the CTS is turned off, output is suspended until the CTS is sent again. Default: `false` |
| `setFOutxDsrFlow` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, the Data-Set-Ready signal is monitored, and when the DSR is turned off, output is suspended until the DSR is sent again. Default: `false` |
| `setFDtrControl` | `[portName, val]` | `0`, `1`, or `2`. If `0`, the Data-Terminal-Ready line is disabled. If `1`, the DTR line is enabled and left on. If `2`, it is an error to adjust the DTR line. Default: `1` |
| `setFDsrSensitivity` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, the driver is sensitive to the state of the DSR signal - all recieved bytes will be ignored unless the DSR input line is high. Default: `false` |
| `setFTXContinueOnXoff` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, transmission continues after the input buffer has come within `XoffLim` bytes of being full and the driver has transmitted the `XoffChar` character to stop receiving bytes. If false, transmission does not continue until the input buffer is within `XonLim` bytes of being empty and the driver has transmitted the `XonChar` character to resume reception. Default: `false` |
| `setFOutX` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, transmission stops when the `XoffChar` character is received and starts again when the `XonChar` character is received. Default: `false` |
| `setFInX` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, the `XoffChar` character is sent when the input buffer comes within `XoffLim` bytes of being full, and the `XonChar` character is sent when the input buffer comes within `XonLim` bytes of being empty. Defualt: false |
| `setFErrorChar` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true` and `fParity` is true, bytes received with parity errors are replaced with `ErrorChar`. Defualt: false |
| `setFNull` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, null bytes are discarded when received. Defualt: false |
| `setFRtsControl` | `[portName, val]` | `0`, `1`, `2`, or `3`. If `0`, the Request-To-Send line is disabled. If `1`, the RTS line is opened and left on. If `2`, RTS handshaking is enabled - the drived raises the RTS line when the input buffer is less than one half full and lowers the RTS line when the buffer is more than three quarters full, and it is an error to adjust the RTS line. If `3`, the RTS line will be high if bytes are available for transmission, and after all buffered bytes have been sent, the RTS line will be low. Default: `1` |
| `setFAbortOnError` | `[portName, val]` | `false`, `0`, `true`, or `1`. If `true`, the driver terminates all read and write operations with an error status if an error occurs. The driver will not accept any further communications until the extension clears the error (currently not implemented). Default: `false` |
| `setXonLim` | `[portName, val]` | Integer. See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `2048` |
| `setXoffLim` | `[portName, val]` | Integer. See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `512` |
| `setXonChar` | `[portName, val]` | Character (specify as integer ASCII code, e.g. `65` for "A"). Default: `17` (device control 1) |
| `setXoffChar` | `[portName, val]` | Character (specify as integer ASCII code, e.g. `65` for "A"). Default: `19` (device control 3) |
| `setErrorChar` | `[portName, val]` | Character (specify as integer ASCII code, e.g. `65` for "A"). Default: `0` |
| `setEofChar` | `[portName, val]` | Character (specify as integer ASCII code, e.g. `65` for "A"). The character used to signal the end of data. Default: `0` |
| `setEvtChar` | `[portName, val]` | Character (specify as integer ASCII code, e.g. `65` for "A"). The character used to signal an event. Default: `0` |

## Short SQF Example

```
    addMissionEventHandler ["ExtensionCallback", {
	    params ["_name", "_function", "_data"];
        if ((_name isEqualTo "ArmaCOM") && (_function isEqualTo "data_read")) then {
            private _resArr = parseSimpleArray _data;
            systemChat (format ["Received message from serial port %1: %2", _resArr select 0, _resArr select 1]);
        };
    }];
    systemChat ("ArmaCOM" callExtension "listPorts");
    diag_log ("ArmaCOM" callExtension ["connect", ["COM1"]]);
    diag_log ("ArmaCOM" callExtension ["write", ["COM1", "Hello, World!"]]);
```

## Notes

### On connection persistence

It's important to be aware that, since the extension is (by design on Arma's part) not able to know when a mission starts or stops, it can't close its connection when a mission ends. This means that you may want to check that you're not already connected to a serial port, possible the wrong one, when setting up the extension. This also means that unless it's explicitly `disconnect`ed, the extension will keep its lock on the serial port's file handle until Arma closes.

### On threaded writes

I included the option to handle writing to the serial port on a separate thread just in case you're using low enough baud rates or long enough messages that writing causes frame hitching. It's extremely unlikely that that's the case, and if it is you should probably rethink your communication protocol, but this is here just in case. It works by keeping a linked list of messages that haven't been sent yet so that there's no chance of a double-write. When the `write` command is called and threaded writes are enabled, the extension code running in the game thread appends a new entry to the linked list and notifies any threads waiting on there to be a new entry in the list. The write thread starts at the head of the linked list and sends the data to the serial port, deleting entries once it's at the next entry, and waiting for a notification when it reaches a `nullptr` as the next entry. Note that there are several performance issues with this approach, including the fact that data to be written must be copied to the linked list, so it's really only useful if the write to the serial port is the bottleneck. It's also potentially unstable and has not been thoroughly tested.

### On hotswapping

Plugging in/unplugging a serial port while Arma is running *should* work fine, **unless the extension is currently connected to that serial port**, in which case I've got no idea what kinds of things could go wrong. 