# AGRESS = Audio + proGRESSive

## Compiling and Installing

     Before you start compiling, make sure that
     the glib-2.0 library is installed, as well as the library
     popt to parse command line options.
    
     Compilation and installation is as usual:
     
```bash
    # tar -xazf agress-0.1.1.tar.gz
    # cd agress-0.1.1
    # autoreconf -fiv
    # ./configure
    # make
    # make install
```

    See also the file INSTALL

## Program description
     
    This program allows you to progressively encode audio files.
    Progressive ordering of data is very useful in the transmission
    streaming audio over the Internet: when the baud rate drops
    there will be no buffer emptying and, as a consequence, there will not be
    "stuttering" playback, but only temporarily deteriorate the quality.
    
    In addition, progressive ordering allows you to specify exactly
    compression ratio: discarding the "tail" of the encoded frame,
    will only result in some degradation of the playback quality.

    The package includes two programs: agcodec and agplay. As follows from
    name, the first is the encoder / decoder, and the second is the player.

    The name AGRESS is obtained if you play with the words Audio and
    proGRESSive.

    Implemented encoder is extremely simple: the source file is divided into frames
    (the size of which can be specified on the command line), then to each
    the frame uses the Daubechies wavelet transform 9/7, and finally,
    SPIHT coding algorithm. Psychoacoustic modeling has not yet
    is applied. In other words, this project is rather
    demonstrative character.

    The encoder supports files of wav format: 8 or 16 bits, stereo or
    mono mode, sample rate - any. Play received
    agress file can be used with the player agplay, which uses
    sound subsystem OSS.
    
## Feedback

     I would be very interested to hear your feedback / opinions / suggestions
     about this program and the concept itself in general.

     Copyleft (C) 2004, Alexander Simakov
     
    http://www.entropyware.info
    xander@entropyware.info
