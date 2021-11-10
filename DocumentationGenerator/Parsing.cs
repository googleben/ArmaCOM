using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace DocumentationGenerator
{
    class Parsing
    {
        static string argsRegex = "//@Args[^\\r\\n]*?([^\\n\\r]+:\\s*[^\\n\\r]+,?\\s*?)*\\s*";
        static string retRegex = "//@Return[^\\r\\n]*?([^\\n\\r]+)\\s*";
        static string descRegex = "(//@Description[^\\r\\n]*?([^\\n\\r]+)\\s+)+";
        static Regex argsInnerRegex = new Regex("([^:]+):\\s*([^,]+),?\\s*");
        static Regex globalCommandRegex = new Regex("//@GlobalCommand\\s*([^\\n\\r]+)\\s*" + argsRegex + retRegex + descRegex);
        static Regex instanceCommandRegex = new Regex("//@InstanceCommand\\s*([^\\n\\r.]+)\\.([^\\n\\r.]+)\\s*" + argsRegex + retRegex + descRegex);
        static Regex staticCommandRegex = new Regex("//@StaticCommand\\s*([^\\n\\r.]+)\\.([^\\n\\r.]+)\\s*" + argsRegex + retRegex + descRegex);
        static Regex commMethodRegex = new Regex("//@CommMethod\\s*([^\\n\\r]+)\\s*" + descRegex);
        static Regex singleDescRegex = new Regex("//@Description[^\\r\\n]*?([^\\n\\r]+)\\s*");
        public static void ReadAllFiles(List<Command> globalCommands, Dictionary<string, CommMethod> commMethods)
        {
            var files = Directory.GetFiles(".\\..\\..\\..\\");
            foreach (var f in files) {
                var contents = File.ReadAllText(f);
                foreach (Match match in commMethodRegex.Matches(contents)) {
                    if (match.Success) {
                        var gs = match.Groups;
                        var name = gs[1].Value.Trim();
                        var descSb = new StringBuilder();
                        var descMatches = singleDescRegex.Matches(match.Value);
                        bool anyDesc = false;
                        foreach (Match dMatch in descMatches) {
                            if (anyDesc) descSb.Append(' ');
                            anyDesc = true;
                            descSb.Append(dMatch.Groups[1].Value.Trim());
                        }
                        var desc = descSb.ToString();
                        if (!commMethods.ContainsKey(name.ToLower())) {
                            commMethods.Add(name.ToLower(), new CommMethod(name, desc));
                        } else {
                            var m = commMethods[name.ToLower()];
                            m.name = name;
                            m.description = desc;
                        }
                    }
                }
                foreach (Match match in globalCommandRegex.Matches(contents)) {
                    if (match.Success) {
                        var gs = match.Groups;
                        var name = gs[1].Value.Trim();
                        var argsG = gs[2];
                        var ret = gs[3].Value.Trim();
                        var descSb = new StringBuilder();
                        var descMatches = singleDescRegex.Matches(match.Value);
                        bool anyDesc = false;
                        foreach (Match dMatch in descMatches) {
                            if (anyDesc) descSb.Append(' ');
                            anyDesc = true;
                            descSb.Append(dMatch.Groups[1].Value.Trim());
                        }
                        var desc = descSb.ToString();
                        var argsGs = argsInnerRegex.Matches(argsG.Value);
                        var argsL = new List<(string, string)>();
                        for (int i = 0; i < argsGs.Count; i++) {
                            var g = argsGs[i];
                            argsL.Add((g.Groups[1].Value.Trim(), g.Groups[2].Value.Trim()));
                        }
                        var command = new Command(name, argsL, ret, desc);
                        globalCommands.Add(command);
                    }
                }
                foreach (Match match in staticCommandRegex.Matches(contents)) {
                    if (match.Success) {
                        var gs = match.Groups;
                        var commMethodName = gs[1].Value.Trim();
                        var name = gs[2].Value.Trim();
                        var argsG = gs[3];
                        var ret = gs[4].Value.Trim();
                        var descSb = new StringBuilder();
                        var descMatches = singleDescRegex.Matches(match.Value);
                        bool anyDesc = false;
                        foreach (Match dMatch in descMatches) {
                            if (anyDesc) descSb.Append(' ');
                            anyDesc = true;
                            descSb.Append(dMatch.Groups[1].Value.Trim());
                        }
                        var desc = descSb.ToString();
                        var argsGs = argsInnerRegex.Matches(argsG.Value);
                        var argsL = new List<(string, string)>();
                        for (int i = 0; i < argsGs.Count; i++) {
                            var g = argsGs[i];
                            argsL.Add((g.Groups[1].Value.Trim(), g.Groups[2].Value.Trim()));
                        }
                        var command = new Command(name, argsL, ret, desc);
                        if (!commMethods.ContainsKey(commMethodName.ToLower())) {
                            commMethods.Add(commMethodName.ToLower(), new CommMethod(commMethodName, "STUB DESC"));
                        }
                        commMethods[commMethodName].staticCommands.Add(command);
                    }
                }
                foreach (Match match in instanceCommandRegex.Matches(contents)) {
                    if (match.Success) {
                        var gs = match.Groups;
                        var commMethodName = gs[1].Value.Trim();
                        var name = gs[2].Value.Trim();
                        var argsG = gs[3];
                        var ret = gs[4].Value.Trim();
                        var descSb = new StringBuilder();
                        var descMatches = singleDescRegex.Matches(match.Value);
                        bool anyDesc = false;
                        foreach (Match dMatch in descMatches) {
                            if (anyDesc) descSb.Append(' ');
                            anyDesc = true;
                            descSb.Append(dMatch.Groups[1].Value.Trim());
                        }
                        var desc = descSb.ToString();
                        var argsGs = argsInnerRegex.Matches(argsG.Value);
                        var argsL = new List<(string, string)>();
                        for (int i = 0; i < argsGs.Count; i++) {
                            var g = argsGs[i];
                            argsL.Add((g.Groups[1].Value.Trim(), g.Groups[2].Value.Trim()));
                        }
                        var command = new Command(name, argsL, ret, desc);
                        if (!commMethods.ContainsKey(commMethodName.ToLower())) {
                            commMethods.Add(commMethodName.ToLower(), new CommMethod(commMethodName, "STUB DESC"));
                        }
                        commMethods[commMethodName].instanceCommands.Add(command);
                    }
                }
            }
        }
    }
}
