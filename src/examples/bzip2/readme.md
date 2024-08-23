# Dependencies
This application requires the bzip2 library.

```
cd <project_root>/libs
curl -L https://sourceware.org/pub/bzip2/bzip2-latest.tar.gz -o bzip2.tar.gz
tar -xf bzip2.tar.gz
rm bzip2.tar.gz
mv bzip2-1.0.8 bzip2
cd bzip2
make && make install PREFIX=$HOME/.local
```