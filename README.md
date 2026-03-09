# JX-10-note-to-LED-display

This is a simple interface to the serial port of the Roland JX-10 between the assigner board and the voice cards.

It is effectively like a MIDI interface and I was able to pull the notes out of the stream and convert them to LEDs 

6 red LEDs for the lower voice board and 6 green LEDs for the upper voice board, these are located in the first 6 patch buttons of my rack build.

The interface is simple, it just sets Serial to 31250 baud, 8 bits, no parity just like MIDI and connects to the serial wire from the assigner to a voice card.

The signal is converted to 3.3v with a CD4504 and fed into the Arduino R4 stamp.

The stamp drives a pair of 74HC595 shift registers and the first 12 outputs are the 6 lower and 6 upper notes.
