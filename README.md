ICN - ndnSIM
============


Before cloning this repository follow the following steps:-

1. Install libboost dependency for NDN-Sim. Make sure that all other version of boost libraries (-dev packages) are removed, otherwise compilation might fail.
```
sudo apt-get install python-software-properties
sudo add-apt-repository ppa:boost-latest/ppa
sudo apt-get update
sudo apt-get install libboost1.55-all-dev
```

2. Install other dependencies.
```
sudo apt-get install python-dev python-pygraphviz python-kiwi
sudo apt-get install python-pygoocanvas python-gnome2
sudo apt-get install python-rsvg ipython
```

3. Clone NS-3 and NDN-Sim. 
```
mkdir ndnSIM
cd ndnSIM
git clone https://github.com/cawka/ns-3-dev-ndnSIM.git ns-3
git clone https://github.com/cawka/pybindgen.git pybindgen
git clone https://github.com/sharma-dhrv/ibf-ndnsim.git ns-3/src/ndnSIM
```

4. Compile NS-3 with NDN-Sim.
```
cd ns-3
./waf configure --enable-examples
./waf
```
If waf configure fails do this: 
```
sudo apt-get install libcrypto+ sqlite3 libsqlite3-dev
```

5. Run an example senario to test the build.
```
./waf --run=ndn-simple
```
OR
```
./waf --run=ndn-grid
```

Experimental Report and Presentation
--------------------------------------
Presentation : https://docs.google.com/presentation/d/1NGQp8hbmcpTgwTbbILyhigETufvyZZ3denDbExKEFlA/edit?usp=sharing
Report : https://docs.google.com/viewer?a=v&pid=sites&srcid=ZGVmYXVsdGRvbWFpbnxzaGFybWFkaHJ2fGd4OjIxODY4ZDJhZDY3M2NhNGM

Credits
--------

1. Our work of porting semi-stateless forwarding using in-packet bloom filters is based on Christos Tsilopoulos’s Doctoral Thesis (http://mm.aueb.gr/phd_theses/2016_Tsilopoulos.pdf) and their research paper "C. Tsilopoulos, Y. Thomas and G. Xylomenos, 'Reducing Forwarding State in Content-Centric Networks with Semi-Stateless Forwarding' in Proc. of IEEE INFOCOM, pp. 2067-2075, 2014."

2. ndnSIM and NFD developer teams.

About ndnSIM
--------------

A new release of [NS-3 based Named Data Networking (NDN) simulator](http://ndnsim.net/1.0/)
went through extensive refactoring and rewriting.  The key new features of the new
version:

- Packet format changed to [NDN Packet Specification](http://named-data.net/doc/ndn-tlv/)

- ndnSIM uses implementation of basic NDN primitives from
  [ndn-cxx library (NDN C++ library with eXperimental eXtensions)](http://named-data.net/doc/ndn-cxx/)

- All NDN forwarding and management is implemented directly using source code of
  [Named Data Networking Forwarding Daemon (NFD)](http://named-data.net/doc/NFD/)

- Allows [simulation of real applications](http://ndnsim.net/2.1/guide-to-simulate-real-apps.html)
  written against ndn-cxx library

[ndnSIM documentation](http://ndnsim.net)
---------------------------------------------

For more information, including downloading and compilation instruction, please refer to
http://ndnsim.net or documentation in `docs/` folder.
