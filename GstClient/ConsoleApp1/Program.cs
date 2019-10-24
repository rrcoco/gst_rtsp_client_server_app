using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace ConsoleApp1
{
    class Program
    {
        private static string confFile = "rtsp_parameters.conf";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void event_cb_t();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void frame_cb_t(IntPtr bytes, int width, int height, int bpp);

        [DllImport("GstClient.dll")]
        public static extern void Init(event_cb_t start_cb, event_cb_t stop_cb, frame_cb_t frame_cb, string confFile);

        [DllImport("GstClient.dll")]
        public static extern void RTSPStream(bool video_enabled, bool audio_eanbled, bool auth);

        [DllImport("GstClient.dll")]
        public static extern void RTSPClose();

        [DllImport("GstClient.dll")]
        public static extern void UDPStream(bool video_enabled, bool audio_eanbled);

        [DllImport("GstClient.dll")]
        public static extern void UDPClose();

        public static void OnStreamStarted()
        {
            Console.WriteLine("OnStart");
        }

        public static void OnStreamStopped()
        {
            Console.WriteLine("OnStop");
        }

        static int count = 0;

        /// <summary>
        /// GstClient' tarafından frame alınması durumunda çağrılan callback metodu.
        /// İçerik c++ unsigned char* => c# byte array ( bitmap ) için yazılmıştır.
        /// Gelen data - gstreamer ( bgr ) verisi olup bunu rgb ye çeviriyoruz.
        /// </summary>
        /// <param name="ptr"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        public static void OnFrameReceived(IntPtr ptr, int width, int height, int bpp)
        {
            string filename = @"C:\Users\Hakan\Downloads\temp\frame" + (count++) + ".jpg";

            using (Bitmap bitmap = new Bitmap(width, height, width * bpp, PixelFormat.Format24bppRgb, ptr))
            {
                // TODO: If bgr- rgb error happens at the end - uncomment this code
                // 
                //    var bmpdata = bitmap.LockBits(
                //       new Rectangle(0, 0, bitmap.Width, bitmap.Height),
                //       ImageLockMode.ReadOnly,
                //       bitmap.PixelFormat);

                //    int numbytes = bmpdata.Stride * bitmap.Height;

                //    unsafe
                //    {
                //        byte* rgbValues = (byte*)bmpdata.Scan0.ToPointer();

                //        for (int i = 0; i < numbytes; i += 3)
                //        {
                //            byte dummy = rgbValues[i];
                //            rgbValues[i] = rgbValues[i + 2];
                //            rgbValues[i + 2] = dummy;
                //        }
                //    }

                //    bitmap.UnlockBits(bmpdata);

                bitmap.Save(filename, ImageFormat.Jpeg);
            }
        }

        static void Main(string[] args)
        {
            Init(OnStreamStarted, OnStreamStopped, OnFrameReceived, confFile);

            RTSPStream(true, true, false);
        }
    }
}
