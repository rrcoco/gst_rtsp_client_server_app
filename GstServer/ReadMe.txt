ReadMe in Turkish

1- Gstreamer Kurulum

Gstreamer Windows binary sitesinden 64 bit için
gstreamer-1.0-devel-msvc-x86_64-1.16.1.msi 
gstreamer-1.0-msvc-x86_64-1.16.1.msi ( versiyonlar farkli olabilir ) indilir.

Kurulum "Complete" olacak sekilde yapilir.

User Environment degiskenlerinde :

GSTREAMER_1_0_ROOT_X86_64 : C:\gstreamer\1.0\x86_64\
GST_PLUGIN_PATH: %GSTREAMER_1_0_ROOT_X86_64%lib\gstreamer-1.0
GIO_MODULE_DIR: %GSTREAMER_1_0_ROOT_X86_64%lib\gio\modules
GIO_EXTRA_MODULES :%GSTREAMER_1_0_ROOT_X86_64%lib\gio\modules

seklinde degisiklikler yapilir. Logout olunur ve degiskenler aktif hale getirilir.

2 - Gstreamer ile proje geliştirme

https://gstreamer.freedesktop.org/documentation/installing/on-windows.html?gi-language=c 

sitesinde görüleceği üzere kurulum ve kullanım adımları tarif edilmiştir.

"Creating new projects manually" başlığında anlatıldığı şekilde "Property Manager" tool üzerinden props dosyaları vasıtasıyla include,lib vs kolaylıkla halledilecektir.

Ayrıca runtime dll dosyaları execution için gerekli olduğundan ya "GSTREAMER_1_0_ROOT_X86_64\bin" path'i Path'e eklenebilir ya da üzerinde çalışılan projenin working directory bilgisi bu path'e ayarlanabilir.


3- opencv kurulumm ( optional )

c++ projelerinde image yüklemek için kullanılmak maksadıyla yüklenildi.

- windows platformunun packaga manager'ı vcpkg kurulumu yapılır ( github'dan proje indilir - derlenir )
- powershell üzerinden vcpkg.exe search opencv ile opencv'nin kütüphanede olduğu görülür
- vcpkg.exe install opencv ile kurulum yapılır.
- vcpkg.exe integrate install ile vcpkg ile kurulumu yapılan her projenin bütün c++ projelerinde otomatikmak gösterilmesi sağlanır.
