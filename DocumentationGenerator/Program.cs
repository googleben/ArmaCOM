using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace DocumentationGenerator
{
    class Command
    {
        public string name;
        //name, type
        public List<(string, string)> args = new List<(string, string)>();
        public string ret;
        public string description;

        public Command(string name, List<(string, string)> args, string ret, string description)
        {
            this.name = name;
            this.args = args;
            this.ret = ret;
            this.description = description;
        }

        public string GetSyntaxExample()
        {
            StringBuilder ans = new StringBuilder(2000);
            ans.Append("\"ArmaCom\" callExtension [");
            ans.Append(this.name);
            ans.Append("");
            if (args.Count != 0) {
                ans.Append(", [");
                for (int i = 0; i < args.Count; i++) {
                    if (i != 0) ans.Append(", ");
                    var (name, _) = args[i];
                    ans.Append($"\"{name}\"");
                }
                ans.Append("]");
            }
            ans.Append("];");
            return ans.ToString();
        }
    }
    class CommMethod
    {
        public string name;
        public string description;
        public List<Command> instanceCommands = new List<Command>();
        public List<Command> staticCommands = new List<Command>();

        public CommMethod(string name, string description)
        {
            this.name = name;
            this.description = description;
        }
    }
    class Program
    {
        
        static void Main(string[] args)
        {
            Dictionary<string, CommMethod> commMethodsD = new Dictionary<string, CommMethod>();
            List<Command> globalCommands = new List<Command>();
            Parsing.ReadAllFiles(globalCommands, commMethodsD);
            globalCommands.Sort((a, b) => a.name.CompareTo(b.name));
            List<CommMethod> commMethods = commMethodsD.Values.ToList();
            commMethods.Sort((a, b) => a.name.CompareTo(b.name));
            foreach (var cm in commMethods) {
                cm.staticCommands.Sort((a, b) => a.name.CompareTo(b.name));
                cm.instanceCommands.Sort((a, b) => a.name.CompareTo(b.name));
            }
            StringBuilder ans = new StringBuilder();
            ans.AppendLine("# Global Commands")
                .AppendLine()
                .AppendLine("| Name | Arguments | Return Value | SQF Example | Comments |")
                .AppendLine("| ---  | ---       | ---          | ---         | ---      |");
            foreach (var c in globalCommands) {
                ans.Append("| ")
                    .Append($"`{c.name}`")
                    .Append(" | ");
                if (c.args.Count == 0) {
                    ans.Append("None");
                } else {
                    for (int i = 0; i < c.args.Count; i++) {
                        var (name, type) = c.args[i];
                        if (i != 0) ans.Append(", ");
                        ans.Append($"`{name}`: `{type.Replace("|", "\\|")}`");
                    }
                }
                ans.Append(" | ");

                if (c.ret.Trim().Length == 0) ans.Append("None");
                else ans.Append(c.ret);

                ans.Append(" | ")
                    .Append($"`\"ArmaCOM\" callExtension [\"{c.name}\"")
                    .Append(", [");
                if (c.args.Count > 0) {
                    for (int i = 0; i < c.args.Count; i++) {
                        if (i != 0) ans.Append(", ");
                        ans.Append(c.args[i].Item1);
                    }
                }
                ans.Append("]];`")
                    .Append(" | ")
                    .Append(c.description)
                    .AppendLine(" |");
            }
            foreach (var cm in commMethods) {
                ans.AppendLine()
                    .AppendLine($"# Communication Method: {cm.name}")
                    .AppendLine()
                    .AppendLine(cm.description)
                    .AppendLine()
                    .AppendLine("## Static Commands")
                    .AppendLine()
                    .AppendLine("These commands must be called with the format `\"ArmaCOM\" callExtension [\"name of the communication method\", [\"command name\", [arg1, arg2, ...]]`.")
                    .AppendLine("Those familiar with object-oriented programming should think of them as static methods on the communication method's class.")
                    .AppendLine()
                    .AppendLine("| Name | Arguments | Return Value | SQF Example | Comments |")
                    .AppendLine("| ---  | ---       | ---          | ---         | ---      |");
                foreach (var c in cm.staticCommands) {
                    ans.Append("| ")
                        .Append($"`{c.name}`")
                        .Append(" | ");
                    if (c.args.Count == 0) {
                        ans.Append("None");
                    } else {
                        for (int i = 0; i < c.args.Count; i++) {
                            var (name, type) = c.args[i];
                            if (i != 0) ans.Append(", ");
                            ans.Append($"`{name}`: `{type.Replace("|", "\\|")}`");
                        }
                    }
                    ans.Append(" | ");

                    if (c.ret.Trim().Length == 0) ans.Append("None");
                    else ans.Append(c.ret);

                    ans.Append(" | ")
                        .Append($"`\"ArmaCOM\" callExtension [\"{cm.name}\", [\"{c.name}\"");
                    if (c.args.Count > 0) {
                        for (int i = 0; i < c.args.Count; i++) {
                            ans.Append(", ");
                            ans.Append(c.args[i].Item1);
                        }
                    }
                    ans.Append("]];`")
                        .Append(" | ")
                        .Append(c.description)
                        .AppendLine(" |");
                }
                ans.AppendLine("## Instance Commands")
                    .AppendLine()
                    .AppendLine("These commands must be called with the format `\"ArmaCOM\" callExtension [myInstanceUUID, [\"command name\", [arg1, arg2, ...]]`.")
                    .AppendLine("Those familiar with object-oriented programming should think of them as instance methods on your instance of the communication method's class.")
                    .AppendLine()
                    .AppendLine("| Name | Arguments | Return Value | SQF Example | Comments |")
                    .AppendLine("| ---  | ---       | ---          | ---         | ---      |");
                foreach (var c in cm.instanceCommands) {
                    ans.Append("| ")
                        .Append($"`{c.name}`")
                        .Append(" | ");
                    if (c.args.Count == 0) {
                        ans.Append("None");
                    } else {
                        for (int i = 0; i < c.args.Count; i++) {
                            var (name, type) = c.args[i];
                            if (i != 0) ans.Append(", ");
                            ans.Append($"`{name}`: `{type.Replace("|", "\\|")}`");
                        }
                    }
                    ans.Append(" | ");

                    if (c.ret.Trim().Length == 0) ans.Append("None");
                    else ans.Append(c.ret);

                    ans.Append(" | ")
                        .Append($"`\"ArmaCOM\" callExtension [myInstanceUUID, [\"{c.name}\"");
                    if (c.args.Count > 0) {
                        for (int i = 0; i < c.args.Count; i++) {
                            ans.Append(", ");
                            ans.Append(c.args[i].Item1);
                        }
                    }
                    ans.Append("]];`")
                        .Append(" | ")
                        .Append(c.description)
                        .AppendLine(" |");
                }

            }
            File.WriteAllText(".\\..\\..\\..\\Documentation.md", ans.ToString());
        }
    }
}
