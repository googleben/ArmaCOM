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

```SQF
//step 1:
myPort = ("ArmaCOM" callExtension ["serial", ["create", "COM1"]]) select 0; //we have to 'select 0' because extension return arrays with the string response in index 0
//step 3:
diag_log ("ArmaCOM" callExtension ["connect", [myPort]]);
//step 4:
diag_log ("ArmaCOM" callExtension ["write", [myPort, "Hello, World!"]]);
```

If all goes well, the serial port should be opened and written to. Reading data takes a little more boilerplate, since ArmaCOM uses a callback to send receiveed data to SQF (as opposed to SQF having to poll the extension). The game will receive an `ExtensionCallback` event for every line (terminated by `\n`) the extension reads from a serial port, with `name = "ArmaCOM"` and `function = "data_read"` `_data` will be an array where the first element is the UUID and the second element is the received string (remember, Arma extensions can only communicate using strings, so you have to use [parseSimpleArray](https://community.bistudio.com/wiki/parseSimpleArray) to get the array *as* an array). Here's some example SQF for setting up an event handler:

```SQF
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
```SQF
"ArmaCOM" callExtension [commandName, [argument(s)]]
```
If the command takes no arguments, it may optionally be called with this alternative `callExtension` syntax:
```SQF
"ArmaCOM" callExtension commandName
```
Commands are case insensitive.

If you're familiar with object oriented (OO) programming, it's easiest to think of the commands as being representative of an inner OO design; there's classes for each communication method which have both static and instance methods, as well as a constructor. To invoke a "static" method, the first argument to `callExtension` will be the name of the class. For example, if we wanted to list serial ports in an OO language, that might look like this: `Serial.listPorts()` which translates to this: `"ArmaCOM" callExtension ["serial", ["listPorts"]]`. To create an instance of the class (e.g. `myPort = new Serial("COM5")` in OO), we use the `create` command: `myPort = "ArmaCOM" callExtension ["serial", ["create", "COM5"]] select 0`. It's very important that we save the returned value to a variable; when you run `create`, ArmaCOM returns a UUID (if you don't know what that is, just think of it as a random ID) that represents the object you just created. If you lose that UUID, you lose access to that object, because you use the UUID as the first argument when calling "instance" methods like `write`: `myPort.write("my message")` becomes `"ArmaCOM" callExtension [myPort, ["write", "my message"]]`. If that all sounds terribly cumbersome and verbose, thank Arma and the tragedy that is SQF. If you think you have a better way to do it, *please* tell me, because I'm not a fan of this either, believe me.

Note that extensions are only allowed to send strings to Arma, so even if the command returns a boolean, that boolean will still be in string form. Unless otherwise noted, commands return a human-readable string with either a success message describing what was successfully done, or an error message describing what the extension failed to do and, if applicable, a Windows error message describing why it failed (e.g. "File not found").

For a full list of commands, see [Documentation.md](Documentation.md)

## Reading data

Reading data is accomplished using callbacks from the extension whenever a certain condition is met. By default, data is passed back to Arma when a newline (`\n`) is read. To listen for this callback, use `addMissionEventHandler` with the first argument `"ExtensionCallback"`:

```SQF
addMissionEventHandler ["ExtensionCallback", {
    params ["_name", "_function", "_data"];
    if ((_name isEqualTo "ArmaCOM") && (_function isEqualTo "data_read")) then {
        private _resArr = parseSimpleArray _data;
        systemChat (format ["Received message from UUID %1: %2", _resArr select 0, _resArr select 1]);
    };
}];
```

The extension passes `"ArmaCOM"` as the first argument, `"data_read"` as the second, and an array containing the UUID of which instance the data was read from, followed by the data itself. To change when the extension should send data to Arma, you can use the instance commands `callbackOnChar`, `callbackOnCharCode`, and `callbackOnLength`. When in `callbackOnChar` mode (or `callbackOnCharCode`, provided in case you want to use unprintable characters), the extension will read data until it comes across the characted specified, at which point it will send all the data it has read up to (and excluding) the specified characted. In `callbackOnLength` mode, the extension will read data until it has read the specified number of characters, at which point it will send all the data it has read.

## Short SQF Example

```SQF
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

## Development & Maintenance

### Building ArmaCOM

Besides downloading the project and opening the solution in Visual Studio (I'm using VS 2019 currently, version 16.11.15), the project depends on [Boost](https://www.boost.org/). I'm currently using version 1.77.0. If you don't have it already, download Boost, extract it, and create an environment variable named `BOOST_HOME` that points to the base folder (the one that contains `README.md`, `boost.png`, `bootstrap.bat`, and other stuff). Make sure you restart Visual Studio if you had it open before you created the environment variable. ArmaCOM currently uses only header-only Boost libraries, so there's no need to compile anything to get Boost working. 

As an alternative to creating an environment variable, you can point VC++ directly to your Boost folder: in the Solution Explorer, right click "ArmaCOM", click "Properties", and on the left open "Configuration Properties -> C/C++ -> General". Edit "Additional Include Directories" and replace `$(BOOST_HOME)` with the path to your Boost installation. If you do this and submit a contribution, please make sure you don't include this change so the project is kept machine-agnostic.

Once you have Boost set up correctly, Visual Studio should be able to build ArmaCOM normally, through "Build -> Build Solution". ArmaCOM is not set up to be 32-bit compatible, so while the extension should build for 32-bit targets, it won't work without code changes. I recommend you set your build target to Release x64.

### Generating documentation

I wrote a simple documentation generator to pull in-line documentation from C++ comments and generate `Documentation.md`. The program is a small C# VS solution in the `DocumentationGenerator` folder. You should be able to just run it straight from Visual Studio. It's not particularly robust, but as well as the doc comments are well-formed it should work just fine. It's also written using a lot of regex (which I might change to a real parsing solution at some point but probably not), so programmers beware. If the program is run and doesn't error out it should just overwrite `Documentation.md` with the newest information.

### Running tests

I've written a test suite with reasonable coverage in the `ArmaCOMTests` subfolder. The tests are in pure C# and don't require Arma to run since I'm using C#'s ability to interface with native code, inspired by [Killzone_Kid](http://killzonekid.com)'s work and based on the helpful [maca134's ExtensionTesterConsole](https://github.com/maca134/Maca134.Arma.ExtensionTesterConsole). Follow the instructions in `ArmaCOMTests\README.md` to run the tests.