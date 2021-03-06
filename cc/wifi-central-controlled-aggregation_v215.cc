/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 Jose Saldana, University of Zaragoza (jsaldana@unizar.es)
 *
 * If you use this code, please cite the next research article (final reference still pending):
 *
 * Jose Saldana, Omer Topal, Jose Ruiz-Mas, Julian Fernandez-Navajas, "Finding the Sweet Spot
 * for Frame Aggregation in 802.11 WLANs," IEEE Communications Letters, vol. 25, no. 4, pp. 1368-1372,
 * April 2021, doi: 10.1109/LCOMM.2020.3044231.
 *
 * https://ieeexplore.ieee.org/document/9291427
 * Author's self-archive version: https://github.com/wifi-sweet-spot/ns3/blob/main/LCOMM3044231_authors_version.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Some parts are inspired on https://www.nsnam.org/doxygen/wifi-aggregation_8cc.html, by Sebastien Deronne
 * Other parts are inspired on https://www.nsnam.org/doxygen/wifi-wired-bridging_8cc_source.html
 * The flow monitor part is inspired on https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
 * The association record is inspired on https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
 * The hub is inspired on https://www.nsnam.org/doxygen/csma-bridge_8cc_source.html
 *
 * v215
 * Developed and tested for ns-3.30.1, https://www.nsnam.org/releases/ns-3-30/
 */


// PENDING
/*
1) make the 'unset assoc' change the channel to that of the closest AP

Two possibilities:
- WORKING: the STA deassociates by itself. I have to use Yanswifi.
- Not working yet: That it needs the new AP to send beacons to it. In this second option,
  the command 'addoperationalchannel' should be used on each STA. 
  see https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#a948c6d197accf2028529a2842ec68816

  the function AddOperationalChannel has been removed in ns-3.30.1


2) To separate this file into a number of them, using .h files.
*/


//
// The network scenario includes
// - a number of STA wifi nodes, with different mobility patterns
// - a number of AP wifi nodes. They are distributed in rows and columns in an area. An AP may have two WiFi cards
// - a number of servers: each of them communicates with one STA (it is the origin or the destination of the packets)
//
// On each AP node, there is
// - a csma device
// - a wifi device (or two)
// - a bridge that binds both interfaces the whole thing into one network
// 
// IP addresses:
// - the STAs have addresses 10.0.0.0 (mask 255.255.0.0)
// - the servers behind the router (only in topology 2) have addresses 10.1.0.0 (mask 255.255.0.0) 
//
// There are three topologies:
//
// topology = 0
//
//              (*)
//            +--|-+                      10.0.0.0
//    (*)     |STA1|           csma     +-----------+  csma  +--------+ 
//  +--|-+    +----+    +---------------|   hub     |--------| single | All the server applications
//  |STA0|              |               +-----------+        | server | are in this node
//  +----+              |                         |          +--------+    
//                      |                     csma|                        
//        +-------------|--+         +------------|--+                 
//        | +----+  +----+ |         | +----+ +----+ |                 
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 
//        | +----+  +----+ |         | +----+ +----+ |                 
//        |   |       |    |         |   |      |    |                 
//        |  +----------+  |         |  +---------+  |                 
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  
//               AP 0                       AP 1                          
//
//
//
// topology = 1
//
//              (*)
//            +--|-+                      10.0.0.0                       
//    (*)     |STA1|           csma     +-----------+  csma         
//  +--|-+    +----+    +---------------|   hub     |----------------------------------------+
//  |STA0|              |               +-----------+----------------------+                 |
//  +----+              |                         |                        |                 |
//                      |                     csma|                        |                 |
//        +-------------|--+         +------------|--+                 +--------+      +--------+
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 | |CSMA| |      | |CSMA| |  ...
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//        |   |       |    |         |   |      |    |                 |        |      |        |
//        |  +----------+  |         |  +---------+  |                 +--------+      +--------+
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  server 0        server 1
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  talks with      talks with
//               AP 0                       AP 1                          STA 0           STA 1
//
//
//
// topology = 2 (DEFAULT)
//                  
//              (*)                                 10.0.0.0 |        | 10.1.0.0
//            +--|-+                                      <--+        +-->
//            |STA1|
//    (*)     +----+           csma     +-----------+  csma  +--------+       point to point
//  +--|-+              +---------------|   hub     |--------| router |----------------------+
//  |STA0|              |             0 +-----------+ 2      |        |----+                 |
//  +----+              |                         |1         +--------+    |                 |
//                      |                     csma|                        |                 |
//        +-------------|--+         +------------|--+                 +--------+      +--------+
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 | |CSMA| |      | |CSMA| |  ...
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//        |   |       |    |         |   |      |    |                 |        |      |        |
//        |  +----------+  |         |  +---------+  |                 +--------+      +--------+
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  server 0        server 1
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  talks with      talks with
//               AP 0                       AP 1                          STA 0           STA 1
//
//

// When the default aggregation parameters are enabled, the
// maximum A-MPDU size is the one defined by the standard, and the throughput is maximal.
// When aggregation is limited, the throughput is lower
//
// Packets in this simulation can be marked with a QosTag so they
// will be considered belonging to  different queues.
// By default, all the packets belong to the BestEffort Access Class (AC_BE).
//
// The user can select many parameters when calling the program. Examples:
// ns-3.27$ ./waf --run "scratch/wifi-central-controlled-aggregation --PrintHelp"
// ns-3.27$ ./waf --run "scratch/wifi-central-controlled-aggregation --number_of_APs=1 --nodeMobility=1 --nodeSpeed=0.1 --simulationTime=10 --distance_between_APs=20"
//
// if you want different results in different runs, use a different seed each time you call the program
// (see https://www.nsnam.org/docs/manual/html/random-variables.html). One example:
//
// ns-3.26$ NS_GLOBAL_VALUE="RngRun=3" ./waf --run "scratch/wifi-central-controlled-aggregation --simulationTime=2 --nodeMobility=3 --verboseLevel=2 --number_of_APs=10 --number_of_APs_per_row=1"
// you can call it with different values of RngRun to obtain different realizations
//
// for being able to see the logs, do ns-3.26$ export NS_LOG=UdpEchoClientApplication=level_all
// or /ns-3-dev$ export 'NS_LOG=ArpCache=level_all'
// or /ns-3-dev$ export 'NS_LOG=ArpCache=level_error' for showing only errors
// see https://www.nsnam.org/docs/release/3.7/tutorial/tutorial_21.html
// see https://www.nsnam.org/docs/tutorial/html/tweaking.html#usinglogging

// Output files
//  You can establish a 'name' and a 'surname' for the output files, using the parameters:
//    --outputFileName=name --outputFileSurname=seed-1
//
//  You can run a battery of tests with the same name and different surname, and you
//  will finally obtain a single file name_average.txt with all the averaged results.
//
//  Example: if you use --outputFileName=name --outputFileSurname=seed-1
//  you will obtain the next output files:
//
//    - name_average.txt                            it integrates all the tests with the same name, even if they have a different surname
//                                                  the file is not deleted, so each test with the same name is added at the bottom
//    - name_seed-1_flows.txt                       information of all the flows of this run
//    - name_seed-1_flow_1_delay_histogram.txt      delay histogram of flow #1
//    - name_seed-1_flow_1_jitter_histogram.txt
//    - name_seed-1_flow_1_packetsize_histogram.txt
//    - name_seed-1_KPIs.txt                        text file reporting periodically the KPIs (generated if aggregationDynamicAlgorithm==1)
//    - name_seed-1_positions.txt                   text file reporting periodically the positions of the STAs
//    - name_seed-1_AMPDUvalues.txt                 text file reporting periodically the AMPDU values (generated if aggregationDynamicAlgorithm==1)
//    - name_seed-1_flowmonitor.xml
//    - name_seed-1_AP-0.2.pcap                     pcap file of the device 2 of AP #0
//    - name_seed-1_server-2-1.pcap                 pcap file of the device 1 of server #2
//    - name_seed-1_STA-8-1.pcap                    pcap file of the device 1 of STA #8
//    - name_seed-1_hub.pcap                        pcap file of the hub connecting all the APs


#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/nstime.h"
#include "ns3/spectrum-module.h"    // For the spectrum channel
#include <ns3/friis-spectrum-propagation-loss.h>
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/random-variable-stream.h"
#include <sstream>
#include <iomanip>

//#include "ns3/arp-cache.h"  // If you want to do things with the ARPs
//#include "ns3/arp-header.h"

using namespace ns3;

#define VERBOSE_FOR_DEBUG 0

// Maximum AMPDU size of 802.11n
#define MAXSIZE80211n 65535

// Maximum AMPDU size of 802.11ac
//https://groups.google.com/forum/#!topic/ns-3-users/T_21O5mGlgM
//802.11ac allows the maximum A-MPDU length to range from 8 KB to 1 MB.
// The maximum transmission length is defined by time, and is a little less than 5.5 microseconds. At the highest data rates for 802.11ac, 
//an aggregate frame can hold almost four and a half megabytes of data. Rather than represent such a large number of bytes in the PLCP header,
//which is transmitted at the lowest possible data rate, 802.11ac shifts the length indication to the MPDU delimiters that are transmitted 
//as part of the high-data-rate payload
// http://www.rfwireless-world.com/Tutorials/802-11ac-MAC-layer.html
// Max. length of A-MPDU = 2^13+Exp -1 bytes 
// Exp can range from 0 to 7, this yields A-MPDU to be of length:
// - 2^13 - 1 = 8191 (8KB)
// - 2^20 - 1 = 1,048,575 (about 1MB)
#define MAXSIZE80211ac 65535  // FIXME. for 802.11ac max ampdu size should be 4692480
                              // You can set it in src/wifi/model/regular-wifi-mac.cc
                              // https://www.nsnam.org/doxygen/classns3_1_1_sta_wifi_mac.html

#define STEPADJUSTAMPDUDEFAULT 1000  // Default value. We will update AMPDU up and down using this step size

#define AGGRESSIVENESS 10     // Factor to decrease AMPDU down

#define INITIALPORT_VOIP_UPLOAD     10000      // The value of the port for the first VoIP upload communication
#define INITIALPORT_VOIP_DOWNLOAD   20000      // The value of the port for the first VoIP download communication
#define INITIALPORT_TCP_UPLOAD      30000      // The value of the port for the first TCP upload communication
#define INITIALPORT_TCP_DOWNLOAD    40000      // The value of the port for the first TCP download communication
#define INITIALPORT_VIDEO_DOWNLOAD  50000      // The value of the port for the first video download communication

#define MTU 1500    // The value of the MTU of the packets

#define INITIALTIMEINTERVAL 1.0 // time before the applications start (seconds). The same amount of time is added at the end

#define HANDOFFMETHOD 0     // 1 - ns3 is in charge of the channel switch of the STA for performing handoffs 
                            // 0 - the handoff method is implemented in this script

// Define a log component
NS_LOG_COMPONENT_DEFINE ("SimpleMpduAggregation");

/*  this works, but it is not needed
static void SetPhySleepMode (Ptr <WifiPhy> myPhy) {
  myPhy->SetSleepMode();
  //if (myverbose > 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[SetPhySleepMode]\tSTA set to SLEEP mode" << std::endl;  
}

static void SetPhyOff (Ptr <WifiPhy> myPhy) {
  myPhy->SetOffMode();
  //if (myverbose > 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[SetPhyOff]\tSTA set to OFF mode" << std::endl;  
}

static void SetPhyOn (Ptr <WifiPhy> myPhy) {
  myPhy->ResumeFromOff();
  //if (myverbose > 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[SetPhyOn]\tSTA set to ON mode" << std::endl;   
}
*/

std::string getWirelessBandOfChannel(uint8_t channel) {
  // see https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)
  if (channel <= 14 ) {
    return "2.4 GHz";
  }
  else {
    return "5 GHz";
  }
}

std::string getWirelessBandOfStandard(enum ns3::WifiPhyStandard standard) {
  // see https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)
  if (standard == WIFI_PHY_STANDARD_80211a ) {
    return "5 GHz"; // 5GHz
  }
  else if (standard == WIFI_PHY_STANDARD_80211b ) {
    return "2.4 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211g ) {
    return "2.4 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211n_2_4GHZ  ) {
    return "2.4 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211n_5GHZ  ) {
    return "5 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211ac ) {
    return "5 GHz";
  }

  // if I am here, it means I have asked for an unsupported standard (unsupported by this program so far)
  NS_ASSERT(false);
}

ns3::WifiPhyStandard convertVersionToStandard (std::string my_version80211) {
  if (my_version80211 == "11n5") {
    return WIFI_PHY_STANDARD_80211n_5GHZ;
  }
  else if (my_version80211 == "11ac") {
    return WIFI_PHY_STANDARD_80211ac;
  }
  else if (my_version80211 == "11n2.4") {
    return WIFI_PHY_STANDARD_80211n_2_4GHZ;
  }
  else if (my_version80211 == "11g") {
    return WIFI_PHY_STANDARD_80211g;
  }
  else if (my_version80211 == "11a") {
    return WIFI_PHY_STANDARD_80211a;
  }
  // if I am here, it means I have asked for an unsupported version (unsupported by this program so far)
  NS_ASSERT(false);  
}

// returns
// 0  both bands are supported
// 2  2.4GHz band is supported
// 5  5GHz band is supported
std::string bandsSupportedBySTA(int numberWiFiCards, std::string version80211primary, std::string version80211secondary) {
  if (numberWiFiCards == 1) {
    // single WiFi card
    return getWirelessBandOfStandard(convertVersionToStandard(version80211primary));
  }
  else if (numberWiFiCards == 2) {
    // two WiFi cards
    if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "5 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        return "both"; // both bands are supported by the STA
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        std::cout << "ERROR: both interfaces of the STA are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the STA are in the same band
        return "";
      }
    }
    else if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "2.4 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        return "both"; // both bands are supported by the STA
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        std::cout << "ERROR: both interfaces of the STA are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the STA are in the same band
        return "";
      }
    }
  }
  else {
    // only 2 wireless cards are supported so far
    NS_ASSERT(false);
    return "";
  }
  return "";
}


// returns
// 0  both bands are supported
// 2  2.4GHz band is supported
// 5  5GHz band is supported
std::string bandsSupportedByTheAPs(int numberAPsSamePlace, std::string version80211primary, std::string version80211secondary) {
  if (numberAPsSamePlace == 1) {
    // single WiFi card
    return getWirelessBandOfStandard(convertVersionToStandard(version80211primary));
  }
  else if (numberAPsSamePlace == 2) {
    // two WiFi cards
    if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "5 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        return "both"; // both bands are supported by the AP
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        std::cout << "ERROR: both interfaces of the AP are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the AP are in the same band
        return "";
      }
    }
    else if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "2.4 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        return "both"; // both bands are supported by the AP
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        std::cout << "ERROR: both interfaces of the AP are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the AP are in the same band
        return "";
      }
    }
  }
  else {
    // only 2 wireless cards are supported so far
    NS_ASSERT(false);
    return "";
  }
  return "";
}


// Change the frequency of a STA
// Copied from https://groups.google.com/forum/#!topic/ns-3-users/Ih8Hgs2qgeg
// https://10343742895474358856.googlegroups.com/attach/1b7c2a3108d5e/channel-switch-minimal.cc?part=0.1&view=1&vt=ANaJVrGFRkTkufO3dLFsc9u1J_v2-SUCAMtR0V86nVmvXWXGwwZ06cmTSv7DrQUKMWTVMt_lxuYTsrYxgVS59WU3kBd7dkkH5hQsLE8Em0FHO4jx8NbjrPk
void ChangeFrequencyLocal ( NetDeviceContainer deviceslink, uint8_t channel, uint32_t mywifiModel, uint32_t myverbose ) {

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ChangeFrequencyLocal]\tChanging channel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                << "  to:  " << uint16_t(channel) << std::endl;

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0)
      std::cout << "[ChangeFrequencyLocal]\tWARNING: wifidevice IS NULL" << '\n';

    //Ptr<WifiPhy> phy0;
    //phy0->SetChannelNumber (channel); //https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#a2d13cf6ae4c185cae8516516afe4a32a

    // YansWifiPhy
    if (mywifiModel == 0) {
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      // check if the STA is already switching its channel
      if (phy0->IsStateSwitching()) {
        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is already switching its channel" << std::endl; 
      }
      else {
        // as the STA is NOT switching its channel automatically, I do it
        phy0->SetChannelNumber (channel);

        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tChanged channel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  to:  " << uint16_t(channel) << std::endl;      
      }

      // see https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#ac365794e06cc92ae1262cbe72b72213d
      phy0->SetOffMode();
      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                  << "  set to off mode  " << std::endl;    

      // see https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#adcc18a46c4f0faed2f05fe813cc75ed3
      phy0->ResumeFromOff();
      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                  << "  set to on mode  " << std::endl;
    }

    // spectrumWiFiPhy
    else {
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>(); 

      // check if the STA is already switching its channel
      if (phy0->IsStateSwitching()) {
        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is already switching its channel" << std::endl; 
      }
      else {
        // as the STA is NOT switching its channel automatically, I do it
        phy0->SetChannelNumber (channel);

        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tChanged channel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  to:  " << uint16_t(channel) << std::endl;
      }

      /*
      // schedule the switch OFF and ON of this phy - it works, but it is not needed
      Simulator::Schedule (Seconds(0.9), &SetPhySleepMode, phy0);
      Simulator::Schedule (Seconds(1.0), &SetPhyOff, phy0);
      Simulator::Schedule (Seconds(2.0), &SetPhyOn, phy0);
      //phy0->SetOffMode();
      */

      /*
      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                  << "  set to off mode  " << std::endl;   
      */

      //phy0->ResumeFromOff();
      /*
      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                  << "  set to on mode  " << std::endl;
      */
    }
  }
}


/*********** This part is only for the ARPs. Not used **************/
typedef std::pair<Ptr<Packet>, Ipv4Header> Ipv4PayloadHeaderPair;

static void PrintArpCache (Ptr <Node> node, Ptr <NetDevice> nd/*, Ptr <Ipv4Interface> interface*/)
{
  std::cout << "Printing Arp Cache of Node#" << node->GetId() << '\n';
  Ptr <ArpL3Protocol> arpL3 = node->GetObject <ArpL3Protocol> ();
  //Ptr <ArpCache> arpCache = arpL3->FindCache (nd);
  //arpCache->Flush ();

  // Get an interactor to Ipv4L3Protocol instance
   Ptr<Ipv4L3Protocol> ip = node->GetObject<Ipv4L3Protocol> ();
   NS_ASSERT(ip !=0);

  // Get interfaces list from Ipv4L3Protocol iteractor
  ObjectVectorValue interfaces;
  ip->GetAttribute("InterfaceList", interfaces);

  // For each interface
  uint32_t l = 0;
  for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
  {

    // Get an interactor to Ipv4L3Protocol instance
    Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
    NS_ASSERT(ipIface != 0);

    std::cout << "Interface #" << l << " IP address" <<  /*<< */'\n';
    l++;

    // Get interfaces list from Ipv4L3Protocol iteractor
    Ptr<NetDevice> device = ipIface->GetDevice();
    NS_ASSERT(device != 0);

    if (device == nd) {
    // Get MacAddress assigned to this device
    Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());

    // For each Ipv4Address in the list of Ipv4Addresses assign to this interface...
    for(uint32_t k = 0; k < ipIface->GetNAddresses (); k++)
      {
        // Get Ipv4Address
        Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();

        // If Loopback address, go to the next
        if(ipAddr == Ipv4Address::GetLoopback())
        {
          NS_LOG_UNCOND ("[PrintArpCache] Node #" << node->GetId() << " " << addr << ", " << ipAddr << "");
        } else {

          NS_LOG_UNCOND ("[PrintArpCache] Node #" << node->GetId() << " " << addr << ", " << ipAddr << "");

        Ptr<ArpCache> m_arpCache = nd->GetObject<ArpCache> ();
        m_arpCache = ipIface->GetObject<ArpCache> (); // FIXME: THIS DOES NOT WORK
        //m_arpCache = node->GetObject<ArpCache> ();
        //m_arpCache = nd->GetObject<ArpCache> ();
        //m_arpCache->SetAliveTimeout(Seconds(7));

        //if (m_arpCache != 0) {
          NS_LOG_UNCOND ("[PrintArpCache]       " << node->GetId() << " " << addr << ", " << ipAddr << "");
          AsciiTraceHelper asciiTraceHelper;
          Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
          m_arpCache->PrintArpCache(stream);
          m_arpCache->Flush();
          //ArpCache::Entry * entry = m_arpCache->Add(ipAddr);
          //entry->MarkWaitReply(0);
          //entry->MarkAlive(addr);
          //}
        }
      }
    }
  }
  Simulator::Schedule (Seconds(1.0), &PrintArpCache, node, nd);
}

// empty the ARP cache of a node
void emtpyArpCache(Ptr <Node> mynode, uint32_t myverbose)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();

  // do it only if the node has IP
  if ( ip != 0 ) {
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    uint32_t interfaceNum = 0;
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
      //ipIface->SetAttribute("ArpCache", PointerValue(arp));

      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
      arp->PrintArpCache(stream);

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[emtpyArpCache] Initial Arp cache of Node #" << mynode->GetId() 
                  << '\n';
            std::cout << "\t\t\tInterface # " << interfaceNum;
            std::cout << stream << '\n';
      }

      arp->Flush(); //Clear the ArpCache of all entries. It sets the default parameters again

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[emtpyArpCache] Modified Arp cache of Node #" << mynode->GetId()
                  << '\n';
        std::cout << "\t\t\tInterface # " << interfaceNum;
        std::cout << stream << '\n';
      }
      interfaceNum ++;
    }
  }
  // If you want to do this periodically:
  //Simulator::Schedule (Seconds(1.0), &emtpyArpCache, mynode, myverbose);
}

// Modify the ARP parameters of a node
void modifyArpParams(Ptr <Node> mynode, double aliveTimeout, uint32_t myverbose)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();

  // do it only if the node has IP
  if ( ip != 0 ) {
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    uint32_t interfaceNum = 0;
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
      //ipIface->SetAttribute("ArpCache", PointerValue(arp));

      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
      arp->PrintArpCache(stream);

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[modifyArpParams] Initial Arp parameters of Node #" << mynode->GetId() 
                  << '\n';
            std::cout << "\t\t\tInterface # " << interfaceNum
                      << '\n';

            Time mytime = arp->GetAliveTimeout();
            std::cout << "\t\t\t\tAlive Timeout [s]: " << mytime.GetSeconds()
                      << '\n';

            mytime = arp->GetDeadTimeout();
            std::cout << "\t\t\t\tDead Timeout [s]: " << mytime.GetSeconds() 
                      << '\n';
      }

      //arp->Flush(); //Clear the ArpCache of all entries. It sets the default parameters again
      //aliveTimeout = aliveTimeout + 1.0;
      Time alive = Seconds (aliveTimeout);
      arp->SetAliveTimeout (alive);

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[modifyArpParams] Modified Arp parameters of Node #" << mynode->GetId()
                  << '\n';

        std::cout << "\t\t\tInterface # " << interfaceNum
                  << '\n';

        Time mytime = arp->GetAliveTimeout();
        std::cout << "\t\t\t\tAlive Timeout [s]: " << mytime.GetSeconds()
                  << '\n';

        mytime = arp->GetDeadTimeout();
        std::cout << "\t\t\t\tDead Timeout [s]: " << mytime.GetSeconds() 
                  << '\n';
      }
      interfaceNum ++;
    }
  }
  // If you want to do this periodically:
  //Simulator::Schedule (Seconds(1.0), &modifyArpParams, mynode, aliveTimeout, myverbose);
}

void infoArpCache(Ptr <Node> mynode, uint32_t myverbose)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();
  if (ip!=0) {
    std::cout << Simulator::Now().GetSeconds() << '\t'
              << "[infoArpCache] Arp parameters of Node #" << mynode->GetId() 
              << '\n';

    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    uint32_t interfaceNum = 0;
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
      //ipIface->SetAttribute("ArpCache", PointerValue(arp));

      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
      arp->PrintArpCache(stream);

      if (myverbose) {
        std::cout << "\t\t\tInterface # " << interfaceNum
                  << '\n';

        Time mytime = arp->GetAliveTimeout();
        std::cout << "\t\t\t\tAlive Timeout [s]: " << mytime.GetSeconds()
                  << '\n';

        mytime = arp->GetDeadTimeout();
        std::cout << "\t\t\t\tDead Timeout [s]: " << mytime.GetSeconds() 
                  << '\n';
      }
      interfaceNum ++;
    }
  }
  // If you want to do this periodically:
  //Simulator::Schedule (Seconds(1.0), &infoArpCache, mynode, myverbose);
}

// Taken from here https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
// Two typos corrected here https://groups.google.com/forum/#!topic/ns-3-users/JRE_BsNEJrY

// It seems this is not feasible: https://www.nsnam.org/bugzilla/show_bug.cgi?id=187
void PopulateArpCache (uint32_t nodenumber, Ptr <Node> mynode)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();

  Ptr<Packet> dummy = Create<Packet> ();

  // Set ARP Timeout
  //arp->SetAliveTimeout (Seconds(3600 * 24 )); // 1-year
  //arp->SetWaitReplyTimeout (Seconds(200));

  // Populates ARP Cache with information from all nodes
  /*for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {*/

    std::cout << "[PopulateArpCache] Node #" << nodenumber << '\n';

    // Get an interactor to Ipv4L3Protocol instance

    Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();
    NS_ASSERT(ip !=0);

    // Get interfaces list from Ipv4L3Protocol iteractor
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    // For each interface
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++) {
      // Get an interactor to Ipv4L3Protocol instance
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
      NS_ASSERT(ipIface != 0);

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      // Get MacAddress assigned to this device
      Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());

      // For each Ipv4Address in the list of Ipv4Addresses assign to this interface...
      for(uint32_t k = 0; k < ipIface->GetNAddresses (); k++) {
        // Get Ipv4Address
        Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();

        // If Loopback address, go to the next
        if(ipAddr == Ipv4Address::GetLoopback())
          continue;

        std::cout << "[PopulateArpCache] Arp Cache: Adding the pair (" << addr << "," << ipAddr << ")" << '\n';

        // Creates an ARP entry for this Ipv4Address and adds it to the ARP Cache
        Ipv4Header ipHeader;
        ArpCache::Entry * entry = arp->Add(ipAddr);
        //entry->IsPermanent();
        //entry->MarkWaitReply();
        //entry->MarkPermanent();
        //entry->MarkAlive(addr);
        //entry->MarkDead();

        entry->MarkWaitReply (Ipv4PayloadHeaderPair(dummy,ipHeader));
        entry->MarkAlive (addr);
        entry->ClearPendingPacket();
        entry->MarkPermanent ();

        NS_LOG_UNCOND ("[PopulateArpCache] Arp Cache: Added the pair (" << addr << "," << ipAddr << ")");

        AsciiTraceHelper asciiTraceHelper;
        Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");

        arp->PrintArpCache(stream);
      }
//  }

    // Assign ARP Cache to each interface of each node
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {

      Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
      if (ip!=0) {
        std::cout << "[PopulateArpCache] Adding the Arp Cache to Node #" << nodenumber << '\n';

        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);
        for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
        {
          Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

          // Get interfaces list from Ipv4L3Protocol iteractor
          Ptr<NetDevice> device = ipIface->GetDevice();
          NS_ASSERT(device != 0);

          arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details

          //ipIface->SetAttribute("ArpCache", PointerValue(arp));


          AsciiTraceHelper asciiTraceHelper;
          Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");

          arp->PrintArpCache(stream);

          Time mytime = arp->GetAliveTimeout();
          std::cout << "Alive Timeout [s]: " << mytime.GetSeconds() << '\n';
        }
      }
    }
  }
}
/************* END of the ARP part (not used) *************/


// Modify the max AMPDU value of a node
void ModifyAmpdu (uint32_t nodeNumber, uint32_t ampduValue, uint32_t myverbose)
{
  // These are the attributes of regular-wifi-mac: https://www.nsnam.org/doxygen/regular-wifi-mac_8cc_source.html
  // You have to build a line like this (e.g. for node 0):
  // Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize", UintegerValue(ampduValue));
  // There are 4 queues: VI, VO, BE and BK

  // FIXME: Check if I only have to modify the parameters of all the devices (*), or only some of them.

  // I use an auxiliar string for creating the first argument of Config::Set
  std::ostringstream auxString;

  // VI queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VI_MaxAmpduSize";
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // VO queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VO_MaxAmpduSize"; 
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // BE queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize"; 
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // BK queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize"; 
  //std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));  

  if ( myverbose > 1 )
    std::cout << Simulator::Now().GetSeconds()
              << "\t[ModifyAmpdu] Node #" << nodeNumber 
              << " AMPDU max size changed to " << ampduValue << " bytes" 
              << std::endl;
}


/*
// Not used
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
// set the position of a node
static void
SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}*/


// Return a vector with the position of a node
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
static Vector
GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}


// Print the simulation time to std::cout
static void printTime (double period, std::string myoutputFileName, std::string myoutputFileSurname)
{
  std::cout << Simulator::Now().GetSeconds() << "\t" 
            << myoutputFileName << "_" 
            << myoutputFileSurname << '\n';

  // re-schedule 
  Simulator::Schedule (Seconds (period), &printTime, period, myoutputFileName, myoutputFileSurname);
}


// function for tracking mobility changes
static void 
CourseChange (std::string foo, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  std::cout << Simulator::Now ().GetSeconds()
            << "\t[CourseChange] MOBILITY CHANGE. model= " 
            << mobility << ", POS: x=" << pos.x 
            << ", y=" << pos.y
            << ", z=" << pos.z 
            << "; VEL:" << vel.x 
            << ", y=" << vel.y
            << ", z=" << vel.z << std::endl;
}


