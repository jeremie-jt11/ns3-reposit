/*
 * Cisco Packet Tracer Topology Replication in ns-3
 * 
 * Topology:
 * 
 *                    Cloud0 (ISP Gateway)
 *                         |
 *                         |
 *                    Router1 (2911)
 *                         |
 *                         |
 *                    Switch0 (2960)
 *                    /    |    \
 *                   /     |     \
 *              PC0  PC1  PC2  PC3  PC4
 *           (LAN: 192.168.1.0/24)
 * 
 *                    Server0 (Remote)
 *                         |
 *                    Cloud0 -------- Router1
 *                  (Internet Cloud)
 * 
 * Features:
 * - Local LAN with 5 PCs connected via switch
 * - Router connecting LAN to Internet/Cloud
 * - Remote server accessible via cloud
 * - UDP/TCP traffic between PCs and server
 * - ICMP ping tests
 * - NetAnim visualization
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CiscoTopologySimulation");

int main(int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 30.0;
    uint32_t numPcs = 5;
    bool verbose = true;
    bool enablePcap = true;
    bool enableNetAnim = true;
    
    // Network parameters
    std::string lanDataRate = "1Gbps";
    std::string wanDataRate = "100Mbps";
    uint32_t wanDelayMs = 20;  // Internet delay
    
    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("netanim", "Enable NetAnim output", enableNetAnim);
    cmd.Parse(argc, argv);
    
    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }
    
    NS_LOG_INFO("Creating Cisco Packet Tracer Topology");
    
    // ========== Create Nodes ==========
    
    // LAN PCs
    NodeContainer lanPcs;
    lanPcs.Create(numPcs);
    
    // Network devices
    Ptr<Node> switchNode = CreateObject<Node>();  // Switch0 (2960)
    Ptr<Node> routerNode = CreateObject<Node>();  // Router1 (2911)
    Ptr<Node> cloudNode = CreateObject<Node>();   // Cloud0 (ISP Gateway)
    Ptr<Node> serverNode = CreateObject<Node>();  // Server0 (Remote)
    
    NS_LOG_INFO("Created " << numPcs << " PCs, 1 switch, 1 router, 1 cloud, 1 server");
    
    // ========== Create LAN (CSMA for Switch) ==========
    
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(lanDataRate));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    
    // Connect PCs and router to switch
    NodeContainer lanNodes;
    lanNodes.Add(switchNode);  // Switch is first node
    lanNodes.Add(lanPcs);      // Add all PCs
    lanNodes.Add(routerNode);  // Add router to LAN
    
    NetDeviceContainer lanDevices = csma.Install(lanNodes);
    
    NS_LOG_INFO("Created LAN with switch connecting " << numPcs << " PCs and router");
    
    // ========== Create WAN Links (Point-to-Point) ==========
    
    PointToPointHelper p2pWan;
    p2pWan.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
    p2pWan.SetChannelAttribute("Delay", TimeValue(MilliSeconds(wanDelayMs)));
    
    // Router to Cloud
    NodeContainer routerToCloud;
    routerToCloud.Add(routerNode);
    routerToCloud.Add(cloudNode);
    NetDeviceContainer routerCloudDevices = p2pWan.Install(routerToCloud);
    
    // Cloud to Server
    NodeContainer cloudToServer;
    cloudToServer.Add(cloudNode);
    cloudToServer.Add(serverNode);
    NetDeviceContainer cloudServerDevices = p2pWan.Install(cloudToServer);
    
    NS_LOG_INFO("Created WAN links: Router-Cloud-Server");
    
    // ========== Install Internet Stack ==========
    
    InternetStackHelper stack;
    stack.Install(lanPcs);
    stack.Install(switchNode);
    stack.Install(routerNode);
    stack.Install(cloudNode);
    stack.Install(serverNode);
    
    NS_LOG_INFO("Installed Internet stack on all nodes");
    
    // ========== Assign IP Addresses ==========
    
    Ipv4AddressHelper address;
    
    // LAN: 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);
    
    // Router to Cloud: 203.0.113.0/30
    address.SetBase("203.0.113.0", "255.255.255.252");
    Ipv4InterfaceContainer routerCloudInterfaces = address.Assign(routerCloudDevices);
    
    // Cloud to Server: 198.51.100.0/30
    address.SetBase("198.51.100.0", "255.255.255.252");
    Ipv4InterfaceContainer cloudServerInterfaces = address.Assign(cloudServerDevices);
    
    NS_LOG_INFO("Assigned IP addresses to all interfaces");
    
    // ========== Enable Global Routing ==========
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    NS_LOG_INFO("Populated global routing tables");
    
    // ========== Create Applications ==========
    
    // UDP Echo Server on remote server
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(serverNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));
    
    // UDP Echo Clients from different PCs
    
    // PC0 -> Server
    UdpEchoClientHelper echoClient1(cloudServerInterfaces.GetAddress(1), echoPort);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps1 = echoClient1.Install(lanPcs.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simTime));
    
    // PC1 -> Server
    UdpEchoClientHelper echoClient2(cloudServerInterfaces.GetAddress(1), echoPort);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));
    
    ApplicationContainer clientApps2 = echoClient2.Install(lanPcs.Get(1));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(simTime));
    
    // PC2 -> Server
    UdpEchoClientHelper echoClient3(cloudServerInterfaces.GetAddress(1), echoPort);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(30));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.5)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(768));
    
    ApplicationContainer clientApps3 = echoClient3.Install(lanPcs.Get(2));
    clientApps3.Start(Seconds(2.5));
    clientApps3.Stop(Seconds(simTime));
    
    NS_LOG_INFO("Installed UDP Echo applications (PCs -> Server)");
    
    // ========== Configure NetAnim ==========
    
    if (enableNetAnim)
    {
        // Set node positions for visualization
        
        // LAN PCs - Bottom in a row
        AnimationInterface::SetConstantPosition(lanPcs.Get(0), 10, 80);  // PC0
        AnimationInterface::SetConstantPosition(lanPcs.Get(1), 25, 80);  // PC1
        AnimationInterface::SetConstantPosition(lanPcs.Get(2), 40, 80);  // PC2
        AnimationInterface::SetConstantPosition(lanPcs.Get(3), 55, 80);  // PC3
        AnimationInterface::SetConstantPosition(lanPcs.Get(4), 70, 80);  // PC4
        
        // Switch - Center, above PCs
        AnimationInterface::SetConstantPosition(switchNode, 40, 60);
        
        // Router - Above switch
        AnimationInterface::SetConstantPosition(routerNode, 40, 40);
        
        // Cloud - Above router
        AnimationInterface::SetConstantPosition(cloudNode, 40, 20);
        
        // Server - Top right
        AnimationInterface::SetConstantPosition(serverNode, 70, 10);
    }
    
    // ========== Enable Tracing ==========
    
    if (enablePcap)
    {
        // PCAP on WAN links
        p2pWan.EnablePcap("router-cloud", routerCloudDevices.Get(0), true);
        p2pWan.EnablePcap("cloud-server", cloudServerDevices.Get(0), true);
        
        // PCAP on LAN (selected PCs)
        csma.EnablePcap("lan-pc0", lanDevices.Get(1), true);  // PC0
        csma.EnablePcap("lan-pc1", lanDevices.Get(2), true);  // PC1
        
        NS_LOG_INFO("PCAP tracing enabled");
    }
    
    // ASCII tracing
    AsciiTraceHelper ascii;
    p2pWan.EnableAsciiAll(ascii.CreateFileStream("cisco-topology.tr"));
    
    // ========== Print Routing Tables ==========
    
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("cisco-routing.txt", std::ios::out);
    Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt(Seconds(1.0), routingStream);
    
    // ========== Print Network Information ==========
    
    std::cout << "\n========================================\n";
    std::cout << "  Cisco Packet Tracer Topology\n";
    std::cout << "========================================\n\n";
    
    std::cout << "LAN Network (192.168.1.0/24):\n";
    std::cout << "  Switch0: " << lanInterfaces.GetAddress(0) << "\n";
    for (uint32_t i = 0; i < numPcs; ++i)
    {
        std::cout << "  PC" << i << ": " << lanInterfaces.GetAddress(i + 1) << "\n";
    }
    std::cout << "  Router1 (LAN interface): " << lanInterfaces.GetAddress(numPcs + 1) << "\n";
    
    std::cout << "\nWAN Links:\n";
    std::cout << "  Router1 (WAN): " << routerCloudInterfaces.GetAddress(0) << "\n";
    std::cout << "  Cloud0: " << routerCloudInterfaces.GetAddress(1) << "\n";
    std::cout << "  Cloud0 (Server side): " << cloudServerInterfaces.GetAddress(0) << "\n";
    std::cout << "  Server0: " << cloudServerInterfaces.GetAddress(1) << "\n";
    
    std::cout << "\nApplications:\n";
    std::cout << "  PC0 -> Server0 (UDP Echo, 1s interval, 1024 bytes)\n";
    std::cout << "  PC1 -> Server0 (UDP Echo, 2s interval, 512 bytes)\n";
    std::cout << "  PC2 -> Server0 (UDP Echo, 1.5s interval, 768 bytes)\n";
    
    std::cout << "\nSimulation Duration: " << simTime << " seconds\n";
    std::cout << "========================================\n\n";
    
    // ========== Create NetAnim Animation ==========
    
    AnimationInterface* anim = nullptr;
    if (enableNetAnim)
    {
        anim = new AnimationInterface("cisco-topology-animation.xml");
        
        // Set node descriptions
        anim->UpdateNodeDescription(switchNode, "Switch0-2960");
        anim->UpdateNodeDescription(routerNode, "Router1-2911");
        anim->UpdateNodeDescription(cloudNode, "Cloud0-ISP");
        anim->UpdateNodeDescription(serverNode, "Server0");
        
        for (uint32_t i = 0; i < numPcs; ++i)
        {
            anim->UpdateNodeDescription(lanPcs.Get(i), "PC" + std::to_string(i));
        }
        
        // Set node colors
        // LAN PCs - Light Blue
        for (uint32_t i = 0; i < numPcs; ++i)
        {
            anim->UpdateNodeColor(lanPcs.Get(i), 135, 206, 250);  // Sky blue
        }
        
        // Switch - Green
        anim->UpdateNodeColor(switchNode, 50, 205, 50);  // Lime green
        
        // Router - Blue
        anim->UpdateNodeColor(routerNode, 0, 0, 255);
        
        // Cloud - Gray
        anim->UpdateNodeColor(cloudNode, 169, 169, 169);  // Dark gray
        
        // Server - Red
        anim->UpdateNodeColor(serverNode, 220, 20, 60);  // Crimson
        
        // Set node sizes
        anim->UpdateNodeSize(switchNode->GetId(), 4.0, 4.0);
        anim->UpdateNodeSize(routerNode->GetId(), 3.5, 3.5);
        anim->UpdateNodeSize(cloudNode->GetId(), 5.0, 5.0);
        anim->UpdateNodeSize(serverNode->GetId(), 3.0, 3.0);
        
        for (uint32_t i = 0; i < numPcs; ++i)
        {
            anim->UpdateNodeSize(lanPcs.Get(i)->GetId(), 2.5, 2.5);
        }
        
        // Enable packet metadata
        anim->EnablePacketMetadata(true);
        
        NS_LOG_INFO("NetAnim animation configured");
    }
    
    // ========== Run Simulation ==========
    
    NS_LOG_INFO("Starting simulation...");
    
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    
    NS_LOG_INFO("Simulation completed");
    
    Simulator::Destroy();
    
    if (anim != nullptr)
    {
        delete anim;
    }
    
    std::cout << "\n========================================\n";
    std::cout << "  Simulation Complete!\n";
    std::cout << "========================================\n";
    std::cout << "Output files generated:\n";
    std::cout << "  - router-cloud-*.pcap (Router-Cloud traffic)\n";
    std::cout << "  - cloud-server-*.pcap (Cloud-Server traffic)\n";
    std::cout << "  - lan-pc*.pcap (LAN PC traffic)\n";
    std::cout << "  - cisco-topology.tr (ASCII trace)\n";
    std::cout << "  - cisco-routing.txt (Routing tables)\n";
    if (enableNetAnim)
    {
        std::cout << "  - cisco-topology-animation.xml (NetAnim file)\n";
    }
    std::cout << "\nTo view animation:\n";
    std::cout << "  ~/netanim/build/netanim cisco-topology-animation.xml\n";
    std::cout << "========================================\n\n";
    
    return 0;
}
