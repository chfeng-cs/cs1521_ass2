cd examples
for file in `ls *compressed.blob `; do
	echo $file
	hexdump $file | head -2
done
