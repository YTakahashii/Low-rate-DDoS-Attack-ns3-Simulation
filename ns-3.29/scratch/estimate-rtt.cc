/*
 * The topology used to simulate this attack contains 5 nodes as follows:
 * n0 -> alice (sender)
 * n1 -> eve (attacker)
 * n2 -> router (common router between alice and eve)
 * n3 -> router (router conneced to bob)
 * n4 -> bob (receiver)
     n1
        \ pp1 (100 Mbps, 2ms RTT)
         \
          \             -> pp1 (100 Mbps, 2ms RTT)
           \            |
            n2 ---- n3 ---- n4
           /    |
          /     -> pp2 (1.5 Mbps, 40ms RTT)
         /
        / pp1 (100 Mbps, 2ms RTT)
     n0
*/

#include "ns3/nstime.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/v4ping-helper.h"
#include "ns3/node-container.h"
#include <cstring>
#include <cstdlib>


#define TCP_SINK_PORT 9000
#define UDP_SINK_PORT 9001

// Experimentation parameters
#define BULK_SEND_MAX_BYTES 2097152
#define MAX_SIMULATION_TIME 100.0
#define ATTACKER_START 0.0
#define ATTACKER_RATE (std::string)"12000kb/s"
#define ON_TIME (std::string)"0.25"
#define BURST_PERIOD 1
#define OFF_TIME std::to_string(BURST_PERIOD - stof(ON_TIME))
#define SENDER_START 0.75 // Must be equal to OFF_TIME

#define TH_INTERVAL	0.1	// The time interval in seconds for measurement throughput

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");

uint32_t oldTotalBytes=0;
uint32_t newTotalBytes;

void TraceThroughput (Ptr<Application> app, Ptr<OutputStreamWrapper> stream)
{
    Ptr <PacketSink> pktSink = DynamicCast <PacketSink> (app);
    newTotalBytes = pktSink->GetTotalRx ();

	// messure throughput in Kbps
	//fprintf(stdout,"%10.4f %f\n",Simulator::Now ().GetSeconds (), (newTotalBytes - oldTotalBytes)*8/0.1/1024);
  	*stream->GetStream() << Simulator::Now ().GetSeconds () << " \t " << (newTotalBytes - oldTotalBytes)*8.0/0.1/1024 << std::endl;
	oldTotalBytes = newTotalBytes;
	Simulator::Schedule (Seconds (TH_INTERVAL), &TraceThroughput, app, stream);
}

 static void PingRtt (std::string context, Time rtt)
 {
  
   std::cout << rtt.GetMilliSeconds() << std::endl;
 }

