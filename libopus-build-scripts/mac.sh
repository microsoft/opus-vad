OPUS_VERSION=1.3.1

echo "Installing dependencies...."
brew update
brew install doxygen
brew install maven
brew install sox

INSTALL_DIR="../dist/mac"

## Verify the path to Java on your Mac...
export JAVA_HOME=$(/usr/libexec/java_home)
export DYLD_LIBRARY_PATH=.
echo "DONE"
echo


echo "Building Nuance Voice Activity Detector documentation"
cd ..
doxygen
cd libopus-build-scripts
echo "DONE"
echo

echo "Installing opus-${OPUS_VERSION}"
curl -LOk https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz
tar -zxvf opus-${OPUS_VERSION}.tar.gz
cd opus-${OPUS_VERSION}/ && patch -p1 < ../opus.patch
./configure
make ; make install
cp .libs/libopus.a ../${INSTALL_DIR}
cd ..
echo "DONE"
echo

echo "Making OpusVADLib...."
cd ..
make
make libopusvadjava.dylib
mv *.dylib ./dist/mac
echo "DONE"
echo


echo "Making OpusVADTool..."
cd samples/C
make
ln -sf ../../libopus-build-scripts/opus-${OPUS_VERSION}/.libs/*.dylib ../../dist/mac/*.dylib . 2>/dev/null

## Try running opusvadtool...
./opusvadtool -h

## If successful, you will see the following output
# Usage: ./opusvadtool [-h] -f <infile> [-s sos] [-e eos] [-c complexity] [-b bit_rate_type] [-t speech sensitivity threshold] [-a] [-n]
# Input file must be 16000 Hz 16 bit signed little-endian PCM mono
# If <infile> is "-", input is read from standard input
# -s       start of speech window in ms. Default: 220
# -e       end of speech window in ms. Default: 900
# -c       opus vad encoding complexity level (0-10). Default: 3
# -b       opus vad bit rate type (0 = VBR, 1 = CVBR, 2 = CBR). Default: 1
# -t       speech detection sensitivity parameter (0-100). Specify 0 for least sensitivity, 100 for most. Default: 20
# -a       <infile> is treated as IMA-ADPCM 4bit 16kHz
# -n       specify high-nibble order for adpcm encoded <infile>
# Examples:
#         sox in.wav -r 16000 -b 16 -e signed -L -c 1 -t raw - | ./opusvadtool -f - -e 400
#         sox in.wav -t ima -e ima-adpcm -r 16000 -c 1 - | ./opusvadtool -f -a
#         sox in.wav -t ima -e ima-adpcm -r 16000 -c 1 -N - | ./opusvadtool -f - -a -n
#         sox in.wav -r 16000 -b 16 -e signed -L -c 1 -t raw - | ./opusvadtool -f - -t 30 -e 700

cd ../..
echo "DONE"
echo

echo "Making java lib"
cd java
mvn clean install 
cd ..


echo "Making OpusVADJava..."
cd ./samples/java
mvn clean install
ln -sf ../../libopus-build-scripts/opus-${OPUS_VERSION}/.libs/*.dylib ../../dist/mac/*.dylib . 2>/dev/null

## Try running opusvadjava...
java -jar target/Main-0.0.1-jar-with-dependencies.jar -f in.pcm

## If successful, you will see the following output
# Frame bytes: 640
# Buffer size (bytes): 8960
# [7362ca10-f88e-4ac3-b656-07844707cfe5] OPUSVAD_SOS pos: 120

echo "DONE"
echo

echo "Installation is complete!"
echo
rm  ../../obj/*
