# Global Commands

| Name | Arguments | Return Value | SQF Example | Comments |
| ---  | ---       | ---          | ---         | ---      |
| `destroy` | `instance`: `UUID` | Success or failure message | `"ArmaCOM" callExtension ["destroy", [instance]];` | Destroys an instance of a communication method if it is not currently connected |

# Communication Method: Serial

This communication method is an interface for serial/COM ports. Serial ports are different from most other (read: modern) methods of communicating with computers. The main reason this is included is for use with small board computers like Arduinos and Raspberry Pis, but may be useful for other, typically legacy, hardware. Users of legacy or uncommon hardware should beware that serial communication is finnicky even in the most favorable conditions, and this has only been tested thus far with modern equipment. An important note is that due to the limitations of the technology, only **one** instance of this communication method may exist per COM port. If you attempt to create a second instance for a serial port that already has an instance, the extension will simply return the existing instance. This functionality may be useful since you can regain a port's UUID if you lost it by attempting to create a new instance, but is more likely to be a source of headaches. The extension exposes a large number of low-level parameters through getters and setters in the hope that it will be useful to users with nonstandard or legacy hardware. If you're using modern "happy" hardware like an Arduino, or don't know what any of it means, there's a very good chance the defaults will work fine. The only parameters most users will need to touch are baud rate, whether writes are threaded, and when the extension sends data back to Arma. Threaded writes are off by default just because I haven't tested their stability thoroughly, but you should turn threaded writes on unless you have issues. Despite modern hardware being incredibly fast, serial communications defaults to the snail-like 9600 baud, and even at high baud rates the overhead of writes puts a lower bound of rougly **20ms for any write operation on my machine, even using emulated serial ports**. Of course threaded writes don't make that latency magically disappear, but they can shunt it off to one of the 3+ cores Arma never uses and let Arma get on with whatever stuff it needs to do in the meantime. Because Bohemia is working with a very old codebase that was absolutely not written with maintainability and performance in mind, Arma is single-threaded. SQF manages to fake the ability to run multiple scripts at once by using a basic scheduler that begins execution of a "thread", runs it until it passes some predefined allotment of time, and then moves on to the next "thread" if it isn't terribly behind and needs to yield its CPU time to other things. This tends to work well enough with how fast modern CPUs are, but a very important limitation is that the SQF VM **cannot make an extension yield its time**. When `callExtension` is run, Arma has to stop pretty much everything it's doing until the extension returns, which makes waiting around for a write to finish for 20+ms a very bad idea. If you're lucky enough for your game to be running at 60 FPS, that means a new frame comes through every 16.6ms - that's right, a single write to a serial port takes longer than running physics simulations and rendering an entire frame by almost 25%. So please, turn on threaded writes, and if that's not good enough, consider something other than the terribly dated technology that is serial communications.

## Static Commands

These commands must be called with the format `"ArmaCOM" callExtension ["name of the communication method", ["command name", [arg1, arg2, ...]]`.
Those familiar with object-oriented programming should think of them as static methods on the communication method's class.

