#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

/*
			Example Topology: 2 levels, each node connected to 2 leaves per level.

                                Client
                            +------------+
                            |    Root    |
                            +------------+
                            /            \
                           /              \
                +-------------+           +-------------+
                | leftRouther |           | rightRouter |
                +-------------+	          +-------------+
                  /        \                  /       \
                 /          \                /         \
            +----+       +----+            +----+       +----+
            | n1 |       | n2 |            | n3 |       | n4 |
            +----+       +----+            +----+       +----+
            Server       Server            Server       Server
 */

/**
 *  Function to generate network topology as shown above, with an arbitrary number of
 *  levels or leave nodes. This function is recursive.
 *
 *  Ptr<Node> parent is the node to the network topology, it is equivalent to the motherNode
 *  illustrated above
 *
 *  int numLeaves is the number of leaves each parent node should be connected with.
 *  Ipv4InterfaceContainer* ipInterfaces is a variable to keep track of the server nodes addresses
 *  (explained more in the main function)
 *
 *  int level is the level of the network topology, level = 1 would be a parent node connected with
 *  numLeaves
 */
void networkTree(Ptr<Node> parent, int numLeaves, Ipv4InterfaceContainer* ipInterfaces, int level);

/**
 *  Function to install a UDP server application on each server node that echo's back the
 *  packet it receives.
 *
 *  NodeContainer* leaves is a reference to the nodes to install the server application onto.
 *
 *  int port is the port number which all server nodes listen to.
 *
 *  float start, end is the start and end of the application
 */
void installUdpEchoServers(NodeContainer* leaves, int port, float start, float end);

/**
 *  Function to install several UDP client applications to send to all the server nodes
 *  and expect a echo packet reply
 *
 *  Ptr<Node> node, node to intall the several UDP client apps onto
 *
 *  int port is the port number the server nodes are supposed to listen to
 *
 *  Ipv4InterfaceContainer* ipInterfaces is the variable that contains all the addresses
 *  of the server nodes, and is used for the client app to send a packet to them
 *
 *  float start, end is the start and end of the application
 */
void installUdpEchoClient(Ptr<Node> node, int port, Ipv4InterfaceContainer* ipInterfaces,
                          float start, float end);

// Since this code uses recursion, using a global variable to specify a branch was useful
static int branch = 1;


NS_LOG_COMPONENT_DEFINE ("networkTree"); // Naming this script to enable logging (debugging)

int main (int argc, char *argv[])
{
  LogComponentEnable ("networkTree", LOG_LEVEL_INFO); // Enable logging or debugging at the info level

  NS_LOG_INFO ("Testing"); // Code reached here, should output "testing" on the shell

  // We need to log packet info of client node, which contains a UDP application
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);

  // uncomment line below to log server applications listening to packets and echoing them back
  //LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // There is a lot of congestion in this network topology, we need to increase the buffer size,
  // otherwise packets will be dropped, we need to do this at the IP layer and the link layer,
  // below increases buffer size to 1000 at the IP layer, as in, 1000 packets can be queued up
  Config::SetDefault("ns3::ArpCache::PendingQueueSize", UintegerValue(1000));

  Ptr<Node> client = CreateObject<Node> ();

  InternetStackHelper stack;
  stack.Install (client);

  // We need to keep track of the IP addresses of the server nodes for the client to send
  // packets to them, this can be done using a Ipv4InterfaceContainer var. The var ipInterfaces
  // will be used to contain all the IP addresses of the server nodes.
  Ipv4InterfaceContainer ipInterfaces;

  // Generate the topology with connections and IPv4 addresses
  // here, each node has 3 leaves, and it is 2 levels long, so there should be 3*2 = 6 server nodes
  // at the bottom, modify them to create the appropriate topology
  networkTree(client, 3, &ipInterfaces, 2);

  // Install the UDP application on the client node and have these applications send a packet to
  // all the server nodes
  installUdpEchoClient(client, 9, &ipInterfaces, 2.0, 2000.0);

  // Since this is dynamic routing and with a large network topology, populating the routing tables
  // can take quite a long time. To simulate topology with 2 levels and 32 leaves at each level,
  // there would be 32*32 = 1024 server nodes, it takes about 30 minutes to populate the tables.
  NS_LOG_INFO ("Populating table");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  NS_LOG_INFO ("Populating table done");

  Simulator::Stop (Seconds (200));
  NS_LOG_INFO ("Simulation begins now");
  Simulator::Run ();
  NS_LOG_INFO ("Simulation ends");
  Simulator::Destroy ();
  return 0;
}

