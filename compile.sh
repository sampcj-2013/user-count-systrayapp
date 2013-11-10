windres -i resource.rc -o resource.o
g++  -o main.exe u_main.cpp resource.o -mwindows -I/local/include -L/local/lib -lcurl -ljansson -lcomctl32 -O3
strip main.exe
# upx main.exe
