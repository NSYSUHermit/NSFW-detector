avr-gcc -mmcu=atmega168 -DF_CPU=1000000UL -Os test_ultrasonic.c -o test.elf     
avr-objcopy -O ihex -R .eeprom test.elf test.hex
avrdude -c usbasp -p m168 -U flash:w:test.hex