| Name | Arguments | Return Value | SQF Example | Comments |
| ---  | ---       | ---          | ---         | ---      |
| `create` | `portName`: `string` | A success or failure message | `"ArmaCOM" callExtension ["Serial", ["create", portName]];` | Creates an instance of this communication method and returns a UUID representing it. This command does not attempt to connect to the given port; the `connect` command must be called separately. Note that `portName` should be the name of the port (e.g. `COM1`), not its file (e.g. `\\.\\\\COM1`). |
| `listBaudRates` | None | A list of currently available baud rates in the format [[index:int , baudRate: int], ...] | `"ArmaCOM" callExtension ["Serial", ["listBaudRates"]];` | This command simply enumerates the baud rates known to the extension; listed baud rates may not be compatible with the specific serial device. Remember to use `parseSimpleArray` since extensions can only communicate using strings. |
| `listDataBits` | None | A list of currently available data bits in the format [[index: int, dataBits: string], ...] | `"ArmaCOM" callExtension ["Serial", ["listDataBits"]];` | This command simply enumerates the data bits known to the extension; listed data bits may not be compatible with the specific serial device. Remember to use `parseSimpleArray` since extensions can only communicate using strings. |
| `listInstances` | None | A list of instances of this communication method in the format [[UUID: string, portName: string], ...] | `"ArmaCOM" callExtension ["Serial", ["listInstances"]];` | Lists extant instances of the serial communication method and their UUIDs so users have a hope of recovering their instance if they lose the UUID. Remember to use `parseSimpleArray` since extensions can only communicate using strings. |
| `listParities` | None | A list of currently available parities in the format [[index: int, parity: string], ...] | `"ArmaCOM" callExtension ["Serial", ["listParities"]];` | This command simply enumerates the parities known to the extension; listed parities may not be compatible with the specific serial device. Remember to use `parseSimpleArray` since extensions can only communicate using strings. |
| `listPorts` | None | A list of currently available COM (serial) ports in the format [[portName: string, portDriver: string], ...] | `"ArmaCOM" callExtension ["Serial", ["listPorts"]];` | Queries the currently available DOS COM ports using the Windows function `QueryDosDevice` from COM0 to COM254 inclusive. This method is marginally more computationally expensive than checking the registry, but the registry may be out of date, so this command is guaranteed to return the most up-to-date data. Remember to use `parseSimpleArray` since extensions can only communicate using strings. |
| `listStopBits` | None | A list of currently available stop bits in the format [[index: int, stopBits: string], ...] | `"ArmaCOM" callExtension ["Serial", ["listStopBits"]];` | This command simply enumerates the stop bits known to the extension; listed stop bits may not be compatible with the specific serial device. Remember to use `parseSimpleArray` since extensions can only communicate using strings. |
## Instance Commands

These commands must be called with the format `"ArmaCOM" callExtension [myInstanceUUID, ["command name", [arg1, arg2, ...]]`.
Those familiar with object-oriented programming should think of them as instance methods on your instance of the communication method's class.

