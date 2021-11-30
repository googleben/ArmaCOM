using NUnit.Framework;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace ArmaCOMTests
{
    class Utils
    {
        public static T AwaitWithTimeout<T>(Task<T> task, int timeoutMs = 5000)
        {
            Task.WaitAny(task, Task.Delay(timeoutMs));
            Assert.True(task.IsCompleted);
            return task.Result;
        }
        public static List<object> ParseArmaArray(string arr)
        {
            var ans = new List<object>();
            int pos = 1;
            if (arr[0] != '[') throw new Exception("Array did not start with '['");
            var arrStack = new List<List<object>>();
            while (pos < arr.Length - 1) {
                if (arr[pos] == '"') {
                    var sb = new StringBuilder();
                    pos++;
                    while (true) {
                        if (pos == arr.Length) throw new Exception("Unterminated string");
                        char curr = arr[pos];
                        if (curr == '"') {
                            pos++;
                            //double quote is escaped by another double quote
                            if (pos == arr.Length || arr[pos] != '"') {
                                break;
                            }
                        }
                        /*if (curr == '\\') {
                            pos++;
                            char tmp = arr[pos];
                            if (tmp == '"') curr = '"';
                            else if (tmp == 'n') curr = '\n';
                            else if (tmp == '\\') curr = '\\';
                            else throw new Exception("Unknown escape sequence \"\\" + tmp + "\"");
                        }*/
                        sb.Append(curr);
                        pos++;
                    }
                    ans.Add(sb.ToString());
                } else if (arr[pos] == '[') {
                    List<object> newList = new List<object>();
                    ans.Add(newList);
                    arrStack.Add(ans);
                    ans = newList;
                    pos++;
                } else if (arr[pos] == ']') {
                    if (arrStack.Count == 0) throw new Exception("Array ended before input");
                    ans = arrStack[arrStack.Count - 1];
                    arrStack.RemoveAt(arrStack.Count - 1);
                    pos++;
                } else if (arr[pos] == 't' || arr[pos] == 'T') {
                    if ((arr[++pos] != 'r' && arr[pos] != 'R') || (arr[++pos] != 'u' && arr[pos] != 'U') ||
                        (arr[++pos] != 'e' && arr[pos] != 'E')) {
                        throw new Exception("Unrecognized value");
                    }
                    ans.Add(true);
                    pos++;
                } else if (arr[pos] == 'f' || arr[pos] == 'F') {
                    if ((arr[++pos] != 'a' || arr[pos] != 'A') || (arr[++pos] != 'l' && arr[pos] != 'L') ||
                        (arr[++pos] != 's' && arr[pos] != 'S') || (arr[++pos] != 'e' && arr[pos] != 'E')) {
                        throw new Exception("Unrecognized value");
                    }
                    ans.Add(false);
                    pos++;
                } else if (Char.IsDigit(arr[pos])) {
                    StringBuilder sb = new StringBuilder();
                    sb.Append(arr[pos++]);
                    bool anyDecimal = false;
                    while (pos < arr.Length && Char.IsDigit(arr[pos]) || (!anyDecimal && arr[pos] == '.')) {
                        if (arr[pos] == '.') anyDecimal = true;
                        sb.Append(arr[pos++]);
                    }
                    float num = float.Parse(sb.ToString());
                    ans.Add(num);
                } else if (arr[pos] != ',' && !Char.IsWhiteSpace(arr[pos])) {
                    throw new Exception("Unrecognized value");
                } else {
                    pos++;
                }
            }
            if (arr[pos] == ']' && arrStack.Count == 0) return ans;
            else throw new Exception("Array did not end with ']'");
        }
    }

    public class ReadManyWrapper
    {
        Queue<(string, List<object>)> queued = new Queue<(string, List<object>)>();

        //will have a value if a Task needing a value currently exists, otherwise null
        //should be set to null when resolved with a value so a new tcs can be created
        TaskCompletionSource<(string, List<object>)> tcs;
        //mutex that must be held whenever writing or reading `queued` or `tcs`
        Mutex mutex = new Mutex();

        public ReadManyWrapper(ExtensionFixture ef)
        {
            ef.EnsureCallbackEnabled();
            ef.callbackAction = (a, b, c) => {
                mutex.WaitOne();
                var ans = (b, Utils.ParseArmaArray(c));
                if (tcs != null) {
                    tcs.SetResult(ans);
                    tcs = null;
                } else {
                    queued.Enqueue(ans);
                }
                mutex.ReleaseMutex();
            };
        }

        public Task<(string, List<object>)> Read()
        {
            
            mutex.WaitOne();
            if (this.tcs != null) {
                mutex.ReleaseMutex();
                return this.tcs.Task;
            }
            var tcs = new TaskCompletionSource<(string, List<object>)>();
            if (queued.Count != 0) {
                tcs.SetResult(queued.Dequeue());
            } else {
                this.tcs = tcs;
            }
            mutex.ReleaseMutex();
            return tcs.Task;
        } 
    }
}
