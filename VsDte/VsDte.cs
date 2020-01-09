using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

//https://www.codeproject.com/Articles/7859/Building-COM-Objects-in-C
namespace VsDte
{
    [Guid("694C1820-04B6-4988-928F-FD858B954658")]
    [ComVisible(true)]
    public interface VsDte_Interface
    {
        [DispId(1)]
        void OpenFile(string fileName, int line);
    }

    [Guid("9E5E5FB2-219D-4ee7-AB27-E4DBED8E7895")]
    [ComVisible(true)]
    [ClassInterface(ClassInterfaceType.None)]
    [ComSourceInterfaces(typeof(VsDte_Interface))]
    [ProgId("VsDte1.VsDte1")]
    public class VsDte_Impl : VsDte_Interface
    {
        public VsDte_Impl()
        {
        }

        public void OpenFile(string fileName, int line)
        {

        }
    }
}