| Name | Arguments | Return Value | SQF Example | Comments |
| ---  | ---       | ---          | ---         | ---      |
| `callbackOnChar` | `charToLookFor`: `char` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["callbackOnChar", charToLookFor]];` | Makes the extension send data read from this port back to Arma when `charToLookFor`, specified as a `char`, is read. When the character is read, all data up to and **excluding** that character is sent back to Arma via the callback. |
| `callbackOnCharCode` | `charCodeToLookFor`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["callbackOnCharCode", charCodeToLookFor]];` | Makes the extension send data read from this port back to Arma when the character described by `charCodeToLookFor`, specified as an ASCII char code e.g. `65` for "A", is read. When the character is read, all data read since the last callback, up to and **excluding** that character, is sent back to Arma via the callback. |
| `callbackOnLength` | `lengthToStopAt`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["callbackOnLength", lengthToStopAt]];` | Makes the extension send data read from this port back to Arma when the total amount of data read reaches `lengthToStopAt` characters long. When the target amount of data is read, all data read since the last callback is sent back to Arma via the callback. |
| `connect` | None | A success or failure message | `"ArmaCOM" callExtension [myInstanceUUID, ["connect"]];` | Attempts to connect to the port described by this instance. |
| `disconnect` | None | A success or failure message | `"ArmaCOM" callExtension [myInstanceUUID, ["disconnect"]];` | Attempts to disconnect from the port described by this instance. If threaded writes are enabled, the extension will attempt to flush the remaining data **synchronously** before disconnecting. |
| `enableThreadedWrites` | None | Success or failure message | `"ArmaCOM" callExtension [myInstanceUUID, ["enableThreadedWrites"]];` | Attempts to begin using a separate thread for writing. May dramatically reduce the time `write` calls take to return to SQF. See the README for more information. |
| `getBaudRate` | None | The index of the currently set baud rate | `"ArmaCOM" callExtension [myInstanceUUID, ["getBaudRate"]];` | Gets the index of the set baud rate, not the actual baud rate |
| `getDataBits` | None | The index of the currently set data bits | `"ArmaCOM" callExtension [myInstanceUUID, ["getDataBits"]];` | Gets the index of the set data bits, not the actual data bits |
| `getEofChar` | None | The current ASCII charcode of eofChar | `"ArmaCOM" callExtension [myInstanceUUID, ["getEofChar"]];` | The character used to signal the end of data. Default: `0`. |
| `getErrorChar` | None | The current ASCII charcode of errorChar | `"ArmaCOM" callExtension [myInstanceUUID, ["getErrorChar"]];` | Default: `0`. |
| `getEvtChar` | None | The current ASCII charcode of evtChar | `"ArmaCOM" callExtension [myInstanceUUID, ["getEvtChar"]];` | he character used to signal an event. Default: `0`. |
| `getFAbortOnError` | None | `true` or `false` based on the currently set `fAbortOnError` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFAbortOnError"]];` | If `true`, the driver terminates all read and write operations with an error status if an error occurs. The driver will not accept any further communications until the extension clears the error (currently not implemented). Default: `false`. |
| `getFDsrSensitivity` | None | `true` or `false` based on the currently set `fDsrSensitivity` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFDsrSensitivity"]];` | If `true`, the driver is sensitive to the state of the DSR signal - all recieved bytes will be ignored unless the DSR input line is high. Default: `false`. |
| `getFDtrControl` | None | `0`, `1`, or `2` based on the currently set `fDtrControl` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFDtrControl"]];` | If `0`, the Data-Terminal-Ready line is disabled. If `1`, the DTR line is enabled and left on. If `2`, it is an error to adjust the DTR line. Default: `1`. |
| `getFErrorChar` | None | `true` or `false` based on the currently set `fErrorChar` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFErrorChar"]];` | If `true` and `fParity` is true, bytes received with parity errors are replaced with `ErrorChar`. Defualt: `false`. |
| `getFInX` | None | `true` or `false` based on the currently set `fInX` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFInX"]];` | If `true`, the `XoffChar` character is sent when the input buffer comes within `XoffLim` bytes of being full, and the `XonChar` character is sent when the input buffer comes within `XonLim` bytes of being empty. Defualt: `false`. |
| `getFNull` | None | `true` or `false` based on the currently set `fNull` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFNull"]];` | If `true`, null bytes are discarded when received. Defualt: `false`. |
| `getFOutX` | None | `true` or `false` based on the currently set `fOutX` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFOutX"]];` | If `true`, transmission stops when the `XoffChar` character is received and starts again when the `XonChar` character is received. Default: `false`. |
| `getFOutxCtsFlow` | None | `true` or `false` based on the currently set `fOutxCtsFlow` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFOutxCtsFlow"]];` | If `true`, the Clear-To-Send signal is monitored, and when the CTS is turned off, output is suspended until the CTS is sent again. Default: `false`. |
| `getFOutxDsrFlow` | None | `true` or `false` based on the currently set `fOutxDsrFlow` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFOutxDsrFlow"]];` | If `true`, the Data-Set-Ready signal is monitored, and when the DSR is turned off, output is suspended until the DSR is sent again. Default: `false`. |
| `getFParity` | None | `true` or `false` based on the currently set `fParity` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFParity"]];` | If `true`, parity checking is performed. Default: `false`. |
| `getFRtsControl` | None | `0`, `1`, `2`, or `3` based on the currently set `fRtsControl` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFRtsControl"]];` | If `0`, the Request-To-Send line is disabled. If `1`, the RTS line is opened and left on. If `2`, RTS handshaking is enabled - the drived raises the RTS line when the input buffer is less than one half full and lowers the RTS line when the buffer is more than three quarters full, and it is an error to adjust the RTS line. If `3`, the RTS line will be high if bytes are available for transmission, and after all buffered bytes have been sent, the RTS line will be low. Default: `1`. |
| `getFTXContinueOnXoff` | None | `true` or `false` based on the currently set `fTXContinueOnXoff` | `"ArmaCOM" callExtension [myInstanceUUID, ["getFTXContinueOnXoff"]];` | If `true`, transmission continues after the input buffer has come within `XoffLim` bytes of being full and the driver has transmitted the `XoffChar` character to stop receiving bytes. If `false`, transmission does not continue until the input buffer is within `XonLim` bytes of being empty and the driver has transmitted the `XonChar` character to resume reception. Default: `false`. |
| `getParity` | None | The index of the currently set parity bit(s) | `"ArmaCOM" callExtension [myInstanceUUID, ["getParity"]];` | Gets the index of the set parity bit(s), not the actual parity bit(s) |
| `getStopBits` | None | The index of the currently set stop bit(s) | `"ArmaCOM" callExtension [myInstanceUUID, ["getStopBits"]];` | Gets the index of the set stop bit(s), not the actual stop bit(s) |
| `getXoffChar` | None | The current ASCII charcode of XoffChar | `"ArmaCOM" callExtension [myInstanceUUID, ["getXoffChar"]];` | Default: `19` (device control 3). |
| `getXoffLim` | None | The current XoffChar | `"ArmaCOM" callExtension [myInstanceUUID, ["getXoffLim"]];` | See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `512`. |
| `getXonChar` | None | The current ASCII charcode of XonChar | `"ArmaCOM" callExtension [myInstanceUUID, ["getXonChar"]];` | Default: `17` (device control 1). |
| `getXonLim` | None | The current XonLim | `"ArmaCOM" callExtension [myInstanceUUID, ["getXonLim"]];` | See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `2048`. |
| `isUsingWriteThread` | None | `true` or `false` based on whether this serial port is using a thread for writes | `"ArmaCOM" callExtension [myInstanceUUID, ["isUsingWriteThread"]];` | May dramatically improve performance. See the main header of this communication method for more information. |
| `setBaudRate` | `baudRateIndex`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setBaudRate", baudRateIndex]];` | Sets the baud rate. `baudRateIndex` is the index of the desired baud rate (from `listBaudRates`). |
| `setDataBits` | `dataBitsIndex`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setDataBits", dataBitsIndex]];` | Sets the data bits. `dataBitsIndex` is the index of the desired baud rate (from `listDataBits`). |
| `setEofChar` | `EofChar`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setEofChar", EofChar]];` | Sets EofChar. EofChar is an integer ASCII code, e.g. `65` for "A". The character used to signal the end of data. Default: `0`. |
| `setErrorChar` | `ErrorChar`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setErrorChar", ErrorChar]];` | Sets ErrorChar. ErrorChar is an integer ASCII code, e.g. `65` for "A". Default: `0`. |
| `setEvtChar` | `EvtChar`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setEvtChar", EvtChar]];` | Sets EvtChar. EvtChar is an integer ASCII code, e.g. `65` for "A". The character used to signal an event. Default: `0`. |
| `setFAbortOnError` | `fAbortOnError`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFAbortOnError", fAbortOnError]];` | Sets fAbortOnError. If `true`, the driver terminates all read and write operations with an error status if an error occurs. The driver will not accept any further communications until the extension clears the error (currently not implemented). Default: `false`. |
| `setFDsrSensitivity` | `fDsrSensitivity`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFDsrSensitivity", fDsrSensitivity]];` | Sets fDsrSensitivity. If `true`, the driver is sensitive to the state of the DSR signal - all recieved bytes will be ignored unless the DSR input line is high. Default: `false`. |
| `setFDtrControl` | `fDtsControl`: `0 \| 1 \| 2` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFDtrControl", fDtsControl]];` | Sets fDtrControl. If `0`, the Data-Terminal-Ready line is disabled. If `1`, the DTR line is enabled and left on. If `2`, it is an error to adjust the DTR line. Default: `1`. |
| `setFErrorChar` | `fErrorChar`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFErrorChar", fErrorChar]];` | Sets fErrorChar. If `true` and `fParity` is true, bytes received with parity errors are replaced with `ErrorChar`. Defualt: `false`. |
| `setFInX` | `fInX`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFInX", fInX]];` | Sets fInX. If `true`, the `XoffChar` character is sent when the input buffer comes within `XoffLim` bytes of being full, and the `XonChar` character is sent when the input buffer comes within `XonLim` bytes of being empty. Defualt: `false`. |
| `setFNull` | `fNull`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFNull", fNull]];` | Sets fNull. If `true`, null bytes are discarded when received. Defualt: `false. |
| `setFOutX` | `fOutX`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFOutX", fOutX]];` | Sets fOutX. If `true`, transmission stops when the `XoffChar` character is received and starts again when the `XonChar` character is received. Default: `false`. |
| `setFOutxCtsFlow` | `fOutxCtsFlow`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFOutxCtsFlow", fOutxCtsFlow]];` | Sets `fOutxCtsFlow`. If `true`, the Clear-To-Send signal is monitored, and when the CTS is turned off, output is suspended until the CTS is sent again. Default: `false`. |
| `setFOutxDsrFlow` | `fOutxDsrFlow`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFOutxDsrFlow", fOutxDsrFlow]];` | Sets fOutxDsrFlow. If `true`, the Data-Set-Ready signal is monitored, and when the DSR is turned off, output is suspended until the DSR is sent again. Default: `false`. |
| `setFParity` | `fParity`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFParity", fParity]];` | Sets `fParity`. If `true`, parity checking is performed. Default: `false`. |
| `setFRtsControl` | `fRtsControl`: `0 \| 1 \| 2 \| 3` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFRtsControl", fRtsControl]];` | Sets fRtsControl. If `0`, the Request-To-Send line is disabled. If `1`, the RTS line is opened and left on. If `2`, RTS handshaking is enabled - the drived raises the RTS line when the input buffer is less than one half full and lowers the RTS line when the buffer is more than three quarters full, and it is an error to adjust the RTS line. If `3`, the RTS line will be high if bytes are available for transmission, and after all buffered bytes have been sent, the RTS line will be low. Default: `1`. |
| `setFTXContinueOnXoff` | `fTXContinueOnXoff`: `bool` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setFTXContinueOnXoff", fTXContinueOnXoff]];` | Sets fTXContinueOnXoff. If `true`, transmission continues after the input buffer has come within `XoffLim` bytes of being full and the driver has transmitted the `XoffChar` character to stop receiving bytes. If `false`, transmission does not continue until the input buffer is within `XonLim` bytes of being empty and the driver has transmitted the `XonChar` character to resume reception. Default: `false`. |
| `setParity` | `parityIndex`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setParity", parityIndex]];` | Sets the parity. `parityIndex` is the index of the desired parity (from `listParities`). |
| `setStopBits` | `stopBitsIndex`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setStopBits", stopBitsIndex]];` | Sets the stop bit(s). `stopBitsIndex` is the index of the desired stop bit(s) (from `listStopBits`). |
| `setXoffChar` | `XoffChar`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setXoffChar", XoffChar]];` | Sets XoffChar. XoffChar is an integer ASCII code, e.g. `65` for "A". Default: `19` (device control 3). |
| `setXoffLim` | `XoffLim`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setXoffLim", XoffLim]];` | Sets XoffLim. See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `512`. |
| `setXonChar` | `XonChar`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setXonChar", XonChar]];` | Sets XonChar. XonChar is an integer ASCII code, e.g. `65` for "A". Default: `17` (device control 1). |
| `setXonLim` | `XonLim`: `int` | None | `"ArmaCOM" callExtension [myInstanceUUID, ["setXonLim", XonLim]];` | Sets XonLim. See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `2048`. |
| `write` | `data`: `string` | A success or failure message | `"ArmaCOM" callExtension [myInstanceUUID, ["write", data]];` | Attempts to either write the data to the serial port if threaded writes are disabled, or queue the data to be written if threaded writes are enabled. If threaded writes are enabled, this command's return value does not say whether the data has successfully been *written*, only that it has been *queued*. |