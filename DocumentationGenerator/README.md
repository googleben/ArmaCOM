# ArmaCOM Documentation Generator

This is a (admittedly rather low-quality, but it gets the job done) simple program that goes through all the files in the ArmaCOM directory and generates a `Documentation.md` file based on doc comments so that we can keep documentation for commands in the code, which means it's easy to keep documentation up to date when code changes are made. Doc comments use these formats:

```C++
//@GlobalCommand commandName
//@Args arg1name: arg1type, arg2name: arg2type, arg3name: arg3type
//@Return Description of return value
//@Description Description text here
//@Description More description text here
//@Description Even more description text here

//@StaticCommand commMethodName.commandName
//@Args arg1name: arg1type, arg2name: arg2type, arg3name: arg3type
//@Return Description of return value
//@Description Description text here
//@Description More description text here
//@Description Even more description text here

//@InstanceCommand commMethodName.commandName
//@Args arg1name: arg1type, arg2name: arg2type, arg3name: arg3type
//@Return Description of return value
//@Description Description text here
//@Description More description text here
//@Description Even more description text here

//@CommMethod commMethodName
//@Description Description text here
//@Description More description text here
//@Description Even more description text here
```