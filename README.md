# JpnCap

JpnCap is a simple stand-alone tool for GNU/Linux systems that can
capture Japanese text on screen though character recognition lets you
immediately look up this or any text from your clipboard in various
dictionaries, mainly JMdict.

## Dependencies
* gtk3
* tesseract-ocr and tesseract-ocr-jpn data
* sqlite3

## Build
In the jpncap directory execute
* `mkdir build && cd build`
* `cmake .. && make`
and as a super user
* `make install`

## Generating standard dictionary files
In order to look up words, you will need a dictionary file. In the
jpncap directory execute
* `cd dict_gen`
* `curl http://ftp.monash.edu.au/pub/nihongo/JMdict.gz | gzip -d > JMdict.xml`
* `python3 generate_jm_dict.py -l english JMdict.xml dict.db`
* `curl http://www.edrdg.org/kanjidic/kanjidic2.xml.gz | gzip -d > kanjidic2.xml`
* `python3 generate_kanji_dict.py kanjidic2.xml dict.db`
This will create a file `dict.db` which needs to be copied to the share
directory of the installation, by default
* `cp dict.db /usr/local/share/jpncap/dict.db`
In the third line you can also add other langauges with additional `-l`
switches, for exmaple for Japanese-English and Japanese-German
* `python3 generate_jm_dict.py -l english -l german JMdict.xml dict.db`