// Print the statistics to an output file and/or to the screen
void 
print_stats ( FlowMonitor::FlowStats st, 
              double simulationTime, 
              uint32_t mygenerateHistograms, 
              std::string fileName,
              std::string fileSurname,
              uint32_t myverbose,
              std::string flowID,
              uint32_t printColumnTitles ) 
{
  // print the results to a file (they are written at the end of the file)
  if ( fileName != "" ) {

    std::ofstream ofs;
    ofs.open ( fileName + "_flows.txt", std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // Print a line in the output file, with the title of each column
    if ( printColumnTitles == 1 ) {
      ofs << "Flow_ID" << "\t"
          << "Protocol" << "\t"
          << "source_Address" << "\t"
          << "source_Port" << "\t" 
          << "destination_Address" << "\t"
          << "destination_Port" << "\t"
          << "Num_Tx_Packets" << "\t" 
          << "Num_Tx_Bytes" << "\t" 
          << "Tx_Throughput_[bps]" << "\t"  
          << "Num_Rx_Packets" << "\t" 
          << "Num_RX_Bytes" << "\t" 
          << "Num_lost_packets" << "\t" 
          << "Rx_Throughput_[bps]" << "\t"
          << "Average_Latency_[s]" << "\t"
          << "Average_Jitter_[s]" << "\t"
          << "Average_Number_of_hops" << "\t"
          << "Simulation_time_[s]" << "\n";
    }

    // Print a line in the output file, with the data of this flow
    ofs << flowID << "\t" // flowID includes the protocol, IP addresses and ports
        << st.txPackets << "\t" 
        << st.txBytes << "\t" 
        << st.txBytes * 8.0 / simulationTime << "\t"  
        << st.rxPackets << "\t" 
        << st.rxBytes << "\t" 
        << st.txPackets - st.rxPackets << "\t" 
        << st.rxBytes * 8.0 / simulationTime << "\t";

    if (st.rxPackets > 0) 
    { 
      ofs << (st.delaySum.GetSeconds() / st.rxPackets) <<  "\t";

      if (st.rxPackets > 1) { // I need at least two packets for calculating the jitter
        ofs << (st.jitterSum.GetSeconds() / (st.rxPackets - 1.0)) << "\t";
      } else {
        ofs << "\t";
      }

      ofs << st.timesForwarded / st.rxPackets + 1 << "\t"; 

    } else { //no packets arrived
      ofs << "\t" << "\t" << "\t"; 
    }

    ofs << simulationTime << "\n";

    ofs.close();


    // save the histogram to a file
    if ( mygenerateHistograms > 0 ) 
    { 
      std::ofstream ofs_histo;
      ofs_histo.open ( fileName + fileSurname + "_delay_histogram.txt", std::ofstream::out | std::ofstream::trunc);

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples" << std::endl; 
      for (uint32_t i=0; i < st.delayHistogram.GetNBins (); i++) 
        ofs_histo << i << "\t" << st.delayHistogram.GetBinStart (i) << "\t" << st.delayHistogram.GetBinEnd (i) << "\t" << st.delayHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();

      ofs_histo.open ( fileName + fileSurname + "_jitter_histogram.txt", std::ofstream::out | std::ofstream::trunc); // with "trunc", Any contents that existed in the file before it is open are discarded

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples" << std::endl; 
      for (uint32_t i=0; i < st.jitterHistogram.GetNBins (); i++ ) 
        ofs_histo << i << "\t" << st.jitterHistogram.GetBinStart (i) << "\t" << st.jitterHistogram.GetBinEnd (i) << "\t" << st.jitterHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();

      ofs_histo.open ( fileName + fileSurname + "_packetsize_histogram.txt", std::ofstream::out | std::ofstream::trunc); // with "trunc", Any contents that existed in the file before it is open are discarded

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples"<< std::endl; 
      for (uint32_t i=0; i < st.packetSizeHistogram.GetNBins (); i++ ) 
        ofs_histo << i << "\t" << st.packetSizeHistogram.GetBinStart (i) << "\t" << st.packetSizeHistogram.GetBinEnd (i) << "\t" << st.packetSizeHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();
    }
  }

  // print the results by the screen
  if ( myverbose > 0 ) {
    std::cout << " -Flow #" << flowID << "\n";
    if ( mygenerateHistograms > 0) {
      std::cout << "   The name of the output files starts with: " << fileName << fileSurname << "\n";
      std::cout << "   Tx Packets: " << st.txPackets << "\n";
      std::cout << "   Tx Bytes:   " << st.txBytes << "\n";
      std::cout << "   TxOffered:  " << st.txBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";
      std::cout << "   Rx Packets: " << st.rxPackets << "\n";
      std::cout << "   Rx Bytes:   " << st.rxBytes << "\n";
      std::cout << "   Lost Packets: " << st.txPackets - st.rxPackets << "\n";
      std::cout << "   Throughput: " << st.rxBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";      
    }

    if (st.rxPackets > 0) // some packets have arrived
    { 
      std::cout << "   Mean{Delay}: " << (st.delaySum.GetSeconds() / st.rxPackets); 
      
      if (st.rxPackets > 1) // I need at least two packets for calculating the jitter
      { 
        std::cout << "   Mean{Jitter}: " << (st.jitterSum.GetSeconds() / (st.rxPackets - 1.0 ));
      } else {
        std::cout << "   Mean{Jitter}: only one packet arrived. "; 
      }
      
      std::cout << "   Mean{Hop Count}: " << st.timesForwarded / st.rxPackets + 1 << "\n"; 

    } else { //no packets arrived
      std::cout << "   Mean{Delay}: no packets arrived. ";
      std::cout << "   Mean{Jitter}: no packets arrived. "; 
      std::cout << "   Mean{Hop Count}: no packets arrived. \n"; 
    }

    if (( mygenerateHistograms > 0 ) && ( myverbose > 3 )) 
    { 
      std::cout << "   Delay Histogram" << std::endl; 
      for (uint32_t i=0; i < st.delayHistogram.GetNBins (); i++) 
        std::cout << "  " << i << "(" << st.delayHistogram.GetBinStart (i) 
                  << "-" << st.delayHistogram.GetBinEnd (i) 
                  << "): " << st.delayHistogram.GetBinCount (i) 
                  << std::endl; 

      std::cout << "   Jitter Histogram" << std::endl; 
      for (uint32_t i=0; i < st.jitterHistogram.GetNBins (); i++ ) 
        std::cout << "  " << i << "(" << st.jitterHistogram.GetBinStart (i) 
                  << "-" << st.jitterHistogram.GetBinEnd (i) 
                  << "): " << st.jitterHistogram.GetBinCount (i) 
                  << std::endl; 

      std::cout << "   PacketSize Histogram  "<< std::endl; 
      for (uint32_t i=0; i < st.packetSizeHistogram.GetNBins (); i++ ) 
        std::cout << "  " << i << "(" << st.packetSizeHistogram.GetBinStart (i) 
                  << "-" << st.packetSizeHistogram.GetBinEnd (i) 
                  << "): " << st.packetSizeHistogram.GetBinCount (i) 
                  << std::endl; 
    }

    for (uint32_t i=0; i < st.packetsDropped.size (); i++) 
      std::cout << "    Packets dropped by reason " << i << ": " << st.packetsDropped [i] << std::endl; 
    //for (uint32_t i=0; i<st.bytesDropped.size(); i++) 
    //  std::cout << "Bytes dropped by reason " << i << ": " << st.bytesDropped[i] << std::endl;

    std::cout << "\n";
  }
} 


// this class stores a number of records: each one contains a pair AP node id - AP MAC address
// the node id is the one given by ns3 when creating the node
class AP_record
{
  public:
    AP_record ();
    //void SetApRecord (uint16_t thisId, Mac48Address thisMac);
    void SetApRecord (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu);
    //uint16_t GetApid (Mac48Address thisMac);
    uint16_t GetApid ();
    //Mac48Address GetMac (uint16_t thisId);
    std::string GetMac ();
    uint32_t GetMaxSizeAmpdu ();
    uint8_t GetWirelessChannel();
    void setWirelessChannel(uint8_t thisWirelessChannel);
  private:
    uint16_t apId;
    //Mac48Address apMac;
    std::string apMac;
    uint32_t apMaxSizeAmpdu;
    uint8_t apWirelessChannel; 
};

typedef std::vector <AP_record * > AP_recordVector;
AP_recordVector AP_vector;

AP_record::AP_record ()
{
  apId = 0;
  apMac = "02-06-00:00:00:00:00:00";
  apMaxSizeAmpdu = 0;
}

void
//AP_record::SetApRecord (uint16_t thisId, Mac48Address thisMac)
AP_record::SetApRecord (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu)
{
  apId = thisId;
  apMac = thisMac;
  apMaxSizeAmpdu = thisMaxSizeAmpdu;
}


uint16_t
//AP_record::GetApid (Mac48Address thisMac)
AP_record::GetApid ()
{
  return apId;
}

//Mac48Address
std::string
AP_record::GetMac ()
{
  return apMac;
}

uint8_t
AP_record::GetWirelessChannel()
{
  return apWirelessChannel;
}

void
AP_record::setWirelessChannel(uint8_t thisWirelessChannel)
{
  apWirelessChannel = thisWirelessChannel;
}

// obtain the nearest AP of a STA, in a certain frequency band (2.4 or 5 GHz)
// if 'frequencyBand == 0', the nearest AP will be searched in both bands
static Ptr<Node>
nearestAp (NodeContainer APs, Ptr<Node> mySTA, int myverbose, std::string frequencyBand)
{
  // the frequency band MUST be "2.4 GHz" or "5 GHz". It can also be "both", meaning both bands
  NS_ASSERT (( frequencyBand == "2.4 GHz" ) || (frequencyBand == "5 GHz" ) || (frequencyBand == "both" ));

  // vector with the position of the STA
  Vector posSta = GetPosition (mySTA);

  if (frequencyBand != "both") {
    //if (VERBOSE_FOR_DEBUG > 0)
      std::cout << "\n"
                << Simulator::Now().GetSeconds() 
                << "\t[nearestAp] Looking for the nearest AP of STA #" << mySTA->GetId()
                << ", in position: "  << posSta.x << "," << posSta.y
                << ", in the " << frequencyBand << " band"
                << std::endl;
  }
  else {
    //if (VERBOSE_FOR_DEBUG > 0)
      std::cout << "\n"
                << Simulator::Now().GetSeconds() 
                << "\t[nearestAp] Looking for the nearest AP of STA #" << mySTA->GetId()
                << ", in position: "  << posSta.x << "," << posSta.y
                << ", in any frequency band"
                << std::endl;    
  }

  // calculate an initial value for the minimum distance (a very high value)
  double mimimumDistance = APs.GetN() * 100000;

  uint8_t channelNearestAP = 0;

  // variable for storing the nearest AP
  Ptr<Node> nearest;

  // vector with the position of the AP
  Vector posAp;

  // Check all the APs to find the nearest one
  // go through the AP record vector
  AP_recordVector::const_iterator indexAP = AP_vector.begin ();

  // go through the nodeContainer of APs at the same time
  NodeContainer::Iterator i; 
  for (i = APs.Begin (); i != APs.End (); ++i)
  {
    //(*i)->method ();  // some Node method

    // find the frequency band of this AP
    uint8_t channelThisAP = (*indexAP)->GetWirelessChannel();
    std::string frequencyBandThisAP = getWirelessBandOfChannel(channelThisAP);

    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now().GetSeconds() 
                << "\t[nearestAp]\tAP #" << (*i)->GetId()
                <<  ", channel: "  << (int)channelThisAP 
                << ", frequency band: " << frequencyBandThisAP << std::endl;

    // only look for APs in the specified frequency band
    if ((frequencyBand == frequencyBandThisAP ) || (frequencyBand == "both") ) {
      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << "\t\t\t is in the correct band" << std::endl;

      posAp = GetPosition((*i));
      double distance = sqrt ( ( (posSta.x - posAp.x)*(posSta.x - posAp.x) ) + ( (posSta.y - posAp.y)*(posSta.y - posAp.y) ) );
      if (distance < mimimumDistance ) {
        mimimumDistance = distance;
        nearest = *i;
        channelNearestAP = channelThisAP;

        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << "\t\t\t and it is the nearest one so far (distance " << distance << " m)" << std::endl;
      }
      else {
        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << "\t\t\t but it is not the nearest one (distance " << distance << " m)" << std::endl;
      }
    }
    else {
      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << "\t\t\t is not in the correct band" << std::endl;      
    }
    indexAP++;
  }

  if(nearest!=NULL) {
    NS_ASSERT(channelNearestAP!=0);
    if (frequencyBand != "both") {
     //if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] Result: The nearest AP in the " << frequencyBand << " band "
                << "is AP#" << nearest->GetId() 
                << ". Channel: "  << (int)channelNearestAP 
                << ". Frequency band: " << getWirelessBandOfChannel(channelNearestAP)
                << ". Position: "  << GetPosition((nearest)).x 
                << "," << GetPosition((nearest)).y
                << std::endl;     
    }
    else {
      //if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] Result: The nearest AP in any frequency band "
                << "is AP#" << nearest->GetId() 
                << ". Channel: "  << (int)channelNearestAP 
                << ". Frequency band: " << getWirelessBandOfChannel(channelNearestAP)
                << ". Position: "  << GetPosition((nearest)).x 
                << "," << GetPosition((nearest)).y
                << std::endl;        
    }
  }
  else {
    if (frequencyBand != "both") {
      // if an AP in the same band cannot be found, finish the simulation
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] Result: There is no AP in the " << frequencyBand << " band "
                << "- FINISHING SIMULATION"
                << std::endl;
    }
    else {
      // if an AP in the same band cannot be found, finish the simulation
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] Result: There is no AP in any frequency band "
                << "- FINISHING SIMULATION"
                << std::endl;      
    }
    NS_ASSERT(false);
  }
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[nearestAp] Successfully finishing nearestAp" << std::endl;

  return nearest;
}

//MaxSizeAmpdu
uint32_t
AP_record::GetMaxSizeAmpdu ()
{
  return apMaxSizeAmpdu;
}

void
Modify_AP_Record (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu) // FIXME: Can this be done just with Set_AP_Record?
{
  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    //std::cout << Simulator::Now ().GetSeconds() << " ********************** AP with ID " << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << " *****" << std::endl;

    if ( (*index)->GetMac () == thisMac ) {
      (*index)->SetApRecord (thisId, thisMac, thisMaxSizeAmpdu);
      //std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
    }
  }
}


uint32_t
CountAPs ()
{
  uint32_t number = 0;
  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    number ++;
  }
  return number;
}

uint32_t
CountAPs (uint32_t myverbose)
// counts all the APs with their id, mac and current value of MaxAmpdu
{
  if (myverbose > 2)
    std::cout << "\n" << Simulator::Now ().GetSeconds() << "   \t[CountAPs] Report APs" << std::endl;

  uint32_t number = 0;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    number ++;
  }
  return number;
}


uint16_t
GetAnAP_Id (std::string thisMac)
// lists all the STAs associated to an AP, with the MAC of the AP
{
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] Looking for MAC " << thisMac << std::endl;

  bool macFound = false;

  uint16_t APid;

  uint32_t numberAPs = CountAPs();
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] Number of APs: " << (unsigned int)numberAPs << "\n";
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id]   AP with ID " << (*index)->GetApid() << " has MAC:   " << (*index)->GetMac() << std::endl;
    
    if ( (*index)->GetMac () == thisMac ) {
      // check the primary MAC address of each AP
      APid = (*index)->GetApid ();
      macFound = true;
      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] FOUND: AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
    }
  }

  // make sure that an AP with 'thisMac' exists
  if (macFound == false) {
    std::cout << "\n"
              << Simulator::Now ().GetSeconds()
              << "\t[GetAnAP_Id] ERROR: MAC " << thisMac << " not found in the list of APs. Stopping simulation"
              << std::endl;
    NS_ASSERT(macFound == true);
  }
  
  // return the identifier of the AP
  return APid;
}


uint32_t
GetAP_MaxSizeAmpdu (uint16_t thisAPid, uint32_t myverbose)
// returns the max size of the Ampdu of an AP
{
  uint32_t APMaxSizeAmpdu = 0;
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {

    if ( (*index)->GetApid () == thisAPid ) {
      APMaxSizeAmpdu = (*index)->GetMaxSizeAmpdu ();
      if ( myverbose > 2 )
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[GetAP_MaxSizeAmpdu] AP #" << (*index)->GetApid() 
                  << " has AMDPU: " << (*index)->GetMaxSizeAmpdu() 
                  << "" << std::endl;
    }
  }
  return APMaxSizeAmpdu;
}

uint8_t
GetAP_WirelessChannel (uint16_t thisAPid, uint32_t myverbose)
// returns the wireless channel of an AP
{
  uint8_t APWirelessChannel = 0;
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {

    if ( (*index)->GetApid () == thisAPid ) {
      APWirelessChannel = (*index)->GetWirelessChannel();
      if ( myverbose > 2 )
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[GetAP_WirelessChannel] AP #" << (*index)->GetApid() 
                  << " has channel: " << uint16_t((*index)->GetWirelessChannel())
                  << "" << std::endl;
    }
  }
  return APWirelessChannel;
}


void
ListAPs (uint32_t myverbose)
// lists all the APs with their id, mac and current value of MaxAmpdu
{
  std::cout << "\n" << Simulator::Now ().GetSeconds() << "   \t[ListAPs] Report APs. Total " << CountAPs(myverbose) << " APs" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    std::cout //<< Simulator::Now ().GetSeconds()
              << "                  "
              << "   \t\tAP #" << (*index)->GetApid() 
              << " with MAC " << (*index)->GetMac() 
              << " Max size AMPDU " << (*index)->GetMaxSizeAmpdu() 
              << " Channel " << uint16_t((*index)->GetWirelessChannel())
              << std::endl;
  }
  std::cout << std::endl;
}


// This part, i.e. the association record is taken from https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc

// this class stores one record per STA, containing 
// - the information of its association: the MAC of the AP where it is associated
// - the type of application it is running
// note: it does NOT store the MAC address of the STA itself
class STA_record
{
  public:
    STA_record ();
    bool GetAssoc ();
    uint16_t GetStaid ();
    Mac48Address GetMacOfitsAP ();
    uint32_t Gettypeofapplication ();
    uint32_t GetMaxSizeAmpdu ();
    void SetAssoc (std::string context, Mac48Address AP_MAC_address);
    void UnsetAssoc (std::string context, Mac48Address AP_MAC_address);
    void setstaid (uint16_t id);
    void Settypeofapplication (uint32_t applicationid);
    void SetMaxSizeAmpdu (uint32_t MaxSizeAmpdu);
    void SetVerboseLevel (uint32_t myVerboseLevel);
    void SetnumOperationalChannels (uint32_t mynumOperationalChannels);
    void SetstaRecordNumberWiFiCards (int mynumWiFiCards);
    void Setversion80211primary (std::string myversion80211);
    void Setversion80211Secondary (std::string myversion80211);
    void activatePrimaryCard();
    void activateSecondaryCard();
    void deactivatePrimaryCard();
    void deactivateSecondaryCard();
    void SetaggregationDisableAlgorithm (uint16_t myaggregationDisableAlgorithm);
    void SetAmpduSize (uint32_t myAmpduSize);
    void SetmaxAmpduSizeWhenAggregationLimited (uint32_t mymaxAmpduSizeWhenAggregationLimited);
    void SetWifiModel (uint32_t mywifiModel);
  private:
    bool assoc;
    uint16_t staid;
    Mac48Address apMac;
    uint32_t typeofapplication; // 0 no application; 1 VoIP upload; 2 VoIP download; 3 TCP upload; 4 TCP download; 5 Video download
    uint32_t staRecordMaxSizeAmpdu;
    uint32_t staRecordVerboseLevel;
    uint32_t staRecordnumOperationalChannels;
    int staRecordNumberWiFiCards;
    std::string staRecordversion80211primary;
    std::string staRecordVersion80211Secondary;
    bool staRecordPrimaryCardActive;
    bool staRecordSecondaryCardActive;
    uint16_t staRecordaggregationDisableAlgorithm;
    uint32_t staRecordMaxAmpduSize;
    uint32_t staRecordmaxAmpduSizeWhenAggregationLimited;
    uint32_t staRecordwifiModel;
};

// this is the constructor. Set the default parameters
STA_record::STA_record ()
{
  assoc = false;
  staid = 0;
  apMac = "00:00:00:00:00:00";    //MAC address of the AP to which the STA is associated
  typeofapplication = 0;
  staRecordMaxSizeAmpdu = 0;
  staRecordVerboseLevel = 0;
  staRecordnumOperationalChannels = 0;
  staRecordNumberWiFiCards = 1;
  staRecordversion80211primary = "";
  staRecordVersion80211Secondary = "";
  staRecordPrimaryCardActive = false;    // FIXME: maybe this is not needed and ns3 can store it
  staRecordSecondaryCardActive = false;  // FIXME: maybe this is not needed and ns3 can store it
  staRecordaggregationDisableAlgorithm = 0;
  staRecordMaxAmpduSize = 0;
  staRecordmaxAmpduSizeWhenAggregationLimited = 0;
  staRecordwifiModel = 0;
}

void
STA_record::setstaid (uint16_t id)
{
  staid = id;
}

typedef std::vector <STA_record * > STA_recordVector;
STA_recordVector assoc_vector;

void
STA_record::activatePrimaryCard()
{
  staRecordPrimaryCardActive = true;
}

void
STA_record::activateSecondaryCard()
{
  staRecordSecondaryCardActive = true;
}

void
STA_record::deactivatePrimaryCard()
{
  staRecordPrimaryCardActive = false;
}

void
STA_record::deactivateSecondaryCard()
{
  staRecordSecondaryCardActive = false;
}

uint32_t
Get_STA_record_num ()
// counts the number or STAs associated
{
  uint32_t AssocNum = 0;
  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
    if ((*index)->GetAssoc ()) {
      AssocNum++;
    }
  }
  return AssocNum;
}

uint8_t
GetChannelOfAnSTA (uint16_t id)
// it returns the channel of the AP where the STA is associated
{
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[GetChannelOfAnSTA] Looking for the channel of STA #" << id
              << std::endl;

  uint8_t channel = 0;
  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
    if ((*index)->GetStaid () == id) {

      Mac48Address nullMAC = "00:00:00:00:00:00";

      if ((*index)->GetMacOfitsAP() == nullMAC) {
        if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now().GetSeconds()
                      << "\t[GetChannelOfAnSTA] STA #" << (*index)->GetStaid ()
                      << " nas no associated AP"
                      << std::endl;
      }
      else {
        // auxiliar string
        std::ostringstream auxString;
        // create a string with the MAC
        auxString << "02-06-" << (*index)->GetMacOfitsAP();
        std::string myaddress = auxString.str();

        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfAnSTA] STA with Id" << (*index)->GetStaid ()
                    << " found"
                    << ". MAC of its associated AP: " << myaddress
                    << std::endl; 

        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfAnSTA] Calling GetAnAP_Id()"
                    << std::endl;

        // Get the wireless channel of the AP with the corresponding address
        NS_ASSERT(myaddress!="02-06-00:00:00:00:00:00");
        channel = GetAP_WirelessChannel (GetAnAP_Id(myaddress), 0);      
      }
    }
  }
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[GetChannelOfAnSTA] Return value: channel " << (int)channel
              << std::endl;

  return channel;
}

// Print the channel of a STA
static void
ReportChannel (double period, uint16_t id, int myverbose)
{
  if (myverbose > 2) {
    // Find the AP to which the STA is associated
    for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
      if ((*index)->GetStaid () == id) {
        // auxiliar string
        std::ostringstream auxString;
        // create a string with the MAC
        auxString << "02-06-" << (*index)->GetMacOfitsAP();
        std::string myaddress = auxString.str();
        NS_ASSERT(!myaddress.empty());

        if (myaddress=="02-06-00:00:00:00:00:00") {
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ReportChannel] STA #" << id 
                    << " Not associated to any AP"
                    << std::endl;          
        }
        else {
          NS_ASSERT(myaddress!="02-06-00:00:00:00:00:00");
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ReportChannel] STA #" << id 
                    << " Associated to AP#" << GetAnAP_Id(myaddress)
                    << ". Channel: " << uint16_t(GetChannelOfAnSTA (id))
                    << std::endl;          
        }
      }
    }
  }

  // re-schedule
  Simulator::Schedule (Seconds (period), &ReportChannel, period, id, myverbose);
}

// lists all the STAs, with the MAC of the AP if they are associated to it
void
List_STA_record ()
{
  std::cout << "\n" << Simulator::Now ().GetSeconds() << "\t[List_STA_record] Report STAs. Total associated: " << Get_STA_record_num() << "" << std::endl;

  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
    if ((*index)->GetAssoc ()) {

      // auxiliar string
      std::ostringstream auxString;
      // create a string with the MAC
      auxString << "02-06-" << (*index)->GetMacOfitsAP();
      std::string myaddress = auxString.str();
      NS_ASSERT(!myaddress.empty());

      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[List_STA_record] Calling GetAnAP_Id()"
                  << std::endl; 

      std::cout //<< Simulator::Now ().GetSeconds() 
                << "\t\t\t\tSTA #" << (*index)->GetStaid() 
                << "\tassociated to AP #" << GetAnAP_Id(myaddress) 
                << "\twith MAC " << (*index)->GetMacOfitsAP() 
                << "\ttype of application " << (*index)->Gettypeofapplication()
                << "\tValue of Max AMPDU " << (*index)->GetMaxSizeAmpdu()
                << std::endl;
    } else {
      std::cout //<< Simulator::Now ().GetSeconds() 
                << "\t\t\t\tSTA #" << (*index)->GetStaid()
                << "\tnot associated to any AP \t\t\t" 
                << "\ttype of application " << (*index)->Gettypeofapplication()
                << "\tValue of Max AMPDU " << (*index)->GetMaxSizeAmpdu()
                << std::endl;      
    }
  }
}

// This is called with a callback every time a STA is associated to an AP
void
STA_record::SetAssoc (std::string context, Mac48Address AP_MAC_address)
{
  // 'context' is something like "/NodeList/9/DeviceList/1/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc"
  if(VERBOSE_FOR_DEBUG > 0)
    std::cout << "\t[SetAssoc] context: " << context << std::endl;
  if(VERBOSE_FOR_DEBUG > 0)
    std::cout << "\t[SetAssoc] STA #" << staid << " has associated to the AP with MAC " << AP_MAC_address << std::endl;

  // update the data in the STA_record structure
  assoc = true;
  apMac = AP_MAC_address;

  // I have this info available in the STA record:
  //  staid
  //  typeofapplication
  //  staRecordMaxSizeAmpdu

  // auxiliar string
  std::ostringstream auxString;
  // create a string with the MAC
  auxString << "02-06-" << AP_MAC_address;
  std::string myaddress = auxString.str();

  NS_ASSERT(!myaddress.empty());

  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[SetAssoc] Calling GetAnAP_Id()"
              << std::endl;

  uint32_t apId = GetAnAP_Id(myaddress);

  uint8_t apChannel = GetAP_WirelessChannel ( apId, staRecordVerboseLevel );

  if (staRecordVerboseLevel > 0)
    std::cout << Simulator::Now ().GetSeconds() 
              << "\t[SetAssoc] STA #" << staid 
              << "\twith AMPDU size " << staRecordMaxSizeAmpdu 
              << "\trunning application " << typeofapplication 
              << "\thas associated to AP #" << apId
              << " with MAC " << apMac  
              << " with channel " <<  uint16_t (apChannel)
              << "" << std::endl;

  // This part only runs if the aggregation algorithm is activated
  if (staRecordaggregationDisableAlgorithm == 1) {
    // check if the STA associated to the AP is running VoIP. In this case, I have to disable aggregation:
    // - in the AP
    // - in all the associated STAs
    if ( typeofapplication == 1 || typeofapplication == 2 ) {

      // disable aggregation in the AP

      // check if the AP is aggregating
      if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) > 0 ) {

        // I modify the A-MPDU of this AP
        ModifyAmpdu ( apId, staRecordmaxAmpduSizeWhenAggregationLimited, 1 );

        // Modify the data in the table of APs
        //for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
          //if ( (*index)->GetMac () == myaddress ) {
            Modify_AP_Record ( apId, myaddress, staRecordmaxAmpduSizeWhenAggregationLimited);
            //std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
        //  }
        //}

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] Aggregation in AP #" << apId 
                    << "\twith MAC: " << myaddress 
                    << "\tset to " << staRecordmaxAmpduSizeWhenAggregationLimited 
                    << "\t(limited)" << std::endl;

        // disable aggregation in all the STAs associated to that AP
        for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

          // I only have to disable aggregation for TCP STAs
          if ((*index)->Gettypeofapplication () > 2) {

            // if the STA is associated
            if ((*index)->GetAssoc ()) {

              // if the STA is associated to this AP
              if ((*index)->GetMacOfitsAP() == AP_MAC_address ) {

                ModifyAmpdu ((*index)->GetStaid(), staRecordmaxAmpduSizeWhenAggregationLimited, 1);   // modify the AMPDU in the STA node
                (*index)->SetMaxSizeAmpdu(staRecordmaxAmpduSizeWhenAggregationLimited);               // update the data in the STA_record structure

                if (staRecordVerboseLevel > 0)
                  std::cout << Simulator::Now ().GetSeconds() 
                            << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                            << ", associated to AP #" << apId
                            << "\twith MAC " << (*index)->GetMacOfitsAP() 
                            << "\tset to " << staRecordmaxAmpduSizeWhenAggregationLimited 
                            << "\t(limited)" << std::endl;
              }
            }
          }
        }
      }

    // If this associated STA is using TCP
    } else {

      // If the new AP is not aggregating
      if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) == 0) {

        // Disable aggregation in this STA
        ModifyAmpdu (staid, staRecordmaxAmpduSizeWhenAggregationLimited, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordmaxAmpduSizeWhenAggregationLimited;        // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] Aggregation in STA #" << staid 
                    << ", associated to AP #" << apId 
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "\t(limited)" << std::endl;

  /*    for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

          if ( (*index)->GetMac () == AP_MAC_address ) {
              ModifyAmpdu ((*index)->GetStaid(), 0, 1);  // modify the AMPDU in the STA node
              (*index)->SetMaxSizeAmpdu(0);// update the data in the STA_record structure

              if (staRecordVerboseLevel > 0)
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                          << ", associated to AP #" << GetAnAP_Id(myaddress) 
                          << "\twith MAC " << (*index)->GetMac() 
                          << "\tset to " << 0 
                          << "\t(limited)" << std::endl;
          }
        }*/

      // If the new AP is aggregating, I have to enable aggregation in this STA
     
      } else {
        // Enable aggregation in this STA
        ModifyAmpdu (staid, staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordMaxAmpduSize;        // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] Aggregation in STA #" << staid 
                    << ", associated to AP #" << apId 
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "(enabled)" << std::endl;
  /*      // Enable aggregation in the STA
          for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
            if ( (*index)->GetMac () == AP_MAC_address ) {
              ModifyAmpdu ((*index)->GetStaid(), maxAmpduSize, 1);  // modify the AMPDU in the STA node
              (*index)->SetMaxSizeAmpdu(maxAmpduSize);// update the data in the STA_record structure

              if (myverbose > 0)
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                          << ", associated to AP #" << GetAnAP_Id(myaddress) 
                          << "\twith MAC " << (*index)->GetMac() 
                          << "\tset to " << maxAmpduSize 
                          << "\t(enabled)" << std::endl;
          }
        }*/
      }
    }
  }
  if (staRecordVerboseLevel > 0) {
    List_STA_record ();
    ListAPs (staRecordVerboseLevel);
  }

  if (false) {
    // empty the ARP caches of all the nodes after a new association
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
      //uint32_t identif;
      //identif = (*i)->GetId();
      emtpyArpCache ( (*i), 2);
    }
  }

  // after a new association, momentarily modify the ARP timeout to 1 second, and then to 50 seconds again
  if (false) {
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
        //uint32_t identif;
        //identif = (*i)->GetId();
        modifyArpParams( (*i), 1.0, staRecordVerboseLevel);
        Simulator::Schedule (Seconds(5.0), &modifyArpParams, (*i), 50.0, staRecordVerboseLevel);
    }
  }
}

