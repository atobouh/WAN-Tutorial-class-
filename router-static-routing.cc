/*
 * Exercise 5: Policy-Based Routing Main Script
 * Requires: PbrRouting.cc and PbrRouting.h
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "PbrRouting.h" // Your custom class header

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable Logs
    LogComponentEnable("PbrRouting", LOG_LEVEL_INFO);

    // Topology: Studio (n0) -> Router (n1) -> Cloud (n2)
    // We need two links between n1 and n2 for the PBR to choose from.
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> studio = nodes.Get(0);
    Ptr<Node> router = nodes.Get(1);
    Ptr<Node> cloud = nodes.Get(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link 1: Studio -> Router
    NetDeviceContainer d0 = p2p.Install(studio, router);
    // Link 2: Router -> Cloud (Primary/Video)
    NetDeviceContainer d1 = p2p.Install(router, cloud);
    // Link 3: Router -> Cloud (Secondary/Data)
    NetDeviceContainer d2 = p2p.Install(router, cloud);

    // Install Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0 = ipv4.Assign(d0);

    ipv4.SetBase("10.0.2.0", "255.255.255.0"); // Primary
    Ipv4InterfaceContainer i1 = ipv4.Assign(d1);

    ipv4.SetBase("10.0.3.0", "255.255.255.0"); // Secondary
    Ipv4InterfaceContainer i2 = ipv4.Assign(d2);

    // --- Install PBR on Router (n1) ---
    // We manually replace the static routing with our PBR logic
    Ptr<Ipv4> ipv4Router = router->GetObject<Ipv4>();
    Ptr<PbrRouting> pbr = CreateObject<PbrRouting>(
        i1.GetAddress(1), // Video Next Hop (10.0.2.2)
        i2.GetAddress(1), // Data Next Hop (10.0.3.2)
        2,                // Video Interface Index (NetDevice 1 on Router)
        3                 // Data Interface Index (NetDevice 2 on Router)
    );
    pbr->SetIpv4(ipv4Router);
    ipv4Router->SetRoutingProtocol(pbr);

    // --- Traffic Generation ---
    uint16_t port = 9;
    
    // 1. Video Flow (DSCP EF = 0x2e)
    OnOffHelper videoApp("ns3::UdpSocketFactory", InetSocketAddress(i1.GetAddress(1), port));
    videoApp.SetAttribute("PacketSize", UintegerValue(1024));
    videoApp.SetAttribute("DataRate", StringValue("1Mbps"));
    videoApp.SetAttribute("ToS", UintegerValue(0x2e << 2)); // Shift left for NS-3 ToS
    videoApp.Install(studio).Start(Seconds(1.0));

    // 2. Data Flow (DSCP BE = 0x00)
    OnOffHelper dataApp("ns3::UdpSocketFactory", InetSocketAddress(i2.GetAddress(1), port));
    dataApp.SetAttribute("PacketSize", UintegerValue(1024));
    dataApp.SetAttribute("DataRate", StringValue("1Mbps"));
    dataApp.SetAttribute("ToS", UintegerValue(0x00));
    dataApp.Install(studio).Start(Seconds(1.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    sink.Install(cloud).Start(Seconds(0.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
