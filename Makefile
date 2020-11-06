c: compile
	@ ./blobby -x examples/small.compressed.blob 
c0: compile
	./blobby -c a.blob code

d: compile
	./blobby -x examples/text_file.blob



good: compile
	@ ./blobby -l examples/3_files.blob

bad: compile
	@ ./blobby -l examples/3_files.bad_magic.blob

compile: blobby
	@ gcc -o blobby blobby.c

rm:
	rm -f last_goodbye.txt hello.txt these_days.txt
up:
	scp blobby.c unsw:~/1521/blobby/