// This is called with a callback every time a STA is de-associated from an AP
void
STA_record::UnsetAssoc (std::string context, Mac48Address AP_MAC_address)
{
  // 'context' is something like "/NodeList/9/DeviceList/1/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc"

  // update the data in the STA_record structure
  assoc = false;
  apMac = "00:00:00:00:00:00";
   
  // auxiliar string
  std::ostringstream auxString;
  // create a string with the MAC
  auxString << "02-06-" << AP_MAC_address;
  std::string myaddress = auxString.str();
  NS_ASSERT(!myaddress.empty());

  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[UnsetAssoc] Calling GetAnAP_Id()"
              << std::endl;

  uint32_t apId = GetAnAP_Id(myaddress);

  uint8_t apChannel = GetAP_WirelessChannel ( apId, staRecordVerboseLevel );

  if (staRecordVerboseLevel > 0) {
    std::cout << Simulator::Now ().GetSeconds() 
              << "\t[UnsetAssoc] STA #" << staid
              << "\twith AMPDU size " << staRecordMaxSizeAmpdu               
              << "\trunning application " << typeofapplication 
              << "\tde-associated from AP #" << apId
              << " with MAC " << AP_MAC_address 
              << " with channel " << uint16_t (apChannel)
              << "" << std::endl;
    std::cout << "\t\t802.11 version primary interface: "
              << staRecordversion80211primary
              << std::endl;              
  }

  // this is the frequency band where the STA can find an AP
  // if it has a single card, only one band can be used
  // if it has two cards, it depends on the band(s) supported by the present AP(s)
  std::string frequencybandsSupportedBySTA = bandsSupportedBySTA(staRecordNumberWiFiCards, staRecordversion80211primary, staRecordVersion80211Secondary);

  // This only runs if the aggregation algorithm is running
  if(staRecordaggregationDisableAlgorithm == 1) {

    // check if there is some VoIP STA already associated to the AP. In this case, I have to enable aggregation:
    // - in the AP
    // - in all the associated STAs
    if ( typeofapplication == 1 || typeofapplication == 2 ) {

      // check if the AP is not aggregating
      /*if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) == 0 ) {
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] This AP is not aggregating" 
                    << std::endl;*/

        // check if there is no STA running VoIP associated
        bool anyStaWithVoIPAssociated = false;

        // Check all the associated STAs, except the one de-associating
        for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

          // Only consider STAs associated to this AP
          if ( (*index)->GetMacOfitsAP () == AP_MAC_address ) {

            // Only consider VoIP STAs
            if ( ( (*index)->Gettypeofapplication() == 1 ) || ( (*index)->Gettypeofapplication() == 2 ) ) {

              // It cannot be the one being de-associated
              if ( (*index)->GetStaid() != staid)

                anyStaWithVoIPAssociated = true;
            }
          }
        }

        // If there is no remaining STA running VoIP associated
        if ( anyStaWithVoIPAssociated == false ) {
          // enable aggregation in the AP
          // Modify the A-MPDU of this AP
          ModifyAmpdu (apId, staRecordMaxAmpduSize, 1);
          Modify_AP_Record (apId, myaddress, staRecordMaxAmpduSize);

          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc]\tAggregation in AP #" << apId 
                      << "\twith MAC: " << myaddress 
                      << "\tset to " << staRecordMaxAmpduSize 
                      << "\t(enabled)" << std::endl;

          // enable aggregation in all the STAs associated to that AP
          for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

            // if the STA is associated
            if ((*index)->GetAssoc()) {

              // if the STA is associated to this AP
              if ( (*index)->GetMacOfitsAP() == AP_MAC_address ) {

                // if the STA is not running VoIP. NOT NEEDED. IF I AM HERE IT MEANS THAT ALL THE STAs ARE TCP
                //if ((*index)->Gettypeofapplication () > 2) {

                  ModifyAmpdu ((*index)->GetStaid(), staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
                  (*index)->SetMaxSizeAmpdu(staRecordMaxAmpduSize);// update the data in the STA_record structure

                  if (staRecordVerboseLevel > 0)  
                    std::cout << Simulator::Now ().GetSeconds() 
                              << "\t[UnsetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                              << "\tassociated to AP #" << apId 
                              << "\twith MAC " << (*index)->GetMacOfitsAP() 
                              << "\tset to " << staRecordMaxAmpduSize 
                              << "\t(enabled)" << std::endl;
                //}
              }
            }
          }
        }
        else {
          // there is still some VoIP STA associatedm so aggregation cannot be enabled
          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc] There is still at least a VoIP STA in this AP " << apId 
                      << " so aggregation cannot be enabled" << std::endl;
        }
      //}

    // If thede-associated STA  is using TCP
    } else {

      // If the AP is not aggregating
      if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) == staRecordmaxAmpduSizeWhenAggregationLimited) {

        // Enable aggregation in this STA
        ModifyAmpdu (staid, staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordMaxAmpduSize;  // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] Aggregation in STA #" << staid 
                    << ", de-associated from AP #" << apId
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "\t(enabled)" << std::endl;

      // Enable aggregation in the STA
      /*
      for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

        // if the STA is associated to this AP
        if ( (*index)->GetMac() == AP_MAC_address ) {

          // if the STA is not running VoIP
          if ((*index)->Gettypeofapplication () > 2) {

            ModifyAmpdu ((*index)->GetStaid(), maxAmpduSize, 1);  // modify the AMPDU in the STA node
            (*index)->SetMaxSizeAmpdu(maxAmpduSize);// update the data in the STA_record structure

            if (staRecordVerboseLevel > 0)
              std::cout << Simulator::Now ().GetSeconds() 
                        << "\t[UnsetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                        << ", de-associated from AP #" << apId
                        << "\twith MAC " << (*index)->GetMac() 
                        << "\tset to " << maxAmpduSize 
                        << "\t(enabled)" << std::endl; 
          }
        }*/      
      }
    }
  }
  if (staRecordVerboseLevel > 0) {
    List_STA_record ();
    ListAPs (staRecordVerboseLevel);
  }

  
  // If wifiModel==1, I don't need to manually change the channel. It will do it automatically
  // If wifiModel==0, I have to manually set the channel of the STA to that of the nearest AP
  //if (staRecordwifiModel == 0) {  // staRecordwifiModel is the local version of the variable wifiModel
  
    // wifiModel = 0
    // Put the STA in the channel of the nearest AP
    if (staRecordnumOperationalChannels > 1) {
      // Only for wifiModel = 0. With WifiModel = 1 it is supposed to scan for other APs in other channels 
      //if (staRecordwifiModel == 0) {
        // Put all the APs in a nodecontainer
        // and get a pointer to the STA
        Ptr<Node> mySTA;
        NodeContainer APs;
        uint32_t numberAPs = CountAPs (staRecordVerboseLevel);

        for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
          uint32_t identif;
          identif = (*i)->GetId();
       
          if ( (identif >= 0) && (identif < numberAPs) ) {
            APs.Add(*i);
          } else if ( identif == staid) {
            mySTA = (*i);
          }
        }


        // Find the nearest AP (in order to switch the STA to the channel of the nearest AP)
        Ptr<Node> nearest;
        nearest = nearestAp (APs, mySTA, staRecordVerboseLevel, frequencybandsSupportedBySTA);

        // Move this STA to the channel of the AP identified as the nearest one
        NetDeviceContainer thisDevice;
        thisDevice.Add( (mySTA)->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why
     
        uint8_t newChannel = GetAP_WirelessChannel ( (nearest)->GetId(), staRecordVerboseLevel );

        if ( HANDOFFMETHOD == 0 )
          ChangeFrequencyLocal (thisDevice, newChannel, staRecordwifiModel, staRecordVerboseLevel);

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] STA #" << staid 
                    << " de-associated from AP #" << apId 
                    << ". Channel set to " << uint16_t (newChannel) 
                    << ", i.e. the channel of the nearest AP (AP #" << (nearest)->GetId()
                    << ")" << std::endl << std::endl;

    }
    else { // numOperationalChannels == 1
      if (staRecordVerboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[UnsetAssoc] STA #" << staid 
                  << " de-associated from AP #" << apId 
                  << "\tnot modified because numOperationalChannels=" << staRecordnumOperationalChannels 
                  << "\tchannel is still " << uint16_t (apChannel) 
                  << std::endl << std::endl;
    }
  //} 

  /*
  else {
    // wifiModel = 1
    if (staRecordVerboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[UnsetAssoc] STA #" << staid 
                  << " de-associated from AP #" << GetAnAP_Id(myaddress) 
                  << "\tnot modified because wifimodel=" << staRecordwifiModel
                  << "\tchannel is still " << uint16_t (apChannel) 
                  << std::endl << std::endl;
  }*/
}

void
STA_record::Settypeofapplication (uint32_t applicationid)
{
  typeofapplication = applicationid;
}

void
STA_record::SetMaxSizeAmpdu (uint32_t MaxSizeAmpdu)
{
  staRecordMaxSizeAmpdu = MaxSizeAmpdu;
}

void
STA_record::SetVerboseLevel (uint32_t myVerboseLevel)
{
  staRecordVerboseLevel = myVerboseLevel;
}

void
STA_record::SetnumOperationalChannels (uint32_t mynumOperationalChannels)
{
  staRecordnumOperationalChannels = mynumOperationalChannels;
}

void
STA_record::SetstaRecordNumberWiFiCards (int mynumWiFiCards) {
  staRecordNumberWiFiCards = mynumWiFiCards;
}

void
STA_record::Setversion80211primary (std::string myversion80211)
{
  staRecordversion80211primary = myversion80211;
}

void
STA_record::Setversion80211Secondary (std::string myversion80211)
{
  staRecordVersion80211Secondary = myversion80211;
}

void
STA_record::SetaggregationDisableAlgorithm (uint16_t myaggregationDisableAlgorithm)
{
  staRecordaggregationDisableAlgorithm = myaggregationDisableAlgorithm;
}

void
STA_record::SetAmpduSize (uint32_t myAmpduSize)
{
  staRecordMaxAmpduSize = myAmpduSize;
}

void
STA_record::SetmaxAmpduSizeWhenAggregationLimited (uint32_t mymaxAmpduSizeWhenAggregationLimited)
{
  staRecordmaxAmpduSizeWhenAggregationLimited = mymaxAmpduSizeWhenAggregationLimited;
}

void
STA_record::SetWifiModel (uint32_t mywifiModel)
{
  staRecordwifiModel = mywifiModel;
}

bool
STA_record::GetAssoc ()
// returns true or false depending whether the STA is associated or not
{
  return assoc;
}

uint16_t
STA_record::GetStaid ()
// returns the id of the Sta
{
  return staid;
}

Mac48Address
STA_record::GetMacOfitsAP ()
// returns the MAC address of the AP where the STA is associated
{
  return apMac;
}

uint32_t
STA_record::Gettypeofapplication ()
// returns the id of the Sta
{
  return typeofapplication;
}

uint32_t
STA_record::GetMaxSizeAmpdu ()
// returns the id of the Sta
{
  return staRecordMaxSizeAmpdu;
}

uint32_t
Get_STA_record_num_AP_app (Mac48Address apMac, uint32_t typeofapplication)
// counts the number or STAs associated to an AP, with a type of application
{
  uint32_t AssocNum = 0;
  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
    if (((*index)->GetAssoc ()) && ((*index)->GetMacOfitsAP() == apMac) && ( (*index)->Gettypeofapplication()==typeofapplication) ) {
      AssocNum++;
    }
  }
  return AssocNum;
}

/* I don't need this function
uint32_t
GetstaRecordMaxSizeAmpdu (uint16_t thisSTAid, uint32_t myverbose)
// returns the max size of the Ampdu of an AP
{
  uint32_t staRecordMaxSizeAmpdu = 0;
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
  //std::cout << Simulator::Now ().GetSeconds() << " ********************** AP with ID " << (*index)->GetApid() << " has MAC: " << (*index)->GetMacOfitsAP() << " *****" << std::endl;

    if ( (*index)->GetStaid () == thisSTAid ) {
      staRecordMaxSizeAmpdu = (*index)->GetMaxSizeAmpdu ();
      if ( myverbose > 0 )
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[GetstaRecordMaxSizeAmpdu]\tAP #" << (*index)->GetMacOfitsAP() 
                  << " has AMDPU: " << (*index)->GetMaxSizeAmpdu() 
                  << "" << std::endl;
    }
  }
  return staRecordMaxSizeAmpdu;
}
*/

// Save the position of a STA in a file (to be performed periodically)
static void
SavePositionSTA (double period, Ptr<Node> node, NodeContainer myApNodes, uint16_t portNumber, std::string fileName)
{
  // print the results to a file (they are written at the end of the file)
  if ( fileName != "" ) {

    std::ofstream ofs;
    ofs.open ( fileName, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // Find the position of the STA
    Vector posSTA = GetPosition (node);

    // Find the nearest AP
    Ptr<Node> myNearestAP;
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now ().GetSeconds()
                << "\t[SavePositionSTA] Looking for the nearest AP of node "
                << node->GetId()
                << std::endl;

    myNearestAP = nearestAp (myApNodes, node, 0, "both"); // FIXME
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now ().GetSeconds()
                << "\t[SavePositionSTA] the nearest AP has id "
                << myNearestAP->GetId () << std::endl;

    Vector posMyNearestAP = GetPosition (myNearestAP);
    double distanceToNearestAP = sqrt ( ( (posSTA.x - posMyNearestAP.x)*(posSTA.x - posMyNearestAP.x) ) + ( (posSTA.y - posMyNearestAP.y)*(posSTA.y - posMyNearestAP.y) ) );

    std::string MACaddressAP;

    // find the AP to which the STA is associated
    for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

      if ((*index)->GetStaid () == node->GetId()) {
        // if the STA is associated, find the MAC of the AP
        if ((*index)->GetAssoc ()) {
          // auxiliar string
          std::ostringstream auxString;
          // create a string with the MAC
          auxString << "02-06-" << (*index)->GetMacOfitsAP();
          MACaddressAP = auxString.str();
          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now ().GetSeconds() << "\t[SavePositionSTA] STA with id " << (*index)->GetStaid () << " is associated to the AP with MAC " << MACaddressAP << std::endl;
        }
        else {
          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now ().GetSeconds() << "\t[SavePositionSTA] STA with id " << (*index)->GetStaid () << " is not associated to any AP" << std::endl;
        }
      }
    }

    if (!(MACaddressAP.empty())) {
      // the STA is associated to an AP

      // Find the position and distance of the AP where this STA is associated
      Ptr<Node> myAP;

      NS_ASSERT(!(MACaddressAP.empty()));
      myAP = myApNodes.Get (GetAnAP_Id(MACaddressAP));

      Vector posMyAP = GetPosition (myAP);
      double distanceToMyAP = sqrt ( ( (posSTA.x - posMyAP.x)*(posSTA.x - posMyAP.x) ) + ( (posSTA.y - posMyAP.y)*(posSTA.y - posMyAP.y) ) );

      // print a line in the output file
      ofs << Simulator::Now().GetSeconds() << "\t"
          << (node)->GetId() << "\t"
          << portNumber << "\t"
          << posSTA.x << "\t"
          << posSTA.y << "\t"
          << (myNearestAP)->GetId() << "\t"
          << posMyNearestAP.x << "\t"
          << posMyNearestAP.y << "\t"
          << distanceToNearestAP << "\t"
          << GetAnAP_Id(MACaddressAP) << "\t"
          << posMyAP.x << "\t"
          << posMyAP.y << "\t"
          << distanceToMyAP << "\t"
          << std::endl;
    }
    else {
      // the STA is NOT associated to any AP
      // print a line in the output file
      ofs << Simulator::Now().GetSeconds() << "\t"
          << (node)->GetId() << "\t"
          << portNumber << "\t"
          << posSTA.x << "\t"
          << posSTA.y << "\t"
          << (myNearestAP)->GetId() << "\t"
          << posMyNearestAP.x << "\t"
          << posMyNearestAP.y << "\t"
          << distanceToNearestAP << "\t"
          << "" << "\t"   // as it is not associated, leave this blank
          << "" << "\t"   // as it is not associated, leave this blank
          << "" << "\t"   // as it is not associated, leave this blank
          << "" << "\t"   // as it is not associated, leave this blank
          << std::endl;
    }
    
    // re-schedule
    Simulator::Schedule (Seconds (period), &SavePositionSTA, period, node, myApNodes, portNumber, fileName);
  }
}


// Print the position of a node
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
static void
ReportPosition (double period, Ptr<Node> node, int i, int type, int myverbose, NodeContainer myApNodes)
{
  Vector posSTA = GetPosition (node);

  if (myverbose > 2) {
    // type = 0 means it will write the position of an AP
    if (type == 0) {
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ReportPosition] AP  #" << i 
                <<  " Position: "  << posSTA.x 
                << "," << posSTA.y 
                << std::endl;
    // type = 1 means it will write the position of an STA
    }
    else {
      // Find the nearest AP
      Ptr<Node> myNearestAP;
      myNearestAP = nearestAp (myApNodes, node, myverbose, "both"); // FIXME
      Vector posMyNearestAP = GetPosition (myNearestAP);
      double distance = sqrt ( ( (posSTA.x - posMyNearestAP.x)*(posSTA.x - posMyNearestAP.x) ) + ( (posSTA.y - posMyNearestAP.y)*(posSTA.y - posMyNearestAP.y) ) );

      std::cout << Simulator::Now().GetSeconds()
                << "\t[ReportPosition] STA #" << i 
                <<  " Position: "  << posSTA.x
                << "," << posSTA.y 
                << ". The nearest AP is AP#" << (myNearestAP)->GetId()
                << ". Distance: " << distance << " m"
                << std::endl;
    }
  }

  // re-schedule
  Simulator::Schedule (Seconds (period), &ReportPosition, period, node, i, type, myverbose, myApNodes);
}



/********* FUNCTIONS ************/
FlowMonitorHelper flowmon;  // FIXME avoid this global variable


// Struct for storing the statistics of the VoIP flows
struct FlowStatistics {
  double acumDelay;
  double acumJitter;
  uint32_t acumRxPackets;
  uint32_t acumLostPackets;
  uint32_t acumRxBytes;
  double lastIntervalDelay;
  double lastIntervalJitter;
  uint32_t lastIntervalRxPackets;
  uint32_t lastIntervalLostPackets;
  uint32_t lastIntervalRxBytes;
  uint16_t destinationPort;
};

struct AllTheFlowStatistics {
  uint32_t numberVoIPUploadFlows;
  uint32_t numberVoIPDownloadFlows;
  uint32_t numberTCPUploadFlows;
  uint32_t numberTCPDownloadFlows;
  uint32_t numberVideoDownloadFlows;
  FlowStatistics* FlowStatisticsVoIPUpload;
  FlowStatistics* FlowStatisticsVoIPDownload;
  FlowStatistics* FlowStatisticsTCPUpload;
  FlowStatistics* FlowStatisticsTCPDownload;
  FlowStatistics* FlowStatisticsVideoDownload;
};


// The number of parameters for calling functions using 'schedule' is limited to 6, so I have to create a struct
struct adjustAmpduParameters {
  uint32_t verboseLevel;
  double timeInterval;
  double latencyBudget;
  uint32_t maxAmpduSize;
  std::string mynameAMPDUFile;
  uint16_t methodAdjustAmpdu;
  uint32_t stepAdjustAmpdu; 
};


// Dynamically adjust the size of the AMPDU
void adjustAMPDU (//FlowStatistics* myFlowStatistics,
                  AllTheFlowStatistics myAllTheFlowStatistics,
                  adjustAmpduParameters myparam,
                  uint32_t* belowLatencyAmpduValue,
                  uint32_t* aboveLatencyAmpduValue,
                  uint32_t myNumberAPs)  
{
  // For each AP, find the highest value of the delay of the associated STAs
  for (AP_recordVector::const_iterator indexAP = AP_vector.begin (); indexAP != AP_vector.end (); indexAP++) {
    if (myparam.verboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds()
                << "\t[adjustAMPDU]"
                << "\tAP #" << (*indexAP)->GetApid() 
                << " with MAC " << (*indexAP)->GetMac() 
                << " Max size AMPDU " << (*indexAP)->GetMaxSizeAmpdu() 
                << " Channel " << uint16_t((*indexAP)->GetWirelessChannel())
                << std::endl;

    // find the highest latency of all the VoIP STAs associated to that AP
    double highestLatencyVoIPFlows = 0.0;

    for (STA_recordVector::const_iterator indexSTA = assoc_vector.begin (); indexSTA != assoc_vector.end (); indexSTA++) {

      // check if the STA is associated to an AP
      if ((*indexSTA)->GetAssoc()) {
        // auxiliar string
        std::ostringstream auxString;
        // create a string with the MAC
        auxString << "02-06-" << (*indexSTA)->GetMacOfitsAP();
        std::string addressOfTheAPwhereThisSTAis = auxString.str();
        NS_ASSERT(!addressOfTheAPwhereThisSTAis.empty());

        // check if the STA is associated to this AP
        if ( (*indexAP)->GetMac() == addressOfTheAPwhereThisSTAis ) {

          if (myparam.verboseLevel > 0) 
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[adjustAMPDU]"
                      << "\t\tSTA #" << (*indexSTA)->GetStaid() 
                      << "\tassociated to AP #" << GetAnAP_Id(addressOfTheAPwhereThisSTAis) 
                      << "\twith MAC " << (*indexSTA)->GetMacOfitsAP();

          // VoIP upload
          if ((*indexSTA)->Gettypeofapplication () == 1) {
            if (myparam.verboseLevel > 0)
              std::cout << "\tVoIP upload";

            // index for the vector of statistics of VoIPDownload flows
            uint32_t indexForVector = (*indexSTA)->GetStaid()
                                      - AP_vector.size();

            // 'std::isnan' checks if the value is not a number
            if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPUpload[indexForVector].lastIntervalDelay)) {
              if (myparam.verboseLevel > 0)
                std::cout << "\tDelay: " << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[indexForVector].lastIntervalDelay 
                          << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[indexForVector].lastIntervalRxBytes * 8 / myparam.timeInterval
                          //<< "\t indexForVector is " << indexForVector
                          ;

              // if the latency of this STA is the highest one so far, update the value of the highest latency
              if (  myAllTheFlowStatistics.FlowStatisticsVoIPUpload[ indexForVector ].lastIntervalDelay > highestLatencyVoIPFlows && 
                    !std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPUpload[ indexForVector].lastIntervalDelay))

                highestLatencyVoIPFlows = myAllTheFlowStatistics.FlowStatisticsVoIPUpload[ indexForVector].lastIntervalDelay;

            } else {
              if (myparam.verboseLevel > 0) 
                std::cout << "\tDelay not defined in this period" 
                          //<< "\t (*indexSTA)->GetStaid()  - AP_vector.size() is " << (*indexSTA)->GetStaid() - AP_vector.size()
                          ;
            }


          // VoIP download
          } else if ((*indexSTA)->Gettypeofapplication () == 2) {
            if (myparam.verboseLevel > 0)
              std::cout << "\tVoIP download";

            // index for the vector of statistics of VoIPDownload flows
            uint32_t indexForVector = (*indexSTA)->GetStaid()
                                      - AP_vector.size()
                                      - myAllTheFlowStatistics.numberVoIPUploadFlows;

            // 'std::isnan' checks if the value is not a number                                      
            if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector].lastIntervalDelay)) {
              if (myparam.verboseLevel > 0)
                std::cout << "\tDelay: " << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalDelay 
                          << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval
                          //<< "\t indexForVector is " << indexForVector
                          ;

              // if the latency of this STA is the highest one so far, update the value of the highest latency
              if (  myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalDelay > highestLatencyVoIPFlows && 
                    !std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalDelay))

                highestLatencyVoIPFlows = myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalDelay;

            } else {
              if (myparam.verboseLevel > 0) 
                std::cout << "\tDelay not defined in this period" 
                          //<< "\t indexForVector is " << indexForVector
                          ;
            }


          // TCP upload
          } else if ((*indexSTA)->Gettypeofapplication () == 3) {
            if (myparam.verboseLevel > 0)
              std::cout << "\tTCP upload";

            // index for the vector of statistics of TCPload flows
            uint16_t indexForVector = (*indexSTA)->GetStaid()
                                      - AP_vector.size()
                                      - myAllTheFlowStatistics.numberVoIPDownloadFlows
                                      - myAllTheFlowStatistics.numberTCPUploadFlows;

            // 'std::isnan' checks if the value is not a number
            if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsTCPUpload[ indexForVector ].lastIntervalRxBytes)) {
              if (myparam.verboseLevel > 0)
              std::cout << "\t\t\t"
                        << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsTCPUpload[ indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval 
                        //<< "\t indexForVector is " << indexForVector
                        ;

            } else {
              if (myparam.verboseLevel > 0)
                std::cout << "\tThroughput not defined in this period" 
                          //<< "\t (*indexSTA)->GetStaid()  - AP_vector.size() is " << (*indexSTA)->GetStaid() - AP_vector.size()
                          ;
            }


          // TCP download
          } else if ((*indexSTA)->Gettypeofapplication () == 4) {
            if (myparam.verboseLevel > 0)
              std::cout << "\tTCP download";

            // index for the vector of statistics of TCPDownload flows
            uint32_t indexForVector = (*indexSTA)->GetStaid()
                                      - AP_vector.size()
                                      - myAllTheFlowStatistics.numberVoIPUploadFlows
                                      - myAllTheFlowStatistics.numberVoIPDownloadFlows
                                      - myAllTheFlowStatistics.numberTCPUploadFlows;

            // 'std::isnan' checks if the value is not a number
            if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsTCPDownload[ indexForVector ].lastIntervalRxBytes)) {
              if (myparam.verboseLevel > 0)
              std::cout << "\t\t\t"
                        << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsTCPDownload[ indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval 
                        //<< "\t indexForVector is " << indexForVector
                        ;

            } else {
              if (myparam.verboseLevel > 0)
                std::cout << "\tThroughput not defined in this period" 
                          //<< "\t indexForVector is " << indexForVector
                          ;
            }


          // Video download
          } else if ((*indexSTA)->Gettypeofapplication () == 5) {
            if (myparam.verboseLevel > 0)
              std::cout << "\tVideo download";

            // index for the vector of statistics of VideoDownload flows
            uint32_t indexForVector = (*indexSTA)->GetStaid()
                                      - AP_vector.size()
                                      - myAllTheFlowStatistics.numberVoIPUploadFlows
                                      - myAllTheFlowStatistics.numberVoIPDownloadFlows
                                      - myAllTheFlowStatistics.numberTCPUploadFlows
                                      - myAllTheFlowStatistics.numberTCPDownloadFlows;

            // 'std::isnan' checks if the value is not a number
            if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsVideoDownload[ indexForVector ].lastIntervalRxBytes)) {
              if (myparam.verboseLevel > 0)
              std::cout << "\t\t"
                        << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsVideoDownload[ indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval 
                        //<< "\t indexForVector is " << indexForVector
                        ;
            } else {
              if (myparam.verboseLevel > 0)
                std::cout << "\tThroughput not defined in this period" 
                          //<< "\t indexForVector is " << indexForVector
                          ;
            }
          }
          if (myparam.verboseLevel > 0)           
            std::cout << "\n";
        }

      // this STA is not associated to any AP
      } else {
        if (myparam.verboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[adjustAMPDU]"
                    << "\t\tSTA #" << (*indexSTA)->GetStaid() 
                    << "\tnot associated to any AP #";
      }
    }


    // Adjust the value of the AMPDU

    // Variable to store the new value of the max AMPDU
    uint32_t newAmpduValue;
    
    // Variable to store the current value of the max AMPDU
    uint32_t currentAmpduValue = (*indexAP)->GetMaxSizeAmpdu();

    // Variable to store the minimum AMPDU value
    uint32_t minimumAmpduValue = MTU + 100;

    // First method to adjust AMPDU: linear increase and linear decrease
    if ( myparam.methodAdjustAmpdu == 0 ) {

      // if the latency is above the latency budget, we decrease the AMPDU value
      if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {

        // linearly decrease the AMPDU value

        // check if the current value is smaller than the step
        if (currentAmpduValue < ( AGGRESSIVENESS * myparam.stepAdjustAmpdu ) ) {
          // I can only decrease to the minimum
          newAmpduValue = minimumAmpduValue;

        } else {
          // decrease a step
          newAmpduValue = currentAmpduValue - ( AGGRESSIVENESS * myparam.stepAdjustAmpdu );

          // make sure that the value is at least the minimum
          if ( newAmpduValue < minimumAmpduValue )
            newAmpduValue = minimumAmpduValue;
        }

      // if the latency is below the latency budget, we increase the AMPDU value
      } else {
        // increase the AMPDU value
        newAmpduValue = std::min(( currentAmpduValue + myparam.stepAdjustAmpdu ), myparam.maxAmpduSize); // avoid values above the maximum
      }
    }

    // Second method to adjust AMPDU: linear increase (double aggressiveness, i.e. factor of 2), drastic decrease (instantaneous reduction to the minimum),  
    else if ( myparam.methodAdjustAmpdu == 1 ) {

      //  if the latency is above the latency budget
      if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
        // decrease the AMPDU value
        newAmpduValue = minimumAmpduValue;

      // if the latency is below the latency budget  
      } else {
        // increase the AMPDU value
        newAmpduValue = std::min(( currentAmpduValue + ( 2 * myparam.stepAdjustAmpdu) ), myparam.maxAmpduSize); // avoid values above the maximum
      }
    }

    // Third method to adjust AMPDU: half of what is left 
    else if ( myparam.methodAdjustAmpdu == 2 ) {

      //  if the latency is above the latency budget
      if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
        // decrease the AMPDU value
        newAmpduValue = std::floor((currentAmpduValue - minimumAmpduValue) / 2);

      // if the latency is below the latency budget  
      } else {
        // increase the AMPDU value
        newAmpduValue = currentAmpduValue + std::ceil(( myparam.maxAmpduSize - currentAmpduValue + 1 ) / 2);
      }
    }

    // Fourth method to adjust AMPDU: geometric increase (x2) and geometric decrease (x0.618)
    else if ( myparam.methodAdjustAmpdu == 3 ) {

      //  if the latency is below the latency budget
      if ( highestLatencyVoIPFlows < myparam.latencyBudget ) {
        // increase the AMPDU value
        newAmpduValue = std::min(uint32_t( currentAmpduValue * 2), myparam.maxAmpduSize);

      // if the latency is above the latency budget  
      } else {
        // decrease the AMPDU value
        newAmpduValue = std::max(uint32_t( currentAmpduValue * 0.618 ), minimumAmpduValue); // avoid values above the maximum
      }
    }

    // Fifth method for adjusting the AMPDU
    else if ( myparam.methodAdjustAmpdu == 4 ) {

      //  if the latency is above the latency budget
      if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
        if (myparam.verboseLevel > 2)
          std::cout << "[adjustAMPDU] above latency\n";

        *aboveLatencyAmpduValue = std::max( currentAmpduValue - myparam.stepAdjustAmpdu, minimumAmpduValue);
        *belowLatencyAmpduValue = std::max( *belowLatencyAmpduValue - myparam.stepAdjustAmpdu, minimumAmpduValue);
        newAmpduValue = std::ceil((*aboveLatencyAmpduValue + *belowLatencyAmpduValue + 1 ) / 2);
        if ( newAmpduValue > myparam.maxAmpduSize ) 
          newAmpduValue = myparam.maxAmpduSize;
        if ( newAmpduValue < minimumAmpduValue ) 
          newAmpduValue = minimumAmpduValue;

      } else if (std::abs( myparam.latencyBudget - highestLatencyVoIPFlows ) > 0.001 ) {      
        // if the latency is not very close to the latency budget (epsilon = 0.001 s)
        if (myparam.verboseLevel > 2)
          std::cout << "[adjustAMPDU] not very close to the limit\n";

        *belowLatencyAmpduValue = std::min( currentAmpduValue + myparam.stepAdjustAmpdu, myparam.maxAmpduSize); // avoid values above the maximum
        *aboveLatencyAmpduValue = std::min( *aboveLatencyAmpduValue + myparam.stepAdjustAmpdu, myparam.maxAmpduSize); // avoid values above the maximum

        newAmpduValue = std::ceil((*aboveLatencyAmpduValue + *belowLatencyAmpduValue + 1 ) / 2);
        if ( newAmpduValue > myparam.maxAmpduSize ) 
          newAmpduValue = myparam.maxAmpduSize;
        if ( newAmpduValue < minimumAmpduValue ) 
          newAmpduValue = minimumAmpduValue;

      } else {
        // do nothing
        if (myparam.verboseLevel > 2)
          std::cout << "[adjustAMPDU] very close to the limit\n";
        newAmpduValue = currentAmpduValue;
      }

      if (myparam.verboseLevel > 2) {
        std::cout << Simulator::Now ().GetSeconds()  << '\t';
        std::cout << "[adjustAMPDU] latencyBudget: " << myparam.latencyBudget << '\t';
        std::cout << "highest latency: " << highestLatencyVoIPFlows << '\t';
        std::cout << "currentAmpduValue: " << currentAmpduValue << '\t';
        std::cout << "belowLatencyAmpduValue: " << *belowLatencyAmpduValue << '\t';
        std::cout << "aboveLatencyAmpduValue: " << *aboveLatencyAmpduValue << '\t';
        std::cout << "newAmpduValue: " << newAmpduValue << '\n';
      }
    }

    // Sixth method for adjusting the AMPDU: drastic increase (instantaneous increase to the maximum) and linear decrease (double aggressiveness, i.e. factor of 2)
    else if ( myparam.methodAdjustAmpdu == 5 ) {

      //  if the latency is above the latency budget
      if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
        // linearly decrease the AMPDU value

        // check if the current value is smaller than the step
        if (currentAmpduValue < ( AGGRESSIVENESS * myparam.stepAdjustAmpdu ) ) {
          // I can only decrease to the minimum
          newAmpduValue = minimumAmpduValue;

        } else {
          // decrease a step
          newAmpduValue = currentAmpduValue - ( AGGRESSIVENESS * myparam.stepAdjustAmpdu );

          // make sure that the value is at least the minimum
          if ( newAmpduValue < minimumAmpduValue )
            newAmpduValue = minimumAmpduValue;
        }

      // if the latency is below the latency budget  
      } else {
        // increase the AMPDU value to the maximum
        newAmpduValue = myparam.maxAmpduSize;
      }
    }

    // If the selected method does not exist, exit
    else {
      std::cout << "AMPDU adjust method unknown\n";
      exit (1);
    }

    // write the AMPDU value to a file (it is written at the end of the file)
    if ( myparam.mynameAMPDUFile != "" ) {

      std::ofstream ofsAMPDU;
      ofsAMPDU.open ( myparam.mynameAMPDUFile, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

      ofsAMPDU << Simulator::Now().GetSeconds() << "\t";    // timestamp
      ofsAMPDU << GetAnAP_Id((*indexAP)->GetMac()) << "\t"; // write the ID of the AP to the file
      ofsAMPDU << "AP\t";                                   // type of node
      ofsAMPDU << "-\t";                                    // It is not associated to any AP, since it is an AP
      ofsAMPDU << newAmpduValue << "\n";                    // new value of the AMPDU
    }

    // Check if the AMPDU has to be modified or not
    if (newAmpduValue == currentAmpduValue) {

      // Report that the AMPDU has not been modified
      if (myparam.verboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[adjustAMPDU]"
                  //<< "\tAP #" << GetAnAP_Id((*indexAP)->GetMac())
                  << "\t\tHighest Latency of VoIP flows: " << highestLatencyVoIPFlows << "s (limit " << myparam.latencyBudget << " s)"
                  //<< "\twith MAC: " << (*indexAP)->GetMac() 
                  << "\tAMPDU of the AP not changed (" << (*indexAP)->GetMaxSizeAmpdu() << ")"
                  << std::endl;

    // the AMPDU of the AP has to be modified
    } else {

      // Modify the AMPDU value of the AP itself
      ModifyAmpdu ( GetAnAP_Id((*indexAP)->GetMac()), newAmpduValue, 1 );
      Modify_AP_Record (GetAnAP_Id((*indexAP)->GetMac()), (*indexAP)->GetMac(), newAmpduValue );

      // Report the AMPDU modification
      if (myparam.verboseLevel > 0) {
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[adjustAMPDU]"
                  //<< "\tAP #" << GetAnAP_Id((*indexAP)->GetMac())
                  << "\t\tHighest Latency of VoIP flows: " << highestLatencyVoIPFlows;
                  //<< "\twith MAC: " << (*indexAP)->GetMac();

        if ( newAmpduValue > currentAmpduValue )
          std::cout << "\tAMPDU of the AP increased to " << (*indexAP)->GetMaxSizeAmpdu();
        else 
          std::cout << "\tAMPDU of the AP reduced to " << (*indexAP)->GetMaxSizeAmpdu();

        std::cout << std::endl;
      }


      // Modify the AMPDU value of the STAs associated to the AP which are NOT running VoIP (VoIP STAs never use aggregation)
      for (STA_recordVector::const_iterator indexSTA = assoc_vector.begin (); indexSTA != assoc_vector.end (); indexSTA++) {

        // if the STA is associated
        if ((*indexSTA)->GetAssoc()) {

          // if the STA is NOT running VoIP
          if ( ( (*indexSTA)->Gettypeofapplication () > 2) ) {

            // auxiliar string
            std::ostringstream auxString;
            // create a string with the MAC
            auxString << "02-06-" << (*indexSTA)->GetMacOfitsAP();
            std::string addressOfTheAPwhereThisSTAis = auxString.str();

            // if the STA is associated to this AP
            if ( (*indexAP)->GetMac() == addressOfTheAPwhereThisSTAis ) {
              // modify the AMPDU value
              ModifyAmpdu ((*indexSTA)->GetStaid(), newAmpduValue, 1);  // modify the AMPDU in the STA node
              (*indexSTA)->SetMaxSizeAmpdu(newAmpduValue);              // update the data in the STA_record structure

              // Report this modification
              if (myparam.verboseLevel > 0) {
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[adjustAMPDU]"
                          << "\t\t\tSTA #" << (*indexSTA)->GetStaid() 
                          //<< "\tassociated to AP #" << GetAnAP_Id(addressOfTheAPwhereThisSTAis) 
                          //<< "\twith MAC " << (*indexSTA)->GetMacOfitsAP()
                          ;

                if ((*indexSTA)->Gettypeofapplication () == 3)
                  std::cout << "\t TCP upload";
                else if ((*indexSTA)->Gettypeofapplication () == 4)
                  std::cout << "\t TCP download";
                else if ((*indexSTA)->Gettypeofapplication () == 5)
                  std::cout << "\t Video download";

                if ( newAmpduValue > currentAmpduValue )
                  std::cout << "\t\tAMPDU of the STA increased to " << newAmpduValue;
                else 
                  std::cout << "\t\tAMPDU of the STA reduced to " << newAmpduValue;

                std::cout << "\n";              
              }

              // write the new AMPDU value to a file (it is written at the end of the file)
              if ( myparam.mynameAMPDUFile != "" ) {

                std::ofstream ofsAMPDU;
                ofsAMPDU.open ( myparam.mynameAMPDUFile, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

                ofsAMPDU << Simulator::Now().GetSeconds() << "\t";    // timestamp
                ofsAMPDU << (*indexSTA)->GetStaid() << "\t";          // ID of the AP
                ofsAMPDU << "STA \t";
                ofsAMPDU << GetAnAP_Id((*indexAP)->GetMac()) << "\t";
                ofsAMPDU << newAmpduValue << "\n";                    // new value of the AMPDU
              }
            }
          }
        }
      }
    }
  }

  // Reschedule the calculation
  Simulator::Schedule(  Seconds(myparam.timeInterval),
                        &adjustAMPDU,
                        myAllTheFlowStatistics,
                        myparam,
                        belowLatencyAmpduValue,
                        aboveLatencyAmpduValue,
                        myNumberAPs);
}


// Periodically obtain the statistics of the VoIP flows, using Flowmonitor
void obtainKPIs (Ptr<FlowMonitor> monitor/*, FlowMonitorHelper flowmon*/, 
                  FlowStatistics* myFlowStatistics,
                  uint16_t typeOfFlow,
                  uint32_t verboseLevel,
                  double timeInterval)  //Interval between monitoring moments
{

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint16_t k = 0;

  // for each flow, obtain and update the statistics
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    uint32_t RxPacketsThisInterval;
    uint32_t lostPacketsThisInterval;
    uint32_t RxBytesThisInterval;
    double averageLatencyThisInterval;
    double averageJitterThisInterval;

    //std::cout << "Flow #" << k << "destinationPort: " << t.destinationPort << "/10000 = " << t.destinationPort / 10000 << "\n";

    // avoid the flows of ACKs (TCP also generates these kind of flows)
    if (t.destinationPort != 49153) {
      if (t.destinationPort / 10000 == typeOfFlow ) {
        // obtain the average latency and jitter only in the last interval
        RxPacketsThisInterval = i->second.rxPackets - myFlowStatistics[k].acumRxPackets;
        lostPacketsThisInterval = i->second.lostPackets - myFlowStatistics[k].acumLostPackets;
        RxBytesThisInterval = i->second.rxBytes - myFlowStatistics[k].acumRxBytes;
        averageLatencyThisInterval = (i->second.delaySum.GetSeconds() - myFlowStatistics[k].acumDelay) / RxPacketsThisInterval;
        averageJitterThisInterval = (i->second.jitterSum.GetSeconds() - myFlowStatistics[k].acumJitter) / RxPacketsThisInterval;

        // update the values of the statistics      
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].acumDelay = i->second.delaySum.GetSeconds();
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].acumJitter = i->second.jitterSum.GetSeconds();
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].acumRxPackets = i->second.rxPackets;
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].acumLostPackets = i->second.lostPackets;
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].acumRxBytes = i->second.rxBytes;
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].lastIntervalDelay = averageLatencyThisInterval;
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].lastIntervalJitter = averageJitterThisInterval;
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].lastIntervalRxPackets = RxPacketsThisInterval;
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].lastIntervalLostPackets = lostPacketsThisInterval;
        myFlowStatistics[t.destinationPort % (10000 * typeOfFlow)].lastIntervalRxBytes = RxBytesThisInterval;

        k++;

        if (verboseLevel > 1) {

          std::cout << Simulator::Now().GetSeconds();
          std::cout << "\t[obtainKPIs] flow " << i->first;

          if (t.destinationPort / 10000 == 1 )
            std::cout << "\tVoIP upload\n";
          else if (t.destinationPort / 10000 == 2 )
            std::cout << "\tVoIP download\n";
          else if (t.destinationPort / 10000 == 3 )
            std::cout << "\tTCP upload\n";
          else if (t.destinationPort / 10000 == 4 )
            std::cout << "\tTCP download\n";
          else if (t.destinationPort / 10000 == 5 )
            std::cout << "\tVideo download\n";

          if (verboseLevel > 2) {
            std::cout << "\t\t\tAcum delay at the end of the period: " << i->second.delaySum.GetSeconds() << " [s]\n";
            std::cout << "\t\t\tAcum number of Rx packets: " << i->second.rxPackets << "\n";
            std::cout << "\t\t\tAcum number of Rx bytes: " << i->second.rxBytes << "\n";
            std::cout << "\t\t\tAcum number of lost packets: " << i->second.lostPackets << "\n"; // FIXME
            std::cout << "\t\t\tAcum throughput: " << i->second.rxBytes * 8.0 / (Simulator::Now().GetSeconds() - INITIALTIMEINTERVAL) << "  [bps]\n";  // throughput
            //The previous line does not work correctly. If you add 'monitor->CheckForLostPackets (0.01)' at the beginning of the function, the number
            //of lost packets seems to be higher. However, the obtained number does not correspond to the final number
          }
          std::cout << "\t\t\tAverage delay this period: " << averageLatencyThisInterval << " [s]\n";
          std::cout << "\t\t\tAverage jitter this period: " << averageJitterThisInterval << " [s]\n";
          std::cout << "\t\t\tNumber of Rx packets this period: " << RxPacketsThisInterval << "\n";
          std::cout << "\t\t\tNumber of Rx bytes this period: " << RxBytesThisInterval << "\n";
          std::cout << "\t\t\tNumber of lost packets this period: " << lostPacketsThisInterval << "\n"; // FIXME: This does not work correctly
          std::cout << "\t\t\tThroughput this period: " << RxBytesThisInterval * 8.0 / timeInterval << "  [bps]\n\n";  // throughput
          //std::cout << "\tt\tk= " << k << "\n";
        }
      }
    }
  }

  // Reschedule the calculation
  Simulator::Schedule(  Seconds(timeInterval),
                        &obtainKPIs,
                        monitor/*, flowmon*/, 
                        myFlowStatistics,
                        typeOfFlow,
                        verboseLevel,
                        timeInterval);
}


