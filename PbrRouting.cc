#include "ns3/simulator.h"
#include "PbrRouting.h"
#include "ns3/ipv4-route.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"

NS_LOG_COMPONENT_DEFINE("PbrRouting");

PbrRouting::PbrRouting(Ipv4Address videoNextHop, Ipv4Address dataNextHop, 
                       uint32_t videoIfIndex, uint32_t dataIfIndex)
    : m_videoNextHop(videoNextHop), 
      m_dataNextHop(dataNextHop), 
      m_videoIfIndex(videoIfIndex), 
      m_dataIfIndex(dataIfIndex)
{
}

void PbrRouting::SetIpv4(Ptr<Ipv4> ipv4) 
{
    m_ipv4 = ipv4;
}

// Q2: The core PBR decision logic is here.
Ptr<Ipv4Route> PbrRouting::RouteOutput(Ptr<Packet> p, const Ipv4Header& header, 
                                       Ptr<NetDevice> device, Socket::SocketErrno& errno)
{
    // Define DSCP values (EF for Video, BE for Data)
    const uint8_t DSCP_VIDEO_EF = 0x2e; // 101110 (EF)
    const uint8_t DSCP_DATA_BE = 0x00;  // 000000 (BE/Default)

    // 1. Classification based on DSCP/TOS
    uint8_t dscp = header.GetDscp(); // Extract DSCP value
    
    NS_LOG_INFO("Packet Destination: " << header.GetDestination() << ", DSCP: " << (unsigned int)dscp);
    
    // 2. Decision and Route Creation
    if (dscp == DSCP_VIDEO_EF) {
        // Policy: Video traffic (EF) uses the Primary path (Net 2)
        NS_LOG_INFO("PBR: Video traffic, routing via Primary (Net 2)");
        Ptr<Ipv4Route> route = Create<Ipv4Route>();
        route->SetDestination(header.GetDestination());
        route->SetSource(m_ipv4->GetAddress(m_videoIfIndex, 0).GetLocal());
        route->SetGateway(m_videoNextHop); // Next-hop: 10.1.2.2 (DR-B's Net 2 IP)
        route->SetOutputDevice(m_ipv4->GetNetDevice(m_videoIfIndex));
        return route;
    }
    else if (dscp == DSCP_DATA_BE) {
        // Policy: Data traffic (BE) uses the Secondary path (Net 3)
        NS_LOG_INFO("PBR: Data traffic, routing via Secondary (Net 3)");
        Ptr<Ipv4Route> route = Create<Ipv4Route>();
        route->SetDestination(header.GetDestination());
        route->SetSource(m_ipv4->GetAddress(m_dataIfIndex, 0).GetLocal());
        route->SetGateway(m_dataNextHop); // Next-hop: 10.1.3.2 (DR-B's Net 3 IP)
        route->SetOutputDevice(m_ipv4->GetNetDevice(m_dataIfIndex));
        return route;
    }
    
    // Fallback: Default path for anything else (e.g., control/return traffic)
    NS_LOG_INFO("PBR: No match, using default route.");
    return m_ipv4->GetRoutingServices()->RouteOutput(p, header, device, errno); // Assuming a default route exists
}

void PbrRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    // Simplified table printing for PBR
    *stream->GetStream() << "PBR Routes (N1):\n";
    *stream->GetStream() << "  Video (DSCP EF) -> NextHop: " << m_videoNextHop << " Interface: " << m_videoIfIndex << "\n";
    *stream->GetStream() << "  Data (DSCP BE) -> NextHop: " << m_dataNextHop << " Interface: " << m_dataIfIndex << "\n";
}
