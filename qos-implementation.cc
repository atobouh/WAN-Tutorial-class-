/*
 * Exercise 2: Quality of Service Implementation for Mixed Traffic
 * Implements: Traffic Differentiation (Q1), Priority Queueing (Q2), 
 * Performance Measurement (Q3), and Congestion Scenario (Q4).
 * Topology: Triangular Mesh (n0, n1, n2) | Bottleneck link is n0 <-> n2 (5Mbps).
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h" // Q2: Needed for PfifoFastQueueDisc
#include "ns3/flow-monitor-module.h"    // Q3: Needed for QoS metrics

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QoSImplementation");

const std::string LINK_DATA_RATE = "5Mbps"; // Link Capacity (Bottleneck)
const double SIMULATION_TIME = 15.0;       // Total Simulation Time

// --- Q2: Function to Configure and Install PfifoFastQueueDisc ---
void InstallQoS(Ptr<NetDevice> device)
{
    // PfifoFastQueueDisc uses three bands (0=High, 1=Medium, 2=Low).
    // We map DSCP values to these bands using a classifier.
    TrafficControlHelper tcHelper;
    tcHelper.Set<QueueDisc>("ns3::PfifoFastQueueDisc",
                           "Bands", UintegerValue(3),
                           "Limit", UintegerValue(100)); // Total queue limit of 100 packets

    // Configure Classifier: Maps DSCP values to the internal queue bands.
    Ptr<PfifoFastClassifier> classifier = CreateObject<PfifoFastClassifier>();

    // Map DSCP 46 (Expedited Forwarding - EF) to Band 0 (Highest Priority)
    // DSCP 46 is commonly used for VoIP traffic.
    classifier->AddIpv4DscpRange(46, 46, 0); 
    
    // Map DSCP 0 (Best Effort - BE) to Band 2 (Lowest Priority)
    classifier->AddIpv4DscpRange(0, 0, 2);  

    tcHelper.Set<Classifier>("ns3::PfifoFastClassifier", classifier);

    // Install the queueing discipline on the output interface of the sender (N0)
    tcHelper.Install(device);
    NS_LOG_INFO("PfifoFastQueueDisc installed on Node " << device->GetNode()->GetId());
}

int
main(int argc, char* argv[])
{
    // --- Logging & Setup ---
    LogComponentEnable("QoSImplementation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PfifoFastQueueDisc", LOG_LEVEL_INFO); // Optional: for detailed queuing logs

    // Create three nodes: n0 (HQ), n1 (Branch/Router), n2 (DC/Server)
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> n0 = nodes.Get(0); // HQ (Client/Sender)
    Ptr<Node> n2 = nodes.Get(2); // DC (Server/Receiver)

    // Configure Links (5Mbps, 2ms)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(LINK_DATA_RATE));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Topology setup, using the same links and addressing as Exercise 1
    // Link 1: n0 <-> n1 (Network 1: 10.1.1.0/24)
    NetDeviceContainer link1Devices = p2p.Install(NodeContainer(n0, nodes.Get(1)));
    // Link 2: n1 <-> n2 (Network 2: 10.1.2.0/24)
    NetDeviceContainer link2Devices = p2p.Install(NodeContainer(nodes.Get(1), n2));
    // Link 3: n0 <-> n2 (Network 3: 10.1.3.0/24) - **This is our direct WAN link**
    NetDeviceContainer link3Devices = p2p.Install(NodeContainer(n0, n2));

    // Install stacks and addresses
    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address1, address2, address3;
    address1.SetBase("10.1.1.0", "255.255.255.0").Assign(link1Devices);
    address2.SetBase("10.1.2.0", "255.255.255.0").Assign(link2Devices);
    Ipv4InterfaceContainer interfaces3 = address3.SetBase("10.1.3.0", "255.255.255.0").Assign(link3Devices);

    // --- Q2: Apply Priority Queueing ---
    // The bottleneck is the direct link (link3) on the sending node (n0).
    // The device on n0 for link3 is link3Devices.Get(0).
    InstallQoS(link3Devices.Get(0));

    // --- Q1: Traffic Differentiation Setup (Server N2) ---
    uint16_t portVoIP = 9;  // VoIP port
    uint16_t portFTP = 10; // FTP port
    Ipv4Address serverAddress = interfaces3.GetAddress(1); // N2's IP on Net 3 (10.1.3.2)
    
    // Install servers on N2 for both traffic types
    PacketSinkHelper sinkVoIP("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portVoIP));
    sinkVoIP.Install(n2).Start(Seconds(0.0));

    PacketSinkHelper sinkFTP("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portFTP));
    sinkFTP.Install(n2).Start(Seconds(0.0));

    // --- Q1 & Q4: Traffic Differentiation Setup (Client N0) ---

    // 1. Class 1: VoIP-like traffic (High Priority / DSCP 46)
    OnOffHelper onOffVoIP("ns3::UdpSocketFactory", InetSocketAddress(serverAddress, portVoIP));
    onOffVoIP.SetAttribute("PacketSize", UintegerValue(160));  
    onOffVoIP.SetAttribute("DataRate", DataRateValue(DataRate("64Kbps"))); // VoIP rate: 50 pps * 160 Bytes
    onOffVoIP.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffVoIP.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    
    // Set DSCP to 46 (EF) for high priority: 46 * 4 = 184 (or 0xb8)
    onOffVoIP.SetAttribute("ToS", UintegerValue(46 << 2)); // DSCP is bits 0-5 of ToS field
    
    ApplicationContainer clientAppsVoIP = onOffVoIP.Install(n0);
    clientAppsVoIP.Start(Seconds(3.0)); 
    clientAppsVoIP.Stop(Seconds(SIMULATION_TIME - 1.0));

    // 2. Class 2: FTP-like traffic (Best Effort / DSCP 0)
    OnOffHelper onOffFTP("ns3::UdpSocketFactory", InetSocketAddress(serverAddress, portFTP));
    onOffFTP.SetAttribute("PacketSize", UintegerValue(1500)); 
    
    // Q4: Create congestion - Set rate much higher than 5 Mbps link capacity
    onOffFTP.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps"))); 
    onOffFTP.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffFTP.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    // DSCP remains default (0) for best effort
    onOffFTP.SetAttribute("ToS", UintegerValue(0)); 
    
    ApplicationContainer clientAppsFTP = onOffFTP.Install(n0);
    clientAppsFTP.Start(Seconds(3.0)); 
    clientAppsFTP.Stop(Seconds(SIMULATION_TIME - 1.0));

    // --- Q3: Performance Measurement Setup (FlowMonitor) ---
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // --- Run Simulation ---
    Simulator::Stop(Seconds(SIMULATION_TIME));
    Simulator::Run();

    // --- Q3: Performance Measurement Collection and Analysis ---
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->Get
        
    double totalTxVoIP = 0, totalRxVoIP = 0, totalDelayVoIP = 0, totalJitterVoIP = 0;
    double totalTxFTP = 0, totalRxFTP = 0, totalDelayFTP = 0;

    std::cout << "\n=== QoS Simulation Results (With PfifoFastQueueDisc) ===\n";
    std::cout << "Bottleneck Link: 5 Mbps | Offered Load: ~10 Mbps (FTP) + 64 Kbps (VoIP)\n";
    std::cout << "--------------------------------------------------------\n";
    
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) 
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        
        // Accumulate metrics based on destination port (VoIP or FTP)
        if (t.destinationPort == portVoIP) {
            totalTxVoIP += i->second.txPackets;
            totalRxVoIP += i->second.rxPackets;
            totalDelayVoIP += i->second.delaySum.GetSeconds();
            totalJitterVoIP += i->second.jitterSum.GetSeconds();
        } else if (t.destinationPort == portFTP) {
            totalTxFTP += i->second.txPackets;
            totalRxFTP += i->second.rxPackets;
            totalDelayFTP += i->second.delaySum.GetSeconds();
        }
    }

    // --- Summary Output (Q3) ---
    std::cout << "\n--- Performance Metrics ---\n";

    // Calculate metrics for VoIP (Class 1)
    if (totalRxVoIP > 0) {
        double lossVoIP = (totalTxVoIP - totalRxVoIP) / totalTxVoIP * 100.0;
        double avgDelayVoIP = totalDelayVoIP / totalRxVoIP * 1000.0;
        double avgJitterVoIP = totalJitterVoIP / totalRxVoIP * 1000.0;

        std::cout << "VoIP (Class 1) - High Priority (EF):\n";
        std::cout << "  Packet Loss: " << std::fixed << std::setprecision(2) << lossVoIP << " % [Expected: Near 0%]\n";
        std::cout << "  Avg Latency: " << std::fixed << std::setprecision(2) << avgDelayVoIP << " ms [Expected: Low]\n";
        std::cout << "  Avg Jitter:  " << std::fixed << std::setprecision(2) << avgJitterVoIP << " ms [Expected: Low]\n";
    }

    // Calculate metrics for FTP (Class 2)
    if (totalRxFTP > 0) {
        double lossFTP = (totalTxFTP - totalRxFTP) / totalTxFTP * 100.0;
        // Calculation interval is (SIMULATION_TIME - StartTime * 2). Using 12 seconds (15 - 3)
        double throughputFTP = totalRxFTP * 1500 * 8 / (SIMULATION_TIME - 3.0) / 1000000.0; 
        double avgDelayFTP = totalDelayFTP / totalRxFTP * 1000.0;

        std::cout << "FTP (Class 2) - Best Effort (BE):\n";
        std::cout << "  Packet Loss: " << std::fixed << std::setprecision(2) << lossFTP << " % [Expected: High]\n";
        std::cout << "  Avg Latency: " << std::fixed << std::setprecision(2) << avgDelayFTP << " ms [Expected: High]\n";
        std::cout << "  Throughput:  " << std::fixed << std::setprecision(2) << throughputFTP << " Mbps [Expected: ~5 Mbps Max]\n";
    }
    
    std::cout << "========================================================\n";

    Simulator::Destroy();

    return 0;
}