// Periodically obtain the statistics of the VoIP flows, using Flowmonitor
void saveKPIs ( std::string mynameKPIFile,
                AllTheFlowStatistics myAllTheFlowStatistics,
                uint32_t verboseLevel,
                double timeInterval)  //Interval between monitoring moments
{
  // print the results to a file (they are written at the end of the file)
  if ( mynameKPIFile != "" ) {

    std::ofstream ofs;
    ofs.open ( mynameKPIFile, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberVoIPUploadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i << "\t"; // number of the flow
      ofs << "VoIP_upload\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberVoIPDownloadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i
              + myAllTheFlowStatistics.numberVoIPUploadFlows << "\t"; // number of the flow
      ofs << "VoIP_download\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberTCPUploadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i 
              + myAllTheFlowStatistics.numberVoIPUploadFlows 
              + myAllTheFlowStatistics.numberVoIPDownloadFlows << "\t"; // number of the flow
      ofs << "TCP_upload\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberTCPDownloadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i 
              + myAllTheFlowStatistics.numberVoIPUploadFlows 
              + myAllTheFlowStatistics.numberVoIPDownloadFlows 
              + myAllTheFlowStatistics.numberTCPUploadFlows << "\t"; // number of the flow
      ofs << "TCP_download\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberVideoDownloadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i  
              + myAllTheFlowStatistics.numberVoIPUploadFlows 
              + myAllTheFlowStatistics.numberVoIPDownloadFlows 
              + myAllTheFlowStatistics.numberTCPUploadFlows 
              + myAllTheFlowStatistics.numberTCPDownloadFlows << "\t"; // number of the flow
      ofs << "Video_download\t"; 
      ofs << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }
  }

  // Reschedule the writing
  Simulator::Schedule(  Seconds(timeInterval),
                        &saveKPIs,
                        mynameKPIFile,
                        myAllTheFlowStatistics,
                        verboseLevel,
                        timeInterval);
}


