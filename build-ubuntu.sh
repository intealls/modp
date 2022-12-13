apt update
apt install -y build-essential git cmake libsdl2-dev libarchive-dev portaudio19-dev libxmp-dev libopenmpt-dev libfftw3-dev libsidplayfp-dev
git clone https://bitbucket.org/mpyne/game-music-emu.git
cd game-music-emu/
patch -p1 < ../contrib/gme-b3d158a-playlist.patch
mkdir build
cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/usr/local
make && make install
cd ..
./configure
make
