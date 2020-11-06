# cd examples
# for file in `ls *compressed.blob `; do
# 	echo $file
# 	hexdump $file | head -2
# done
for file in `ls |egrep -v "(Makefile|1.sh|.gitignore|blobby.c|examples|stat.c|blobby)"`; do
	# echo $file
	rm -rf $file
done
