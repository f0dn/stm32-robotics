run: compile_flags.txt
	@pio -f run --target upload --target monitor

upload: compile_flags.txt
	@pio -f run --target upload

clean:
	@pio -f run --target clean

compile_flags.txt: platformio.ini
	@pio project init --ide vim
	@echo "-fgnuc-version=6.4" >> .ccls
	@mv .ccls compile_flags.txt
