/*
 * Exercise 4: Multi-Hop WAN Architecture (RegionalBank)
 * Topology: Branch-C <-> DC-A <-> DR-B (Linear Primary)
 * DC-A <-> DR-B (Redundant Link)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RouterMultiHop");

void TearDownLink(Ptr<Node> node, uint32_t interfaceIndex)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetDown(interfaceIndex);
    NS_LOG_INFO("Link Failover Triggered: Interface " << interfaceIndex << " on Node " << node->GetId() << " is DOWN.");
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> branchC = nodes.Get(0);
    Ptr<Node> dcA = nodes.Get(1);
    Ptr<Node> drB = nodes.Get(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Link 1: Branch-C <-> DC-A (192.168.1.0/24)
    NetDeviceContainer d1 = p2p.Install(branchC, dcA);
    // Link 2: DC-A <-> DR-B (Primary) (192.168.2.0/24)
    NetDeviceContainer d2 = p2p.Install(dcA, drB);
    // Link 3: DC-A <-> DR-B (Backup) (192.168.3.0/24)
    NetDeviceContainer d3 = p2p.Install(dcA, drB);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = address.Assign(d1); // Branch-C is .1, DC-A is .2

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = address.Assign(d2); // DC-A is .1, DR-B is .2

    address.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3 = address.Assign(d3); // DC-A is .1, DR-B is .2

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- Q2: Static Routing for Failover (DC-A) ---
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> dcRouting = staticRoutingHelper.GetStaticRouting(dcA->GetObject<Ipv4>());
    
    // Primary Route to DR-B via 192.168.2.2 (Metric 0)
    dcRouting->AddNetworkRouteTo(Ipv4Address("192.168.2.0"), Ipv4Mask("255.255.255.0"), i2.GetAddress(1), 2, 0);
    // Backup Route to DR-B via 192.168.3.2 (Metric 5)
    dcRouting->AddNetworkRouteTo(Ipv4Address("192.168.2.0"), Ipv4Mask("255.255.255.0"), i3.GetAddress(1), 3, 5);

    // --- Q3: Failure Event at t=5s ---
    // Disable DC-A's interface on the primary link (index 2 corresponds to 2nd device added)
    Simulator::Schedule(Seconds(5.0), &TearDownLink, dcA, 2);

    // Application: Branch-C sends to DR-B (Target IP: 192.168.2.2)
    UdpEchoServerHelper echoServer(9);
    echoServer.Install(drB).Start(Seconds(1.0));

    UdpEchoClientHelper echoClient(i2.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.Install(branchC).Start(Seconds(2.0));

    AnimationInterface anim("scratch/router-multi-hop.xml");
    p2p.EnablePcapAll("scratch/router-multi-hop");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
