/*
 * Exercise 1: Multi-Site WAN Extension (HQ, Branch, DC)
 * Topology: Triangular Mesh (n0 <-> n1 <-> n2, plus n0 <-> n2)
 * All links 5Mbps, 2ms. Static routing with Metric 0 (Direct) / Metric 1 (Via Branch).
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RouterStaticRouting");

// Function for Q3: Disables a network device (simulating a link failure)
void SetLinkDown(Ptr<NetDevice> netDevice)
{
    // Get the node and its IPv4 object
    Ptr<Node> node = netDevice->GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    
    // Find the interface index for this device and set it down
    int32_t interfaceIndex = ipv4->GetInterfaceForDevice(netDevice);
    if (interfaceIndex != -1)
    {
        ipv4->SetDown(interfaceIndex);
        NS_LOG_INFO("Primary Link Interface " << interfaceIndex << " on Node " 
                    << node->GetId() << " is DOWN. Failover expected.");
    }
}

int
main(int argc, char* argv[])
{
    // Enable logging for the applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes: n0 (HQ), n1 (Branch/Router), n2 (DC/Server)
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> n0 = nodes.Get(0); // HQ
    Ptr<Node> n1 = nodes.Get(1); // Branch/Router
    Ptr<Node> n2 = nodes.Get(2); // DC/Server

    // Configure all links (5Mbps, 2ms)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link 1: n0 <-> n1 (Network 1: 10.1.1.0/24)
    NodeContainer link1Nodes(n0, n1);
    NetDeviceContainer link1Devices = p2p.Install(link1Nodes);

    // Link 2: n1 <-> n2 (Network 2: 10.1.2.0/24)
    NodeContainer link2Nodes(n1, n2);
    NetDeviceContainer link2Devices = p2p.Install(link2Nodes);

    // --- Q1: New Link 3: n0 <-> n2 (Network 3: 10.1.3.0/24) ---
    NodeContainer link3Nodes(n0, n2);
    NetDeviceContainer link3Devices = p2p.Install(link3Nodes);

    // Install mobility model (fixed positions for triangular layout)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set positions for the triangle (HQ, Branch, DC)
    n0->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 20.0, 0.0));  // HQ (Top Left)
    n1->GetObject<MobilityModel>()->SetPosition(Vector(15.0, 5.0, 0.0));  // Branch (Bottom)
    n2->GetObject<MobilityModel>()->SetPosition(Vector(25.0, 20.0, 0.0)); // DC (Top Right)

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // --- Q1: IP Address Assignment ---

    // Network 1 (10.1.1.0/24) - n0 <-> n1
    Ipv4AddressHelper address1;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address1.Assign(link1Devices);

    // Network 2 (10.1.2.0/24) - n1 <-> n2
    Ipv4AddressHelper address2;
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address2.Assign(link2Devices);

    // Network 3 (10.1.3.0/24) - n0 <-> n2
    Ipv4AddressHelper address3;
    address3.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address3.Assign(link3Devices);

    // Enable IP forwarding on all nodes (n0, n1, n2)
    n0->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    n1->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    n2->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));


    // --- Q2: Configure Static Routing with Metrics ---

    Ipv4StaticRoutingHelper staticRoutingHelper;

    // --- Routing on n0 (HQ) ---
    Ptr<Ipv4StaticRouting> staticRoutingN0 = staticRoutingHelper.GetStaticRouting(n0->GetObject<Ipv4>());
    // 1. PRIMARY Route (Metric 0): HQ -> DC via Net 3 (Direct link)
    staticRoutingN0->AddNetworkRouteTo(
        Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), // Destination Net 2
        interfaces3.GetAddress(1),   // Next hop: 10.1.3.2 (N2's Net 3 IP)
        2,                         // Outgoing Interface Index (N0's Net 3 interface)
        0                          // Metric 0 (Primary)
    );
    // 2. BACKUP Route (Metric 1): HQ -> DC via Net 1 (Through Branch N1)
    staticRoutingN0->AddNetworkRouteTo(
        Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), // Destination Net 2
        interfaces1.GetAddress(1),   // Next hop: 10.1.1.2 (N1's Net 1 IP)
        1,                         // Outgoing Interface Index (N0's Net 1 interface)
        1                          // Metric 1 (Backup)
    );

    // --- Routing on n2 (DC) ---
    Ptr<Ipv4StaticRouting> staticRoutingN2 = staticRoutingHelper.GetStaticRouting(n2->GetObject<Ipv4>());
    // 1. PRIMARY Return Route (Metric 0): DC -> HQ via Net 3 (Direct link)
    staticRoutingN2->AddNetworkRouteTo(
        Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), // Destination Net 1
        interfaces3.GetAddress(0),   // Next hop: 10.1.3.1 (N0's Net 3 IP)
        2,                         // Outgoing Interface Index (N2's Net 3 interface)
        0                          // Metric 0 (Primary)
    );
    // 2. BACKUP Return Route (Metric 1): DC -> HQ via Net 2 (Through Branch N1)
    staticRoutingN2->AddNetworkRouteTo(
        Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), // Destination Net 1
        interfaces2.GetAddress(0),   // Next hop: 10.1.2.1 (N1's Net 2 IP)
        1,                         // Outgoing Interface Index (N2's Net 2 interface)
        1                          // Metric 1 (Backup)
    );


    // Print routing tables
    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>("scratch/router-static-routing.routes", std::ios::out);
    staticRoutingHelper.PrintRoutingTableAllAt(Seconds(1.0), routingStream);

    // --- Q1: Console Output (Verification) ---
    std::cout << "\n=== Network Configuration ===\n";
    std::cout << "Node 0 (HQ) Interface 1 (Net 1): " << interfaces1.GetAddress(0) << "\n";
    std::cout << "Node 0 (HQ) Interface 2 (Net 3): " << interfaces3.GetAddress(0) << "\n";
    std::cout << "-----------------------------\n";
    std::cout << "Node 1 (Branch) Interface 1 (Net 1): " << interfaces1.GetAddress(1) << "\n";
    std::cout << "Node 1 (Branch) Interface 2 (Net 2): " << interfaces2.GetAddress(0) << "\n";
    std::cout << "-----------------------------\n";
    std::cout << "Node 2 (DC) Interface 1 (Net 2): " << interfaces2.GetAddress(1) << "\n";
    std::cout << "Node 2 (DC) Interface 2 (Net 3): " << interfaces3.GetAddress(1) << "\n";
    std::cout << "=============================\n\n";

    // Application Setup (Client N0 targets Server N2's IP on Net 2)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(n2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces2.GetAddress(1), port); // Target: 10.1.2.2
    echoClient.SetAttribute("MaxPackets", UintegerValue(10)); // Increased packets to observe failure
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(n0);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // --- Q3: Schedule Link Failure at t=4 seconds (Verification) ---
    // We get the device on n0 corresponding to Network 3 (from link3Devices, not interfaces3).
    Ptr<NetDevice> n0_net3_device = link3Devices.Get(0); 
    Simulator::Schedule(Seconds(4.0), &SetLinkDown, n0_net3_device); 


    // --- NetAnim Configuration ---
    AnimationInterface anim("scratch/router-static-routing.xml");

    // Set node descriptions (Q1 Verification)
    anim.UpdateNodeDescription(n0, "HQ\\n10.1.1.1 | 10.1.3.1");
    anim.UpdateNodeDescription(n1, "Branch\\n10.1.1.2 | 10.1.2.1");
    anim.UpdateNodeDescription(n2, "DC\\n10.1.2.2 | 10.1.3.2");

    // Set node colors
    anim.UpdateNodeColor(n0, 0, 255, 0);   // Green for HQ
    anim.UpdateNodeColor(n1, 255, 255, 0); // Yellow for Branch
    anim.UpdateNodeColor(n2, 0, 0, 255);   // Blue for DC

    // Enable PCAP tracing on all devices
    p2p.EnablePcapAll("scratch/router-static-routing");

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\n=== Simulation Complete ===\n";
    std::cout << "Animation trace saved to: scratch/router-static-routing.xml\n";
    std::cout << "Routing tables saved to: scratch/router-static-routing.routes\n";
    std::cout << "PCAP traces saved to: scratch/router-static-routing-*.pcap\n";

    return 0;
}
