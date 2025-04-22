# nbtgTimer

firmware for the upcoming f-stop timer by www.nbtg.dev

## why?
existing f-stop timers are either expensive, or lacking the features I'm looking for. so why not build my own?

* based on STM32G0, low cost
* interrupt-driven logic to ensure accurate timing
* safety features such as a snubber circuit on the output, discharge capacitors, etc
* open-source and extensible under MIT

## features

* oled display, red filter, dedicated on/off toggle for color work
  * possibly swappable filters if I can source a proper 598nm one that doesn't cost three times what the hardware itself costs lol 
* linear or F-stop mode
  * F-stop timing resolution of 1/6th stop, 1/12th is a possibility
* 100ms resolution
  * can reliably be increased down to 10ms or so, but 100ms seems granular enough for me
* test strip mode
  * give it a base time and a resolution, it gives you -3/N/+3 times in sequence. defaults to 7 steps including base time
* 1m, 10s, 1s, .1s granularity, + and -
* split-grade timing
* dry-down correction
* footswitch support, configurable
* metronome on configurable interval
* saveable settings in EEPROM