/*****************************/
/************ main ***********/
/*****************************/
int main (int argc, char *argv[]) {

  //bool populatearpcache = false; // Provisional variable FIXME: It should not be necessary

  // Variables to store some fixed parameters
  static uint32_t VoIPg729PayoladSize = 32; // Size of the UDP payload (also includes the RTP header) of a G729a packet with 2 samples
  static double VoIPg729IPT = 0.02; // Time between g729a packets (50 pps)

  static double x_position_first_AP = 0.0;
  static double y_position_first_AP = 0.0;

  static double x_distance_STA_to_AP = 0.0; // initial X distance from the first STA to the first AP
  static double y_distance_STA_to_AP = 5.0; // initial Y distance from the first STA to the first AP

  static double pause_time = 2.0;           // maximum pause time for the random waypoint mobility model


  // Variables to store the input parameters. I add the default value
  uint32_t number_of_APs = 4;
  uint32_t number_of_APs_per_row = 2;
  double distance_between_APs = 50.0; // X-axis and Y-axis distance between APs (meters)

  double distance_between_STAs = 0.0;
  double distanceToBorder = 0.0; // It is used for establishing the coordinates of the square where the STA move randomly

  uint32_t number_of_STAs_per_row;

  uint32_t nodeMobility = 0;
  double constantSpeed = 3.5;  // X-axis speed (m/s) in the case the constant speed model is used (https://en.wikipedia.org/wiki/Preferred_walking_speed)

  double rateAPsWithAMPDUenabled = 1.0; // rate of APs with A-MPDU enabled at the beginning of the simulation

  uint16_t aggregationDisableAlgorithm = 0;  // Set this to 1 in order to make the central control algorithm limiting the AMPDU run
  uint32_t maxAmpduSizeWhenAggregationLimited = 0;  // Only for TCP. Minimum size (to be used when aggregation is 'limited')

  uint16_t aggregationDynamicAlgorithm = 0;  // Set this to 1 in order to make the central control algorithm dynamically modifying AMPDU run
  double latencyBudget = 0.0;  // This is the maximum latency (seconds) tolerated by VoIP applications

  uint16_t topology = 2;    // 0: all the server applications are in a single server
                            // 1: each server application is in a node connected to the hub
                            // 2: each server application is in a node behind the router, connected to it with a P2P connection

  double arpAliveTimeout = 5.0;       // seconds by default
  double arpDeadTimeout = 0.1;       // seconds by default
  uint16_t arpMaxRetries = 30;       // maximum retries or ARPs

  uint32_t TcpPayloadSize = MTU - 52; // 1448 bytes. Prevent fragmentation. Taken from https://www.nsnam.org/doxygen/codel-vs-pfifo-asymmetric_8cc_source.html
  uint32_t VideoMaxPacketSize = MTU - 20 - 8;  //1472 bytes. Remove 20 (IP) + 8 (UDP) bytes from MTU (1500)

  std::string TcpVariant = "TcpNewReno"; // other options "TcpHighSpeed", "TcpWestwoodPlus"

  double simulationTime = 10.0; //seconds

  double timeMonitorKPIs = 0.25;  //seconds

  uint32_t numberVoIPupload = 0;
  uint32_t numberVoIPdownload = 0;
  uint32_t numberTCPupload = 0;
  uint32_t numberTCPdownload = 0;
  uint32_t numberVideoDownload = 0;

  //Using different priorities for VoIP and TCP
  //https://groups.google.com/forum/#!topic/ns-3-users/J3BvzGVJhXM
  //https://groups.google.com/forum/#!topic/ns-3-users/n8h8VbIekoQ
  //http://code.nsnam.org/ns-3-dev/file/06676d0e299f/src/wifi/doc/source/wifi-user.rst
  // the selection of the Access Category (AC) for an MSDU is based on the
  // value of the DS field in the IP header of the packet (ToS field in case of IPv4).
  // You can see the values in WireShark:
  //   IEEE 802.11 QoS data
  //     QoS Control
  uint32_t prioritiesEnabled = 0;
  uint8_t VoIpPriorityLevel = 0xc0;   // corresponds to 1100 0000 (AC_VO)
  uint8_t TcpPriorityLevel = 0x00;    // corresponds to 0000 0000 (AC_BK)
  uint8_t VideoPriorityLevel = 0x80;  // corresponds to 1000 0000 (AC_VI) FIXME check if this is what we want

  uint32_t RtsCtsThreshold = 999999;  // RTS/CTS is disabled by defalult

  double powerLevel = 30.0;  // in dBm

  std::string rateModel = "Ideal"; // Model for 802.11 rate control (Constant; Ideal; Minstrel)

  bool writeMobility = false;
  bool enablePcap = 0; // set this to 1 and .pcap files will be generated (in the ns-3.26 folder)
  uint32_t verboseLevel = 0; // verbose level.
  uint32_t printSeconds = 0; // print the time every 'printSeconds' simulation seconds
  uint32_t generateHistograms = 0; // generate histograms
  std::string outputFileName; // the beginning of the name of the output files to be generated during the simulations
  std::string outputFileSurname; // this will be added to certain files
  bool saveXMLFile = false; // save per-flow results in an XML file

  uint32_t numOperationalChannels = 4; // by default, 4 different channels are used in the APs
  uint32_t numOperationalChannelsSecondary = 4; // by default, 4 different channels are used in the APs

  uint32_t channelWidth = 20;
  uint32_t channelWidthSecondary = 20;

  // https://www.nsnam.org/doxygen/wifi-spectrum-per-example_8cc_source.html
  uint32_t wifiModel = 0;

  uint32_t propagationLossModel = 0; // 0: LogDistancePropagationLossModel (default); 1: FriisPropagationLossModel; 2: FriisSpectrumPropagationLossModel

  uint32_t errorRateModel = 0; // 0 means NistErrorRateModel (default); 1 means YansErrorRateModel

  uint32_t maxAmpduSize, maxAmpduSizeSecondary;     // taken from https://www.nsnam.org/doxygen/minstrel-ht-wifi-manager-example_8cc_source.html

  uint16_t methodAdjustAmpdu = 0;  // method for adjusting the AMPDU size

  uint32_t stepAdjustAmpdu = STEPADJUSTAMPDUDEFAULT; // step for adjusting the AMPDU size. Assign the default value

  bool defineAPsManually = false;   // Define APs positions, versions and parameters manually in the program
                                // if set to 1, the value of version80211primary, numOperationalChannels and channelWidth has to be defined manually in the program

  bool defineSTAsManually = false;    // Define STAs positions, versions and parameters manually in the program
                                      // if set to 1, the value of version80211primary, numOperationalChannels and channelWidth has to be defined manually in the program

  uint32_t numberAPsSamePlace = 1;    // Default 1 (one AP on each place). If set to 2, there will be 2 APs on each place, i.e. '2 x number_of_APs' will be created

  uint32_t numberWiFiDevicesInSTAs = 1; // Default 1 (one wifi card per STA). If set to 2, each STA will have 2 WiFi cards

  //uint32_t version80211primary = 0; // 0 means 802.11n in 5GHz; 1 means 802.11ac; 2 means 802.11n in 2.4GHz 
  std::string version80211primary = "11ac";

  //uint32_t version80211secondary = 1; // Version of 802.11 in secondary APs and in the secondary device of STAs. 0 means 802.11n in 5GHz; 1 means 802.11ac; 2 means 802.11n in 2.4GHz 
  std::string version80211secondary = "11n2.4";

  // Assign the selected value of the MAX AMPDU
  if ( (version80211primary == "11n5") || (version80211primary == "11n2.4") ) {
    // 802.11n
    maxAmpduSize = MAXSIZE80211n;
  }
  else if (version80211primary == "11ac") {
    // 802.11ac
    maxAmpduSize = MAXSIZE80211ac;
  }
  else if (version80211primary == "11g") {
    // 802.11g
    maxAmpduSize = 0;
  }
  else if (version80211primary == "11a") {
    // 802.11a
    maxAmpduSize = 0;
  }

  // Assign the selected value of the MAX AMPDU
  if ( (version80211secondary == "11n5") || (version80211secondary == "11n2.4") ) {
    // 802.11n
    maxAmpduSizeSecondary = MAXSIZE80211n;
  }
  else if (version80211secondary == "11ac") {
    // 802.11ac
    maxAmpduSizeSecondary = MAXSIZE80211ac;
  }
  else if (version80211secondary == "11g") {
    // 802.11g
    maxAmpduSizeSecondary = 0;
  }
  else if (version80211secondary == "11a") {
    // 802.11a
    maxAmpduSizeSecondary = 0;
  }


  /***************** declaring the command line parser (input parameters) **********************/
  CommandLine cmd;

  // General scenario topology parameters
  cmd.AddValue ("simulationTime", "Simulation time [s]", simulationTime);
  
  cmd.AddValue ("timeMonitorKPIs", "Time interval to monitor KPIs of the flows. Also used for adjusting the AMPDU algorithm. 0 (default) means no monitoring", timeMonitorKPIs);

  cmd.AddValue ("numberVoIPupload", "Number of nodes running VoIP up", numberVoIPupload);
  cmd.AddValue ("numberVoIPdownload", "Number of nodes running VoIP down", numberVoIPdownload);
  cmd.AddValue ("numberTCPupload", "Number of nodes running TCP up", numberTCPupload);
  cmd.AddValue ("numberTCPdownload", "Number of nodes running TCP down", numberTCPdownload);
  cmd.AddValue ("numberVideoDownload", "Number of nodes running video down", numberVideoDownload);

  cmd.AddValue ("number_of_APs", "Number of wifi APs", number_of_APs);
  cmd.AddValue ("number_of_APs_per_row", "Number of wifi APs per row", number_of_APs_per_row);
  cmd.AddValue ("distance_between_APs", "Distance in meters between the APs", distance_between_APs);
  cmd.AddValue ("distanceToBorder", "Distance in meters between the AP and the border of the scenario", distanceToBorder);

  cmd.AddValue ("number_of_STAs_per_row", "Number of wifi STAs per row", number_of_STAs_per_row);
  cmd.AddValue ("distance_between_STAs", "Initial distance in meters between the STAs (only for static and linear mobility)", distance_between_STAs);

  cmd.AddValue ("nodeMobility", "Kind of movement of the nodes: '0' static (default); '1' linear; '2' Random Walk 2d; '3' Random Waypoint", nodeMobility);
  cmd.AddValue ("constantSpeed", "Speed of the nodes (in linear and random mobility), default 1.5 m/s", constantSpeed);

  cmd.AddValue ("topology", "Topology: '0' all server applications in a server; '1' all the servers connected to the hub (default); '2' all the servers behind a router", topology);

  // ARP parameters, see https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html
  cmd.AddValue ("arpAliveTimeout", "ARP Alive Timeout [s]: Time an ARP entry will be in ALIVE state (unless refreshed)", arpAliveTimeout);
  cmd.AddValue ("arpDeadTimeout", "ARP Dead Timeout [s]: Time an ARP entry will be in DEAD state before being removed", arpDeadTimeout);
  cmd.AddValue ("arpMaxRetries", "ARP max retries for a resolution", arpMaxRetries);

  // Aggregation parameters
  // The central controller runs an algorithm that dynamically
  // disables aggregation in an AP if a VoIP flow appears, and
  // enables it when the VoIP user leaves. Therefore, VoIP users
  // will always see a non-aggregating AP, whereas TCP users
  // will receive non-aggregated frames in some moments
  cmd.AddValue ("rateAPsWithAMPDUenabled", "Initial rate of APs with AMPDU aggregation enabled", rateAPsWithAMPDUenabled);
  cmd.AddValue ("aggregationDisableAlgorithm", "Is the algorithm enabling/disabling AMPDU aggregation enabled?", aggregationDisableAlgorithm);
  cmd.AddValue ("maxAmpduSize", "Maximum value of the AMPDU [bytes]", maxAmpduSize);
  // The objective of '--maxAmpduSizeWhenAggregationLimited=8000' is the next:
  // the algorithm is used but, instead of deactivating the aggregation,
  // a maximum size of e.g. 8 kB is set when a VoIP flow is present
  // in an AP, in order to limit the added delay.
  cmd.AddValue ("maxAmpduSizeWhenAggregationLimited", "Max AMPDU size to use when aggregation is limited", maxAmpduSizeWhenAggregationLimited);

  // This algorithm dynamically adjusts AMPDU trying to keep VoIP latency below a threshold ('latencyBudget')
  cmd.AddValue ("aggregationDynamicAlgorithm", "Is the algorithm dynamically adapting AMPDU aggregation enabled?", aggregationDynamicAlgorithm);
  cmd.AddValue ("latencyBudget", "Maximum latency [s] tolerated by VoIP applications", latencyBudget);
  cmd.AddValue ("methodAdjustAmpdu", "Method for adjusting AMPDU size: '0' (default), '1' ...", methodAdjustAmpdu);
  cmd.AddValue ("stepAdjustAmpdu", "Step for adjusting AMPDU size [bytes]", stepAdjustAmpdu);

  // TCP parameters
  cmd.AddValue ("TcpPayloadSize", "Payload size [bytes]", TcpPayloadSize);
  cmd.AddValue ("TcpVariant", "TCP variant: TcpNewReno (default), TcpHighSpped, TcpWestwoodPlus", TcpVariant);

  // 802.11 priorities, version, channels
  cmd.AddValue ("prioritiesEnabled", "Use different 802.11 priorities for VoIP / TCP: '0' no (default); '1' yes", prioritiesEnabled);
  cmd.AddValue ("numOperationalChannels", "Number of different channels to use on the APs: 1, 4 (default), 9, 16", numOperationalChannels);
  cmd.AddValue ("channelWidth", "Width of the wireless channels: 20 (default), 40, 80, 160", channelWidth);
  cmd.AddValue ("numOperationalChannelsSecondary", "Number of different channels to use on the APs: 1, 4 (default), 9, 16", numOperationalChannelsSecondary);
  cmd.AddValue ("channelWidthSecondary", "Width of the wireless channels: 20 (default), 40, 80, 160", channelWidthSecondary);
  cmd.AddValue ("rateModel", "Model for 802.11 rate control: 'Constant'; 'Ideal'; 'Minstrel')", rateModel);  
  cmd.AddValue ("RtsCtsThreshold", "Threshold for using RTS/CTS (bytes). Examples: '0' always; '500' only 500 bytes-packes or higher will require RTS/CTS; '999999' never (default)", RtsCtsThreshold);

  // Wi-Fi power, propagation and error models
  cmd.AddValue ("powerLevel", "Power level of the wireless interfaces (dBm), default 30", powerLevel);
  cmd.AddValue ("wifiModel", "WiFi model: '0' YansWifiPhy (default); '1' SpectrumWifiPhy with MultiModelSpectrumChannel", wifiModel);
  // Path loss exponent in LogDistancePropagationLossModel is 3 and in Friis it is supposed to be lower maybe 2.
  cmd.AddValue ("propagationLossModel", "Propagation loss model: '0' LogDistancePropagationLossModel (default); '1' FriisPropagationLossModel; '2' FriisSpectrumPropagationLossModel", propagationLossModel);
  cmd.AddValue ("errorRateModel", "Error Rate model: '0' NistErrorRateModel (default); '1' YansErrorRateModel", errorRateModel);

  // Parameters of the output of the program
  cmd.AddValue ("writeMobility", "Write mobility trace", writeMobility); // creates an output file with the positions of the nodes
  cmd.AddValue ("enablePcap", "Enable/disable pcap file generation", enablePcap);
  cmd.AddValue ("verboseLevel", "Tell echo applications to log if true", verboseLevel);
  cmd.AddValue ("printSeconds", "Periodically print simulation time (even in verboseLevel=0)", printSeconds);
  cmd.AddValue ("generateHistograms", "Generate histograms?", generateHistograms);
  cmd.AddValue ("outputFileName", "First characters to be used in the name of the output files", outputFileName);
  cmd.AddValue ("outputFileSurname", "Other characters to be used in the name of the output files (not in the average one)", outputFileSurname);
  cmd.AddValue ("saveXMLFile", "Save per-flow results to an XML file?", saveXMLFile);

  // Parameters added in order to allow the manual definition of the scenario
  cmd.AddValue ("defineAPsManually", "Define APs positions, versions and parameters manually", defineAPsManually);
  cmd.AddValue ("defineSTAsManually", "Define STAs positions, versions and parameters manually", defineSTAsManually);
  cmd.AddValue ("numberAPsSamePlace", "Default 1 (one AP on each place). If set to 2, there will be 2 APs on each place, i.e. '2 x number_of_APs' will be created", numberAPsSamePlace);
  cmd.AddValue ("numberWiFiDevicesInSTAs", "Default '1'. If set to '2', each STA will have a secondary WiFi device", numberWiFiDevicesInSTAs);
  cmd.AddValue ("version80211primary", "Version of 802.11 in primary APs and in the primary device of STAs: '11n5' (default); '11ac'; '11n2.4'; '11g'; '11a'", version80211primary);
  cmd.AddValue ("version80211secondary", "Version of 802.11 in secondary APs and in the secondary device of STAs: '11n5' (default); '11ac'; '11n2.4'; '11g'; '11a'", version80211secondary);

  cmd.Parse (argc, argv);


  // Variable to store the last AMPDU value for which latency was below the limit
  // I use a pointer because it has to be modified by a function
  uint32_t *belowLatencyAmpduValue;
  belowLatencyAmpduValue = (uint32_t *) malloc (1);
  *belowLatencyAmpduValue = MTU + 100;

  // Variable to store the last AMPDU value for which latency was above the limit
  // I use a pointer because it has to be modified by a function
  uint32_t *aboveLatencyAmpduValue;
  aboveLatencyAmpduValue = (uint32_t *) malloc (1);
  *aboveLatencyAmpduValue = maxAmpduSize;


  // If these parameters have not been set, set the default values
  if ( distance_between_STAs == 0.0 )
    distance_between_STAs = distance_between_APs;

  if ( distanceToBorder == 0.0 )
    distanceToBorder = 0.5 * distance_between_APs; // It is used for establishing the coordinates of the square where the STA move randomly


  // Other variables
  uint32_t number_of_STAs = numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload;   // One STA runs each application
  double x_position_first_STA = x_position_first_AP + x_distance_STA_to_AP;
  double y_position_first_STA = y_position_first_AP + y_distance_STA_to_AP; // by default, the first STA is located some meters above the first AP
  uint32_t number_of_Servers = number_of_STAs;  // the number of servers is the same as the number of STAs. Each server attends a STA


  //the list of channels is here: https://www.nsnam.org/docs/models/html/wifi-user.html
  // see https://en.wikipedia.org/wiki/List_of_WLAN_channels
  uint8_t availableChannels24GHz20MHz[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  uint8_t availableChannels20MHz[34] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112,
                                        116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 
                                        161, 165, 169, 173, 184, 188, 192, 196, 8, 12, 16};

  uint8_t availableChannels40MHz[12] = {38, 46, 54, 62, 102, 110, 118, 126, 134, 142, 151, 159}; 

  uint8_t availableChannels80MHz[6] = {42, 58, 106, 122, 138, 155}; 

  uint8_t availableChannels160MHz[2] = {50, 114};


  //FIXME: Pending to add 2.4GHz channels

  uint32_t j;  //FIXME: remove this variable and declare it when needed


  // One server interacts with each STA
  if (topology == 0) {
    number_of_Servers = 0;
  } else {
    number_of_Servers = number_of_STAs;
  }

  BooleanValue error = 0;



  /********** check input parameters **************/
  // Test some conditions before starting
  if ( (version80211primary == "11n5") || (version80211primary == "11n2.4") ) {
    if ( maxAmpduSize > MAXSIZE80211n ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211n << ". Stopping the simulation." << '\n';
      error = 1;      
    }
  }
  else if (version80211primary == "11ac") {
    if ( maxAmpduSize > MAXSIZE80211ac ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211ac << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else if ((version80211primary == "11g") || (version80211primary == "11a") ) {
    if ( maxAmpduSize > 0 ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << 0 << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else {
    std::cout << "INPUT PARAMETER ERROR: Wrong version of 802.11. Selected: " << version80211primary << ". Stopping the simulation." << '\n';
    error = 1;      
  }

  if ( (version80211secondary == "11n5") || (version80211secondary == "11n2.4") ) {
    if ( maxAmpduSize > MAXSIZE80211n ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211n << ". Stopping the simulation." << '\n';
      error = 1;      
    }
  }
  else if (version80211secondary == "11ac") {
    if ( maxAmpduSize > MAXSIZE80211ac ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211ac << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else if ((version80211secondary == "11g") || (version80211secondary == "11a") ) {
    if ( maxAmpduSize > 0 ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << 0 << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else {
    std::cout << "INPUT PARAMETER ERROR: Wrong version of 802.11. Selected: " << version80211secondary << ". Stopping the simulation." << '\n';
    error = 1;      
  }


  if ((aggregationDisableAlgorithm == 1 ) && (rateAPsWithAMPDUenabled < 1.0 )) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm has to start with all the APs with A-MPDU enabled (--rateAPsWithAMPDUenabled=1.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ( maxAmpduSizeWhenAggregationLimited > maxAmpduSize ) {
    std::cout << "INPUT PARAMETER ERROR: The Max AMPDU size to use when aggregation is limited (" << maxAmpduSizeWhenAggregationLimited << ") has to be smaller or equal than the Max AMPDU size (" << maxAmpduSize << "). Stopping the simulation." << '\n';      
    error = 1;        
  }

  if ((maxAmpduSizeWhenAggregationLimited > 0 ) && ( aggregationDisableAlgorithm == 0 ) ) {
    std::cout << "INPUT PARAMETER ERROR: You cannot set 'maxAmpduSizeWhenAggregationLimited' if 'aggregationDisableAlgorithm' is not active. Stopping the simulation." << '\n';      
    error = 1;        
  }

  if ((aggregationDisableAlgorithm == 1 ) && (aggregationDynamicAlgorithm == 1 )) {
    std::cout << "INPUT PARAMETER ERROR: Only one algorithm for modifying AMPDU can be active ('aggregationDisableAlgorithm' and 'aggregationDynamicAlgorithm'). Stopping the simulation." << '\n';      
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (rateAPsWithAMPDUenabled < 1.0 )) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm has to start with all the APs with A-MPDU enabled (--rateAPsWithAMPDUenabled=1.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (timeMonitorKPIs == 0)) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm for dynamic AMPDU adaptation (--aggregationDynamicAlgorithm=1) requires KPI monitoring ('timeMonitorKPIs' should not be 0.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (latencyBudget == 0.0)) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm for dynamic AMPDU adaptation (--aggregationDynamicAlgorithm=1) requires a latency budget ('latencyBudget' should not be 0.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (numberVoIPupload + numberVoIPdownload == 0)) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm for dynamic AMPDU adaptation (--aggregationDynamicAlgorithm=1) cannot work if there are no VoIP applications. Stopping the simulation." << '\n';
    error = 1;
  }

  if ((rateModel != "Constant") && (rateModel != "Ideal") && (rateModel != "Minstrel")) {
    std::cout << "INPUT PARAMETER ERROR: The wifi rate model MUST be 'Constant', 'Ideal' or 'Minstrel'. Stopping the simulation." << '\n';
    error = 1;
  }

  if (number_of_APs % number_of_APs_per_row != 0) {
    std::cout << "INPUT PARAMETER ERROR: The number of APs MUST be a multiple of the number of APs per row. Stopping the simulation." << '\n';
    error = 1;
  }

  if ((nodeMobility == 0) || (nodeMobility == 1)) {
    if (number_of_APs % number_of_APs_per_row != 0) {
      std::cout << "INPUT PARAMETER ERROR: With static and linear mobility, the number of APs MUST be a multiple of the number of APs per row. Stopping the simulation." << '\n';
      error = 1;
    }
  }

  if ((nodeMobility == 0) || (nodeMobility == 1)) {
    if ( number_of_STAs_per_row == 0 ) {
      std::cout << "INPUT PARAMETER ERROR: With static and linear mobility, the number of STAs per row MUST be defined. Stopping the simulation." << '\n';
      error = 1;
    }
  }

  if ((nodeMobility == 2) || (nodeMobility == 3)) {
    if ( number_of_STAs_per_row != 0 ) {
      std::cout << "INPUT PARAMETER ERROR: With random mobility, the number of STAs per row CANNOT be defined as a parameter. The initial positions are random. Stopping the simulation." << '\n';
      error = 1;
    }
  }

  // check if the channel width is correct
  if ((channelWidth != 20) && (channelWidth != 40) && (channelWidth != 80) && (channelWidth != 160)) {
    std::cout << "INPUT PARAMETER ERROR: The witdth of the channels has to be 20, 40, 80 or 160. Stopping the simulation." << '\n';
    error = 1;    
  }

  if ((channelWidth == 20) && (numOperationalChannels > 34) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 20 MHz channels is 34. Stopping the simulation." << '\n';
    error = 1;    
  }

  if ((channelWidth == 40) && (numOperationalChannels > 12) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 40 MHz channels is 12. Stopping the simulation." << '\n';
    error = 1;    
  }

  if ((channelWidth == 80) && (numOperationalChannels > 6) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 80 MHz channels is 6. Stopping the simulation." << '\n';
    error = 1;    
  }

  if ((channelWidth == 160) && (numOperationalChannels > 2) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 160 MHz channels is 2. Stopping the simulation." << '\n';
    error = 1;    
  }

  // LogDistancePropagationLossModel does not work properly in 2.4 GHz
  if ((version80211primary == "11n2.4" ) && (propagationLossModel == 0)) {
    std::cout << "INPUT PARAMETER ERROR: LogDistancePropagationLossModel does not work properly in 2.4 GHz. Stopping the simulation." << '\n';
    error = 1;      
  }

  if ((version80211secondary == "11n2.4" ) && (propagationLossModel == 0)) {
    std::cout << "INPUT PARAMETER ERROR: LogDistancePropagationLossModel does not work properly in 2.4 GHz. Stopping the simulation." << '\n';
    error = 1;
  }

  if (( numberAPsSamePlace != 1 ) && (numberAPsSamePlace != 2) ) {
    std::cout << "INPUT PARAMETER ERROR: numberAPsSamePlace can only be '1' or '2' so far. Stopping the simulation." << '\n';
    error = 1;
  }

  if (( version80211primary == version80211secondary )) {
    std::cout << "INPUT PARAMETER ERROR: frequencyBandPrimaryAP cannot be equal to version80211secondary. Stopping the simulation." << '\n';
    error = 1;
  }


  // the primary and secondary interfaces CANNOT be in the same band
  std::string frequencyBandPrimary, frequencyBandSecondary;
  if ( (version80211primary == "11n5") || (version80211primary == "11ac") || (version80211primary == "11a") )
    frequencyBandPrimary = "5 GHz"; // 5GHz
  else
    frequencyBandPrimary = "2.4 GHz"; // 2.4 GHz

  if ( (version80211secondary == "11n5") || (version80211secondary == "11ac") || (version80211secondary == "11a") )
    frequencyBandSecondary = "5 GHz"; // 5GHz
  else
    frequencyBandSecondary = "2.4 GHz"; // 2.4 GHz  


  if ((numberAPsSamePlace == 2) || (numberWiFiDevicesInSTAs == 2)) {
    if ( frequencyBandPrimary == frequencyBandSecondary ) {
      std::cout << "INPUT PARAMETER ERROR: Primary and Secondary interfaces CANNOT be in the same band. Stopping the simulation." << '\n';
      error = 1;    
    }    
  }

  // in the 2.4 GHz band, only 20 MHz channels are supported (by this program so far)
  if ((frequencyBandPrimary == "2.4 GHz" ) && (channelWidth != 20)) {
    std::cout << "INPUT PARAMETER ERROR: Only 20 MHz channels can be used (so far) in the 2.4GHz band. Stopping the simulation." << '\n';
    error = 1;  
  }

  // in the 2.4 GHz band, only 20 MHz channels are supported (by this program so far)
  if ((frequencyBandSecondary == "2.4 GHz" ) && (channelWidthSecondary != 20)) {
    std::cout << "INPUT PARAMETER ERROR: Only 20 MHz channels can be used (so far) in the 2.4GHz band. Stopping the simulation." << '\n';
    error = 1;  
  }

  if (error) return 0;
  /********** end of - check input parameters **************/

  
  /******** fill the variable with the available channels *************/
  uint8_t availableChannels[numOperationalChannels];
  for (uint32_t i = 0; i < numOperationalChannels; ++i) {
    if (frequencyBandPrimary == "5 GHz") {
      if (channelWidth == 20)
        availableChannels[i] = availableChannels20MHz[i];
      else if (channelWidth == 40)
        availableChannels[i] = availableChannels40MHz[i];
      else if (channelWidth == 80)
        availableChannels[i] = availableChannels80MHz[i];
      else if (channelWidth == 160)
        availableChannels[i] = availableChannels160MHz[i];      
    }
    else {
      availableChannels[i] = availableChannels24GHz20MHz[i];
    }
  }

  uint8_t availableChannelsSecondary[numOperationalChannels];
  for (uint32_t i = 0; i < numOperationalChannelsSecondary; ++i) {
    if (frequencyBandSecondary == "5 GHz") {
      if (channelWidthSecondary == 20)
        availableChannelsSecondary[i] = availableChannels20MHz[i];
      else if (channelWidthSecondary == 40)
        availableChannelsSecondary[i] = availableChannels40MHz[i];
      else if (channelWidthSecondary == 80)
        availableChannelsSecondary[i] = availableChannels80MHz[i];
      else if (channelWidthSecondary == 160)
        availableChannelsSecondary[i] = availableChannels160MHz[i];      
    }
    else {
      availableChannelsSecondary[i] = availableChannels24GHz20MHz[i];
    }
  }
  /******** end of - fill the variable with the available channels *************/


  /************* Show the parameters by the screen *****************/
  if (verboseLevel > 0) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("SimpleMpduAggregation", LOG_LEVEL_INFO);

    // write the input parameters to the screen

    // General scenario topology parameters
    std::cout << "Simulation Time: " << simulationTime <<" sec" << '\n';
    std::cout << "Time interval to monitor KPIs of the flows. Also used for adjusting the AMPDU algorithm. (0 means no monitoring): " << timeMonitorKPIs <<" sec" << '\n';
    std::cout << "Number of nodes running VoIP up: " << numberVoIPupload << '\n';
    std::cout << "Number of nodes running VoIP down: " << numberVoIPdownload << '\n';
    std::cout << "Number of nodes running TCP up: " << numberTCPupload << '\n';
    std::cout << "Number of nodes running TCP down: " << numberTCPdownload << '\n';
    std::cout << "Number of nodes running video down: " << numberVideoDownload << '\n';
    std::cout << "Number of APs: " << number_of_APs << '\n';    
    std::cout << "Number of APs per row: " << number_of_APs_per_row << '\n'; 
    std::cout << "Distance between APs: " << distance_between_APs << " meters" << '\n';
    std::cout << "Distance between the AP and the border of the scenario: " << distanceToBorder << " meters" << '\n';
    std::cout << "Total number of STAs: " << number_of_STAs << '\n'; 
    std::cout << "Number of STAs per row: " << number_of_STAs_per_row << '\n';
    std::cout << "Initial distance between STAs (only for static and linear mobility): " << distance_between_STAs << " meters" << '\n';
    std::cout << "Node mobility: '0' static; '1' linear; '2' Random Walk 2d; '3' Random Waypoint: " << nodeMobility << '\n';
    std::cout << "Speed of the nodes (in linear and random mobility): " << constantSpeed << " m/s"<< '\n';
    std::cout << "Topology: '0' all server applications in a server; '1' all the servers connected to the hub; '2' all the servers behind a router: " << topology << '\n';
    std::cout << "ARP Alive Timeout [s]: Time an ARP entry will be in ALIVE state (unless refreshed): " << arpAliveTimeout << " s\n";
    std::cout << "ARP Dead Timeout [s]: Time an ARP entry will be in DEAD state before being removed: " << arpDeadTimeout << " s\n";
    std::cout << "ARP max retries for a resolution: " << arpMaxRetries << " s\n";
    std::cout << '\n';
    // Aggregation parameters    
    std::cout << "Initial rate of APs with AMPDU aggregation enabled: " << rateAPsWithAMPDUenabled << '\n';
    std::cout << "Is the algorithm enabling/disabling AMPDU aggregation enabled?: " << aggregationDisableAlgorithm << '\n';
    std::cout << "Maximum value of the AMPDU size: " << maxAmpduSize << " bytes" << '\n';
    std::cout << "Maximum value of the AMPDU size when aggregation is limited: " << maxAmpduSizeWhenAggregationLimited << " bytes" << '\n';
    std::cout << "Is the algorithm dynamically adapting AMPDU aggregation enabled?: " << aggregationDynamicAlgorithm << '\n';
    std::cout << std::setprecision(3) << std::fixed; // to specify the latency budget with 3 decimal digits
    std::cout << "Maximum latency tolerated by VoIP applications: " << latencyBudget << " s" << '\n';
    std::cout << "Method for adjusting AMPDU size: '0' (default), '1' PENDING: " << methodAdjustAmpdu << '\n';
    std::cout << "Step for adjusting AMPDU size: " << stepAdjustAmpdu << " bytes" << '\n';

    std::cout << '\n';
    // TCP parameters
    std::cout << "TCP Payload size: " << TcpPayloadSize << " bytes"  << '\n';
    std::cout << "TCP variant: " << TcpVariant << '\n';
    std::cout << '\n';
    // 802.11 priorities, version, channels  
    std::cout << "Use different 802.11 priorities for VoIP / TCP?: '0' no; '1' yes: " << prioritiesEnabled << '\n';

    // parameters of primary APs and primary devices of STAs
    std::cout << "Version of 802.11: '0' 802.11n 5GHz; '1' 802.11ac; '2' 802.11n 2.4GHz: " << version80211primary << '\n';
    std::cout << "Number of different channels to use on the primary APs: " << numOperationalChannels << '\n';
    std::cout << "Channels being used in primary interfaces: ";
    for (uint32_t i = 0; i < numOperationalChannels; ++i) {
      std::cout << uint16_t (availableChannels[i]) << " ";
    }
    std::cout << '\n';
    std::cout << "Width of the wireless channels of primary interfaces: " << channelWidth << '\n';

    // parameters of secondary APs and secondary interfaces of STAs
    if (numberAPsSamePlace == 2) {
      std::cout << "Version of 802.11 in secondary APs: '0' 802.11n 5GHz; '1' 802.11ac; '2' 802.11n 2.4GHz: " << version80211secondary << '\n';
      std::cout << "Number of different channels to use on secondary APs: " << numOperationalChannelsSecondary << '\n';
      std::cout << "Channels being used in secondary interfaces: ";
      for (uint32_t i = 0; i < numOperationalChannels; ++i) {
        std::cout << uint16_t (availableChannelsSecondary[i]) << " ";
      }
      std::cout << '\n';
      std::cout << "Width of the wireless channels of primary interfaces: " << channelWidthSecondary << '\n';      
    }

    std::cout << "Model for 802.11 rate control 'Constant'; 'Ideal'; 'Minstrel': " << rateModel << '\n';  
    std::cout << "Threshold for using RTS/CTS. Examples. '0' always; '500' only 500 bytes-packes or higher will require RTS/CTS; '999999' never: " << RtsCtsThreshold << " bytes" << '\n';
    std::cout << '\n';
    // Wi-Fi power, propagation and error models  
    std::cout << "Power level of the wireless interfaces: " << powerLevel << " dBm" << '\n';
    std::cout << "WiFi model: '0' YansWifiPhy; '1' SpectrumWifiPhy with MultiModelSpectrumChannel: " << wifiModel << '\n';
    std::cout << "Propagation loss model: '0' LogDistancePropagationLossModel; '1' FriisPropagationLossModel; '2' FriisSpectrumPropagationLossModel: " << propagationLossModel << '\n';
    std::cout << "Error Rate model: '0' NistErrorRateModel; '1' YansErrorRateModel: " << errorRateModel << '\n';
    std::cout << '\n';
    // Parameters of the output of the program  
    std::cout << "pcap generation enabled ?: " << enablePcap << '\n';
    std::cout << "verbose level: " << verboseLevel << '\n';
    std::cout << "Periodically print simulation time every " << printSeconds << " seconds" << '\n';    
    std::cout << "Generate histograms (delay, jitter, packet size): " << generateHistograms << '\n';
    std::cout << "First characters to be used in the name of the output file: " << outputFileName << '\n';
    std::cout << "Other characters to be used in the name of the output file (not in the average one): " << outputFileSurname << '\n';
    std::cout << "Save per-flow results to an XML file?: " << saveXMLFile << '\n';
    std::cout << '\n';
  }
  /************* end of - Show the parameters by the screen *****************/


  // Variable to store the flowmonitor statistics of the flows during the simulation
  // VoIP applications generate 1 flow
  // TCP applications generate 2 flows: 1 for data and 1 for ACKs (statistics are not stored for ACK flows)
  // video applications generate 1 flow
  FlowStatistics myFlowStatisticsVoIPUpload[numberVoIPupload];
  FlowStatistics myFlowStatisticsVoIPDownload[numberVoIPdownload];
  FlowStatistics myFlowStatisticsTCPUpload[numberTCPupload];
  FlowStatistics myFlowStatisticsTCPDownload[numberTCPdownload];
  FlowStatistics myFlowStatisticsVideoDownload[numberVideoDownload];

  AllTheFlowStatistics myAllTheFlowStatistics;
  myAllTheFlowStatistics.numberVoIPUploadFlows = numberVoIPupload;
  myAllTheFlowStatistics.numberVoIPDownloadFlows = numberVoIPdownload;
  myAllTheFlowStatistics.numberTCPUploadFlows = numberTCPupload;
  myAllTheFlowStatistics.numberTCPDownloadFlows = numberTCPdownload;
  myAllTheFlowStatistics.numberVideoDownloadFlows = numberVideoDownload;
  myAllTheFlowStatistics.FlowStatisticsVoIPUpload = myFlowStatisticsVoIPUpload;
  myAllTheFlowStatistics.FlowStatisticsVoIPDownload = myFlowStatisticsVoIPDownload;
  myAllTheFlowStatistics.FlowStatisticsTCPUpload = myFlowStatisticsTCPUpload;
  myAllTheFlowStatistics.FlowStatisticsTCPDownload = myFlowStatisticsTCPDownload;
  myAllTheFlowStatistics.FlowStatisticsVideoDownload = myFlowStatisticsVideoDownload;


  // Initialize to 0
  for (uint16_t i = 0 ; i < numberVoIPupload; i++) {
    myFlowStatisticsVoIPUpload[i].acumDelay = 0.0;
    myFlowStatisticsVoIPUpload[i].acumJitter = 0.0;
    myFlowStatisticsVoIPUpload[i].acumRxPackets = 0;
    myFlowStatisticsVoIPUpload[i].acumRxBytes = 0;
    myFlowStatisticsVoIPUpload[i].acumLostPackets = 0;
    myFlowStatisticsVoIPUpload[i].destinationPort = INITIALPORT_VOIP_UPLOAD + i;
  }

  // Initialize to 0
  for (uint16_t i = 0 ; i < numberVoIPdownload; i++) {
    myFlowStatisticsVoIPDownload[i].acumDelay = 0.0;
    myFlowStatisticsVoIPDownload[i].acumJitter = 0.0;
    myFlowStatisticsVoIPDownload[i].acumRxPackets = 0;
    myFlowStatisticsVoIPDownload[i].acumRxBytes = 0;
    myFlowStatisticsVoIPDownload[i].acumLostPackets = 0;
    myFlowStatisticsVoIPDownload[i].destinationPort = INITIALPORT_VOIP_DOWNLOAD + i;
  }

  // Initialize to 0
  for (uint16_t i = 0 ; i < numberTCPupload; i++) {
    myFlowStatisticsTCPUpload[i].acumDelay = 0.0;
    myFlowStatisticsTCPUpload[i].acumJitter = 0.0;
    myFlowStatisticsTCPUpload[i].acumRxPackets = 0;
    myFlowStatisticsTCPUpload[i].acumRxBytes = 0;
    myFlowStatisticsTCPUpload[i].acumLostPackets = 0;
    myFlowStatisticsTCPUpload[i].destinationPort = INITIALPORT_TCP_UPLOAD + i;
  }

  // Initialize to 0
  for (uint16_t i = 0 ; i < numberTCPdownload; i++) {
    myFlowStatisticsTCPDownload[i].acumDelay = 0.0;
    myFlowStatisticsTCPDownload[i].acumJitter = 0.0;
    myFlowStatisticsTCPDownload[i].acumRxPackets = 0;
    myFlowStatisticsTCPDownload[i].acumRxBytes = 0;
    myFlowStatisticsTCPDownload[i].acumLostPackets = 0;
    myFlowStatisticsTCPDownload[i].destinationPort = INITIALPORT_TCP_DOWNLOAD + i;
  }

  // Initialize to 0
  for (uint16_t i = 0 ; i < numberVideoDownload; i++) {
    myFlowStatisticsVideoDownload[i].acumDelay = 0.0;
    myFlowStatisticsVideoDownload[i].acumJitter = 0.0;
    myFlowStatisticsVideoDownload[i].acumRxPackets = 0;
    myFlowStatisticsVideoDownload[i].acumRxBytes = 0;
    myFlowStatisticsVideoDownload[i].acumLostPackets = 0;
    myFlowStatisticsVideoDownload[i].destinationPort = INITIALPORT_VIDEO_DOWNLOAD + i;
  }


  // ARP parameters
  Config::SetDefault ("ns3::ArpCache::AliveTimeout", TimeValue (Seconds (arpAliveTimeout)));
  Config::SetDefault ("ns3::ArpCache::DeadTimeout", TimeValue (Seconds (arpDeadTimeout)));
  Config::SetDefault ("ns3::ArpCache::MaxRetries", UintegerValue (arpMaxRetries));


  /******** create the node containers *********/
  NodeContainer apNodes;
  // apNodes have:
  //  - a csma device
  //  - a wifi device
  //  - a bridgeAps connecting them
  //  - I do not install the IP stack, as they do not need it

  // wireless STAs. They have a mobility pattern 
  NodeContainer staNodes;

  // server that interacts with the STAs
  NodeContainer singleServerNode;

  // servers that interact with the STAs
  NodeContainer serverNodes;

  // a router connecting to the servers' network
  NodeContainer routerNode;

  // a single csma hub that connects everything
  NodeContainer csmaHubNode;


  /******** create the nodes *********/
  // The order in which you create the nodes is important
  apNodes.Create (number_of_APs * numberAPsSamePlace);

  staNodes.Create (number_of_STAs);

  if (topology == 0) {
    // create a single server
    singleServerNode.Create(1);

  } else {
    // create one server per STA
    serverNodes.Create (number_of_Servers);

    if (topology == 2)
      // in this case, I also need a router
      routerNode.Create(1);
  }

  // Create a hub
  // Inspired on https://www.nsnam.org/doxygen/csma-bridge_8cc_source.html
  csmaHubNode.Create (1);


  /************ Install Internet stack in the nodes ***************/
  InternetStackHelper stack;

  //stack.Install (apNodes); // I do not install it because they do not need it

  stack.Install (staNodes);

  if (topology == 0) {
    // single server
    stack.Install (singleServerNode);

  } else {
    // one server per STA
    stack.Install (serverNodes);

    if (topology == 2)
      // in this case, I also need a router
      stack.Install (routerNode);
  }


  /******** create the net device containers *********/
  NetDeviceContainer apCsmaDevices;
  std::vector<NetDeviceContainer> apWiFiDevices;
  std::vector<NetDeviceContainer> staDevices;
  std::vector<NetDeviceContainer> staDevicesSecondary;  
  NetDeviceContainer serverDevices;
  NetDeviceContainer singleServerDevices;
  NetDeviceContainer csmaHubDevices;                // each network card of the hub
  //NetDeviceContainer bridgeApDevices;             // A bridge container for the bridge of each AP node
  NetDeviceContainer routerDeviceToAps, routerDeviceToServers;


  /******* IP interfaces *********/
  //Ipv4InterfaceContainer apInterfaces;
  std::vector<Ipv4InterfaceContainer> staInterfaces;
  std::vector<Ipv4InterfaceContainer> staInterfacesSecondary;
  Ipv4InterfaceContainer serverInterfaces;
  Ipv4InterfaceContainer singleServerInterfaces;
  //Ipv4InterfaceContainer csmaHubInterfaces;
  Ipv4InterfaceContainer routerInterfaceToAps, routerInterfaceToServers;


  /********* IP addresses **********/
  Ipv4AddressHelper ipAddressesSegmentA;
  ipAddressesSegmentA.SetBase ("10.0.0.0", "255.255.0.0"); // If you use 255.255.255.0, you can only use 256 nodes

  Ipv4AddressHelper ipAddressesSegmentB;    // The servers are behind the router, so they are in other network
  ipAddressesSegmentB.SetBase ("10.1.0.0", "255.255.0.0");


  /******** mobility *******/
  MobilityHelper mobility;

  // mobility of the backbone nodes (i.e. APs and servers): constant position
  for (uint32_t j = 0; j < numberAPsSamePlace; ++j) {

    // create an auxiliary container and put only the nodes with the same WiFi configuration
    // i.e. the APs that are in different places
    NodeContainer apNodesAux;
    for (uint32_t i = 0; i < number_of_APs; ++i) {
      apNodesAux.Add(apNodes.Get(i + j*number_of_APs));
    }

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (x_position_first_AP),
                                  "MinY", DoubleValue (y_position_first_AP),
                                  "DeltaX", DoubleValue (distance_between_APs),
                                  "DeltaY", DoubleValue (distance_between_APs),
                                  "GridWidth", UintegerValue (number_of_APs_per_row), // size of the row
                                  "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  //mobility.Install (backboneNodes);
    mobility.Install (apNodesAux);
  }


  if (verboseLevel >= 2) {
    for (uint32_t j = 0; j < numberAPsSamePlace; ++j) {
      for (uint32_t i = 0; i < number_of_APs; ++i) {
        //ReportPosition (timeMonitorKPIs, backboneNodes.Get(i), i, 0, 1, apNodes); this would report the position every second
        //Vector pos = GetPosition (backboneNodes.Get (i));
        Vector pos = GetPosition (apNodes.Get (i + j*number_of_APs));
        std::cout << "AP#" << i + j*number_of_APs << " Position: " << pos.x << "," << pos.y << '\n';
      }      
    }
  }


  // Set the positions and the mobility of the STAs
  // Taken from https://www.nsnam.org/docs/tutorial/html/building-topologies.html#building-a-wireless-network-topology

  // STAs do not move
  if (nodeMobility == 0) {

    mobility.SetPositionAllocator ( "ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (x_position_first_STA),
                                    "MinY", DoubleValue (y_position_first_STA),
                                    "DeltaX", DoubleValue (distance_between_STAs),
                                    "DeltaY", DoubleValue (distance_between_STAs),
                                    "GridWidth", UintegerValue (number_of_STAs_per_row),  // size of the row
                                    "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    mobility.Install (staNodes);

  // STAs linear mobility: constant speed to the right
  }
  else if (nodeMobility == 1) {
    mobility.SetPositionAllocator ( "ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (x_position_first_STA),
                                    "MinY", DoubleValue (y_position_first_STA),
                                    "DeltaX", DoubleValue (distance_between_STAs),
                                    "DeltaY", DoubleValue (1),
                                    "GridWidth", UintegerValue (number_of_STAs_per_row),  // size of the row
                                    "LayoutType", StringValue ("RowFirst"));

    Ptr<ConstantVelocityMobilityModel> mob;
    Vector m_velocity = Vector(constantSpeed, 0.0, 0.0);

    // https://www.nsnam.org/doxygen/classns3_1_1_constant_velocity_mobility_model.html#details
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install (staNodes);

    for ( j = 0; j < staNodes.GetN(); ++j) {
      // https://www.nsnam.org/doxygen/classns3_1_1_constant_velocity_mobility_model.html#details
      mob = staNodes.Get(j)->GetObject<ConstantVelocityMobilityModel>();
      mob->SetVelocity(m_velocity);
    }

  // STAs random walk 2d mobility mobility
  }
  else if (nodeMobility == 2) { 

    // Each instance moves with a speed and direction choosen at random with the user-provided random variables until either a fixed distance has been walked or until a fixed amount of time. If we hit one of the boundaries (specified by a rectangle), of the model, we rebound on the boundary with a reflexive angle and speed. This model is often identified as a brownian motion model.
    /*
    // https://www.nsnam.org/doxygen/classns3_1_1_rectangle.html
    // Rectangle (double _xMin, double _xMax, double _yMin, double _yMax)
    mobility.SetMobilityModel ( "ns3::RandomWalk2dMobilityModel",
                                //"Mode", StringValue ("Time"),
                                //"Time", StringValue ("2s"),
                                //"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", RectangleValue (Rectangle ( x_position_first_AP - distanceToBorder, 
                                                                      ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder,
                                                                      y_position_first_AP - distanceToBorder,
                                                                      ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder)));
    */
    /*
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Mode", StringValue ("Time"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Time", StringValue ("2s"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Bounds", StringValue ("0|200|0|200"));
    */

    // auxiliar string
    std::ostringstream auxString;

    // create a string with the X boundaries
    auxString << "ns3::UniformRandomVariable[Min=" << x_position_first_AP - distanceToBorder 
              << "|Max=" << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder << "]"; 
    std::string XString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Limits for X: " << XString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the Y boundaries
    auxString   << "ns3::UniformRandomVariable[Min=" << y_position_first_AP - distanceToBorder 
                << "|Max=" << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder << "]"; 
    std::string YString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Limits for Y: " << YString << '\n';

    // Locate the STAs initially
    mobility.SetPositionAllocator ( "ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue (XString),
                                    "Y", StringValue (YString));

    // clean the string
    auxString.str(std::string());

    // create a string with the time between speed modifications
    auxString << "5s";
    std::string timeString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "The STAs will change their trajectory every " << timeString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the speed
    auxString  << "ns3::ConstantRandomVariable[Constant=" << constantSpeed << "]";
    std::string speedString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Speed with which the STAs move: " << speedString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string like this: "0|200|0|200"
    auxString << x_position_first_AP - distanceToBorder << "|" 
              << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder << "|"
              << y_position_first_AP - distanceToBorder << "|"
              << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder;
    std::string boundsString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Rectangle where the STAs move: " << boundsString << '\n';

    mobility.SetMobilityModel ( "ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", StringValue (timeString),   // the node will modify its speed every 'timeString' seconds
                                "Speed", StringValue (speedString), // the speed is always between [-speedString,speedString]
                                //"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", StringValue (boundsString));
                                //"Bounds", StringValue ("-50|250|-50|250"));

    mobility.Install (staNodes);

  // STAs random waypoint model
  }
  else if (nodeMobility == 3) {
    // https://www.nsnam.org/doxygen/classns3_1_1_random_waypoint_mobility_model.html#details
    // Each object starts by pausing at time zero for the duration governed by the random variable 
    // "Pause". After pausing, the object will pick a new waypoint (via the PositionAllocator) and a 
    // new random speed via the random variable "Speed", and will begin moving towards the waypoint 
    // at a constant speed. When it reaches the destination, the process starts over (by pausing).

    // auxiliar string
    std::ostringstream auxString;

    // create a string with the X boundaries
    auxString << "ns3::UniformRandomVariable[Min="
              << x_position_first_AP - distanceToBorder 
              << "|Max=" << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder 
              << "]";

    std::string XString = auxString.str();

    if ( verboseLevel > 2 )
      std::cout << "Limits for X: " << XString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the Y boundaries
    auxString   << "ns3::UniformRandomVariable[Min=" << y_position_first_AP - distanceToBorder 
                << "|Max=" << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder 
                << "]"; 

    std::string YString = auxString.str();

    if ( verboseLevel > 2 )
      std::cout << "Limits for Y: " << YString << '\n';


    ObjectFactory pos;
    pos.SetTypeId ( "ns3::RandomRectanglePositionAllocator");

    pos.Set ("X", StringValue (XString));
    pos.Set ("Y", StringValue (YString));

    // clean the string
    auxString.str(std::string());

    // create a string with the speed
    auxString  << "ns3::UniformRandomVariable[Min=0.0|Max=" << constantSpeed << "]";
    std::string speedString = auxString.str();
    if ( verboseLevel > 2 )
      std::cout << "Speed with which the STAs move: " << speedString << '\n';


    // clean the string
    auxString.str(std::string());

    // create a string with the pause time
    auxString  << "ns3::UniformRandomVariable[Min=0.0|Max=" << pause_time << "]";
    std::string pauseTimeString = auxString.str();
    if ( verboseLevel > 2 )
      std::cout << "The STAs will pause during " << pauseTimeString << '\n';

    Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    mobility.SetMobilityModel ( "ns3::RandomWaypointMobilityModel",
                                "Speed", StringValue (speedString),
                                //"Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                "Pause", StringValue (pauseTimeString),
                                //"Pause", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                "PositionAllocator", PointerValue (taPositionAlloc));
    mobility.SetPositionAllocator (taPositionAlloc);
    mobility.Install (staNodes);
  }

/* 
  if (verboseLevel > 0)
    for ( j = 0; j < number_of_STAs; ++j) {
      Vector pos = GetPosition (staNodes.Get (j));
      std::cout << "STA#" << number_of_APs + number_of_Servers + j << " Position: " << pos.x << "," << pos.y << '\n';
    }
*/


  // Periodically report the positions of all the STAs
  if ((verboseLevel > 2) && (timeMonitorKPIs > 0)) {
    for (uint32_t j = 0; j < number_of_STAs; ++j) {
      Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL),
                            &ReportPosition, 
                            timeMonitorKPIs, 
                            staNodes.Get(j), 
                            (number_of_APs * numberAPsSamePlace )+ j, 
                            1, 
                            verboseLevel, 
                            apNodes);
    }

    // This makes a callback every time a STA changes its course
    // see trace sources in https://www.nsnam.org/doxygen/classns3_1_1_random_walk2d_mobility_model.html
    Config::Connect ( "/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChange));
  }


  if (timeMonitorKPIs > 0) {
    // Write the values of the positions of the STAs to a file
    // create a string with the name of the output file
    std::ostringstream namePositionsFile;

    namePositionsFile << outputFileName
                << "_"
                << outputFileSurname
                << "_positions.txt";

    std::ofstream ofs;
    ofs.open ( namePositionsFile.str(), std::ofstream::out | std::ofstream::trunc); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // write the first line in the file (includes the titles of the columns)
    ofs << "timestamp [s]\t"
        << "STA ID\t"
        << "destinationPort\t"
        << "STA x [m]\t"
        << "STA y [m]\t"
        << "Nearest AP ID\t" 
        << "AP x [m]\t"
        << "AP y [m]\t"
        << "distance STA-nearest AP [m]\t"
        << "Associated to AP ID\t"
        << "AP x [m]\t"
        << "AP y [m]\t"
        << "distance STA-my AP [m]\t"
        << "\n";

    for (uint16_t j = 0; j < numberVoIPupload; ++j) {
      Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                            &SavePositionSTA, 
                            timeMonitorKPIs, 
                            staNodes.Get(j), 
                            apNodes, 
                            INITIALPORT_VOIP_UPLOAD + j, 
                            namePositionsFile.str());
    }

    for (uint16_t j = numberVoIPupload; j < numberVoIPupload + numberVoIPdownload; ++j) {
      Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                            &SavePositionSTA, 
                            timeMonitorKPIs, 
                            staNodes.Get(j), 
                            apNodes, 
                            INITIALPORT_VOIP_DOWNLOAD + j, 
                            namePositionsFile.str());
    }

    for (uint16_t j = numberVoIPupload + numberVoIPdownload; j < numberVoIPupload + numberVoIPdownload + numberTCPupload; ++j) {
      Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                            &SavePositionSTA, 
                            timeMonitorKPIs, 
                            staNodes.Get(j), 
                            apNodes, 
                            INITIALPORT_TCP_UPLOAD + j, 
                            namePositionsFile.str());
    }

    for (uint16_t j = numberVoIPupload + numberVoIPdownload + numberTCPupload; j < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; ++j) {
      Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                            &SavePositionSTA, 
                            timeMonitorKPIs, 
                            staNodes.Get(j), 
                            apNodes, 
                            INITIALPORT_TCP_DOWNLOAD + j, 
                            namePositionsFile.str());
    }

    for (uint16_t j = numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; j < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload; ++j) {
      Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                            &SavePositionSTA, 
                            timeMonitorKPIs, 
                            staNodes.Get(j), 
                            apNodes, 
                            INITIALPORT_VIDEO_DOWNLOAD + j, 
                            namePositionsFile.str());
    }
  }


  /******** create the channels (wifi, csma and point to point) *********/

  // create the wifi phy layer, using 802.11n in 5GHz

  // create the wifi channel

  // if wifiModel == 0
  // we use the "yans" model. See https://www.nsnam.org/doxygen/classns3_1_1_yans_wifi_channel_helper.html#details
  // it is described in this paper: http://cutebugs.net/files/wns2-yans.pdf
  // The yans name stands for "Yet Another Network Simulator"
  //YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();

  // if wifiModel == 1
  // we use the spectrumWifi model
  SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();

  if (wifiModel == 0) {

    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel;

    // propagation models: https://www.nsnam.org/doxygen/group__propagation.html
    if (propagationLossModel == 0) {
      wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
    } else if (propagationLossModel == 1) {
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    }

    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (powerLevel)); // a value of '1' means 1 dBm (1.26 mW)
    wifiPhy.Set ("TxPowerEnd", DoubleValue (powerLevel));
    // Experiences:   at 5GHz,  with '-15' the coverage is less than 70 m
    //                          with '-10' the coverage is about 70 m (recommended for an array with distance 50m between APs)

    wifiPhy.Set ("ShortGuardEnabled", BooleanValue (false));
    wifiPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
    //wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNo)); //This is done later

    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    if (errorRateModel == 0) { // Nist
      wifiPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
    } else { // errorRateModel == 1 (Yans)
      wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");      
    }

    /*     //FIXME: Can this be done with YANS?

    //Ptr<wifiPhy> myphy = node->GetObject<wifiPhy> ();
    if (numOperationalChannels > 1) 
      for (uint32_t k = 0; k < numOperationalChannels; k++)
        wifiPhy.AddOperationalChannel ( availableChannels[k] );
    */

  }
  else {
    // wifiModel == 1

    // The energy of a received signal should be higher than this threshold (dbm) to allow the PHY layer to declare CCA BUSY state
    // this works until ns3-29:
    // Config::SetDefault ("ns3::WifiPhy::CcaMode1Threshold", DoubleValue (-95.0));
    // this should work for ns3-30 (but it does not work):
    //Config::SetDefault ("ns3::WifiPhy::SetCcaEdThreshold", DoubleValue (-95.0));
    // The energy of a received signal should be higher than this threshold (dbm) to allow the PHY layer to detect the signal.
    Config::SetDefault ("ns3::WifiPhy::EnergyDetectionThreshold", DoubleValue (-95.0));
    

    // Use multimodel spectrum channel, https://www.nsnam.org/doxygen/classns3_1_1_multi_model_spectrum_channel.html
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();

    // propagation models: https://www.nsnam.org/doxygen/group__propagation.html
    if (propagationLossModel == 0) {
      //spectrumChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
    } else if (propagationLossModel == 1) {
      //spectrumChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
      Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
    } else if (propagationLossModel == 2) {
      //spectrumChannel.AddPropagationLoss ("ns3::FriisSpectrumPropagationLossModel");
      Ptr<FriisSpectrumPropagationLossModel> lossModel = CreateObject<FriisSpectrumPropagationLossModel> ();
      spectrumChannel->AddSpectrumPropagationLossModel (lossModel);
    }

    // delay model
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
    spectrumChannel->SetPropagationDelayModel (delayModel);

    spectrumPhy.SetChannel (spectrumChannel);
    if (errorRateModel == 0) { //Nist
      spectrumPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
    } else { // errorRateModel == 1 (Yans)
      spectrumPhy.SetErrorRateModel ("ns3::YansErrorRateModel");      
    }


    //spectrumPhy.Set ("Frequency", UintegerValue (5180));
    //spectrumPhy.Set ("ChannelNumber", UintegerValue (ChannelNo)); //This is done later
    spectrumPhy.Set ("TxPowerStart", DoubleValue (powerLevel));  // a value of '1' means 1 dBm (1.26 mW)
    spectrumPhy.Set ("TxPowerEnd", DoubleValue (powerLevel));

    spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (false));
    spectrumPhy.Set ("ChannelWidth", UintegerValue (channelWidth));

    Ptr<SpectrumWifiPhy> m_phy;
    m_phy = CreateObject<SpectrumWifiPhy> ();

    // Test of the Addoperationalchannel functionality
    //(*m_phy).DoChannelSwitch (uint8_t(40) ); It is private
    // Add a channel number to the list of operational channels.
    // https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#a948c6d197accf2028529a2842ec68816

    // https://groups.google.com/forum/#!topic/ns-3-users/Ih8Hgs2qgeg

/*    // This does not work, i.e. the STA does not scan in other channels
    if (numOperationalChannels > 1) 
      for (uint32_t k = 0; k < numOperationalChannels; k++)
        (*m_phy).AddOperationalChannel ( availableChannels[k] );*/
  }


  // Create a Wifi helper in an empty state: all its parameters must be set before calling ns3::WifiHelper::Install.
  // https://www.nsnam.org/doxygen/classns3_1_1_wifi_helper.html
  // The default state is defined as being an Adhoc MAC layer with an ARF rate control
  // algorithm and both objects using their default attribute values. By default, configure MAC and PHY for 802.11a.
  WifiHelper wifi;
  WifiHelper wifiSecondary; // only used for the secondary WiFi card of each STA

  if ( verboseLevel > 3 )
    wifi.EnableLogComponents ();  // Turn on all Wifi logging

  // The SetRemoteStationManager method tells the helper the type of rate control algorithm to use.
  // constant_rate_wifi_manager uses always the same transmission rate for every packet sent.
  // https://www.nsnam.org/doxygen/classns3_1_1_constant_rate_wifi_manager.html#details
  //wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"), "ControlMode", StringValue ("HtMcs0"));

  // Select the most appropriate wifimanager
  // ARF Rate control algorithm.
  // arf_wifi_manager implements the so-called ARF algorithm which was initially described in WaveLAN-II: A High-performance wireless LAN for the unlicensed band, by A. Kamerman and L. Monteban. in Bell Lab Technical Journal, pages 118-133, Summer 1997.
  // https://www.nsnam.org/doxygen/classns3_1_1_arf_wifi_manager.html
  // This RAA (Rate Adaptation Algorithm) does not support HT, VHT nor HE modes and will error exit if the user tries to configure 
  // this RAA with a Wi-Fi MAC that has VhtSupported, HtSupported or HeSupported set
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  // The next line is not necessary, as it only defines the default WifiRemoteStationManager
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", 
  //                    enableRtsCts ? StringValue ("0") : StringValue ("999999")); // if enableRtsCts is true, I select the first option


  // MinstrelHt and Ideal do support HT/VHT (i.e. 802.11n and above)
  if (rateModel == "Constant") {
    // more rates here https://www.nsnam.org/doxygen/wifi-spectrum-per-example_8cc_source.html
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("HtMcs7"),
                                  "ControlMode", StringValue ("HtMcs0"),
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold));

    wifiSecondary.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("HtMcs7"),
                                  "ControlMode", StringValue ("HtMcs0"),
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold));
  /*  This is another option:
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", DataRate,
                                  "ControlMode", DataRate,
                                  "RtsCtsThreshold", UintegerValue (ctsThr));
  */

  }

  else if (rateModel == "Ideal") {
    // Ideal Wifi Manager, https://www.nsnam.org/doxygen/classns3_1_1_ideal_wifi_manager.html#details
    wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                  //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold));

    wifiSecondary.SetRemoteStationManager ("ns3::IdealWifiManager",
                                  //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold));

    /*  This is another option:
    wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                  //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                  "RtsCtsThreshold", enableRtsCts ? UintegerValue (RtsCtsThreshold) : UintegerValue (999999)); // if enableRtsCts is true, I select the first option
    */

  }

  else if (rateModel == "Minstrel") {
    // I obtain some errors when running Minstrel
    // https://www.nsnam.org/bugzilla/show_bug.cgi?id=1797
    // https://www.nsnam.org/doxygen/classns3_1_1_minstrel_ht_wifi_manager.html
    wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold),
                                  "PrintStats", BooleanValue (false)); // if you set this to true, you will obtain a file with the stats

    wifiSecondary.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold),
                                  "PrintStats", BooleanValue (false)); // if you set this to true, you will obtain a file with the stats
  }



  // Create the MAC helper
  // https://www.nsnam.org/doxygen/classns3_1_1_wifi_mac_helper.html
  WifiMacHelper wifiMacPrimary;
  WifiMacHelper wifiMacSecondary; // only used for the secondary WiFi device of the STAs



  /*************************** Define the APs ******************************/
  // create '2 x number_of_APs' 
  //
  // example: the user selects 'number_of_APs=3' and 'numberAPsSamePlace=2'
  //    in this case, 6 APs will be created:
  //    - AP0 and AP3 will be in the same place, and will have different WiFi versions
  //    - AP1 and AP4 will be in the same place, and will have different WiFi versions
  //    - AP2 and AP5 will be in the same place, and will have different WiFi versions

  for (uint32_t j = 0 ; j < numberAPsSamePlace ; ++j) {

    for (uint32_t i = 0 ; i < number_of_APs ; ++i ) {

      // Create an empty record per AP
      AP_record *m_AP_record = new AP_record;
      AP_vector.push_back(m_AP_record);

      // I use an auxiliary device container and an auxiliary interface container
      NetDeviceContainer apWiFiDev;

      // create an ssid for each wifi AP
      std::ostringstream oss;

      if (j == 0)
        // primary AP
        oss << "wifi-default-" << i; // Each AP will have a different SSID
      else if (j == 1)
        // secondary AP (an AP in the same place)
        oss << "wifi-default-" << i << "-secondary"; // SSID for the secondary AP: just add 'secondary' at the end

      Ssid apssid = Ssid (oss.str());

      // Max AMPDU size of this AP
      uint32_t my_maxAmpduSize;

      // channel to be used by these APs (main and secondary)
      uint8_t ChannelNoForThisAP;


      if (!defineAPsManually) {
        // define the APs automatically

        // define the standard to follow (see https://www.nsnam.org/doxygen/group__wifi.html#ga1299834f4e1c615af3ca738033b76a49)
        if (j == 0) {
          // primary AP
          wifi.SetStandard (convertVersionToStandard(version80211primary));
        }
        else if (j == 1) {
          // secondary AP
          wifi.SetStandard (convertVersionToStandard(version80211secondary));
        }

        // setup the APs. Modify the maxAmpduSize depending on a random variable
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
        
        double random = uv->GetValue ();

        if (j == 0) {
          // primary AP
          if ( random < rateAPsWithAMPDUenabled ) {
            my_maxAmpduSize = maxAmpduSize;
          }
          else {
            my_maxAmpduSize = 0;        
          }
        }
        else if (j == 1) {
          // secondary AP
          if ( random < rateAPsWithAMPDUenabled ) {
            my_maxAmpduSize = maxAmpduSizeSecondary;
          }
          else {
            my_maxAmpduSize = 0;        
          }
        }


        // define AMPDU size
        wifiMacPrimary.SetType ( "ns3::ApWifiMac",
                          "Ssid", SsidValue (apssid),
                          "QosSupported", BooleanValue (true),
                          "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                          "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

        wifiMacSecondary.SetType ( "ns3::ApWifiMac",
                          "Ssid", SsidValue (apssid),
                          "QosSupported", BooleanValue (true),
                          "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                          "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

        /*  Other options:
          // - enable A-MSDU (with maximum size of 8 kB) but disable A-MPDU;
          wifiMacPrimary.SetType ( "ns3::ApWifiMac",
                            "Ssid", SsidValue (apssid),
                            "BeaconGeneration", BooleanValue (true),
                            "BE_MaxAmpduSize", UintegerValue (0), //Disable A-MPDU
                            "BE_MaxAmsduSize", UintegerValue (7935)); //Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)

          // - use two-level aggregation (A-MPDU with maximum size of 32 kB and A-MSDU with maximum size of 4 kB).
          wifiMacPrimary.SetType ( "ns3::ApWifiMac",
                            "Ssid", SsidValue (apssid),
                            "BeaconGeneration", BooleanValue (true),
                            "BE_MaxAmpduSize", UintegerValue (32768), //Enable A-MPDU with a smaller size than the default one
                            "BE_MaxAmsduSize", UintegerValue (3839)); //Enable A-MSDU with the smallest maximum size allowed by the standard (3839 bytes)
        */

        // Define the channel of the AP
        // Use the available channels in turn
        if (j == 0)
          // primary AP
          ChannelNoForThisAP = availableChannels[i % numOperationalChannels];
        else if (j == 1)
          // secondary AP
          ChannelNoForThisAP = availableChannelsSecondary[i % numOperationalChannels]; // same channel
      }

      else {
        // define the characteristics of the APs manually
        // define the standard to follow (see https://www.nsnam.org/doxygen/group__wifi.html#ga1299834f4e1c615af3ca738033b76a49)
        if (j == 0) {
          // primary AP
          wifi.SetStandard (convertVersionToStandard(version80211primary));
        }
        else if (j == 1) {
          // secondary AP
          wifi.SetStandard (convertVersionToStandard(version80211secondary));
        }


        // Modify the maxAmpduSize depending on the AP number
        uint32_t my_maxAmpduSize = maxAmpduSize;
        if ( j == 0 ) {
          // primary AP
          my_maxAmpduSize = maxAmpduSize;
        }
        else if ( j == 1 ) {
          // secondary AP
          my_maxAmpduSize = maxAmpduSizeSecondary;
        }

        wifiMacPrimary.SetType ( "ns3::ApWifiMac",
                          "Ssid", SsidValue (apssid),
                          "QosSupported", BooleanValue (true),
                          "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                          "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

        wifiMacSecondary.SetType ( "ns3::ApWifiMac",
                          "Ssid", SsidValue (apssid),
                          "QosSupported", BooleanValue (true),
                          "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                          "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                          "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

        // Define the channel of each AP
        if ( j == 0 ) {
          // primary AP
          ChannelNoForThisAP = availableChannels[0];
        }
        else if ( j == 1 ) {
          // secondary AP
          ChannelNoForThisAP = availableChannels[1];
        }      
      }


      // install the wifi Phy and MAC in the APs
      if (j == 0) {
        // primary AP
        if (wifiModel == 0) { 
          // Yans wifi
          wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
          apWiFiDev = wifi.Install (wifiPhy, wifiMacPrimary, apNodes.Get (i + (j*number_of_APs)));
        }
        else {
          // spectrumwifi
          spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
          apWiFiDev = wifi.Install (spectrumPhy, wifiMacPrimary, apNodes.Get (i + (j*number_of_APs)));
        }           
      }
      else if (j == 1) {
        // secondary AP
        if (wifiModel == 0) { 
          // Yans wifi
          wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
          apWiFiDev = wifi.Install (wifiPhy, wifiMacSecondary, apNodes.Get (i + (j*number_of_APs)));
        }
        else {
          // spectrumwifi
          spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
          apWiFiDev = wifi.Install (spectrumPhy, wifiMacSecondary, apNodes.Get (i + (j*number_of_APs)));
        } 
      }

      // auxiliar string
      std::ostringstream auxString;
      std::string myaddress;

      // create a string with the MAC
      auxString << apWiFiDev.Get(0)->GetAddress();
      myaddress = auxString.str();

      // update the AP record with the correct value, using the correct version of the function
      AP_vector[i + j*number_of_APs]->SetApRecord (i + j*number_of_APs, myaddress, my_maxAmpduSize);

      // fill the values of the vector of APs
      AP_vector[i + j*number_of_APs]->setWirelessChannel(ChannelNoForThisAP);


      // print the IP and the MAC address, and the WiFi channel
      if (verboseLevel >= 1) {
        std::cout << "AP     #" << i + j*number_of_APs << "\tMAC address:  " << apWiFiDev.Get(0)->GetAddress() << '\n';
        std::cout << "        " << "\tWi-Fi channel: " << uint32_t(AP_vector[i + (j*number_of_APs)]->GetWirelessChannel()) << '\n'; // convert to uint32_t in order to print it
        std::cout << "\t\tAP with MAC " << myaddress << " added to the list of APs. AMPDU size " << my_maxAmpduSize << " bytes" << '\n';
      }

      // also report the position of the AP (defined above)
      if (verboseLevel >= 1) {
        //ReportPosition (timeMonitorKPIs, backboneNodes.Get(i), i, 0, 1, apNodes); this would report the position every second
        //Vector pos = GetPosition (backboneNodes.Get (i));
        Vector pos = GetPosition (apNodes.Get (i + j*number_of_APs));
        std::cout << "\t\tPosition: " << pos.x << "," << pos.y << '\n';
      }

      // save everything in containers (add a line to the vector of containers, including the new AP device and interface)
      apWiFiDevices.push_back (apWiFiDev);
    }    
  }
  /*************************** end of - Define the APs ******************************/


  /*************************** Define the STAs ******************************/
  // connect each of the STAs to the wifi channel

  // An ssid variable for the STAs
  Ssid stassid; // If you leave it blank, the STAs will send broadcast assoc requests

  for (uint32_t i = 0; i < number_of_STAs; i++) {

    // I use two auxiliary device containers
    // at the end of the 'for', they are added to staDevices using 'staDevices.push_back (staDev);'
    NetDeviceContainer staDev, staDevSecondary;

    // I use two auxiliary interface containers
    // at the end of the 'for', they are assigned an IP address,
    //and they are added to staInterfaces using 'staInterfaces.push_back (staInterface);'
    Ipv4InterfaceContainer staInterface, staInterfaceSecondary;

    // If none of the aggregation algorithms is enabled, all the STAs aggregate
    if ((aggregationDisableAlgorithm == 0) && (aggregationDynamicAlgorithm == 0)) {
      wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid));

      wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid));
    }

    else {
      // The aggregation algorithm is enabled
      // Install:
      // - non aggregation in the VoIP STAs
      // - aggregation in the TCP STAs

      
      if ( i < numberVoIPupload + numberVoIPdownload ) {
        // The VoIP STAs do NOT aggregate

        if ( (HANDOFFMETHOD == 1) && (wifiModel == 1) ) {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (0),
                            "BK_MaxAmpduSize", UintegerValue (0),
                            "VI_MaxAmpduSize", UintegerValue (0),
                            "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (0),
                            "BK_MaxAmpduSize", UintegerValue (0),
                            "VI_MaxAmpduSize", UintegerValue (0),
                            "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs
        }
        else {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                    "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                    "QosSupported", BooleanValue (true),
                    //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                    "BE_MaxAmpduSize", UintegerValue (0),
                    "BK_MaxAmpduSize", UintegerValue (0),
                    "VI_MaxAmpduSize", UintegerValue (0),
                    "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                    "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                    "QosSupported", BooleanValue (true),
                    //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                    "BE_MaxAmpduSize", UintegerValue (0),
                    "BK_MaxAmpduSize", UintegerValue (0),
                    "VI_MaxAmpduSize", UintegerValue (0),
                    "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs          
        }
      }

      else {
        // The TCP STAs do aggregate
        if ( (HANDOFFMETHOD == 1) && (wifiModel == 1) ) {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));
        }
        else {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid), // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));          

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid), // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));  
        }
      }
      /*
      // Other options (Enable AMSDU, and also enable AMSDU and AMPDU at the same time)
      wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid),
                        "BE_MaxAmpduSize", UintegerValue (0), //Disable A-MPDU
                        "BE_MaxAmsduSize", UintegerValue (7935)); //Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)

      wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid),
                        "BE_MaxAmpduSize", UintegerValue (32768), //Enable A-MPDU with a smaller size than the default one
                        "BE_MaxAmsduSize", UintegerValue (3839)); //Enable A-MSDU with the smallest maximum size allowed by the standard (3839 bytes)
      */
    }


    // channel number that will be used by this STA
    uint8_t ChannelNoForThisSTA = 0;          // initially set to 0, but it MUST get a value
    uint8_t ChannelNoForThisSTASecondary = 0; // initially set to 0, but it MUST get a value (if the secondary device exists)

    // this is the frequency band where the STA can find an AP
    // if it has a single card, only one band can be used
    // if it has two cards, it depends on the band(s) supported by the present AP(s)
    std::string bandsSupportedByThisSTA;

    std::string bandsSupportedByAPs = bandsSupportedByTheAPs(numberAPsSamePlace, version80211primary, version80211secondary);

    // pointer to the nearest AP
    Ptr<Node> myNearestAp, myNearestAp2GHz, myNearestAp5GHz;  
    uint32_t myNearestApId;

    // install the wifi in the STAs
    // I have to distinguish different options
    if (!defineSTAsManually) {
      // define the STAs automatically

      bandsSupportedByThisSTA = bandsSupportedBySTA(numberWiFiDevicesInSTAs, version80211primary, version80211secondary);
      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << "\t\tFrequency band(s) supported by STA#" << staNodes.Get(i)->GetId() 
                  << ": " <<  bandsSupportedByThisSTA 
                  << std::endl;

      if (bandsSupportedByThisSTA == "both") {
        // the STAs support both bands (they have 2 wifi devices)

        if (bandsSupportedByAPs == "both") {
          // the APs support both bands, and the STAs support both bands

          // look for the nearest AP in the 5GHz band              
          myNearestAp5GHz = nearestAp (apNodes, staNodes.Get(i), verboseLevel, "5 GHz");
          // look for the nearest AP in the 2.4GHz band           
          myNearestAp2GHz = nearestAp (apNodes, staNodes.Get(i), verboseLevel, "2.4 GHz");

          if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "5GHz") {
            // the primary card is in 5GHz and the secondary in 2.4GHz
            myNearestApId = (myNearestAp5GHz)->GetId();
            ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, verboseLevel);
            if (verboseLevel >= 2)
              std::cout << "\t\tThe nearest AP for STA#" << staNodes.Get(i)->GetId() 
                        << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                        << " is AP#" <<  myNearestApId 
                        << ", in channel " << uint16_t(ChannelNoForThisSTA)
                        << '\n';

            myNearestApId = (myNearestAp2GHz)->GetId();
            ChannelNoForThisSTASecondary = GetAP_WirelessChannel (myNearestApId, verboseLevel);
            if (verboseLevel >= 2)
              std::cout << "\t\tThe nearest AP for STA#" << staNodes.Get(i)->GetId() 
                        << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                        << " is AP#" <<  myNearestApId 
                        << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                        << '\n';
            /*
            // install the wifi Phy and MAC in both cards of the STAs
            // Yans wifi
            if (wifiModel == 0) {
              wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
              //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

              staDev = wifi.Install (wifiPhy, wifiMacPrimary, staNodes.Get(i));
              if (verboseLevel >= 2) {
                std::cout << "\t\tInstalled primary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                          << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                          << ", in channel " << uint16_t(ChannelNoForThisSTA)
                          << '\n';                
              }

              if (numberWiFiDevicesInSTAs==2) {
                // this installs a second WiFi card in the STA. It will have another MAC address
                wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTASecondary));
                //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);
              
                staDevSecondary = wifiSecondary.Install (wifiPhy, wifiMacSecondary, staNodes.Get(i));
                if (verboseLevel >= 2) {
                  std::cout << "\t\tInstalled secondary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                            << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                            << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                            << '\n';                
                }
              }
            }
            else {
              // spectrumwifi
              spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
              //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

              staDev = wifi.Install (spectrumPhy, wifiMacPrimary, staNodes.Get(i));
              if (verboseLevel >= 2) {
                std::cout << "\t\tInstalled primary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                          << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                          << ", in channel " << uint16_t(ChannelNoForThisSTA)
                          << '\n';                
              }

              if (numberWiFiDevicesInSTAs==2) {
                spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTASecondary));
                // this installs a second WiFi card in the STA. It will have another MAC address
                staDevSecondary = wifiSecondary.Install (spectrumPhy, wifiMacSecondary, staNodes.Get (i));
                if (verboseLevel >= 2) {
                  std::cout << "\t\tInstalled secondary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                            << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                            << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                            << '\n';                
                }
              }
            }*/
          }
          else {
            // the primary card is in 2.4GHz and the secondary in 5GHz
            myNearestApId = (myNearestAp2GHz)->GetId();
            ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, verboseLevel);
            if (verboseLevel >= 2)
              std::cout << "\t\tThe nearest AP for STA#" << staNodes.Get(i)->GetId() 
                        << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                        << " is AP#" <<  myNearestApId 
                        << ", in channel " << uint16_t(ChannelNoForThisSTA)
                        << '\n';

            myNearestApId = (myNearestAp5GHz)->GetId();
            ChannelNoForThisSTASecondary = GetAP_WirelessChannel (myNearestApId, verboseLevel);
            if (verboseLevel >= 2)
              std::cout << "\t\tThe nearest AP for STA#" << staNodes.Get(i)->GetId() 
                        << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                        << " is AP#" <<  myNearestApId 
                        << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                        << '\n';

            /*
            // install the wifi Phy and MAC in both cards of the STAs
            // Yans wifi
            if (wifiModel == 0) {
              wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
              //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

              staDev = wifi.Install (wifiPhy, wifiMacPrimary, staNodes.Get(i));
              if (verboseLevel >= 2) {
                std::cout << "\t\tInstalled primary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                          << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                          << ", in channel " << uint16_t(ChannelNoForThisSTA)
                          << '\n';                
              }

              if (numberWiFiDevicesInSTAs==2) {
                // this installs a second WiFi card in the STA. It will have another MAC address
                wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTASecondary));
                //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);
              
                staDevSecondary = wifiSecondary.Install (wifiPhy, wifiMacSecondary, staNodes.Get(i));
                if (verboseLevel >= 2) {
                  std::cout << "\t\tInstalled secondary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                            << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                            << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                            << '\n';                
                }
              }
            }
            else {
              // spectrumwifi
              spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
              //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

              staDev = wifi.Install (spectrumPhy, wifiMacPrimary, staNodes.Get(i));
              if (verboseLevel >= 2) {
                std::cout << "\t\tInstalled primary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                          << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                          << ", in channel " << uint16_t(ChannelNoForThisSTA)
                          << '\n';                
              }

              if (numberWiFiDevicesInSTAs==2) {
                spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTASecondary));
                // this installs a second WiFi card in the STA. It will have another MAC address
                staDevSecondary = wifiSecondary.Install (spectrumPhy, wifiMacSecondary, staNodes.Get (i));
                if (verboseLevel >= 2) {
                  std::cout << "\t\tInstalled secondary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                            << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                            << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                            << '\n';                
                }
              }
            }*/
          }
        }
        else {
          // the APs only support one band, but the STAs support both bands
          // look for the nearest AP in that band
          myNearestAp = nearestAp (apNodes, staNodes.Get(i), verboseLevel, bandsSupportedByAPs);
          myNearestApId = (myNearestAp)->GetId();
          ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, verboseLevel);
          if (verboseLevel >= 2)
            std::cout << "\t\tThe nearest AP for STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                      << " is AP#" <<  myNearestApId 
                      << ", in channel " << uint16_t(ChannelNoForThisSTA)
                      << '\n';

          // the second interface is put in the first channel of the other band
          if (getWirelessBandOfChannel(ChannelNoForThisSTA) == "2.4GHz") {
            ChannelNoForThisSTASecondary = availableChannels20MHz[0];            
          }
          else {
            ChannelNoForThisSTASecondary = availableChannels24GHz20MHz[0];
          }
          if (verboseLevel >= 2)
            std::cout << "\t\tThere is no AP in the band of STA#" << staNodes.Get(i)->GetId() 
                      << " (" << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                      << ")" 
                      << ". I put the second interface of the STA in the first channel of the band: " << uint16_t(ChannelNoForThisSTASecondary)
                      << '\n';


        }
        // install the wifi Phy and MAC in both cards of the STAs
        // Yans wifi
        if (wifiModel == 0) {
          wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
          //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

          staDev = wifi.Install (wifiPhy, wifiMacPrimary, staNodes.Get(i));
          if (verboseLevel >= 2) {
            std::cout << "\t\tInstalled primary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                      << ", in channel " << uint16_t(ChannelNoForThisSTA)
                      << '\n';                
          }

          // secondary card
          NS_ASSERT (numberWiFiDevicesInSTAs==2);
          // this installs a second WiFi card in the STA. It will have another MAC address
          wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTASecondary));
          //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);
        
          staDevSecondary = wifiSecondary.Install (wifiPhy, wifiMacSecondary, staNodes.Get(i));
          if (verboseLevel >= 2) {
            std::cout << "\t\tInstalled secondary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                      << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                      << '\n';                
          }
        }
        else {
          // spectrumwifi
          spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
          //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

          staDev = wifi.Install (spectrumPhy, wifiMacPrimary, staNodes.Get(i));
          if (verboseLevel >= 2) {
            std::cout << "\t\tInstalled primary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                      << ", in channel " << uint16_t(ChannelNoForThisSTA)
                      << '\n';                
          }

          // secondary card
          NS_ASSERT (numberWiFiDevicesInSTAs==2);
          spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTASecondary));
          // this installs a second WiFi card in the STA. It will have another MAC address
          staDevSecondary = wifiSecondary.Install (spectrumPhy, wifiMacSecondary, staNodes.Get (i));
          if (verboseLevel >= 2) {
            std::cout << "\t\tInstalled secondary WiFi card in STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTASecondary)
                      << ", in channel " << uint16_t(ChannelNoForThisSTASecondary)
                      << '\n';                
          }
        }
      }
      else {
        // the STA only supports one band (they have a single wifi device)

        if (bandsSupportedByAPs == "both") {
          // the APs support both bands but the STA supports a single band
          // look for the nearest AP in the band of the STA
          myNearestAp = nearestAp (apNodes, staNodes.Get(i), verboseLevel, bandsSupportedByThisSTA);
          myNearestApId = (myNearestAp)->GetId();
          ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, verboseLevel);
          if (verboseLevel >= 2)
            std::cout << "\t\tThe nearest AP for STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                      << " is AP#" <<  myNearestApId 
                      << ", in channel " << uint16_t(ChannelNoForThisSTA)
                      << '\n';
        }
        else {
          // the AP supports a single band and the STA supports a single band
          if (bandsSupportedByThisSTA == bandsSupportedByAPs) {
            // the same band is supported by STAs and APs
            myNearestAp = nearestAp (apNodes, staNodes.Get(i), verboseLevel, bandsSupportedByThisSTA);
            myNearestApId = (myNearestAp)->GetId();
            ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, verboseLevel);
            if (verboseLevel >= 2)
              std::cout << "\t\tThe nearest AP for STA#" << staNodes.Get(i)->GetId() 
                        << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                        << " is AP#" <<  myNearestApId 
                        << ", in channel " << uint16_t(ChannelNoForThisSTA)
                        << '\n';
          }
          else {
            // the STAs support one band, but the APs support the other band. Stop the simulation
            std::cout << "\t\tERROR: Frequency band(s) supported by STA#" << staNodes.Get(i)->GetId() 
                      << ": " <<  bandsSupportedByThisSTA
                      << ". But APs only support band " << bandsSupportedByAPs
                      << std::endl;                
            NS_ASSERT(false);
          }
        }
        // install the wifi Phy and MAC in the card of the STAs
        // Yans wifi
        if (wifiModel == 0) {
          wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
          //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

          staDev = wifi.Install (wifiPhy, wifiMacPrimary, staNodes.Get(i));
          if (verboseLevel >= 2) {
            std::cout << "\t\tInstalled primary (and only) WiFi card in STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                      << ", in channel " << uint16_t(ChannelNoForThisSTA)
                      << '\n';                
          }
        }
        else {
          // spectrumwifi
          spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
          //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTA);

          staDev = wifi.Install (spectrumPhy, wifiMacPrimary, staNodes.Get(i));
          if (verboseLevel >= 2) {
            std::cout << "\t\tInstalled primary (and only) WiFi card in STA#" << staNodes.Get(i)->GetId() 
                      << ", in band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                      << ", in channel " << uint16_t(ChannelNoForThisSTA)
                      << '\n';                
          }
        }
      }
    }

    else {
      // define STAs manually

      // define the standard to follow (see https://www.nsnam.org/doxygen/group__wifi.html#ga1299834f4e1c615af3ca738033b76a49)

      // Manually define the Wi-Fi standard and the channel of each STA

      // even STAs
      if ( i % 2 == 0 ) {
        wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
        if (numberWiFiDevicesInSTAs==2)
          wifiSecondary.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

        ChannelNoForThisSTA = 1;
        //ChannelNoForThisSTA = availableChannels[0];
        if (numberWiFiDevicesInSTAs==2)
          ChannelNoForThisSTASecondary = availableChannels[0];
      }

      // odd STAs
      else if ( i % 2 == 1 ) {
        wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);        
        if (numberWiFiDevicesInSTAs==2)
          wifiSecondary.SetStandard (WIFI_PHY_STANDARD_80211g);

        ChannelNoForThisSTA = availableChannels[1];
        if (numberWiFiDevicesInSTAs==2)
          ChannelNoForThisSTASecondary = 1;
          //ChannelNoForThisSTASecondary = availableChannels[1];
      }
    }

    // add the primary device
    staDevices.push_back (staDev);

    // add the secondary device
    if (numberWiFiDevicesInSTAs==2) {
      staDevicesSecondary.push_back (staDevSecondary);
    }


    // add an IP address (Segment A, i.e. 10.0.0.0) to the primary interface
    staInterface = ipAddressesSegmentA.Assign (staDev);
    staInterfaces.push_back (staInterface);
    // add an IP address (Segment A, i.e. 10.0.0.0) to the secondary interface
    if (numberWiFiDevicesInSTAs == 2) {
      staInterfaceSecondary = ipAddressesSegmentA.Assign (staDevSecondary);
      staInterfacesSecondary.push_back (staInterfaceSecondary);      
    }


    if (verboseLevel > 0) {
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = staNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1

      // first IP address or the STA
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "STA    #" << node->GetId() << "\tprimary IP address: " << addr << '\n';
      std::cout << "        " << "\tprimary MAC address: " << staDevices[i].Get(0)->GetAddress() << '\n';
      std::cout << "        " << "\tprimary Wi-Fi channel: " << uint32_t(ChannelNoForThisSTA) << '\n';

      // second IP address of the STA
      if(numberWiFiDevicesInSTAs==2) {
        addr = ipv4->GetAddress (2, 0).GetLocal ();
        std::cout << "        " << "\tsecondary IP address: " << addr << '\n';
        std::cout << "        " << "\tsecondary MAC address: " << staDevicesSecondary[i].Get(0)->GetAddress() << '\n';
        std::cout << "        " << "\tsecondary Wi-Fi channel: " << uint32_t(ChannelNoForThisSTASecondary) << '\n';        
      }

      // I print the position 
      Vector pos = GetPosition (node);
      std::cout << "        " << "\tPosition: " << pos.x << "," << pos.y << '\n';     
      if (!defineSTAsManually) {
        std::cout << "        " << "\tInitially near AP# " << myNearestApId << '\n';
      }      
    }

    // Periodically report the channels of all the STAs
    if ((verboseLevel > 2) && (timeMonitorKPIs > 0)) {
      Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL),
                            &ReportChannel, 
                            timeMonitorKPIs,
                            ( number_of_APs * numberAPsSamePlace ) + i,
                            verboseLevel);
    }


    // EXPERIMENTAL: on each STA, add support for other operational channels
    // FIXME: I remove AddOperationalChannel Dec 2019
    if (false) {
      if ( (HANDOFFMETHOD == 1) && (wifiModel == 1) ) {
        // This is only needed if more than 1 channel is in use, and if wifiModel == 1
        if (numOperationalChannels > 1 && wifiModel == 1) {
          Ptr<SpectrumWifiPhy> wifiPhyPtrClient;
          //Ptr<StaWifiMac> myStaWifiMac;

          wifiPhyPtrClient = staDevices[i].Get(0)->GetObject<WifiNetDevice>()->GetPhy()->GetObject<SpectrumWifiPhy>();

          if (verboseLevel > 0)
            std::cout << "STA\t#" << staNodes.Get(i)->GetId()
                      << "\tAdded operational channels: ";

          for (uint32_t k = 0; k < numOperationalChannels; k++) {
            //(*wifiPhyPtrClient).AddOperationalChannel ( availableChannels[k] );
            if (verboseLevel > 0)
              std::cout << uint16_t(availableChannels[k]) << " "; 
          }
          if (verboseLevel > 0)
            std::cout << '\n';

          //myStaWifiMac = staDevices[i].Get(0)->GetObject<WifiNetDevice>()->GetMacOfitsAP()->GetObject<StaWifiMac>();
          //(*myStaWifiMac).TryToEnsureAssociated();
          //if ( (*myStaWifiMac).IsAssociated() )
          //  std::cout << "######################" << '\n';
        }
      }      
    }



    // EXPERIMENTAL: on each STA, clear all the operational channels https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#acb3f2f3a32236fa5fdeed1986b28fe04
    // it doesn't work

    // FIXME: I remove ClearOperationalChannelList Dec 2019
    if (false) {
      if (numOperationalChannels > 1 && wifiModel == 1) {
        Ptr<SpectrumWifiPhy> wifiPhyPtrClient;

        wifiPhyPtrClient = staDevices[i].Get(0)->GetObject<WifiNetDevice>()->GetPhy()->GetObject<SpectrumWifiPhy>();

        //(*wifiPhyPtrClient).ClearOperationalChannelList ();

        if (verboseLevel > 0)
          std::cout << "STA\t#" << staNodes.Get(i)->GetId()
                    << "\tCleared operational channels in STA #" << ( number_of_APs * numberAPsSamePlace ) + i
                    << '\n';
      }
    }

    /** create a STA_record per STA **/
    // the STA has been defined, so now I create a STA_record for it,
    //in order to store its association parameters

    // This calls the constructor, i.e. the function that creates a record to store the association of each STA
    STA_record *m_STArecord = new STA_record();

    // Set the value of the id of the STA in the record
    //m_STArecord->setstaid ((*mynode)->GetId());
    m_STArecord->setstaid ((staNodes.Get(i))->GetId());

    // Establish the type of application
    if ( i < numberVoIPupload ) {
      m_STArecord->Settypeofapplication (1);  // VoIP upload
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (i < numberVoIPupload + numberVoIPdownload ) {
      m_STArecord->Settypeofapplication (2);  // VoIP download
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (i < numberVoIPupload + numberVoIPdownload + numberTCPupload) {
      m_STArecord->Settypeofapplication (3);               // TCP upload
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    } else if (i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload) {
      m_STArecord->Settypeofapplication (4);                // TCP download
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    } else if (i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload) {
      m_STArecord->Settypeofapplication (5);                // Video download
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled      
    } else {

    }

    // Establish the verbose level in the STA record
    m_STArecord->SetVerboseLevel (verboseLevel);

    // Establish the rest of the private variables of the STA record
    m_STArecord->SetnumOperationalChannels (numOperationalChannels);
    m_STArecord->Setversion80211primary (version80211primary);
    m_STArecord->Setversion80211Secondary (version80211secondary);
    m_STArecord->SetaggregationDisableAlgorithm (aggregationDisableAlgorithm);
    m_STArecord->SetAmpduSize (maxAmpduSize);
    m_STArecord->SetmaxAmpduSizeWhenAggregationLimited (maxAmpduSizeWhenAggregationLimited);
    m_STArecord->SetWifiModel (wifiModel);

    // Set a callback function to be called each time a STA gets associated to an AP
    std::ostringstream STA;
    //STA << (*mynode)->GetId();
    STA << (staNodes.Get(i))->GetId();
    std::string strSTA = STA.str();

    // Check if we are using the algoritm for deactivating / activating aggregation
    //if ( aggregationDisableAlgorithm == 1) {

      // This makes a callback every time a STA gets associated to an AP
      // see trace sources in https://www.nsnam.org/doxygen/classns3_1_1_sta_wifi_mac.html#details
      // trace association. Taken from https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
      // some info here: https://groups.google.com/forum/#!msg/ns-3-users/zqdnCxzYGM8/MdCshgYKAgAJ
      Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc", 
                        MakeCallback (&STA_record::SetAssoc, m_STArecord));

      // Set a callback function to be called each time a STA gets de-associated from an AP
      Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc", 
                        MakeCallback (&STA_record::UnsetAssoc, m_STArecord));
    //}

    // Add the new record to the vector of STA associations
    assoc_vector.push_back (m_STArecord);
    /** end of - create a STA_record per STA **/
  }

  /* THIS HAS BEEN MOVED TO THE PREVIOUS FOR
  I WILL REMOVE IT SOON
  // the STAs have been defined, so now I create a STA_record per STA,
  //in order to store its association parameters
  NodeContainer::Iterator mynode;
  uint16_t l = 0;
  for (mynode = staNodes.Begin (); mynode != staNodes.End (); ++mynode) { // run this for all the STAs

    // This calls the constructor, i.e. the function that creates a record to store the association of each STA
    STA_record *m_STArecord = new STA_record();

    // Set the value of the id of the STA in the record
    m_STArecord->setstaid ((*mynode)->GetId());

    // Establish the type of application
    if ( l < numberVoIPupload ) {
      m_STArecord->Settypeofapplication (1);  // VoIP upload
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (l < numberVoIPupload + numberVoIPdownload ) {
      m_STArecord->Settypeofapplication (2);  // VoIP download
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (l < numberVoIPupload + numberVoIPdownload + numberTCPupload) {
      m_STArecord->Settypeofapplication (3);               // TCP upload
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    } else if (l < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload) {
      m_STArecord->Settypeofapplication (4);                // TCP download
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    } else if (l < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload) {
      m_STArecord->Settypeofapplication (5);                // Video download
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled      
    } else {

    }

    // Establish the verbose level in the STA record
    m_STArecord->SetVerboseLevel (verboseLevel);

    // Establish the rest of the private variables of the STA record
    m_STArecord->SetnumOperationalChannels (numOperationalChannels);
    m_STArecord->Setversion80211primary (version80211primary);
    m_STArecord->Setversion80211Secondary (version80211secondary);
    m_STArecord->SetaggregationDisableAlgorithm (aggregationDisableAlgorithm);
    m_STArecord->SetAmpduSize (maxAmpduSize);
    m_STArecord->SetmaxAmpduSizeWhenAggregationLimited (maxAmpduSizeWhenAggregationLimited);
    m_STArecord->SetWifiModel (wifiModel);

    l++;

    // Set a callback function to be called each time a STA gets associated to an AP
    std::ostringstream STA;
    STA << (*mynode)->GetId();
    std::string strSTA = STA.str();

    // Check if we are using the algoritm for deactivating / activating aggregation
    //if ( aggregationDisableAlgorithm == 1) {

      // This makes a callback every time a STA gets associated to an AP
      // see trace sources in https://www.nsnam.org/doxygen/classns3_1_1_sta_wifi_mac.html#details
      // trace association. Taken from https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
      // some info here: https://groups.google.com/forum/#!msg/ns-3-users/zqdnCxzYGM8/MdCshgYKAgAJ
      Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*//*$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc", 
                        MakeCallback (&STA_record::SetAssoc, m_STArecord));

      // Set a callback function to be called each time a STA gets de-associated from an AP
      Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*//*$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc", 
                        MakeCallback (&STA_record::UnsetAssoc, m_STArecord));
    //}

    // Add the new record to the vector of STA associations
    assoc_vector.push_back (m_STArecord);
  }*/
  /*************************** end of - Define the STAs ******************************/


  // Set channel width of all the devices
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));



  /************************** define wired connections *******************************/
  // create the ethernet channel for connecting the APs and the router
  CsmaHelper csma;
  //csma.SetChannelAttribute ("DataRate", StringValue ("10000Mbps")); // to avoid this being the bottleneck
  csma.SetChannelAttribute ("DataRate", DataRateValue (100000000000)); // 100 gbps
  // set the speed-of-light delay of the channel to 6560 nano-seconds (arbitrarily chosen as 1 nanosecond per foot over a 100 meter segment)
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  //csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (65600)));

  // https://www.nsnam.org/doxygen/classns3_1_1_bridge_helper.html#aba53f6381b7adda00d9163840b072fa6
  // This method creates an ns3::BridgeNetDevice with the attributes configured by BridgeHelper::SetDeviceAttribute, 
  // adds the device to the node (first argument), and attaches the given NetDevices (second argument) as ports of the bridge.
  // In this case, it adds it to the backbonenodes and attaches the backboneDevices of apWiFiDev
  // Returns a container holding the added net device.

  // taken from https://www.nsnam.org/doxygen/csma-bridge-one-hop_8cc_source.html
  //   +----------------+
  //   |   csmaHubNode  |                         The node named csmaHubNode
  //   +----------------+                         has a number CSMA net devices that are bridged
  //   CSMA   CSMA   CSMA   csmaHubDevices        together using a BridgeNetDevice (created with a bridgehelper) called 'bridgehub'.
  //    1|      |      |
  //     |      |      |                          The bridge node talks over three CSMA channels
  //    0|      |      |                          to three other CSMA net devices
  //   CSMA   CSMA   CSMA
  //   +--+   +--+  +------+
  //   |AP|   |AP|  |router|  (the router only appears in topology = 2)
  //   +--+   +--+  +------+
  //   wifi   wifi    p2p
  //                   |
  //                   |
  //                   |
  //                  p2p
  //                   
  //                servers   


  // install a csma channel between the ith AP node and the bridge (csmaHubNode) node
  for ( uint32_t i = 0; i < number_of_APs * numberAPsSamePlace; i++) {
    NetDeviceContainer link = csma.Install (NodeContainer (apNodes.Get(i), csmaHubNode));
    apCsmaDevices.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));
  }

  if (topology == 0) {
    // install a csma channel between the singleServer and the bridge (csmaHubNode) node
    NetDeviceContainer link = csma.Install (NodeContainer (singleServerNode.Get(0), csmaHubNode));
    singleServerDevices.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));

    // Assign an IP address (10.0.0.0) to the single server
    singleServerInterfaces = ipAddressesSegmentA.Assign (singleServerDevices);

  } else if (topology == 1) {
    // install a csma channel between the ith server node and the bridge (csmaHubNode) node
    for ( uint32_t i = 0; i < number_of_Servers; i++) {
      NetDeviceContainer link = csma.Install (NodeContainer (serverNodes.Get(i), csmaHubNode));
      serverDevices.Add (link.Get(0));
      csmaHubDevices.Add (link.Get(1));
    }  
      
    // Assign IP addresses (10.0.0.0) to the servers
    serverInterfaces = ipAddressesSegmentA.Assign (serverDevices);

  } else { // if (topology == 2)
    // install a csma channel between the router and the bridge (csmaHubNode) node
    NetDeviceContainer link = csma.Install (NodeContainer (routerNode.Get(0), csmaHubNode));
    routerDeviceToAps.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));

    // Assign an IP address (10.0.0.0) to the router (AP part)
    routerInterfaceToAps = ipAddressesSegmentA.Assign (routerDeviceToAps);
  }
  /************************** end of - define wired connections *******************************/



  // on each AP, I install a bridge between two devices: the WiFi device and the csma device
  BridgeHelper bridgeAps, bridgeHub;
  NetDeviceContainer apBridges;


  for (uint32_t i = 0; i < number_of_APs * numberAPsSamePlace; i++) {

    // Create a bridge between two devices (WiFi and CSMA) of the same node: the AP

    // initial version
    //bridgeAps.Install (apNodes.Get (i), NetDeviceContainer ( apWiFiDevices[i].Get(0), apCsmaDevices.Get (i) ) );

    // improved version
    NetDeviceContainer bridge = bridgeAps.Install (apNodes.Get (i), NetDeviceContainer ( apWiFiDevices[i].Get(0), apCsmaDevices.Get (i) ) );
    apBridges.Add (bridge.Get(0));

    // If needed, I could assign an IP address to the bridge (not to the wifi or the csma devices)
    //apInterface = ipAddressesSegmentA.Assign (bridgeApDevices);
  }


  //Create the bridge netdevice, which will do the packet switching.  The
  // bridge lives on the node csmaHubNode.Get(0) and bridges together the csmaHubDevices and the routerDeviceToAps
  // which are the CSMA net devices 
  bridgeHub.Install (csmaHubNode.Get(0), csmaHubDevices );


  // create a point to point helper for connecting the servers with the router (if topology == 2)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10000Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));


  //if (populatearpcache)
    // Assign IP address (10.0.0.0) to the bridge
    //csmaHubInterfaces = ipAddressesSegmentA.Assign (bridgeApDevices);

  if (topology == 2) {

    // connect each server to the router with a point to point connection
    for ( uint32_t i = 0; i < number_of_Servers; i++) {
      // Create a point to point link between the router and the server
      NetDeviceContainer devices;
      devices = p2p.Install ( NodeContainer (routerNode.Get (0), serverNodes.Get (i))); // this returns two devices. Add them to the router and the server
      routerDeviceToServers.Add (devices.Get(0));
      serverDevices.Add (devices.Get(1));
    }

    // Assign IP addresses (10.1.0.0) to the servers
    serverInterfaces = ipAddressesSegmentB.Assign (serverDevices);

    // Assign an IP address (10.1.0.0) to the router
    routerInterfaceToServers = ipAddressesSegmentB.Assign (routerDeviceToServers);
  }


  if (verboseLevel > 0) {
    Ptr<Node> node;
    Ptr<Ipv4> ipv4;
    Ipv4Address addr;

    if (topology == 0) { 
      // print the IP and the MAC addresses of the server
      node = singleServerNode.Get (0); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "Server\t" << "\tIP address: " << addr << '\n';
      std::cout << "      \t" << "\tMAC address: " << singleServerDevices.Get(0)->GetAddress() << '\n';
      
    } else if (topology == 1) {
      // print the IP and the MAC addresses of each server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        node = serverNodes.Get (i); // Get pointer to ith node in container
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress (1, 0).GetLocal (); 
        std::cout << "Server #" << node->GetId() << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << serverDevices.Get(i)->GetAddress() << '\n';  
      }

    } else { //(topology == 2)
      // print the IP and the MAC addresses of the router
      node = routerNode.Get (0); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "Router(AP side)" << "\tIP address: " << addr << '\n';
      std::cout << "               " << "\tMAC address: " << routerDeviceToAps.Get(0)->GetAddress() << '\n';

      // print the IP and the MAC addresses of the router and its corresponding server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        // print the IP and the MAC of the p2p devices of the router
        node = routerNode.Get(0); // Get pointer to the router
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the router      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress ( i + 2, 0).GetLocal (); // address 0 is local, and address 1 is for the AP side 
        std::cout << "Router IF #" << i + 2 << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << routerDeviceToServers.Get(i)->GetAddress() << '\n';
      }

      // print the IP and the MAC addresses of the router and its corresponding server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        // addresses of the server
        node = serverNodes.Get (i); // Get pointer to ith node in container
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress (1, 0).GetLocal (); 
        std::cout << "Server #" << node->GetId() << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << serverDevices.Get(i)->GetAddress() << '\n'; 
      }
    } 
  }


  // print a blank line after printing IP addresses
  if (verboseLevel > 0)
    std::cout << "\n";


  // Fill the routing tables. It is necessary in topology 2, which includes two different networks
  if(topology == 2) {
    //NS_LOG_INFO ("Enabling global routing on all nodes");
    //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    // I obtain this error: aborted. msg="ERROR: L2 forwarding loop detected!", file=../src/internet/model/global-router-interface.cc, line=1523
    // Global routing helper does not work in this case
    // As a STA can connect through many APs, loops appear
    // Therefore, I have to add the routes manually
    // https://www.nsnam.org/doxygen/classns3_1_1_ipv4_static_routing.html

    // get the IPv4 address of the router interface to the APs
    Ptr<Node> node;
    Ptr<Ipv4> ipv4;
    Ipv4Address addrRouterAPs, addrRouterSrv;
    node = routerNode.Get (0); // Get pointer to ith node in container
    ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
    addrRouterAPs = ipv4->GetAddress (1, 0).GetLocal ();

    // On each Sta, add a route to the network of the servers
    for (uint32_t i = 0; i < number_of_STAs; i++) {
      // get the ipv4 instance of the STA
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = staNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);

      // Add a route to the network of the servers
      staticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.0.0"), Ipv4Mask ("255.255.0.0"), addrRouterAPs, 1);

      if (verboseLevel > 0) {
        std::cout << "Routing in STA #" << staNodes.Get(i)->GetId() << " with IP address "<< ipv4->GetAddress (1, 0).GetLocal();
        if (numberWiFiDevicesInSTAs==2) {
          std::cout << " and " << ipv4->GetAddress (2, 0).GetLocal();
        }        
        std::cout << ". added route to network 10.1.0.0/255.255.0.0 through gateway " << addrRouterAPs << '\n';
      }
    }

    // On each server, add a route to the network of the Stas
    for (uint32_t i = 0; i < number_of_Servers; i++) {
      // get the ipv4 instance of the server
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = serverNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);

      // get the ipv4 instance of the router
      Ptr<Ipv4> ipv4Router;
      ipv4Router = routerNode.Get (0)->GetObject<Ipv4> (); // Get Ipv4 instance of the router

      // Add a route to the network of the Stas
      staticRouting->AddNetworkRouteTo (Ipv4Address ("10.0.0.0"), Ipv4Mask ("255.255.0.0"), ipv4Router->GetAddress ( i + 2, 0).GetLocal(), 1);

      if (verboseLevel > 0) {
        std::cout << "Routing in server #" << serverNodes.Get(i)->GetId() << " with IP address "<< ipv4->GetAddress (1, 0).GetLocal() << ": ";
        std::cout << "\tadded route to network 10.0.0.0/255.255.0.0 through gateway " << ipv4Router->GetAddress ( i + 2, 0).GetLocal() << '\n';        
      }
    }

    // On the router, add a route to each server
    for (uint32_t i = 0; i < number_of_Servers; i++) {
      // get the ipv4 instance of the server
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addrServer;
      node = serverNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      addrServer = ipv4->GetAddress (1, 0).GetLocal ();

      // get the ipv4 instance of the router
      Ptr<Ipv4> ipv4Router;
      ipv4Router = routerNode.Get (0)->GetObject<Ipv4> (); // Get Ipv4 instance of the router
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4Router);

      // Tell the interface to be used for going to this server
      staticRouting->AddHostRouteTo (addrServer, i + 2, 1); // use interface 'i + 2': interface #0 is localhost, and interface #1 is for talking with the APs

      if (verboseLevel > 0) {
        std::cout << "Routing in the router (id #" << routerNode.Get(0)->GetId() << "): " ;
        std::cout << "\tadded route to host " << addrServer << " through interface with IP address " << ipv4Router->GetAddress ( i + 2, 0).GetLocal() << '\n';
      }      
    }

    // print a blank line after printing routing information
    if (verboseLevel > 0)
      std::cout << "\n";
  }




  // usage examples of functions controlling ARPs
  if (false) {
    infoArpCache ( staNodes.Get(1), 3);
    emtpyArpCache ( staNodes.Get(1), 2);
    modifyArpParams ( staNodes.Get(1), 100.0, 2);
  }


  /************* Setting applications ***********/

  // Variable for setting the port of each communication
  uint16_t port;

  // VoIP upload applications

  // UDPClient runs in the STA and UDPServer runs in the server(s)
  // traffic goes STA -> server
  port = INITIALPORT_VOIP_UPLOAD;
  UdpServerHelper myVoipUpServer;
  ApplicationContainer VoipUpServer;

  for (uint32_t i = 0 ; i < numberVoIPupload ; i++ ) {
    myVoipUpServer = UdpServerHelper(port); // Each UDP connection requires a different port

    if (topology == 0) {
      VoipUpServer = myVoipUpServer.Install (singleServerNode.Get(0));
    } else {
      VoipUpServer = myVoipUpServer.Install (serverNodes.Get(i));
    }

    VoipUpServer.Start (Seconds (0.0));
    VoipUpServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

    // UdpClient runs in the STA, so I must create a UdpClient per STA
    UdpClientHelper myVoipUpClient;
    ApplicationContainer VoipUpClient;

    // I associate all the servers (all running in the STAs) to each server application

    Ipv4Address myaddress;

    if (topology == 0) {
      myaddress = singleServerInterfaces.GetAddress (0);
    } else {
      myaddress = serverInterfaces.GetAddress (i);
    }

    InetSocketAddress destAddress (myaddress, port);

    // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
    if (prioritiesEnabled == 0) {
      destAddress.SetTos (TcpPriorityLevel);
    // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
    } else {
      destAddress.SetTos (VoIpPriorityLevel);
    }

    myVoipUpClient = UdpClientHelper(destAddress);

    myVoipUpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
    //myVoipUpClient.SetAttribute ("Interval", TimeValue (Time ("0.02")));
    myVoipUpClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
    myVoipUpClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

    VoipUpClient = myVoipUpClient.Install (staNodes.Get(i));
    VoipUpClient.Start (Seconds (INITIALTIMEINTERVAL));
    VoipUpClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i)->GetId()
                  << "\t with IP address " << staInterfaces[i].GetAddress(0)
                  << "\t-> to the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t and port " << port
                  << '\n';  
      } else {
        std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i)->GetId()
                  << "\t with IP address " << staInterfaces[i].GetAddress(0)
                  << "\t-> to server #" << serverNodes.Get(i)->GetId()
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t and port " << port
                  << '\n';
      }
    }
    /*
    if (numberWiFiDevicesInSTAs==2) {
      VoipUpClient = myVoipUpClient.Install (staNodes.Get(i));
      VoipUpClient.Start (Seconds (INITIALTIMEINTERVAL));
      VoipUpClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
      if (verboseLevel > 0) {
        if (topology == 0) {
          std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i)->GetId()
                    << "\t with IP address " << staInterfacesSecondary[i].GetAddress(0) << " (secondary)"
                    << "\t-> to the server"
                    << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                    << "\t and port " << port
                    << '\n';  
        } else {
          std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i)->GetId()
                    << "\t with IP address " << staInterfacesSecondary[i].GetAddress(0) << " (secondary)"
                    << "\t-> to server #" << serverNodes.Get(i)->GetId()
                    << "\t with IP address " << serverInterfaces.GetAddress (i) 
                    << "\t and port " << port
                    << '\n';
        }
      }
    }*/ 

    port ++; // Each UDP connection requires a different port
  }


  // VoIP download applications

  // UDPClient in the server(s) and UDPServer in the STA
  // traffic goes Server -> STA
  // I have taken this as an example: https://groups.google.com/forum/#!topic/ns-3-users/ej8LaxQO1Gc
  // UdpServer runs in each STA. It waits for input UDP packets and uses the
  // information carried into their payload to compute delay and to determine
  // if some packets are lost. https://www.nsnam.org/doxygen/classns3_1_1_udp_server_helper.html#details
  port = INITIALPORT_VOIP_DOWNLOAD;

  UdpServerHelper myVoipDownServer;

  ApplicationContainer VoipDownServer;

  for (uint32_t i = numberVoIPupload ; i < numberVoIPupload + numberVoIPdownload ; i++ ) {
    myVoipDownServer = UdpServerHelper(port);
    VoipDownServer = myVoipDownServer.Install (staNodes.Get(i));
    VoipDownServer.Start (Seconds (0.0));
    VoipDownServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

    // I must create a UdpClient per STA
    UdpClientHelper myVoipDownClient;
    ApplicationContainer VoipDownClient;
    // I associate all the servers (all running in the Stas) to each server application
    // GetAddress() will return the address of the UdpServer

    InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i].GetAddress(0), port));
    
    // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
    if (prioritiesEnabled == 0) {
      destAddress.SetTos (TcpPriorityLevel);
    // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
    } else {
      destAddress.SetTos (VoIpPriorityLevel);
    }

    myVoipDownClient = UdpClientHelper(destAddress);

    myVoipDownClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
    //myVoipDownClient.SetAttribute ("Interval", TimeValue (Time ("0.02"))); //packets/s
    myVoipDownClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
    myVoipDownClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

    //VoipDownClient = myVoipDownClient.Install (wifiApNodesA.Get(0));
    if (topology == 0) {
      VoipDownClient = myVoipDownClient.Install (singleServerNode.Get(0));
    } else {
      VoipDownClient = myVoipDownClient.Install (serverNodes.Get (i));
    }

    VoipDownClient.Start (Seconds (INITIALTIMEINTERVAL));
    VoipDownClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application VoIP download from the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t and port " << port
                  << '\n';          
      } else {
        std::cout << "Application VoIP download from server #" << serverNodes.Get(i)->GetId()
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId()
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0)  
                  << "\t and port " << port
                  << '\n';     
      }     
    }
    port ++;
  }


  // Configurations for TCP applications

  // This is necessary, or the packets will not be of this size
  // Taken from https://www.nsnam.org/doxygen/codel-vs-pfifo-asymmetric_8cc_source.html
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (TcpPayloadSize));

  // The next lines seem to be useless
  // 4 MB of TCP buffer
  //Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  //Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));

  // Activate TCP selective acknowledgement (SACK)
  // bool sack = true;
  // Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));

  // TCP variants, see 
  //  https://www.nsnam.org/docs/models/html/tcp.html
  //  https://www.nsnam.org/doxygen/tcp-variants-comparison_8cc_source.html

  // TCP NewReno is the default in ns3, and also in this script
  if (TcpVariant.compare ("TcpNewReno") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  } 
  else if (TcpVariant.compare ("TcpHighSpeed") == 0) {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHighSpeed::GetTypeId ()));
  } 
  else if (TcpVariant.compare ("TcpWestwoodPlus") == 0) {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
    Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
  } else {
    std::cout << "INPUT PARAMETER ERROR: Bad TCP variant. Supported: TcpNewReno, TcpHighSpeed, TcpWestwoodPlus. Stopping the simulation." << '\n';
    return 0;  
  }

  // Activate the log of BulkSend application
  if (verboseLevel > 2) {
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  }


  // TCP upload applications
  port = INITIALPORT_TCP_UPLOAD;

  // Create a PacketSinkApplication and install it on the remote nodes
  ApplicationContainer PacketSinkTcpUp;

  // Create a BulkSendApplication and install it on all the staNodes
  // it will send traffic to the servers
  ApplicationContainer BulkSendTcpUp;

  for (uint16_t i = numberVoIPupload + numberVoIPdownload; i < numberVoIPupload + numberVoIPdownload + numberTCPupload; i++) {

    PacketSinkHelper myPacketSinkTcpUp ("ns3::TcpSocketFactory",
                                        InetSocketAddress (Ipv4Address::GetAny (), port));

    myPacketSinkTcpUp.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

    if (topology == 0) {
      PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (singleServerNode.Get(0)));
    } else {
      PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (serverNodes.Get (i)));
    }

    Ipv4Address myaddress;

    if (topology == 0) {
      myaddress = singleServerInterfaces.GetAddress (0); 
    } else {
      myaddress = serverInterfaces.GetAddress (i);
    }

    InetSocketAddress destAddress (InetSocketAddress (myaddress, port));

    // TCP will have TcpPriorityLevel, whether priorities are enabled or not
    destAddress.SetTos (TcpPriorityLevel);

    BulkSendHelper myBulkSendTcpUp ( "ns3::TcpSocketFactory", destAddress);

    // Set the amount of data to send in bytes. Zero is unlimited.
    myBulkSendTcpUp.SetAttribute ("MaxBytes", UintegerValue (0));
    myBulkSendTcpUp.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
    // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

    // install the application on every staNode
    BulkSendTcpUp = myBulkSendTcpUp.Install (staNodes.Get(i));

    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i)->GetId() 
                  << "\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t-> to the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t and port " << port
                  << '\n';
      } else {
        std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i)->GetId() 
                  << "\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t-> to server #" << serverNodes.Get(i)->GetId() 
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t and port " << port
                  << '\n';
      } 
    }
    port++;
  }
  PacketSinkTcpUp.Start (Seconds (0.0));
  PacketSinkTcpUp.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

  BulkSendTcpUp.Start (Seconds (INITIALTIMEINTERVAL));
  BulkSendTcpUp.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));


  // TCP download applications
  port = INITIALPORT_TCP_DOWNLOAD;

  // Create a PacketSink Application and install it on the wifi STAs
  ApplicationContainer PacketSinkTcpDown;

  // Create a BulkSendApplication and install it on all the servers
  // it will send traffic to the STAs
  ApplicationContainer BulkSendTcpDown, BulkSendTcpDownSecondary;

  for (uint16_t i = numberVoIPupload + numberVoIPdownload + numberTCPupload; 
                i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; 
                i++) {

    // Install a sink on each STA
    // Each sink will have a different port
    PacketSinkHelper myPacketSinkTcpDown ( "ns3::TcpSocketFactory",
                                            InetSocketAddress (Ipv4Address::GetAny (), port ));

    myPacketSinkTcpDown.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

    PacketSinkTcpDown = myPacketSinkTcpDown.Install(staNodes.Get (i));
    PacketSinkTcpDown.Start (Seconds (0.0));
    PacketSinkTcpDown.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

    // Install a sender on the sender node
    InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i].GetAddress(0), port ));

    // TCP will have TcpPriorityLevel, whether priorities are enabled or not
    destAddress.SetTos (TcpPriorityLevel);
    
    BulkSendHelper myBulkSendTcpDown ( "ns3::TcpSocketFactory", destAddress);

    // Set the amount of data to send in bytes.  Zero is unlimited.
    myBulkSendTcpDown.SetAttribute ("MaxBytes", UintegerValue (0));
    myBulkSendTcpDown.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
    // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

    // Install a sender on the server
    if (topology == 0) {
      BulkSendTcpDown.Add(myBulkSendTcpDown.Install (singleServerNode.Get(0)));
    } else {
      BulkSendTcpDown.Add(myBulkSendTcpDown.Install (serverNodes.Get (i)));
    }

    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application TCP download  from the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t and port " << port
                  << '\n';    
      } else {
        std::cout << "Application TCP download  from server #" << serverNodes.Get(i)->GetId()
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t and port " << port
                  << '\n';
      }
    }

    /*
    if (numberWiFiDevicesInSTAs==2) {
      // Install a sender on the sender node
      InetSocketAddress destAddressSecondary (InetSocketAddress (staInterfacesSecondary[i].GetAddress(0), port ));      

      // TCP will have TcpPriorityLevel, whether priorities are enabled or not
      destAddressSecondary.SetTos (TcpPriorityLevel);
      
      BulkSendHelper myBulkSendTcpDownSecondary ( "ns3::TcpSocketFactory", destAddressSecondary);

      // Set the amount of data to send in bytes.  Zero is unlimited.
      myBulkSendTcpDown.SetAttribute ("MaxBytes", UintegerValue (0));
      myBulkSendTcpDown.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
      // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

      // Install a sender on the server
      if (topology == 0) {
        BulkSendTcpDownSecondary.Add(myBulkSendTcpDownSecondary.Install (singleServerNode.Get(0)));
      } else {
        BulkSendTcpDownSecondary.Add(myBulkSendTcpDownSecondary.Install (serverNodes.Get (i)));
      }

      if (verboseLevel > 0) {
        if (topology == 0) {
          std::cout << "Application TCP download  from the server"
                    << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                    << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                    << "\t\t with IP address " << staInterfacesSecondary[i].GetAddress(0) 
                    << "\t and port " << port
                    << '\n';    
        } else {
          std::cout << "Application TCP download  from server #" << serverNodes.Get(i)->GetId()
                    << "\t with IP address " << serverInterfaces.GetAddress (i) 
                    << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                    << "\t\t with IP address " << staInterfacesSecondary[i].GetAddress(0) 
                    << "\t and port " << port
                    << '\n';
        }
      }
    }*/

    port++;
  }

  BulkSendTcpDown.Start (Seconds (INITIALTIMEINTERVAL));
  BulkSendTcpDown.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

  if (numberWiFiDevicesInSTAs==2) {
    BulkSendTcpDownSecondary.Start (Seconds (INITIALTIMEINTERVAL));
    BulkSendTcpDownSecondary.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
  }

  // Video download applications
  port = INITIALPORT_VIDEO_DOWNLOAD;
  // Using UdpTraceClient, see  https://www.nsnam.org/doxygen/udp-trace-client_8cc_source.html
  //                            https://www.nsnam.org/doxygen/structns3_1_1_udp_trace_client.html
  // The traffic traces are      http://www2.tkn.tu-berlin.de/research/trace/ltvt.html (the 2 first lines of the file should be removed)
  /*A valid trace file is a file with 4 columns:
    -1- the first one represents the frame index
    -2- the second one indicates the type of the frame: I, P or B
    -3- the third one indicates the time on which the frame was generated by the encoder (integer, milliseconds)
    -4- the fourth one indicates the frame size in byte
  */

  // VideoDownServer in the STA (receives the video)
  // Client in the corresponding server
  UdpServerHelper myVideoDownServer;
  ApplicationContainer VideoDownServer;

  for (uint16_t i = numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; 
                i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload; 
                i++) {

    myVideoDownServer = UdpServerHelper(port);
    VideoDownServer = myVideoDownServer.Install (staNodes.Get (i));
    VideoDownServer.Start (Seconds (0.0));
    VideoDownServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

    UdpTraceClientHelper myVideoDownClient;
    ApplicationContainer VideoDownClient;

    InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i].GetAddress(0), port));

    if (prioritiesEnabled == 0) {
      destAddress.SetTos (VideoPriorityLevel);
    } else {
      destAddress.SetTos (VideoPriorityLevel);
    }

    myVideoDownClient = UdpTraceClientHelper(destAddress, port,"");
    myVideoDownClient.SetAttribute ("MaxPacketSize", UintegerValue (VideoMaxPacketSize));
    myVideoDownClient.SetAttribute ("TraceLoop", BooleanValue (1));


    // Random integer for choosing the movie
    uint16_t numberOfMovies = 4;
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
    double random_number = x->GetValue(0.0, numberOfMovies * 1.0);

    if ( verboseLevel > 2 )
      std::cout << "Random number: " << random_number << "\n\n";

    std::string movieFileName;  // The files have to be in the /ns-3-allinone/ns-3-dev/traces folder

    if (random_number < 1.0 )
      movieFileName = "traces/Verbose_Jurassic.dat";   //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_Jurassic.dat
    else if (random_number < 2.0 )
    movieFileName = "traces/Verbose_FirstContact.dat"; //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_FirstContact.dat
    else if (random_number < 3.0 )
      movieFileName = "traces/Verbose_StarWarsIV.dat";  //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_StarWarsIV.dat
    else if (random_number < 4.0 )
      movieFileName = "traces/Verbose_DieHardIII.dat";  //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_DieHardIII.dat

    myVideoDownClient.SetAttribute ("TraceFilename", StringValue (movieFileName)); 

    //VoipDownClient = myVoipDownClient.Install (wifiApNodesA.Get(0));
    if (topology == 0) {
      VideoDownClient = myVideoDownClient.Install (singleServerNode.Get(0));
    } else {
      VideoDownClient = myVideoDownClient.Install (serverNodes.Get (i));
    }

    VideoDownClient.Start (Seconds (INITIALTIMEINTERVAL));
    VideoDownClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application Video download from the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t and port " << port
                  << "\t " << movieFileName
                  << '\n';          
      } else {
        std::cout << "Application Video download from server #" << serverNodes.Get(i)->GetId()
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId()
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0)  
                  << "\t and port " << port
                  << "\t " << movieFileName
                  << '\n';     
      }     
    }
    port ++;
  }

  // print a blank line after the info about the applications
  if (verboseLevel > 0)
    std::cout << "\n";


  // Enable the creation of pcap files
  if (enablePcap) {

    // pcap trace of the APs and the STAs
    if (wifiModel == 0) {

      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

      for (uint32_t i=0; i< number_of_APs * numberAPsSamePlace; i++) {
        wifiPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_AP", apWiFiDevices[i]);
      }

      for (uint32_t i=0; i < number_of_STAs; i++) {
        wifiPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_STA", staDevices[i]);
        if (numberWiFiDevicesInSTAs==2) {
          wifiPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_STA_secondary", staDevicesSecondary[i]);
        }
      }

    } else {

      spectrumPhy.SetPcapDataLinkType (SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);

      for (uint32_t i=0; i< number_of_APs * numberAPsSamePlace; i++){
        spectrumPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_AP", apWiFiDevices[i]);     
      }

      // pcap trace of the STAs
      for (uint32_t i=0; i < number_of_STAs; i++) {
        spectrumPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_STA", staDevices[i]);    
        if (numberWiFiDevicesInSTAs==2) {
          spectrumPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_STA_secondary", staDevicesSecondary[i]);
        } 
      }
    }

    // pcap trace of the server(s)
    if (topology == 0) {
      // pcap trace of the single server
      csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_single_server", singleServerDevices.Get(0));

    } else {
      // pcap trace of the servers
      for (uint32_t i=0; i < number_of_Servers; i++)
        p2p.EnablePcap (outputFileName + "_" + outputFileSurname + "_p2p_server", serverDevices.Get(i));
    }


    // pcap trace of the hub ports
    if (topology == 0) {
      // pcap trace of the hub: it has number_of_APs * numberAPsSamePlace + 1 devices (the one connected to the server)
      for (uint32_t i=0; i < (number_of_APs * numberAPsSamePlace) + 1; i++)
        csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hub", csmaHubDevices.Get(i));
    }

    else if (topology == 1) {
       // pcap trace of the hub: it has number_of_APs * numberAPsSamePlace + number_of_server devices
      for (uint32_t i=0; i < (number_of_APs * numberAPsSamePlace) + number_of_Servers; i++)
        csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hub", csmaHubDevices.Get(i)); 
    }

    else if (topology == 2) {
      // pcap trace of the hub: it has number_of_APs * numberAPsSamePlace
      for (uint32_t i=0; i < (number_of_APs * numberAPsSamePlace); i++)
        csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hubToAPs", csmaHubDevices.Get(i));

      // the hub has another device (the one connected to the router). This trace is this one:
      csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hubToRouter", routerDeviceToAps.Get(0)); // would be the same as 'csmaHubDevices.Get(number_of_APs* numberAPsSamePlace)'
    }
  }


  // Install FlowMonitor on the nodes
  // see https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
  // and https://www.nsnam.org/docs/models/html/flow-monitor.html
  //FlowMonitorHelper flowmon; // FIXME Avoid the use of a global variable 'flowmon' https://groups.google.com/forum/#!searchin/ns-3-users/const$20ns3$3A$3AFlowMonitorHelper$26)$20is$20private%7Csort:date/ns-3-users/1WbpLwvYTcM/dQUQEJKkAQAJ
  Ptr<FlowMonitor> monitor;


  // It is not necessary to monitor the APs, because I am not getting statistics from them
  if (false)
    monitor = flowmon.Install(apNodes);

  // install monitor in the STAs
  monitor = flowmon.Install(staNodes);

  // install monitor in the server(s)
  if (topology == 0) {
    monitor = flowmon.Install(singleServerNode);
  } else {
    monitor = flowmon.Install(serverNodes);
  }


  // If the delay monitor is on, periodically calculate the statistics
  if (timeMonitorKPIs > 0.0) {
    // Schedule a periodic obtaining of statistics    
    Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                          &obtainKPIs,
                          monitor
                          /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                          myFlowStatisticsVoIPUpload,
                          1,
                          verboseLevel,
                          timeMonitorKPIs);

    // Schedule a periodic obtaining of statistics    
    Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                          &obtainKPIs,
                          monitor
                          /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                          myFlowStatisticsVoIPDownload,
                          2,
                          verboseLevel,
                          timeMonitorKPIs);

    // Schedule a periodic obtaining of statistics    
    Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                          &obtainKPIs,
                          monitor
                          /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                          myFlowStatisticsTCPUpload,
                          3,
                          verboseLevel,
                          timeMonitorKPIs);

    // Schedule a periodic obtaining of statistics    
    Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                          &obtainKPIs,
                          monitor
                          /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                          myFlowStatisticsTCPDownload,
                          4,
                          verboseLevel,
                          timeMonitorKPIs);

    // Schedule a periodic obtaining of statistics    
    Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                          &obtainKPIs,
                          monitor
                          /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                          myFlowStatisticsVideoDownload,
                          5,
                          verboseLevel,
                          timeMonitorKPIs);

    // Write the values of the network KPIs (delay, etc.) to a file
    // create a string with the name of the output file
    std::ostringstream nameKPIFile;

    nameKPIFile << outputFileName
                << "_"
                << outputFileSurname
                << "_KPIs.txt";

    std::ofstream ofs;
    ofs.open ( nameKPIFile.str(), std::ofstream::out | std::ofstream::trunc); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // write the first line in the file (includes the titles of the columns)
    ofs << "timestamp [s]" << "\t"
        << "flow ID" << "\t"
        << "application" << "\t"
        << "destinationPort" << "\t"
        << "delay [s]" << "\t"
        << "jitter [s]" << "\t" 
        << "numRxPackets" << "\t"
        << "numlostPackets" << "\t"
        << "throughput [bps]" << "\n";

    // schedule this after the first time when statistics have been obtained
    Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL + timeMonitorKPIs + 0.0001),
                          &saveKPIs,
                          nameKPIFile.str(),
                          myAllTheFlowStatistics,
                          verboseLevel,
                          timeMonitorKPIs);

    // Algorithm for dynamically adjusting aggregation
    if (aggregationDynamicAlgorithm ==1) {
      // Write the values of the AMPDU to a file
      // create a string with the name of the output file
      std::ostringstream nameAMPDUFile;

      nameAMPDUFile << outputFileName
                    << "_"
                    << outputFileSurname
                    << "_AMPDUvalues.txt";

      std::ofstream ofsAMPDU;
      ofsAMPDU.open ( nameAMPDUFile.str(), std::ofstream::out | std::ofstream::trunc); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

      // write the first line in the file (includes the titles of the columns)
      ofsAMPDU  << "timestamp" << "\t"
                << "ID" << "\t"
                << "type" << "\t"
                << "associated to AP\t"
                << "AMPDU set to [bytes]" << "\n";

      // prepare the parameters to call the function adjustAMPDU
      adjustAmpduParameters myparam;
      myparam.verboseLevel = verboseLevel;
      myparam.timeInterval = timeMonitorKPIs;
      myparam.latencyBudget = latencyBudget;
      myparam.maxAmpduSize = maxAmpduSize;
      myparam.mynameAMPDUFile = nameAMPDUFile.str();
      myparam.methodAdjustAmpdu = methodAdjustAmpdu;
      myparam.stepAdjustAmpdu = stepAdjustAmpdu;

      // Modify the AMPDU of the APs where there are VoIP flows
      Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL + timeMonitorKPIs + 0.0002),
                            &adjustAMPDU,
                            myAllTheFlowStatistics,
                            myparam,
                            belowLatencyAmpduValue,
                            aboveLatencyAmpduValue,
                            number_of_APs * numberAPsSamePlace);
    }
  }


  // mobility trace
  if (writeMobility) {
    AsciiTraceHelper ascii;
    MobilityHelper::EnableAsciiAll (ascii.CreateFileStream (outputFileName + "_" + outputFileSurname + "-mobility.txt"));
  }


