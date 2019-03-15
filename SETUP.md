INSTALLATION INSTRUCTIONS
=========================

Follow the instructions to download and run simulations using this ndnDIM codebase:

- Install on latest edition of Ubuntu Desktop/Server version 16 LTS

- First install pre-requisites

  sudo apt-get install build-essential libcrypto++-dev libsqlite3-dev libboost-all-dev libssl-dev

- Make a directory for the simulation. You can use your own name for the simulation directory

  mkdir ndnSIM

  cd ndnSIM

- Clone the repositories from NSOL GitHub website

  git clone --recursive https://github.com/nsol-nmsu/ns3-smartgrid.git ns-3

  git clone --recursive https://github.com/nsol-nmsu/ndnSIM.git ns-3/src/ndnSIM

- Switch to the latest codebase for ns-3

  cd ns-3

  git checkout ndnSIM-v2

- Switch to the latest codebase for ndnSIM

  cd ns-3/src/ndnSIM

  git checkout nsol-v2

- Switch to the latest codebase for NFD

  cd ns-3/src/ndnSIM/NFD

  git checkout nsol-v2

- Switch to the latest codebase for ndn-cxx

  cd ns-3/src/ndnSIM/ndn-cxx

  git checkout nsol-v2

- Change directory back to ns-3

  cd ns-3

- Configure the source code, you may see some warnings but they can be ignored
 
  sudo ./waf configure --enable-examples

- Compile the code
 
  sudo ./waf

- To run a simple scenario, use below command. 
  This scenario file is located in "ns-3/src/ndnSIM/examples/ndn-simple.cpp" and can be modified as desired
  
  NS_LOG=ndn.SpontaneousProducer:ndn.Subscriber ./waf --run ndn-simple

