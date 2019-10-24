using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace ConsoleApp1
{
    class Program
    {
        private static string confFileName = "rtsp_parameters.conf";
        /// <summary>
        /// Server dll import methods
        /// </summary>
        [DllImport(@"C:\Users\Hakan\source\repos\GstServer\lib\GstServer.dll")]
        public static extern void RTSPClose();

        [DllImport(@"C:\Users\Hakan\source\repos\GstServer\lib\GstServer.dll")]
        public static extern void RTSPStream(bool stream_audio, bool stream_video, bool auth);

        [DllImport(@"C:\Users\Hakan\source\repos\GstServer\lib\GstServer.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void RTSPFeedData(byte[] array, int nSize);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void event_cb_t();

        [DllImport(@"C:\Users\Hakan\source\repos\GstServer\lib\GstServer.dll")]
        public static extern void RTSPInit(
        [MarshalAs(UnmanagedType.FunctionPtr)] event_cb_t callback, string confFileName);

        static int count = 0;

        /// <summary>
        /// TODO: Bitmap bilgisi BGR olarak yüklendiği için yeniden bir rgb dönüştürme yapılıyor bu nedenle burada bir gecikme var.
        /// Kendi kütüphanenizde byte array oluşturmak için daha uygun bir çözüm varsa ( raw byte array olacak ) kullanın.
        /// </summary>
        /// <param name="bitmap"></param>
        /// <returns></returns>
        public static byte[] BitmapToByteArray(Bitmap bitmap)
        {
            BitmapData bmpdata = bitmap.LockBits(
                  new Rectangle(0, 0, bitmap.Width, bitmap.Height),
                  ImageLockMode.ReadOnly,
                  bitmap.PixelFormat);

            int numbytes = bmpdata.Stride * bitmap.Height;
            byte[] bytedata = new byte[numbytes];

            // TODO: IF RGB - BGR error occurs at output - uncomment this section

            //unsafe
            //{
            //    byte* rgbValues = (byte*)bmpdata.Scan0.ToPointer();

            //    for (int i = 0; i < numbytes; i += 3)
            //    {
            //        byte dummy = rgbValues[i];
            //        rgbValues[i] = rgbValues[i + 2];
            //        rgbValues[i + 2] = dummy;
            //    }
            //}

            Marshal.Copy(bmpdata.Scan0, bytedata, 0, numbytes);
            
            bitmap.UnlockBits(bmpdata);

            return bytedata;
        }


        /// <summary>
        /// Feeding frames to pipeline : bu mekanizmanın çalışması için harici bir thread üzerinden besleme yapılmalıdır.
        /// Callback olarak rtsp-server hazır olduğunda burası çağrılacaktır.
        /// </summary>
        public static void FeedCB()
        {
            string filename = @"C:\Users\Hakan\Downloads\output\frame" + (count++).ToString() + ".jpg";
            Bitmap bitmap = new Bitmap(filename);
            var bytes = BitmapToByteArray(bitmap);

            RTSPFeedData(bytes, bytes.Length);

            count %= 700;

            Console.WriteLine("feeding");
        }

        static void Main(string[] args)
        {
            event_cb_t callback = new event_cb_t(FeedCB);

            RTSPInit(callback,confFileName);
            RTSPStream(true,true,true);
        }
    }
}
