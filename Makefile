d: compile
	./blobby -x examples/3_files.subdirectory.blob

good: compile
	@ ./blobby -l examples/3_files.blob

bad: compile
	@ ./blobby -l examples/3_files.bad_magic.blob

compile:
	@ dcc -o blobby blobby.c

rm:
	rm -f last_goodbye.txt hello.txt these_days.txt
