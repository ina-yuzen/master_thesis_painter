using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace RecConv
{
    class Program
    {
        static int Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Usage: source.dae rec_9999-9999_xx_yy");
                return 1;
            }

            string dirname = args[1];
            if (dirname.EndsWith("\\") || dirname.EndsWith("/")) {
                dirname = dirname.Remove(dirname.Length-1);
            }

            Dictionary<string, Matrix> pose = new Dictionary<string,Matrix>();
            using (TextReader r = File.OpenText(dirname + "\\pose.txt"))
            {
                string line;
                while ((line = r.ReadLine()) != null)
                {
                    string[] fields = line.Split('\t', ' ');
                    Debug.Assert(fields.Length == 17);
                    Matrix m = new Matrix();
                    for (int i = 0; i < 16; i ++) {
                        Debug.Assert(double.TryParse(fields[i + 1], out m.a[i]));
                    }
                    pose.Add(fields[0], m);
                }
            }


            XmlDocument dae = new XmlDocument();
            XmlNamespaceManager manager = new XmlNamespaceManager(dae.NameTable);
            manager.AddNamespace("c", "http://www.collada.org/2005/11/COLLADASchema");
            dae.Load(args[0]);
            foreach (XmlNode node in dae.SelectNodes("//c:node", manager)) 
            {
                String name = node.Attributes.GetNamedItem("name").InnerText;
                Matrix m;
                if (pose.TryGetValue(name, out m))
                {
                    foreach (XmlNode child in node.ChildNodes)
                    {
                        if (child.Name == "matrix")
                        {
                            child.InnerText = m.ToString();
                        }
                        else if (child.Name == "rotate")
                        {
                            Console.WriteLine("<rotate> needs to be supported, but not yet.");
                            return 1;
                        }
                    }
                }
            }
            foreach (XmlNode imageFile in dae.SelectNodes("//c:library_images/c:image/c:init_from", manager))
            {
                imageFile.InnerText = System.IO.Path.GetFileName(imageFile.InnerText);
            }
            string outputName = dirname + "\\" + System.IO.Path.GetFileName(args[0]);
            dae.Save(outputName);
            Console.WriteLine("Saved to " + outputName);

            return 0;
        }
    }

    class Matrix
    {
        public double[] a = new double[16];

        public override string ToString()
        {
            return String.Format("{0} {4} {8} {12}\r\n{1} {5} {9} {13}\r\n{2} {6} {10} {14}\r\n{3} {7} {11} {15}", 
                Array.ConvertAll(a, i=> (object) i));
        }
    }
}
