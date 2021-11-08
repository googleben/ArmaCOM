# ArmaCOM

ArmaCOM (ArmaCOMmunicationExtensions) is an Arma 3 extension which allows communication between SQF code in-game and a variety of message transport methods, such as serial ports or a TCP connection. As of now the extension does not work with BattlEye enabled, and is only for 64-bit Windows.

## Installation

Simply copy `ArmaCOM_x64.dll` to your game directory (e.g. `C:\Program Files\Steam\steamapps\common\arma3\`).

## Quick start guide

Every interaction with the extension except for recieving data is handled through SQF's [callExtension](https://community.bistudio.com/wiki/callExtension) command. Functions and arguments are listed under the [Commands heading](#Commands). There are just a few steps to get started; here's an example using serial ports:

1. Find out which serial port you'll be connecting to, and use its name (e.g. `COM1`) as the argument to `"serial", "create"`. Save the return value (a UUID referring to the created object).
2. If applicable, set any settings such as baud rate or parity bits. See [Commands](#Commands) for more information
3. Run the `connect` command
4. Run the `write` command

Here's the steps in SQF using `COM1` to show how the syntax looks:

```
//step 1:
myPort = ("ArmaCOM" callExtension ["serial", ["create", "COM1"]]) select 0; //we have to 'select 0' because extension return arrays with the string response in index 0
//step 3:
diag_log ("ArmaCOM" callExtension ["connect", [myPort]]);
//step 4:
diag_log ("ArmaCOM" callExtension ["write", [myPort, "Hello, World!"]]);
```

If all goes well, the serial port should be opened and written to. Reading data takes a little more boilerplate, since ArmaCOM uses a callback to send receiveed data to SQF (as opposed to SQF having to poll the extension). The game will receive an `ExtensionCallback` event for every line (terminated by `\n`) the extension reads from a serial port, with `name = "ArmaCOM"` and `function = "data_read"` `_data` will be an array where the first element is the UUID and the second element is the received string (remember, Arma extensions can only communicate using strings, so you have to use [parseSimpleArray](https://community.bistudio.com/wiki/parseSimpleArray) to get the array *as* an array). Here's some example SQF for setting up an event handler:

```
    addMissionEventHandler ["ExtensionCallback", {
	    params ["_name", "_function", "_data"];
        if ((_name isEqualTo "ArmaCOM") && (_function isEqualTo "data_read")) then {
            private _resArr = parseSimpleArray _data;
            systemChat (format ["Received message from UUID %1: %2", _resArr select 0, _resArr select 1]);
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

If you're familiar with object oriented (OO) programming, it's easiest to think of the commands as being representative of an inner OO design; there's classes for each communication method which have both static and instance methods, as well as a constructor. To invoke a "static" method, the first argument to `callExtension` will be the name of the class. For example, if we wanted to list serial ports in an OO language, that might look like this: `Serial.listPorts()` which translates to this: `"ArmaCOM" callExtension ["serial", ["listPorts"]]`. To create an instance of the class (e.g. `myPort = new Serial("COM5")` in OO), we use the `create` command: `myPort = "ArmaCOM" callExtension ["serial", ["create", "COM5"]] select 0`. It's very important that we save the returned value to a variable; when you run `create`, ArmaCOM returns a UUID (if you don't know what that is, just think of it as a random ID) that represents the object you just created. If you lose that UUID, you lose access to that object, because you use the UUID as the first argument when calling "instance" methods like `write`: `myPort.write("my message")` becomes `"ArmaCOM" callExtension [myPort, ["write", "my message"]]`. If that all sounds terribly cumbersome and verbose, thank Arma and the tragedy that is SQF. If you think you have a better way to do it, *please* tell me, because I'm not a fan of this either, believe me.

Note that extensions are only allowed to send strings to Arma, so even if the command returns a boolean, that boolean will still be in string form. Unless otherwise noted, commands return a human-readable string with either a success message describing what was successfully done, or an error message describing what the extension failed to do and, if applicable, a Windows error message describing why it failed (e.g. "File not found").
Additionally, `portName` in the arguments of the commands must be the name of the *port* (e.g. `COM1`), not the name of the *DOS device* (e.g. `\Device\SerialPort0`).

## Commands shared between communication methods

The following commands are shared between all communication methods. Make sure to follow the rules set out above; for static commands, the first argument must be the communication method (or "class name"), and for instance commands, the first argument must be the instance's UUID.

### Static commands

| Command | Arguments | Description |
| --- | --- | --- |
| `create` | Varies, see specific comm methods for details | Creates an instance of this communication method and returns a UUID representing it |
| `listInstances` | None | Returns a list of all existing instances of this communication method and their UUIDs |

### Instance commands

| Command | Arguments | Description |
| --- | --- | --- |
| `connect` | None | Attempts to connect to this communication method. Must be called before reading/writing any data |
| `callbackOnChar` | `onChar: char` | Causes the read callback to be called when `onChar` is read. See [Reading data](#Reading data) for more information. |
| `callbackOnCharCode` | `onCharCode: int` | Causes the read callback to be called when the char represented by `onCharCode` is read. See [Reading data](#Reading data) for more information. |
| `callbackOnLength` | `length: int` | Causes the read callback to be called when a total of `length` characters have been read. See [Reading data](#Reading data) for more information. |
| `disconnect` | None | Attempts to disconnect from this communication method. The extension will attempt to flush the read/write queue before disconnecting |
| `isConnected` | None | Returns `true` if connected, `false` otherwise |
| `isUsingWriteThread` | None | Returns `true` or `false` based on whether the extension is currently using a write thread for the given instance |
| `useWriteThread` | None | Switches from unthreaded (synchronous) writes to threaded (asynchronous) writes |
| `write` | `message: any` | Queues `message` to be written.  Note: `message` does not have to be a string, but if it's not, you may not get what you expect |

## Reading data

Reading data is accomplished using callbacks from the extension whenever a certain condition is met. By default, data is passed back to Arma when a newline (`\n`) is read. To listen for this callback, use `addMissionEventHandler` with the first argument `"ExtensionCallback"`:

```
    addMissionEventHandler ["ExtensionCallback", {
	    params ["_name", "_function", "_data"];
        if ((_name isEqualTo "ArmaCOM") && (_function isEqualTo "data_read")) then {
            private _resArr = parseSimpleArray _data;
            systemChat (format ["Received message from UUID %1: %2", _resArr select 0, _resArr select 1]);
        };
    }];
```

The extension passes `"ArmaCOM"` as the first argument, `"data_read"` as the second, and an array containing the UUID of which instance the data was read from, followed by the data itself. To change when the extension should send data to Arma, you can use the instance commands `callbackOnChar`, `callbackOnCharCode`, and `callbackOnLength`. When in `callbackOnChar` mode (or `callbackOnCharCode`, provided in case you want to use unprintable characters), the extension will read data until it comes across the characted specified, at which point it will send all the data it has read up to (and excluding) the specified characted. In `callbackOnLength` mode, the extension will read data until it has read the specified number of characters, at which point it will send all the data it has read.

## Communication method: serial ports

### Static commands

Static commands should be called with the first argument `"serial"`, e.g. `"ArmaCOM" callExtension ["serial", ["listPorts"]]`.

| Command | Arguments | Description |
| --- | --- | --- |
| `create` | `portName: string` | Creates an instance of this communication method and returns a UUID representing it. Note that `portName` should be the name of the port (e.g. `COM1`), not its file (e.g. `\\.\\\\COM1`) |
| `listInstances` | None | Returns a list of all existing instances of this communication method and their UUIDs |
| `listPorts` | None | Returns a list of open COM ports and their DOS device names |
| `listBaudRates` | None | Returns a list of available baud rate setting values and their indices |
| `listStopBits` | None | Returns a list of available stop bits setting values and their indicies |
| `listParities` | None | Returns a list of available parity bit setting values and their indicies |
| `listDataBits` | None | Returns a list of available data bits setting values and their indicies |

### Instance commands

Instance commands should be called with the first argument of the port's UUID, e.g. `"ArmaCOM callExtension [myPort, ["connect"]]`.

| Command | Arguments | Description |
| --- | --- | --- |
| `connect` | None | Connects to the serial port at `portName`. |
| `disconnect` | `portName` | Disconnects from the given serial port |
| `write` | `[portName, data]` | Writes `data` to the serial port |
| `isConnected` | None | Returns `true` or `false` based on whether the extension is currently connected to the specified serial port |
| `callbackOnChar` | `onChar: char` | Causes the read callback to be called when `onChar` is read. See [Reading data](#Reading data) for more information. |
| `callbackOnCharCode` | `onCharCode: int` | Causes the read callback to be called when the char represented by `onCharCode` is read. See [Reading data](#Reading data) for more information. |
| `callbackOnLength` | `length: int` | Causes the read callback to be called when a total of `length` characters have been read. See [Reading data](#Reading data) for more information. |
| `isUsingWriteThread` | None | Returns `true` or `false` based on whether the extension is currently using a write thread for the given port |
| `useWriteThread` | None | EXPERIMENTAL! Tells the extension to use a separate thread to make writes to the serial port. As of now, may not be undone without a game restart. |
| `getBaudRate` | None | Returns the index of the currently set baud rate |
| `getParity` | None | Returns the index of the currently set parity bit(s) |
| `getStopBits` | None | Returns the index of the currently set stop bit(s) |
| `getDataBits` | None | Returns the index of the currently set data bits |
| `getFParity` | None | Returns `true` or `false` based on the currently set `fParity` |
| `getFOutxCtsFlow` | None | Returns `true` or `false` based on the currently set `fOutxCtsFlow` |
| `getFOutxDsrFlow` | None | Returns `true` or `false` based on the currently set `fOutxDsrFlow` |
| `getFDtrControl` | None | Returns `true` or `false` based on the currently set `fDtrControl` |
| `getFDsrSensitivity` | None | Returns `true` or `false` based on the currently set `fDsrSensitivity` |
| `getFTXContinueOnXoff` | None | Returns `true` or `false` based on the currently set `fTXContinueOnXoff` |
| `getFOutX` | None | Returns `true` or `false` based on the currently set `fOutX` |
| `getFInX` | None | Returns `true` or `false` based on the currently set `fInX` |
| `getFErrorChar` | None | Returns `true` or `false` based on the currently set `fErrorChar` |
| `getFNull` | None | Returns `true` or `false` based on the currently set `fNull` |
| `getFRtsControl` | None | Returns `true` or `false` based on the currently set `fRtsControl` |
| `getFAbortOnError` | None | Returns `true` or `false` based on the currently set `fAbortOnError` |
| `getXonLim` | None | Returns `true` or `false` based on the currently set `XonLim` |
| `getXoffLim` | None | Returns `true` or `false` based on the currently set `XoffLim` |
| `getXonChar` | None | Returns `true` or `false` based on the currently set `XonChar` |
| `getXoffChar` | None | Returns `true` or `false` based on the currently set `XoffChar` |
| `getErrorChar` | None | Returns `true` or `false` based on the currently set `ErrorChar` |
| `getEofChar` | None | Returns `true` or `false` based on the currently set `EofChar` |
| `getEvtChar` | None | Returns `true` or `false` based on the currently set `EvtChar` |
| `setBaudRate` | `baudRateIndex: int` | Sets the baud rate. `baudRateIndex` is the index of the desired baud rate (from `listBaudRates`) |
| `setStopBits` | `stopBitsIndex: int` | Sets the stop bit(s). `stopBitsIndex` is the index of the desired stop bit(s) (from `listStopBits`) |
| `setDataBits` | `dataBitsIndex: int` | Sets the data bits. `dataBitsIndex` is the index of the desired baud rate (from `listDataBits`) |
| `setParity` | `parityIndex: int` | Sets the parity. `parityIndex` is the index of the desired parity (from `listParities`) |
| `setFParity` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, parity checking is performed. Default: `false`. |
| `setFOutxCtsFlow` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, the Clear-To-Send signal is monitored, and when the CTS is turned off, output is suspended until the CTS is sent again. Default: `false` |
| `setFOutxDsrFlow` | `valL bool` | `false`, `0`, `true`, or `1`. If `true`, the Data-Set-Ready signal is monitored, and when the DSR is turned off, output is suspended until the DSR is sent again. Default: `false` |
| `setFDtrControl` | `val: bool` | `0`, `1`, or `2`. If `0`, the Data-Terminal-Ready line is disabled. If `1`, the DTR line is enabled and left on. If `2`, it is an error to adjust the DTR line. Default: `1` |
| `setFDsrSensitivity` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, the driver is sensitive to the state of the DSR signal - all recieved bytes will be ignored unless the DSR input line is high. Default: `false` |
| `setFTXContinueOnXoff` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, transmission continues after the input buffer has come within `XoffLim` bytes of being full and the driver has transmitted the `XoffChar` character to stop receiving bytes. If false, transmission does not continue until the input buffer is within `XonLim` bytes of being empty and the driver has transmitted the `XonChar` character to resume reception. Default: `false` |
| `setFOutX` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, transmission stops when the `XoffChar` character is received and starts again when the `XonChar` character is received. Default: `false` |
| `setFInX` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, the `XoffChar` character is sent when the input buffer comes within `XoffLim` bytes of being full, and the `XonChar` character is sent when the input buffer comes within `XonLim` bytes of being empty. Defualt: false |
| `setFErrorChar` | `val: bool` | `false`, `0`, `true`, or `1`. If `true` and `fParity` is true, bytes received with parity errors are replaced with `ErrorChar`. Defualt: false |
| `setFNull` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, null bytes are discarded when received. Defualt: false |
| `setFRtsControl` | `val: bool` | `0`, `1`, `2`, or `3`. If `0`, the Request-To-Send line is disabled. If `1`, the RTS line is opened and left on. If `2`, RTS handshaking is enabled - the drived raises the RTS line when the input buffer is less than one half full and lowers the RTS line when the buffer is more than three quarters full, and it is an error to adjust the RTS line. If `3`, the RTS line will be high if bytes are available for transmission, and after all buffered bytes have been sent, the RTS line will be low. Default: `1` |
| `setFAbortOnError` | `val: bool` | `false`, `0`, `true`, or `1`. If `true`, the driver terminates all read and write operations with an error status if an error occurs. The driver will not accept any further communications until the extension clears the error (currently not implemented). Default: `false` |
| `setXonLim` | `val: int` | See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `2048` |
| `setXoffLim` | `val: int` | See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `512` |
| `setXonChar` | `val: int` | `val` is an integer ASCII code, e.g. `65` for "A". Default: `17` (device control 1) |
| `setXoffChar` | `val: int` | `val` is an integer ASCII code, e.g. `65` for "A". Default: `19` (device control 3) |
| `setErrorChar` | `val: int` | `val` is an integer ASCII code, e.g. `65` for "A". Default: `0` |
| `setEofChar` | `val: int` | `val` is an integer ASCII code, e.g. `65` for "A". The character used to signal the end of data. Default: `0` |
| `setEvtChar` | `val: int` | `val` is an integer ASCII code, e.g. `65` for "A". The character used to signal an event. Default: `0` |

## Short SQF Example

```
addMissionEventHandler ["ExtensionCallback", {
    params ["_name", "_function", "_data"];
    if ((_name isEqualTo "ArmaCOM") && (_function isEqualTo "data_read")) then {
        private _resArr = parseSimpleArray _data;
        systemChat (format ["Received message from UUID %1: %2", _resArr select 0, _resArr select 1]);
    };
}];
systemChat ("ArmaCOM" callExtension "listPorts");
myPort = ("ArmaCOM" callExtension ["serial", ["create", "COM1"]]) select 0;
diag_log ("ArmaCOM" callExtension ["connect", [myPort]]);
diag_log ("ArmaCOM" callExtension ["write", [myPort, "Hello, World!"]]);
```

## Notes

### On connection persistence

It's important to be aware that, since the extension is (by design on Arma's part) not able to know when a mission starts or stops, it can't close its connection when a mission ends. This means that you may want to check that you're not already connected to a serial port, possible the wrong one, when setting up the extension. This also means that unless it's explicitly `disconnect`ed, the extension will keep its lock on the serial port's file handle until Arma closes.

### On threaded writes

I included the option to handle writing to the serial port on a separate thread just in case you're using low enough baud rates or long enough messages that writing causes frame hitching. It's extremely unlikely that that's the case, and if it is you should probably rethink your communication protocol, but this is here just in case. It works by keeping a linked list of messages that haven't been sent yet so that there's no chance of a double-write. When the `write` command is called and threaded writes are enabled, the extension code running in the game thread appends a new entry to the linked list and notifies any threads waiting on there to be a new entry in the list. The write thread starts at the head of the linked list and sends the data to the serial port, deleting entries once it's at the next entry, and waiting for a notification when it reaches a `nullptr` as the next entry. Note that there are several performance issues with this approach, including the fact that data to be written must be copied to the linked list, so it's really only useful if the write to the serial port is the bottleneck. It's also potentially unstable and has not been thoroughly tested.

### On hotswapping

Plugging in/unplugging a serial port while Arma is running *should* work fine, **unless the extension is currently connected to that serial port**, in which case I've got no idea what kinds of things could go wrong. 