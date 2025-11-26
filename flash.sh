avr-gcc -mmcu=atmega168 -DF_CPU=1000000UL -Os finals.c -o proj.elf
avr-objcopy -O ihex -R .eeprom proj.elf proj.hex
avrdude -c usbasp -p m168 -U flash:w:proj.hex
