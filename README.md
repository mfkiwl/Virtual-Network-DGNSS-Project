# Virtual-Network-DGNSS-Project
This project provides a software implementation for a publicly available, open-source client/server VN-DGNSS system with global coverage. The server receives real-time information in SSR format and supplies clients with virtual base station RTCM OSR formatted messages. The system currently generates OSR messages for GPS L1, BeiDou B1, and Galileo E1. This solution eliminates the need for clients to have access to a local physical reference station.

keywords: PPP, Differential GNSS, Lane-level positioning, Virtual Base Station

Contact: whu027@ucr.edu

Feel free to contact me if you have any questions. Collaboration is greatly welcomed and appreciated.

Copyright (c) 2020 UC Regents

## I. Paper
If you are interested in this work, please cite our [paper](https://ieeexplore.ieee.org/document/9811415)
```
@article{wang2022using,
  author={Hu, Wang and Neupane, Ashim and Farrell, Jay A.},
  journal={IEEE Transactions on Vehicular Technology}, 
  title={{Using PPP Information to Implement a Global Real-Time Virtual Network DGNSS Approach}}, 
  year={2022},
  volume={71},
  number={10},
  pages={10337-10349},
  doi={10.1109/TVT.2022.3187416}
}
```

## II. Code Implementation Contributors
* Wang Hu - [GitHub](https://github.com/Azurehappen)
* Xiaojun Dong - [GitHub](https://github.com/Akatsukis)
* Ashim Neupane - [GitHub](https://github.com/ashimneu)
* Dan Vyenielo - [GitHub](https://github.com/dvnlo)
* Farzana Rahman - [GitHub](https://github.com/FarzanaRahman)
* Jay Farrell - [GitHub](https://github.com/jaffarrell)

## III. Server Setup
### System Requirement
Ubuntu 18.04
#### Attention: The executable file provided here was compiled from X86 Linux. If you are using ARM or other architecture, you should compile this project before using it.
### Setup
Compiling packages are needed: gcc, g++, cmake. If not, do "sudo apt install xxx".

1. Install packages
(If you use Ubuntu 20. LTS, do 'sudo add-apt-repository ppa:rock-core/qt4' first)
```
sudo apt update
sudo apt install libcurl4-openssl-dev libqtwebkit4 libqt4-svg
sudo apt-get install zlib1g-dev
sudo ldconfig
```

2. Compile VN-DGNSS server
Go to Virtual-Network-DGNSS-Project/VN_DGNSS_Server, then
```
mkdir build
cd build/
cmake ..
make
```
Run VN-DGNSS server: Set the IP address and port
```
./server (IP) (Port)
```
If you would like to connect it to the external network. You may use 'ifconfig' to find your internal IP address. in your router 'port forwarding' setup, forward your internal IP address&port to the external port.

3. Install BKG Ntrip Client  
Download the BKG software version at 'https://igs.bkg.bund.de/root_ftp/NTRIP/software/'.
For example:
In a new terminal,
```
sudo apt-get install libcanberra-gtk-module
wget https://igs.bkg.bund.de/root_ftp/NTRIP/software/bnc-2.12.17-ubuntu-64bit-shared.zip
```
Unzip the folder  
```
unzip bnc-2.12.17-ubuntu-shared.zip  
```
Move to **./bnc-2.12.17-ubuntu-shared/** directory and run BNC.  
```
cd bnc-2.12.17-ubuntu-shared  
./bnc-2.12.17-ubuntu-shared  
```

4. BNC software setup
* set up the local IP port. Port number 6699 for the **Broadcast Corrections** and 3536 for **RINEX Ephemeris**. Please select 'version 3' on the **RINEX Ephemeris** tab page. If you would like to change the port numbers, the port numbers configured in the server also need to be changed.
* setup the stream: click **Add Stream** -> **Caster**
* The recommended NTRIP Caster: Caster host (products.igs-ip.net), Caster port (2101), Ntrip Version (2)
* Fill in "User" and "Password". (You may need an account for SSR stream usage. Please check http://www.igs-ip.net/home for registration.)
* Click **Get table**
* Click an navigation message stream (Defaul: BCEP00BKG0) then click **select**.
* Click SSR message streams (SSR00CNE0 and SSR00WHU0) then click **select**. (Currently, by default, VN-DGNSS use Orbit/Clock info from WHU and VTEC product from CNES)
* Run (click **Start**) the BNC software.

## IV. Client Setup
The executable file "VN_DGNSS_Client" for Linux (generated from Ubuntu 18.04) is located at VN_DGNSS_Client/bin.
The executable file "VN_DGNSS_Client.exe" for Windows (generated from Windows 10) is located at VN_DGNSS_Client/app.

Please check 'VN_DGNSS_Client/README' for details.

## V. System Architecture
![Server-client](https://user-images.githubusercontent.com/45580484/131876233-beb25066-cfce-431e-8ec3-81182328b99f.png)

PPP model usage:
* Satellite orbit and clock corrections: SSR orbit and clock products
* Satellite hardware bias correction: CAS GIPP Observable-specific Code Biases (OSB) product
* Ionosphere correction model: SSR VTEC using Spherical Harmonic expansion
* Troposphere correction model: IGGtrop_SH


## VI. Experimental results
**For detailed descriptions and conclusions, please check Sec. VI in the VN-DGNSS paper.**

<img width="487" alt="Screen Shot 2021-09-02 at 11 53 19 PM" src="https://user-images.githubusercontent.com/45580484/131876595-1893578b-9f5d-4c52-b21b-2a236332e23b.png">

### Stationary platform results

![stationary_all](https://user-images.githubusercontent.com/45580484/131878787-eb9e0861-bc43-4383-877f-7f986f3ca5d4.jpg)

### Moving platform results
* With single-band antenna
![moving_SF_err](https://user-images.githubusercontent.com/45580484/133939394-121a02df-67ca-4b08-b509-51eae1b1424b.jpg)

* With dual-band antenna
![moving_DF_err](https://user-images.githubusercontent.com/45580484/133939402-ce4681fc-1a77-4f08-9cb8-79598004f1c7.jpg)

## VII. Implementation Notes
1. The BKG data stream will provide both I/NAV and F/NAV for Galileo. In terms of IGS SSR standard, this VN-DGNSS server will only support I/NAV ephemeris. 
2. The RTCM message function was originally referred from RTKLIB but was modified to our specific purpose. 
3. No Cycle-Slip function is needed since VN-DGNSS only generates the GNSS code measurements. 
4. Note: In terms of RINEX 3.04. BDS System Time Week has a rollover after 8191. Galileo System Time (GST) week has a rollover after 4095. currently, this code doesn't consider the rollover since it will be valid after many years. GAL week = GST week + 1024 + n*4096 (n: number of GST roll-overs).

## VIII. Acknowledge
The ideas reported herein originated during a project supported by SiriusXM. The client-server implementation project was supported by Caltrans under agreement number 65A0767.
IGGtrop_SH model data and original MATLAB functions are provided by the author of IGGtrop Dr. Wei Li, liwei@whigg.ac.cn.
Some portions of the VN-DGNSS (e.g., RTCM message generator) are modifications of open-source functions in RTKLIB (https://github.com/tomojitakasu/RTKLIB).
Functions of the Spherical Harmonic expansion ionosphere correction model are modifications of functions in BNC (https://igs.bkg.bund.de/ntrip/download).