int main(int argc, char *argv[])
{
    std::string prefix_file_name = "estimate";
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);


    NodeContainer nodes;
    nodes.Create(8);


    // Default TCP is TcpNewReno (no need to change, unless experimenting with other variants)
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    // Define the Point-to-Point links (helpers) and their paramters
    PointToPointHelper pp1, pp2, pp3, pp4;
    pp1.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pp1.SetChannelAttribute("Delay", StringValue("1ms"));

    // Add a DropTailQueue to the bottleneck link
    pp2.SetQueue ("ns3::DropTailQueue","MaxSize", StringValue ("50p"));
    pp2.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pp2.SetChannelAttribute("Delay", StringValue("20ms"));

    pp3.SetDeviceAttribute("DataRate", StringValue("30Mbps"));
    pp3.SetChannelAttribute("Delay", StringValue("15ms"));

    pp4.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    pp4.SetChannelAttribute("Delay", StringValue("10ms"));
    // Install the Point-to-Point links between nodes
    NetDeviceContainer d02, d12, d23, d34, d52, d62, d72;
    d02 = pp1.Install(nodes.Get(0), nodes.Get(2));
    d12 = pp1.Install(nodes.Get(1), nodes.Get(2));
    d23 = pp2.Install(nodes.Get(2), nodes.Get(3));
    d34 = pp1.Install(nodes.Get(3), nodes.Get(4));
    //add
    d52 = pp1.Install(nodes.Get(5), nodes.Get(2));
    d62 = pp1.Install(nodes.Get(6), nodes.Get(2));
    d72 = pp1.Install(nodes.Get(7), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);


    Ipv4AddressHelper a02, a12, a23, a34, a52, a62, a72;
    a02.SetBase("10.1.1.0", "255.255.255.0");
    a12.SetBase("10.1.2.0", "255.255.255.0");
    a23.SetBase("10.1.3.0", "255.255.255.0");
    a34.SetBase("10.1.4.0", "255.255.255.0");
    a52.SetBase("10.1.5.0", "255.255.255.0");
    a62.SetBase("10.1.6.0", "255.255.255.0");
    a72.SetBase("10.1.7.0", "255.255.255.0");

    Ipv4InterfaceContainer i02, i12, i23, i34, i52, i62, i72;
    i02 = a02.Assign(d02);
    i12 = a12.Assign(d12);
    i23 = a23.Assign(d23);
    i34 = a34.Assign(d34);
    // add
    i52 = a52.Assign(d52);
    i62 = a62.Assign(d62);
    i72 = a72.Assign(d72);
    /*
    // UDP On-Off Application - Application used by attacker (eve) to create the low-rate bursts.
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(i34.GetAddress(1), UDP_SINK_PORT)));
    onoff.SetConstantRate(DataRate(ATTACKER_RATE));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + ON_TIME + "]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + OFF_TIME + "]"));
    ApplicationContainer onOffApp = onoff.Install(nodes.Get(1));
    onOffApp.Start(Seconds(ATTACKER_START));
    onOffApp.Stop(Seconds(MAX_SIMULATION_TIME));


    // TCP Bulk Send Application - Application used by the legit node (alice) to send data to a receiver.
    
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(i34.GetAddress(1), TCP_SINK_PORT));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(BULK_SEND_MAX_BYTES));
    ApplicationContainer bulkSendApp = bulkSend.Install(nodes.Get(0));
    bulkSendApp.Start(Seconds(SENDER_START));
    bulkSendApp.Stop(Seconds(MAX_SIMULATION_TIME));
    */

    // estimate C&C(n7)--Victim Router(n2)'s RTT
    V4PingHelper ping = V4PingHelper(i72.GetAddress(1)); // pingの宛先にn2を指定
    NodeContainer pingers; // ping送信ノードをAdd（複数OK）
    pingers.Add(nodes.Get(7));
    ApplicationContainer pingApp = ping.Install(pingers);
    pingApp.Start(Seconds(0.0));
    pingApp.Stop(Seconds(1.0));

    // Ping sink on the n2
    PacketSinkHelper pingSink = PacketSinkHelper("ns3::Ipv4RawSocketFactory", InetSocketAddress(Ipv4Address::GetAny()));
    ApplicationContainer pingSinkApp = pingSink.Install(nodes.Get(2));
    pingSinkApp.Start(Seconds(0.0));
    pingSinkApp.Stop(Seconds(1.0));

    // estimate n1--n7's RTT
    V4PingHelper ping17 = V4PingHelper(i72.GetAddress(0)); // pingの宛先にn7を指定
    NodeContainer pingers17; // ping送信ノードをAdd（複数OK）
    pingers17.Add(nodes.Get(1));
    ApplicationContainer ping17App = ping.Install(pingers17);
    ping17App.Start(Seconds(1.0));
    ping17App.Stop(Seconds(2.0));

    // Ping sink on the n7
    PacketSinkHelper pingSink17 = PacketSinkHelper("ns3::Ipv4RawSocketFactory", InetSocketAddress(Ipv4Address::GetAny()));
    ApplicationContainer pingSinkApp17 = pingSink17.Install(nodes.Get(7));
    pingSinkApp17.Start(Seconds(1.0));
    pingSinkApp17.Stop(Seconds(2.0));

    //ping
    /*
    V4PingHelper ping = V4PingHelper(i34.GetAddress(1)); // pingの宛先にn4を指定
    NodeContainer pingers; // ping送信ノードをAdd（複数OK）
    pingers.Add(nodes.Get(1));
    ApplicationContainer pingApp = ping.Install(pingers);
    pingApp.Start(Seconds(0.0));
    pingApp.Stop(Seconds(2.0));
    */
    // UDP sink on the receiver (bob).
    PacketSinkHelper UDPsink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
    ApplicationContainer UDPSinkApp = UDPsink.Install(nodes.Get(4));
    UDPSinkApp.Start(Seconds(0.0));
    UDPSinkApp.Stop(Seconds(MAX_SIMULATION_TIME));


    // TCP sink on the receiver (bob).
    PacketSinkHelper TCPsink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), TCP_SINK_PORT));
    ApplicationContainer TCPSinkApp = TCPsink.Install(nodes.Get(4));
    TCPSinkApp.Start(Seconds(0.0));
    TCPSinkApp.Stop(Seconds(MAX_SIMULATION_TIME));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    AsciiTraceHelper ascii;

    // Packet sink on the receiver (bob)

    // make trace file's name
    std::string fname2 = "estimate-rtt-n0-tcp.throughput";
    Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream(fname2);
    Simulator::Schedule (Seconds (TH_INTERVAL), &TraceThroughput, nodes.Get(4)->GetApplication(1), stream2);

 // finally, print the ping rtts.
    Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback (&PingRtt));
    // pp1.EnablePcapAll("PCAPs/estimate-ping/");
    Simulator::Stop (Seconds(MAX_SIMULATION_TIME));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}