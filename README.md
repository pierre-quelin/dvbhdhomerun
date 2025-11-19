Debian package for the [Silicon Dust][1] [HDHomeRun][2] [DVB driver][3] written by [Villy Thomsen][4].

*I will be extending the driver to pass singal strength and symbol
quality information from the hdhomerun library.*

Only package sources are provided.  You must build the DEB's yourself.  It's
not hard, but I highly recommend you read the [Debian Maintainers build guide][5].

    $ git clone https://github.com/pierre-quelin/dvbhdhomerun
    $ cd dvbhdhomerun && dpkg-buildpackage -b
    $ cd ..
    $ sudo dpkg -i dvbhdhomerun-source_<version>_all.deb
    $ sudo dpkg -i dvbhdhomerun-dpkg_<version>_all.deb
    $ sudo nano /etc/dvbhdhomerun
    $ sudo dpkg -i dvbhdhomerun-utils_<version>_<arch>.deb
    $ apt-get moo

[1]: https://www.silicondust.com/
[2]: https://www.silicondust.com/support/linux/
[3]: http://sourceforge.net/projects/dvbhdhomerun/
[4]: mailto:tfylliv@gmail.com
[5]: https://www.debian.org/doc/manuals/maint-guide/build.en.html