// FIXME ***************Trial: Change the parameters of the AP (disable A-MPDU) during the simulation
// how to change attributes: https://www.nsnam.org/docs/manual/html/attributes.html
// https://www.nsnam.org/doxygen/regular-wifi-mac_8cc_source.html
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VI_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VO_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize", UintegerValue(0));
  if(false) {
    for ( uint32_t j = 0 ; j < (number_of_APs * numberAPsSamePlace) ; ++j ) {
      std::cout << "scheculing AMPDU=0 in second 4.0 for node " << j << '\n';
      Simulator::Schedule (Seconds (4.0), &ModifyAmpdu, j, 0, verboseLevel );

      std::cout << "scheculing AMPDU= " << maxAmpduSize << " in second 6.0 for node " << j << '\n';
      Simulator::Schedule (Seconds (6.0), &ModifyAmpdu, j, maxAmpduSize, verboseLevel );
    }
    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j ) {
      std::cout << "scheculing AMPDU=0 in second 4.0 for node " << (number_of_APs * numberAPsSamePlace )+ number_of_STAs + j << '\n';
      Simulator::Schedule (Seconds (4.0), &ModifyAmpdu, (number_of_APs * numberAPsSamePlace) + number_of_STAs + j, 0, verboseLevel );

      std::cout << "scheculing AMPDU= " << maxAmpduSize << " in second 6.0 for node " << (number_of_APs * numberAPsSamePlace) + number_of_STAs + j << '\n';
      Simulator::Schedule (Seconds (6.0), &ModifyAmpdu, (number_of_APs * numberAPsSamePlace) + number_of_STAs + j, maxAmpduSize, verboseLevel );
    }
  }