void networkTree(Ptr<Node> parent, int numLeaves, Ipv4InterfaceContainer* ipInterfaces, int level) {
  if (level > 0) { // Base case, only recursively create more connections if level > 0
    // Create the nodes to be connected as leaves
    NodeContainer leaves;
    leaves.Create(numLeaves);

    // Create the net devices on the nodes and a network channel connecting them
    // according to the topology
    // Create the variable to help create the net devices and connect nodes to channels
    CsmaHelper csma;
    // Increase the buffer size at the link layer
    csma.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));
    // Set the typical Data Centre standard values
    csma.SetChannelAttribute ("DataRate", StringValue ("1Gbps"));
    csma.SetChannelAttribute ("Delay", StringValue ("1ms"));

    // Connect the parent node to its leave nodes
    std::vector<NetDeviceContainer> netC; // save them to assign IP addresses
    for (int leaf = 0; leaf < leaves.GetN(); leaf++) {
      netC.push_back( csma.Install( NodeContainer( parent, leaves.Get(leaf) ) ) );
    }

    // Set up the IP addresses to the leaves
    InternetStackHelper stack;
    stack.Install (leaves);
    // Make sure level == 1 to ensure server nodes are installed at the bottom of the topology
    if (level == 1) installUdpEchoServers(&leaves, 9, 1.0, 2000.0);

    // Assign IP addresses to the leaves
    char buffer [15];
    Ipv4AddressHelper address;
    for (int netDev = 0; netDev < netC.size(); netDev++) {
      // Specify the address using string formating
      sprintf (buffer, "%d.%d.%d.0", 9 + level, branch, netDev + 1);
      address.SetBase (buffer, "255.255.255.0");
      Ipv4InterfaceContainer tempContainer = address.Assign( netC.at(netDev) );

      // Make sure we only obtain the addresses of the leaves nodes at the bottom of the topology
      if (level == 1) ipInterfaces->Add(tempContainer);

      // Recursion, connect each leaf to more nodes
      int leaf = netDev;
      networkTree(leaves.Get(leaf), numLeaves, ipInterfaces, level - 1);
    }
    branch++; // next branch in topology
  }
}

void installUdpEchoServers(NodeContainer* leaves, int port, float start, float end) {
  for (int leaf = 0; leaf < leaves->GetN(); leaf++) {
    Ptr<UdpEchoServer> serverApp = CreateObject<UdpEchoServer>();
    serverApp->SetAttribute("Port", UintegerValue(port)); // server apps listen to this port

    leaves->Get(leaf)->AddApplication(serverApp);

    serverApp->SetStartTime (Seconds (start));
    serverApp->SetStopTime (Seconds (end));
  }
}

void installUdpEchoClient(Ptr<Node> node, int port, Ipv4InterfaceContainer* ipInterfaces,
                          float start, float end) {
  // ipInterfaces contains the address of the net device of the server node and the
  // address of the net device connected to the server node (parent node).
  // In order to obtain only the server node addresses, the first address is parent
  // second is server, so start with ip = 1, and incumbent twice to obtain only server addresses
  for (int ip = 1; ip < ipInterfaces->GetN(); ip+=2) {
    Ptr<UdpEchoClient> echoClient = CreateObject<UdpEchoClient>();

    echoClient->SetRemote(ipInterfaces->GetAddress(ip), port);

    echoClient->SetAttribute ("MaxPackets", UintegerValue (1)); // send only 1 packet
    echoClient->SetAttribute ("PacketSize", UintegerValue (1 << 10)); // 1 KB
    node->AddApplication(echoClient);
    // Start each application and have each send a packet with a delay of 100 micro seconds
    int delay = 10000; // in terms of seconds, so if delay = 1 ms, it is 1000, or 1000th of a second
    echoClient->SetStartTime (Seconds (start + (ip - 1.0)/(2*delay) )); // formula to create delay using ip
    echoClient->SetStopTime (Seconds (end));
  }
}