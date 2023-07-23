using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using EnvDTE;
using System.Runtime.InteropServices;
//using Microsoft.VisualStudio.VCProjectEngine;

namespace Dte
{
    class Program
    {
        [DllImport("kernel32.dll")]
        static extern IntPtr GetConsoleWindow();

        [DllImport("user32.dll")]
        static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        const int SW_HIDE = 0;
        const int SW_SHOW = 5;

        static public EnvDTE80.DTE2 dte2 = null;
        static void Main(string[] args)
        {
            var handle = GetConsoleWindow();
            try
            {
                ShowWindow(handle, SW_HIDE);
                if (args.Length == 2)
                {
                    args[0] = args[0].Replace('*', ' ');
                    //MessageBox.Show(String.Format("Openinig {0}", args[0]), "VS", MessageBoxButtons.OK);
                    getDte();
                    //dte2 = (EnvDTE80.DTE2)System.Runtime.InteropServices.Marshal.GetActiveObject("VisualStudio.DTE.16.0");
                    Window window = dte2.ItemOperations.OpenFile(args[0]);
                    if (window != null)
                    {
                        //dte2.ActiveDocument.Selection
                        //TextSelection.MoveToLineAndOffset(Line, 0, False)
                        int Line = 1;
                        Int32.TryParse(args[1], out Line);
                        if (Line == 0)
                            Line = 1;
                        TextSelection sel = window.Document.Selection;
                        try
                        {
                            sel.GotoLine(Line);
                        }
                        catch (Exception)
                        {
                            sel.GotoLine(1);
                        }
                        dte2.Application.MainWindow.Activate();
                    }
                    else
                    {
                        throw (new Exception(String.Format("Could not open {0}", args[0])));
                    }
                }
                else
                {
                    throw (new Exception(String.Format("Argument count is not 2 but {0}", args.Length)));
                }
            }
            catch (Exception e)
            {
                ShowWindow(handle, SW_SHOW);
                //Console.WriteLine(e.ToString());
                string msg = e.ToString();
                msg += "\nargs:";
                foreach (string s in args)
                {
                    msg += "\n";
                    msg += s;
                }
                MessageBox.Show(msg, "VS", MessageBoxButtons.OK);
            }


        }

        static void getDte()
        {
            for (int i = 25; i > 8; i--)
            {
                try
                {
                    dte2 = (EnvDTE80.DTE2)System.Runtime.InteropServices.Marshal.GetActiveObject("VisualStudio.DTE." + i.ToString() + ".0");
                    break;
                }
                catch (Exception)
                {
                    //don't care... just keep bashing head against wall until success
                }
            }
        }
    }
}