// FIXME *** end of the trial ***


  if ( (verboseLevel > 0) && (aggregationDisableAlgorithm == 1) ) {
    Simulator::Schedule(Seconds(0.0), &List_STA_record);
    Simulator::Schedule(Seconds(0.0), &ListAPs, verboseLevel);
  }

  if (printSeconds > 0) {
    Simulator::Schedule(Seconds(0.0), &printTime, printSeconds, outputFileName, outputFileSurname);
  }

  // Start ARP trial (Failure so far)
  if (false) {
    for ( uint32_t j = 0 ; j < number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.0), &PopulateArpCache, j + (number_of_APs * numberAPsSamePlace), serverNodes.Get(j));

    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.0), &PopulateArpCache, j + (number_of_APs * numberAPsSamePlace) + number_of_Servers, staNodes.Get(j));

    for ( uint32_t j = 0 ; j < number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.0), &infoArpCache, serverNodes.Get(j), verboseLevel);  

    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.0), &infoArpCache, staNodes.Get(j), verboseLevel);

    //Simulator::Schedule(Seconds(2.0), &emtpyArpCache);

    for ( uint32_t j = number_of_APs * numberAPsSamePlace ; j < (number_of_APs * numberAPsSamePlace) + number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.5), &PrintArpCache, staNodes.Get(j), staDevices[j].Get(0));

    for ( uint32_t j = number_of_APs * numberAPsSamePlace ; j < (number_of_APs * numberAPsSamePlace) + number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.5), &PrintArpCache, serverNodes.Get(j), serverDevices.Get(j));


    //  Time mytime2 = Time (10.0);
    //    Config::SetDefault ("ns3::ArpL3Protocol::AliveTimeout", TimeValue(mytime2));

    // Modify the parameters of the ARP caches of the STAs and servers
    // see the parameters of the arp cache https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
    // I only run this for the STAs and the servers. The APs and the hub do not have the IP stack
    for ( uint32_t j = number_of_APs * numberAPsSamePlace ; j < (number_of_APs * numberAPsSamePlace) + number_of_Servers + number_of_STAs; ++j ) {

      // I use an auxiliar string for creating the first argument of Config::Set
      std::ostringstream auxString;

      // Modify the number of retries
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::MaxRetries";
      auxString << "/NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::MaxRetries";
      // std::cout << auxString.str() << '\n';
      Config::Set(auxString.str(),  UintegerValue(1));
      // clean the string
      auxString.str(std::string());

      // Modify the size of the queue for packets pending an arp reply
      auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/0/ArpCache::PendingQueueSize";
      // std::cout << auxString.str() << '\n';
      Config::Set(auxString.str(),  UintegerValue(1));
      // clean the string
      auxString.str(std::string());

      if (numberAPsSamePlace == 2) {
        // Modify the size of the queue for packets pending an arp reply
        auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::PendingQueueSize";
        // std::cout << auxString.str() << '\n';
        Config::Set(auxString.str(),  UintegerValue(1));
        // clean the string
        auxString.str(std::string());        
      }


      // Modify the AliveTimeout
      Time mytime = Time (10.0);
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::AliveTimeout";
      //auxString << "  /NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
      auxString << "/NodeList/*/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
      Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
      // clean the string
      auxString.str(std::string());

      // Modify the DeadTimeout
      mytime = Time (1.0);
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/0/ArpCache::DeadTimeout";
      auxString << "/NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::DeadTimeout";
      Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
    }

    std::ostringstream auxString;
    // Modify the AliveTimeout
    Time mytime = Time (10.0);
    //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::AliveTimeout";
    //auxString << "  /NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
    auxString << "/NodeList/*/ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
    Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
    //Config::Connect(auxString.str(), MakeCallback (&hello) );
    // clean the string
    auxString.str(std::string());
  }
  // End ARP trial (failure so far)


