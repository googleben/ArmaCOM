## Installation

ArmaCOMTests depends on [ArmaExtensionInterface](https://github.com/googleben/ArmaExtensionInterface). You can download it and place it directly in the same folder as this project (so that if you have a folder `ArmaCOMTests` that contains `ArmaCOMTests.sln`, it also contains the folder `ArmaExtensionInterface`), or place it elsewhere and create a symlink using the included `MakeLink.ps1` file, which simply has you select the location of its project file using a file picker GUI.

The tests also depend on a few packages from NuGet which Visual Studio should just automatically take care of for you.

## Running

Before running any tests, make sure you've built ArmaCOM with the Release x64 profile and that `ArmaCOM_x64.dll` exists in `[your ArmaCOM project folder]\x64\Release\`. If for some reason that doesn't work, you can change the path of the dll in the `ExtensionFixture` contructor in `UnitTests.cs`.

Once the dependencies are met and ArmaCOM has been built, the built-in Visual Studio test UI (Test -> Test Explorer) should be able to run the tests. Note that you may need to manually build one or both of the dependencies if VS doesn't pick up the depedency correctly. As a last resort, close VS and replace the `ArmaCOMTests.sln` and `ArmaCOMTests.csproj` with clean versions after making sure your dependency folders/symlinks are in the right place.

For the serial port tests, you need to have one pair of connected COM ports on your system. I use [com0com](http://com0com.sourceforge.net/). You can specify which ports you use in the `Settings` class if the defaults don't work for you. Note that it's a good idea to have a double digit port number on ArmaCOM's side because Windows handles single digit ports less strict, so it's not 100% guaranteed that a program that works with `COM1-9` will work for `COM10` onwards (though the opposite is true as far as I can tell).

For the network tests the program will need firewall access and an available port. You can specify which port to use in the `Settings` class if the default doesn't work for you.