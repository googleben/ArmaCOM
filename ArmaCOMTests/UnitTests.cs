using ArmaExtensionInterface;
using System;
using System.IO.Ports;
using System.Collections.Generic;
using System.Linq;
using static ArmaExtensionInterface.ArmaExtension;
using System.Threading.Tasks;
using RJCP.IO.Ports;
using System.Threading;
using System.Text;
using NUnit.Framework;
using System.Net.Sockets;
using System.Net;
using System.Diagnostics;

namespace ArmaCOMTests
{
    public class Settings
    {
        public const string OUR_PORT = "COM4";
        public const string THEIR_PORT = "COM13";
        public const Int32 TCP_PORT = 8080;
    }

    
    public class ExtensionFixture : IDisposable
    {
        public ArmaExtension extension;
        public Action<string, string, string> callbackAction;
        public int nextCallbackReturn = 0;
        private bool callbackEnabled = false;
        public ExtensionFixture()
        {
            //we go up several levels because the VS integrated test explorer starts us in the build directory
            this.extension = new ArmaExtension(TestContext.CurrentContext.TestDirectory + "\\..\\..\\..\\..\\x64\\Release\\ArmaCOM_x64.dll");
        }
        public string Call(string function)
        {
            (string ans, TimeSpan time) = extension.TimedCall(function);
            TestContext.Out.WriteLine($"Extension call {function} took {time.TotalMilliseconds}ms and returned \"{ans}\"");
            return ans;
        }
        public string CallArgs(string function, params string[] args)
        {
            (string ans, TimeSpan time) = extension.TimedCallArgs(function, args);
            TestContext.Out.WriteLine($"Extension call \"{function}\", args [{args.Select(s => "\""+s+"\"").Aggregate((a, b) => a + ", " + b)}] took {time.TotalMilliseconds}ms and returned \"{ans}\"");
            return ans;
        }
        public string GetVersion()
        {
            (string ans, TimeSpan time) = extension.TimedGetVersion();
            TestContext.Out.WriteLine($"Extension GetVersion took {time.TotalMilliseconds} and returned {ans}");
            return ans;
        }

        //cached TestContext.Out from the last EnsureCallbackEnabled call
        //we have to cache it because the callback will (usually) be called
        //in a separate thread where TestContext won't work correctly
        private System.IO.TextWriter testOut;
        public void EnsureCallbackEnabled()
        {
            this.testOut = TestContext.Out;
            if (!this.callbackEnabled) {
                this.extension.RegisterCallback((name, function, data) => {
                    this.testOut.WriteLine($"Recieved callback: {name}, {function}, {data}");
                    if (this.callbackAction != null) this.callbackAction(name, function, data);
                    return nextCallbackReturn;
                });
                this.callbackEnabled = true;
            }
        }
        public Task<(string, List<object>)> ReadOnce()
        {
            var tcs = new TaskCompletionSource<(string, List<object>)>();
            this.EnsureCallbackEnabled();
            this.callbackAction = (a, b, c) => {
                tcs.SetResult((b, Utils.ParseArmaArray(c)));
            };
            return tcs.Task;
        }

        public ReadManyWrapper ReadMany()
        {
            return new ReadManyWrapper(this);
        }

        public void Dispose()
        {
            extension.Dispose();
            Thread.Sleep(1000);
        }
    }

    [NonParallelizable]
    [TestFixture]
    public class BasicTests
    {
        ExtensionFixture extension;
        [OneTimeSetUp]
        public void SetUp()
        {
            this.extension = new ExtensionFixture();
        }
        [OneTimeTearDown]
        public void Dispose()
        {
            this.extension.Dispose();
        }

        [Test]
        public void VersionNumber()
        {
            Assert.AreEqual("ArmaCOM v2.0-beta", extension.GetVersion());
        }
    }

    [NonParallelizable]
    [TestFixture]
    public class SerialPortSelfContainedTests
    {
        ExtensionFixture extension;

        [OneTimeSetUp]
        public void SetUp()
        {
            this.extension = new ExtensionFixture();
        }

        [Test]
        public void ListPorts()
        {
            string portsString = extension.CallArgs("serial", "listPorts");
            var returned = Utils.ParseArmaArray(portsString);
            List<string> ports = returned.Select(a => (string)(((List<object>)a)[0])).ToList();
            var actualPorts = SerialPort.GetPortNames().ToList();
            Assert.That(ports, Is.EquivalentTo(actualPorts));
        }
        
        private void TestIndexedGetSet(string listCommand, string getCommand, string setCommand, string name)
        {
            string port = extension.CallArgs("serial", "create", Settings.THEIR_PORT);
            string listStr = extension.CallArgs("serial", listCommand);
            List<(int, string)> list = Utils.ParseArmaArray(listStr).Select(
                (l) => ((int) (float)(((List<object>)l)[0]), ((List<object>)l)[1].ToString())
            ).ToList();
            //test each valid value
            foreach ((int i, string s) in list) {
                List<object> setAns = Utils.ParseArmaArray(extension.CallArgs(port, setCommand, i.ToString()));
                Assert.AreEqual("SUCCESS", setAns[0]);
                Assert.AreEqual($"Set {name} to index {i} (value: {s})", setAns[1]);
                string get = extension.CallArgs(port, getCommand + "Index");
                Assert.AreEqual(i.ToString(), get);
                string getVal = extension.CallArgs(port, getCommand + "Value");
                Assert.AreEqual(s, getVal);
            }
            //test missing argument
            List<object> ans = Utils.ParseArmaArray(extension.CallArgs(port, setCommand));
            Assert.AreEqual("FAILURE", ans[0]);
            Assert.AreEqual("Additional argument required", ans[1]);
            //test out of range arguments
            int[] intsToTest = { -1, list.Count, list.Count + 1, int.MaxValue, int.MinValue };
            foreach (int i in intsToTest) {
                ans = Utils.ParseArmaArray(extension.CallArgs(port, setCommand, i.ToString()));
                Assert.AreEqual("FAILURE", ans[0]);
                Assert.AreEqual($"New {name} out of range", ans[1]);
            }
            //test unparseable arguments
            string[] strsToTest = { "", "Hello", "one", "123456789123456789"};
            foreach (string s in strsToTest) {
                ans = Utils.ParseArmaArray(extension.CallArgs(port, setCommand, s));
                Assert.AreEqual("FAILURE", ans[0]);
                Assert.IsTrue(((string)ans[1]).StartsWith("Exception parsing input: "));
            }
            extension.CallArgs("destroy", port);
        }

        [Test]
        public void ListBaudRates()
        {
            const string expected = "[[0, 110], [1, 300], [2, 600], [3, 1200], [4, 2400], [5, 4800], [6, 9600], [7, 14400], [8, 19200], [9, 38400], [10, 57600], [11, 115200], [12, 128000], [13, 256000]]";
            Assert.AreEqual(expected, extension.CallArgs("serial", "listBaudRates"));
        }

        [Test]
        public void GetSetBaudRate()
        {
            TestIndexedGetSet("listBaudRates", "getBaudRate", "setBaudRate", "baud rate");
        }

        [Test]
        public void ListStopBits()
        {
            const string expected = "[[0, \"1 stop bit\"], [1, \"1.5 stop bits\"], [2, \"2 stop bits\"]]";
            Assert.AreEqual(expected, extension.CallArgs("serial", "listStopBits"));
        }

        [Test]
        public void GetSetStopBits()
        {
            TestIndexedGetSet("listStopBits", "getStopBits", "setStopBits", "stop bits");
        }

        [Test]
        public void ListParities()
        {
            const string expected = "[[0, \"No parity\"], [1, \"Odd parity\"], [2, \"Even parity\"], [3, \"Mark parity\"], [4, \"Space parity\"]]";
            Assert.AreEqual(expected, extension.CallArgs("serial", "listParities"));
        }

        [Test]
        public void GetSetParities()
        {
            TestIndexedGetSet("listParities", "getParity", "setParity", "parity");
        }

        [Test]
        public void ListDataBits()
        {
            const string expected = "[[0, \"4 data bits\"], [1, \"5 data bits\"], [2, \"6 data bits\"], [3, \"7 data bits\"], [4, \"8 data bits\"]]";
            Assert.AreEqual(expected, extension.CallArgs("serial", "listDataBits"));
        }

        [Test]
        public void GetSetDataBits()
        {
            TestIndexedGetSet("listDataBits", "getDataBits", "setDataBits", "data bits");
        }

        [Test]
        public void CanCreateAndDestroy()
        {
            Thread.Sleep(100);
            string instancesBefore = extension.CallArgs("serial", "listInstances");
            string port = extension.CallArgs("serial", "create", Settings.THEIR_PORT);
            Assert.That(port, Does.Match("\\b[0-9a-f]{8}\\b-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-\\b[0-9a-f]{12}\\b"));
            string instancesAfter = extension.CallArgs("serial", "listInstances");
            Assert.That(instancesBefore, Does.Not.Contain(port));
            Assert.That(instancesAfter, Does.Contain(port));
            Assert.AreEqual("[\"SUCCESS\", \"Instance destroyed\"]", extension.CallArgs("destroy", port));
            string instancesFinal = extension.CallArgs("serial", "listInstances");
            Assert.That(instancesFinal, Does.Not.Contain(port));
            Thread.Sleep(100);
        }

        [OneTimeTearDown]
        public void Dispose()
        {
            this.extension.Dispose();
        }
    }

    [NonParallelizable]
    [TestFixture]
    public class SerialPortOnlineTests : IDisposable
    {
        ExtensionFixture extension;
        SerialPortStream ourPort;
        string theirPort;

        bool connected = false;

        private void EnsureConnected()
        {
            if (!connected) {
                theirPort = this.extension.CallArgs("serial", "create", Settings.THEIR_PORT);
                this.extension.CallArgs(theirPort, "enableThreadedWrites");
                this.extension.CallArgs(theirPort, "connect");
                ourPort = new SerialPortStream(Settings.OUR_PORT);
                ourPort.Open();
                connected = true;
            }
        }

        [OneTimeSetUp]
        public void SetUp()
        {
            this.extension = new ExtensionFixture();
        }

        [Test, Order(0)]
        public void CanConnect()
        {
            EnsureConnected();
            Assert.That(theirPort, Does.Match("\\b[0-9a-f]{8}\\b-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-\\b[0-9a-f]{12}\\b"));
        }

        [Test, Order(1)]
        public void CanRead()
        {
            EnsureConnected();
            var task = extension.ReadOnce();
            ourPort.WriteTimeout = 1000;
            ourPort.WriteLine("Hello, World!");
            Task.WaitAny(task, Task.Delay(5000));
            Assert.True(task.IsCompleted);
            (string _, List<object> data) = task.Result;
            Assert.AreEqual("Hello, World!", data[1]);
            
        }

        [Test, Order(2)]
        public void CanWrite()
        {
            EnsureConnected();
            extension.CallArgs(theirPort, "write", "Hello, World!\n");
            ourPort.ReadTimeout = 1000;
            var line = ourPort.ReadLine();
            Assert.AreEqual("Hello, World!", line);
        }

        [Test, Order(3)]
        public void CanDisconnectAndDestroy()
        {
            EnsureConnected();
            extension.CallArgs(theirPort, "disconnect");
            extension.CallArgs("destroy", theirPort);
            ourPort.Close();
            connected = false;
        }

        [OneTimeTearDown]
        public void Dispose()
        {
            if (connected) {
                extension.CallArgs(theirPort, "disconnect");
                extension.CallArgs("destroy", theirPort);
                ourPort.Close();
            }
            this.extension.Dispose();
        }
    }

    [NonParallelizable]
    [TestFixture]
    public class TcpOnlineTests : IDisposable
    {
        ExtensionFixture extension;
        TcpListener listener;
        TcpClient ourSocket;
        string theirSocket;

        List<string> messages = new List<string>();

        Thread acceptThread;

        bool connected = false;

        private Task<bool> Connect()
        {
            IPAddress addr = IPAddress.Loopback;
            listener = new TcpListener(addr, Settings.TCP_PORT);
            listener.Start();
            TaskCompletionSource<bool> tcs = new TaskCompletionSource<bool>();
            acceptThread = new Thread(() => {
                ourSocket = listener.AcceptTcpClient();
                Console.WriteLine("Client connected");
                tcs.SetResult(true);
            });
            acceptThread.Start();
            theirSocket = extension.CallArgs("tcpClient", "create", "localhost", Settings.TCP_PORT.ToString());
            var r = extension.ReadOnce();
            extension.CallArgs(theirSocket, "connect");
            Task.WaitAny(tcs.Task, Task.Delay(10000));
            connected = true;
            return tcs.Task;
        }

        private void EnsureConnected()
        {
            if (!connected) Connect();
        }

        [OneTimeSetUp]
        public void SetUp()
        {
            this.extension = new ExtensionFixture();
        }

        private string Read()
        {
            var stream = ourSocket.GetStream();
            byte[] buff = new byte[256];
            string data = null;
            int bytesRead = 0;
            if ((bytesRead = stream.Read(buff, 0, buff.Length)) != 0) {
                data = System.Text.Encoding.ASCII.GetString(buff, 0, bytesRead);
            }
            return data;
        }
        
        private void Write(string str)
        {
            var message = Encoding.ASCII.GetBytes(str);
            ourSocket.GetStream().Write(message, 0, message.Length);
        }

        [Test, Order(0)]
        public void CanConnect()
        {
            var task = Connect();
            Assert.IsTrue(task.IsCompleted);
            Assert.IsTrue(task.Result);
        }

        [Test, Order(1)]
        public void CanRead()
        {
            EnsureConnected();
            var task = extension.ReadOnce();
            byte[] message = Encoding.ASCII.GetBytes("Hello, World!\n");
            ourSocket.GetStream().Write(message, 0, message.Length);
            Task.WaitAny(task, Task.Delay(5000));
            Assert.True(task.IsCompleted);
            (_, List<object> data) = task.Result;
            Assert.AreEqual("Hello, World!", data[1]);
        }

        [Test, Order(2)]
        public void CanWrite()
        {
            EnsureConnected();
            extension.CallArgs(theirSocket, "write", "Hello, World!");
            var line = Read();
            Assert.AreEqual("Hello, World!", line);
        }
        
        [Test, Order(3)]
        public void CallbackOnChar()
        {
            EnsureConnected();
            extension.CallArgs(theirSocket, "callbackOnChar", "\n");

            var reader = extension.ReadMany();
            Write("Hello, World!\n");
            (_, List<object> data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Hello, World!", data[1]);

            extension.CallArgs(theirSocket, "callbackOnChar", "c");
            Write("Test1cTest2c");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test1", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test2", data[1]);

        }

        [Test, Order(4)]
        public void CallbackOnCharCode()
        {
            EnsureConnected();
            extension.CallArgs(theirSocket, "callbackOnCharCode", ((int)'\n').ToString());

            var reader = extension.ReadMany();
            Write("Hello, World!\nTest");
            (_, List<object> data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Hello, World!", data[1]);

            extension.CallArgs(theirSocket, "callbackOnCharCode", ((int)'c').ToString());
            Write("1cTest2c");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test1", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test2", data[1]);
        }

        [Test, Order(5)]
        public void CallbackOnLength()
        {
            EnsureConnected();
            extension.CallArgs(theirSocket, "callbackOnLength", "5");

            var reader = extension.ReadMany();
            Write("12345123");
            (_, List<object> data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("12345", data[1]);

            extension.CallArgs(theirSocket, "callbackOnLength", "6");
            Write("456abcdef");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("123456", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("abcdef", data[1]);
        }

        [Test, Order(6)]
        public void SwitchToCallbackOnLength()
        {
            EnsureConnected();
            var reader = extension.ReadMany();
            extension.CallArgs(theirSocket, "callbackOnChar", "j");
            Write("Test1j1234abcd123412h");
            (_, List<object> data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test1", data[1]);
            extension.CallArgs(theirSocket, "callbackOnLength", "4");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("1234", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("abcd", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("1234", data[1]);
            extension.CallArgs(theirSocket, "callbackOnChar", "h");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("12", data[1]);
            Write("1234abcd5678");
            extension.CallArgs(theirSocket, "callbackOnLength", "4");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("1234", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("abcd", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("5678", data[1]);
        }

        [Test, Order(7)]
        public void SwitchToCallbackOnChar()
        {
            EnsureConnected();
            var reader = extension.ReadMany();
            extension.CallArgs(theirSocket, "callbackOnLength", "40");
            Write("1234567890123456789012345678901234567890Test1hTest2hTest3hTest4");
            (_, List<object> data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("1234567890123456789012345678901234567890", data[1]);
            extension.CallArgs(theirSocket, "callbackOnChar", "h");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test1", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test2", data[1]);
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test3", data[1]);
            extension.CallArgs(theirSocket, "callbackOnChar", "4");
            (_, data) = Utils.AwaitWithTimeout(reader.Read());
            Assert.AreEqual("Test", data[1]);
        }

        [Test, Order(8)]
        public void AtLeastSixtyHzWrite()
        {
            EnsureConnected();
            //warm up
            for (int i = 0; i < 20; i++) {
                string s = (i * 100_000).ToString();
                extension.CallArgs(theirSocket, "write", s);
                Assume.That(Read() == s);
            }
            //send and recieve 120 messages of 3 integers each
            double[] sTaken = new double[120];
            for (int i = 0; i < 120; i++) {
                string s = (i * 10_000, i * 10_001, i * 10_002).ToString();
                var stopwatch = Stopwatch.StartNew();
                extension.CallArgs(theirSocket, "write", s);
                var read = Read();
                stopwatch.Stop();
                Assume.That(read == s);
                sTaken[i] = stopwatch.Elapsed.TotalMilliseconds / 1000;
            }
            TestContext.Out.WriteLine($"Total time: {sTaken.Sum()}, Average time: {sTaken.Sum()/120}, Hertz: {120/sTaken.Sum()}");
            TestContext.Out.WriteLine($"Max time: {sTaken.Max()}, Min time: {sTaken.Min()}");
            //must be less than 2 seconds
            Assert.Less(sTaken.Sum(), 2);
            
        }

        [Test, Order(Int32.MaxValue)]
        public void CanDisconnectAndDestroy()
        {
            EnsureConnected();
            Assert.IsTrue(connected);
            var str = extension.CallArgs(theirSocket, "disconnect");
            str = extension.CallArgs("destroy", theirSocket);
            ourSocket.Close();
            listener.Stop();
            acceptThread.Abort();
            connected = false;
        }

        [OneTimeTearDown]
        public void Dispose()
        {
            if (connected) {
                extension.CallArgs(theirSocket, "disconnect");
                extension.CallArgs("destroy", theirSocket);
                ourSocket.Close();
                listener.Stop();
                acceptThread.Abort();
            }
            this.extension.Dispose();
        }
    }
}