// Trial of channel swithing of a STA
//https://10343742895474358856.googlegroups.com/attach/1b7c2a3108d5e/channel-switch-minimal.cc?part=0.1&view=1&vt=ANaJVrGFRkTkufO3dLFsc9u1J_v2-SUCAMtR0V86nVmvXWXGwwZ06cmTSv7DrQUKMWTVMt_lxuYTsrYxgVS59WU3kBd7dkkH5hQsLE8Em0FHO4jx8NbjrPk
if(false) {
  NetDeviceContainer devices;
  devices.Add (apWiFiDevices[0]);
  devices.Add (staDevices[0]);
  ChangeFrequencyLocal (devices, 44, wifiModel, verboseLevel); // This works since it is executed before the simulation starts.

  Simulator::Schedule(Seconds(2.0), &ChangeFrequencyLocal, devices, 44, wifiModel, verboseLevel); // This does not work with SpectrumWifiPhy. But IT WORKS WITH YANS!!!

  //nearestAp (apNodes, staNodes.Get(0), verboseLevel);
  //Simulator::Schedule(Seconds(3.0), &nearestAp, apNodes, staNodes.Get(0), verboseLevel);
  // This does not work because nearestAp returns a value and you cannot schedule it
}



  if (verboseLevel > 0) {
    NS_LOG_INFO ("Run Simulation");
    NS_LOG_INFO ("");
  }

  Simulator::Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
  Simulator::Run ();

  if (verboseLevel > 0)
    NS_LOG_INFO ("Simulation finished. Writing results");


  /***** Obtain per flow and aggregate statistics *****/

  // This part is inspired on https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
  // and also on https://groups.google.com/forum/#!msg/ns-3-users/iDs9HqrQU-M/ryoVRz4M_fYJ

  monitor->CheckForLostPackets (); // Check right now for packets that appear to be lost.

  // FlowClassifier provides a method to translate raw packet data into abstract flow identifier and packet identifier parameters
  // see https://www.nsnam.org/doxygen/classns3_1_1_flow_classifier.html
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ()); // Returns a pointer to the FlowClassifier object


  // Save the results of flowmon to an XML file
  if (saveXMLFile)
    flowmon.SerializeToXmlFile (outputFileName + "_" + outputFileSurname + "_flowmonitor.xml", true, true);


  // variables used for calculating the averages
  std::string proto; 
  uint32_t this_is_the_first_flow = 1;        // used to add the titles on the first line of the output file

  uint32_t number_of_UDP_upload_flows = 0;      // this index is used for the cumulative calculation of the average
  uint32_t number_of_UDP_download_flows = 0;
  uint32_t number_of_TCP_upload_flows = 0;      // this index is used for the cumulative calculation of the average
  uint32_t number_of_TCP_download_flows = 0;
  uint32_t number_of_video_download_flows = 0;

  uint32_t total_VoIP_upload_tx_packets = 0;
  uint32_t total_VoIP_upload_rx_packets = 0;
  double total_VoIP_upload_latency = 0.0;
  double total_VoIP_upload_jitter = 0.0;

  uint32_t total_VoIP_download_tx_packets = 0;
  uint32_t total_VoIP_download_rx_packets = 0;
  double total_VoIP_download_latency = 0.0;
  double total_VoIP_download_jitter = 0.0;

  double total_TCP_upload_throughput = 0.0;
  double total_TCP_download_throughput = 0.0; // average throughput of all the download TCP flows

  double total_video_download_throughput = 0.0; // average throughput of all the download video flows

  // for each flow
  std::map< FlowId, FlowMonitor::FlowStats > stats = monitor->GetFlowStats(); 
  for (std::map< FlowId, FlowMonitor::FlowStats >::iterator flow=stats.begin(); flow!=stats.end(); flow++) 
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow->first); 

    switch(t.protocol) 
    { 
      case(6): 
        proto = "TCP"; 
      break; 

      case(17): 
        proto = "UDP"; 
      break; 

      default: 
        std::cout << "Protocol unknown" << std::endl;
        exit(1);
      break;

    }


    // create a string with the characteristics of the flow
    std::ostringstream flowID;

    flowID  << flow->first << "\t"      // identifier of the flow (a number)
            << proto << "\t"
            << t.sourceAddress << "\t"
            << t.sourcePort << "\t" 
            << t.destinationAddress << "\t"
            << t.destinationPort;

    // UDP upload flows
    if (  (t.destinationPort >= INITIALPORT_VOIP_UPLOAD ) && 
          (t.destinationPort <  INITIALPORT_VOIP_UPLOAD + numberVoIPupload )) {
      flowID << "\t VoIP upload";
    // UDP download flows
    } else if ( (t.destinationPort >= INITIALPORT_VOIP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VOIP_DOWNLOAD + numberVoIPdownload )) { 
      flowID << "\t VoIP download";
    // TCP upload flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_UPLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_UPLOAD + numberTCPupload )) { 
      flowID << "\t TCP upload";
    // TCP download flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_DOWNLOAD + numberTCPdownload )) { 
      flowID << "\t TCP download";
    } else if ( (t.destinationPort >= INITIALPORT_VIDEO_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VIDEO_DOWNLOAD + numberVideoDownload)) { 
      flowID << "\t Video download";
    } 

    // create a string with the name of the output file
    std::ostringstream nameFlowFile, surnameFlowFile;

    nameFlowFile  << outputFileName
                  << "_"
                  << outputFileSurname;

    surnameFlowFile << "_flow_"
                    << flow->first;

    // Print the statistics of this flow to an output file and to the screen
    print_stats ( flow->second, 
                  simulationTime, 
                  generateHistograms, 
                  nameFlowFile.str(), 
                  surnameFlowFile.str(), 
                  verboseLevel, 
                  flowID.str(), 
                  this_is_the_first_flow );

    // the first time, print_stats will print a line with the title of each column
    // put the flag to 0
    if ( this_is_the_first_flow == 1 )
      this_is_the_first_flow = 0;

    // calculate and print the average of each kind of applications
    // calculate it in a cumulative way

    // UDP upload flows
    if (  (t.destinationPort >= INITIALPORT_VOIP_UPLOAD ) && 
          (t.destinationPort <  INITIALPORT_VOIP_UPLOAD + numberVoIPupload )) {

        total_VoIP_upload_tx_packets = total_VoIP_upload_tx_packets + flow->second.txPackets;
        total_VoIP_upload_rx_packets = total_VoIP_upload_rx_packets + flow->second.rxPackets;
        total_VoIP_upload_latency = total_VoIP_upload_latency + flow->second.delaySum.GetSeconds();
        total_VoIP_upload_jitter = total_VoIP_upload_jitter + flow->second.jitterSum.GetSeconds();
        number_of_UDP_upload_flows ++;

    // UDP download flows
    } else if ( (t.destinationPort >= INITIALPORT_VOIP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VOIP_DOWNLOAD + numberVoIPdownload )) { 

        total_VoIP_download_tx_packets = total_VoIP_download_tx_packets + flow->second.txPackets;
        total_VoIP_download_rx_packets = total_VoIP_download_rx_packets + flow->second.rxPackets;
        total_VoIP_download_latency = total_VoIP_download_latency + flow->second.delaySum.GetSeconds();
        total_VoIP_download_jitter = total_VoIP_download_jitter + flow->second.jitterSum.GetSeconds();
        number_of_UDP_download_flows ++;

    // TCP upload flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_UPLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_UPLOAD + numberTCPupload )) {  

        total_TCP_upload_throughput = total_TCP_upload_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );
        number_of_TCP_upload_flows ++;

    // TCP download flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_DOWNLOAD + numberTCPdownload )) {

        total_TCP_download_throughput = total_TCP_download_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );                                          
        number_of_TCP_download_flows ++;

    // video download flows
    } else if ( (t.destinationPort >= INITIALPORT_VIDEO_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VIDEO_DOWNLOAD + numberVideoDownload)) { 
      
        total_video_download_throughput = total_video_download_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );                                          
        number_of_video_download_flows ++;
    } 
  }

  if (verboseLevel > 0) {
    std::cout << "\n" 
              << "The next figures are averaged per packet, not per flow:" << std::endl;

    if ( total_VoIP_upload_rx_packets > 0 ) {
      std::cout << " Average VoIP upload latency [s]:\t" << total_VoIP_upload_latency / total_VoIP_upload_rx_packets << std::endl;
      std::cout << " Average VoIP upload jitter [s]:\t" << total_VoIP_upload_jitter / total_VoIP_upload_rx_packets << std::endl;
    } else {
      std::cout << " Average VoIP upload latency [s]:\tno packets received" << std::endl;
      std::cout << " Average VoIP upload jitter [s]:\tno packets received" << std::endl;      
    }
    if ( total_VoIP_upload_tx_packets > 0 ) {
      std::cout << " Average VoIP upload loss rate:\t\t" 
                <<  1.0 - ( double(total_VoIP_upload_rx_packets) / double(total_VoIP_upload_tx_packets) )
                << std::endl;
    } else {
      std::cout << " Average VoIP upload loss rate:\t\tno packets sent" << std::endl;     
    }


    if ( total_VoIP_download_rx_packets > 0 ) {
      std::cout << " Average VoIP download latency [s]:\t" << total_VoIP_download_latency / total_VoIP_download_rx_packets 
                << std::endl;
      std::cout << " Average VoIP download jitter [s]:\t" << total_VoIP_download_jitter / total_VoIP_download_rx_packets 
                << std::endl;
    } else {
      std::cout << " Average VoIP download latency [s]:\tno packets received" << std::endl;
      std::cout << " Average VoIP download jitter [s]:\tno packets received" << std::endl;      
    }
    if ( total_VoIP_download_tx_packets > 0 ) {
      std::cout << " Average VoIP download loss rate:\t" 
                <<  1.0 - ( double(total_VoIP_download_rx_packets) / double(total_VoIP_download_tx_packets) )
                << std::endl;
    } else {
     std::cout << " Average VoIP download loss rate:\tno packets sent" << std::endl;     
    }

    std::cout << "\n" 
              << " Number TCP upload flows\t\t"
              << number_of_TCP_upload_flows << "\n"
              << " Total TCP upload throughput [bps]\t"
              << total_TCP_upload_throughput << "\n"

              << " Number TCP download flows\t\t"
              << number_of_TCP_download_flows << "\n"
              << " Total TCP download throughput [bps]\t"
              << total_TCP_download_throughput << "\n";

    std::cout << "\n" 
              << " Number video download flows\t\t"
              << number_of_video_download_flows << "\n"
              << " Total video download throughput [bps]\t"
              << total_video_download_throughput << "\n";
  }

  // save the average values to a file 
  std::ofstream ofs;
  ofs.open ( outputFileName + "_average.txt", std::ofstream::out | std::ofstream::app); // with "app", all output operations happen at the end of the file, appending to its existing contents
  ofs << outputFileSurname << "\t"
      << "Number VoIP upload flows" << "\t"
      << number_of_UDP_upload_flows << "\t";
  if ( total_VoIP_upload_rx_packets > 0 ) {
    ofs << "Average VoIP upload latency [s]" << "\t"
        << total_VoIP_upload_latency / total_VoIP_upload_rx_packets << "\t"
        << "Average VoIP upload jitter [s]" << "\t"
        << total_VoIP_upload_jitter / total_VoIP_upload_rx_packets << "\t";
  } else {
    ofs << "Average VoIP upload latency [s]" << "\t"
        << "\t"
        << "Average VoIP upload jitter [s]" << "\t"
        << "\t";
  }
  if ( total_VoIP_upload_tx_packets > 0 ) {
    ofs << "Average VoIP upload loss rate" << "\t"
        << 1.0 - ( double(total_VoIP_upload_rx_packets) / double(total_VoIP_upload_tx_packets) ) << "\t";
  } else {
    ofs << "Average VoIP upload loss rate" << "\t"
        << "\t";
  }

  ofs << "Number VoIP download flows" << "\t"
      << number_of_UDP_download_flows << "\t";
  if ( total_VoIP_download_rx_packets > 0 ) {
    ofs << "Average VoIP download latency [s]" << "\t"
        << total_VoIP_download_latency / total_VoIP_download_rx_packets << "\t"
        << "Average VoIP download jitter [s]" << "\t"
        << total_VoIP_download_jitter / total_VoIP_download_rx_packets << "\t";
  } else {
    ofs << "Average VoIP download latency [s]" << "\t"
        << "\t"
        << "Average VoIP download jitter [s]" << "\t"
        << "\t";
  }
  if ( total_VoIP_download_tx_packets > 0 ) {
    ofs << "Average VoIP download loss rate" << "\t"
        << 1.0 - ( double(total_VoIP_download_rx_packets) / double(total_VoIP_download_tx_packets) ) << "\t";
  } else {
    ofs << "Average VoIP download loss rate" << "\t"
        << "\t";
  }

  ofs << "Number TCP upload flows" << "\t"
      << number_of_TCP_upload_flows << "\t"
      << "Total TCP upload throughput [bps]" << "\t"
      << total_TCP_upload_throughput << "\t"

      << "Number TCP download flows" << "\t"
      << number_of_TCP_download_flows << "\t"
      << "Total TCP download throughput [bps]" << "\t"
      << total_TCP_download_throughput << "\t";

  ofs << "Number video download flows" << "\t"
      << number_of_video_download_flows << "\t"
      << "Total video download throughput [bps]" << "\t"
      << total_video_download_throughput << "\t";

  ofs << "Duration of the simulation [s]" << "\t"
      << simulationTime << "\n";

  ofs.close();

  // Cleanup
  Simulator::Destroy ();
  if (verboseLevel > 0)
    NS_LOG_INFO ("Done");

  return 0;
}
