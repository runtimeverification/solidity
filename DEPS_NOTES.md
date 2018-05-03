## Debian(stretch) 

* llvm-5.0 not available in default sources
* https://blog.sleeplessbeastie.eu/2017/11/06/how-to-use-backports-repository/

To add official sources (http://apt.llvm.org/):


```
cat <<EOF | sudo tee /etc/apt/sources.list.d/stretch-llvm-5.0.list 
deb http://apt.llvm.org/stretch/ llvm-toolchain-stretch-5.0 main
EOF
```

```
/vagrant/solidity/libiele/IeleIntConstant.cpp: In member function ‘virtual void dev::iele::IeleIntConstant::print(llvm::raw_ostream&, unsigned int) const’:
/vagrant/solidity/libiele/IeleIntConstant.cpp:40:11: error: ‘str’ is not a member of ‘boost’
     OS << boost::str(boost::format("%1$#x") % Val);
           ^~~~~
/vagrant/solidity/libiele/IeleIntConstant.cpp:40:22: error: ‘format’ is not a member of ‘boost’
     OS << boost::str(boost::format("%1$#x") % Val);
                      ^~~~~
libiele/CMakeFiles/iele.dir/build.make:278: recipe for target 'libiele/CMakeFiles/iele.dir/IeleIntConstant.cpp.o' failed
make[2]: *** [libiele/CMakeFiles/iele.dir/IeleIntConstant.cpp.o] Error 1
CMakeFiles/Makefile2:389: recipe for target 'libiele/CMakeFiles/iele.dir/all' failed
make[1]: *** [libiele/CMakeFiles/iele.dir/all] Error 2
Makefile:127: recipe for target 'all' failed
make: *** [all] Error 2
```




