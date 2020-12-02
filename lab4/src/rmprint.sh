cat pipe.c > temp.txt
grep -v "printf" temp.txt > pipe.c
cat bp.c > temp.txt
grep -v "printf" temp.txt > bp.c
cat cache.c > temp.txt
grep -v "printf" temp.txt > cache.c
rm temp.txt
