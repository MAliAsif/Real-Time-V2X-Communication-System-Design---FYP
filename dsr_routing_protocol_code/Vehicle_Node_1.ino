//   Node 2

#include <SPI.h>
#include <LoRa.h>

#define SS 5
#define RST 14
#define DIO0 2

struct NodeInfo {
  String name;
  String lastData;
  int rssi;
  bool dataReceived;
  unsigned long lastUpdate;
  int packetCount;
};

const int MAX_NODES = 3;
NodeInfo nodes[MAX_NODES];
String currentNodeName = "Node2";
String destinationNodeName = "Node4";
bool routeDiscovered = false;
String discoveredRoute = "";
String nearestNode = "";
String source = "";
String destination = "";
String route = "";
static bool ackReceived = false;
static bool ackSent = false;
static bool nearestNodeDetermined = false;
static bool requestRecieved = false;
static bool requestForwarded = false;


// <--- setup Function - executed everytime the ESP32 is Reset --->
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa DSR Node: " + currentNodeName);

  // LoRa initialization
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa initialization failed!");
    while (1);
  }

  // LoRa configuration
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  // Initialize nodes array
  for (int i = 0; i < MAX_NODES; i++) {
    nodes[i].dataReceived = false;
    nodes[i].packetCount = 0;
    nodes[i].lastData = "";
  }

  // Set LoRa to receive mode
  LoRa.receive();

  Serial.println("LoRa Initialized Successfully");
}



// <--- processPacket Function --->
void processPacket(String packet, int rssi) {
  if (packet.startsWith("BEACON:") && !requestForwarded) {
    handleBeaconReply(packet);
  }

  if (packet.startsWith("RREQ:")) {
    handleRouteRequest(packet);
  }

  if (packet.startsWith("BREPLY:") && !nearestNodeDetermined) {
    int firstColon = packet.indexOf(':');
    int secondColon = packet.indexOf(':', firstColon + 1);
    int thirdColon = packet.indexOf(':', secondColon + 1);
    int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;
    int packetDestEnd = packet.indexOf(':', packetDestStart);
    String packetDest = packet.substring(packetDestStart, packetDestEnd);

    if (packetDest == currentNodeName) {
      forwardRouteRequest(packet, rssi);
    } else {
      return;
    }
  }

  if (packet.startsWith("ACK:") && !ackReceived) {
    int firstColon = packet.indexOf(':');
    int secondColon = packet.indexOf(':', firstColon + 1);
    int thirdColon = packet.indexOf(':', secondColon + 1);
    int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;
    int packetDestEnd = packet.indexOf(':', packetDestStart);
    String packetDest = packet.substring(packetDestStart, packetDestEnd);

    if (packetDest == currentNodeName) {
      handleAck(packet);
    } else {
      return;
    }
  }

  else if (packet.startsWith("RREP:")) {
    int firstColon = packet.indexOf(':');
    int secondColon = packet.indexOf(':', firstColon + 1);
    int thirdColon = packet.indexOf(':', secondColon + 1);
    int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;
    int packetDestEnd = packet.indexOf(':', packetDestStart);
    String packetDest = packet.substring(packetDestStart, packetDestEnd);

    if (packetDest == currentNodeName) {
      handleRouteReply(packet);
    } else {
      return;
    }
  }

  else if (packet.startsWith("DATA:")) {
    int firstColon = packet.indexOf(':');
    int secondColon = packet.indexOf(':', firstColon + 1);
    int thirdColon = packet.indexOf(':', secondColon + 1);
    int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;
    int packetDestEnd = packet.indexOf(':', packetDestStart);
    String packetDest = packet.substring(packetDestStart, packetDestEnd);

    if (packetDest == currentNodeName) {
      handleDataPacket(packet);
    } else {
      return;
    }
  }

  else {
    updateNodeInfo(packet, packet, rssi);
  }
}


// <--- handleBeaconReply Function --->
void handleBeaconReply(String packet) {
  String sender = packet.substring(7);
  Serial.println("Preparing to send back Beacon Reply");

  for (int i = 0; i < 700; i += 10) {
    LoRa.beginPacket();
    LoRa.print("BREPLY:PacketDest:" + sender + ":" + currentNodeName);
    LoRa.endPacket();
    delay(10);
  }

  Serial.println("BEACON sent back successfully");
  Serial.println("_______________________________________");
}


// <--- handleRouteRequest Function --->
void handleRouteRequest(String packet) {
  int firstColon = packet.indexOf(':');
  int secondColon = packet.indexOf(':', firstColon + 1);
  int thirdColon = packet.indexOf(':', secondColon + 1);
  int fourthColon = packet.indexOf(':', thirdColon + 1);
  int fifthColon = packet.indexOf(':', fourthColon + 1);

  int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;
  int packetDestEnd = packet.indexOf(':', packetDestStart);
  String packetDest = packet.substring(packetDestStart, packetDestEnd);

  source = packet.substring(thirdColon + 1, fourthColon);
  destination = packet.substring(fourthColon + 1, fifthColon);
  route = packet.substring(fifthColon + 1);

  if (packetDest != currentNodeName) {
    Serial.println("🚫 Ignoring packet: Not intended for this node.");
    return;
  }

  if (ackSent) {
    Serial.println("Discarding request since Ack is already sent!");
    return;
  }

  Serial.println("\n📨 Route Request Received 📨");
  Serial.println("Source: " + source);
  Serial.println("Destination: " + destination);
  Serial.println("Route: " + route);
  requestRecieved = true;

  if (currentNodeName != source && currentNodeName != destination && 
      route.indexOf(currentNodeName) == -1) {
    route = route + currentNodeName + ",";
    sendBackAck(route);
  }
}


// <--- forwardRouteRequest Function --->
void forwardRouteRequest(String packet, int rssi) {
  String nodeName = packet.substring(packet.lastIndexOf(':') + 1);
  float minDistance = pow(10, (-40 - rssi) / (10.0 * 2));

  Serial.print("Received Beacon Reply from: ");
  Serial.print(nodeName);
  Serial.print(" | RSSI: ");
  Serial.print(rssi);
  Serial.print(" | Distance: ");
  Serial.println(minDistance);

  String closestNode = nodeName;

  for (int i = 0; i < 2000; i += 10) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String receivedText = "";
      while (LoRa.available()) {
        receivedText += (char)LoRa.read();
      }

      if (receivedText.startsWith("BREPLY:")) {
        String newNodeName = receivedText.substring(packet.lastIndexOf(':') + 1);
        int newRssi = LoRa.packetRssi();
        float newDistance = pow(10, (-40 - newRssi) / (10.0 * 2));

        if (newDistance < minDistance) {
          minDistance = newDistance;
          closestNode = newNodeName;
        }
      }
    }
    delay(10);
  }

  nearestNode = closestNode;
  nearestNodeDetermined = true;
  Serial.print("📌 Nearest Node Selected: ");
  Serial.println(nearestNode);

  for (int i = 0; i < 700; i += 10) {
    LoRa.beginPacket();
    LoRa.print("RREQ:PacketDest:" + nearestNode + ":" + source + ":" + destination + ":" + route);
    LoRa.endPacket();
    delay(10);
  }

  Serial.println("Forwarded Route Request");
  requestForwarded = true;
}


// <--- handleAck Function --->
void handleAck(String packet) {
  ackReceived = true;
  LoRa.print("ACK:" + currentNodeName);
  String node_name = packet.substring(packet.lastIndexOf(':') + 1);
  Serial.println("\n✅ ACK Received from " + node_name);
}

// <--- handleRouteReply Function --->
void handleRouteReply(String packet) {

  // RREP:PacketDest:dest:source:destination:route
  int firstColon = packet.indexOf(':');
  int secondColon = packet.indexOf(':', firstColon + 1);
  int thirdColon = packet.indexOf(':', secondColon + 1);
  int fourthColon = packet.indexOf(':', thirdColon + 1);
  int fifthColon = packet.indexOf(':', fourthColon + 1);

  source = packet.substring(thirdColon + 1, fourthColon);       // Extract Source
  destination = packet.substring(fourthColon + 1, fifthColon);  // Extract Destination
  route = packet.substring(fifthColon + 1);                     // Extract Route

  Serial.println("\n📬 Route Reply Received 📬");
  Serial.println("Source: " + source);
  Serial.println("Route: " + route);

  if (currentNodeName != source) {
    forwardRouteReply(source, destination, route);
  }
}


// <--- forwardRouteReply Function --->
void forwardRouteReply(String source, String destination, String route) {

  String prevNode = route.substring(route.lastIndexOf(",", route.lastIndexOf("," + currentNodeName) - 1) + 1, route.lastIndexOf("," + currentNodeName));

  Serial.println("Generating Route Reply");

  for (int i = 0; i < 700; i += 10) { 
    LoRa.beginPacket();
    LoRa.print("RREP:PacketDest:" + prevNode + ":" + source + ":" + destination + ":" + route);
    LoRa.endPacket();
    delay(10);
  }

  Serial.println("\n Forwarded Route Reply from intermediate node to " + prevNode);
}


// <--- handleDataPacket Function --->
void handleDataPacket(String packet) {
  // Expected format:
  // DATA:PacketDest:Node2:ROUTE:Node1,Node2,Node3,Node4:SOURCE:Node1:DESTINATION:Node4:PAYLOAD:

  // Extract Next Node
  int startIdx = route.indexOf(currentNodeName) + currentNodeName.length() + 1;
  int nextComma = route.indexOf(",", startIdx);
  String nextNode = (nextComma == -1) ? route.substring(startIdx) : route.substring(startIdx, nextComma);

  Serial.println("Next node is: " + nextNode);

  // Correctly replace only PacketDest (without modifying the rest of the route)
  int packetDestStart = packet.indexOf("PacketDest:") + 11;
  int packetDestEnd = packet.indexOf(":", packetDestStart);
  packet = packet.substring(0, packetDestStart) + nextNode + packet.substring(packetDestEnd);  // Only update PacketDest
  Serial.println("Packet is: " + packet);

  // Extract Destination
  int destStart = packet.indexOf(":DESTINATION:") + 12;
  int destEnd = packet.indexOf(":", destStart);
  String extractedDestination = packet.substring(destStart, destEnd);

  // Forward if the destination is not the current node
  if (extractedDestination != currentNodeName) {
    Serial.println("📩 Received Data Packet - attempting to forward");
    for (int i = 0; i < 2000; i += 10) { 
      LoRa.beginPacket();
      LoRa.print(packet);
      LoRa.endPacket();
    }
    Serial.println("✅ Data Packet Forwarded Successfully");
    Serial.println("______________________________________");
  }
}


// <--- sendBackAck Function --->
void sendBackAck(String ackTO) {
  // ackTO = route, passed from handleRouteReq func. route = Node1,Node2

  int pos = route.lastIndexOf("," + currentNodeName); // Find position of currentNodeName in route
  String previousNode = route.substring(route.lastIndexOf(",", pos - 1) + 1, pos);  
  Serial.println("Previous Node: " + previousNode + ":" + currentNodeName);

  Serial.println("Preparing to send back Ack________ ");

  for (int i = 0; i < 2000; i += 10) {
    LoRa.beginPacket();
    LoRa.print("ACK:PacketDest:" + previousNode + ":" + currentNodeName);
    LoRa.endPacket();
    delay(10);
  }

  Serial.println("ACK sent back successfully");
  Serial.println("_______________________________________");
}


// <--- sendFinalAck Function --->
void sendFinalAck() {
  Serial.println("Preparing to send back final Ack________ ");

  for (int i = 0; i < 2000; i += 10) {
    LoRa.beginPacket();
    LoRa.print("ACK to Source:" + currentNodeName);
    LoRa.endPacket();
    delay(10);
  }

  Serial.println("Final ACK sent back");
  Serial.println("_______________________________________");
}


// <--- updateNodeInfo Function --->
void updateNodeInfo(String nodeName, String data, int rssi) {
  unsigned long currentTime = millis();
  bool nodeUpdated = false;

  for (int i = 1; i < MAX_NODES; i++) {
    if (nodes[i].name == nodeName || nodes[i].name.length() == 0) {
      nodes[i].name = nodeName;
      nodes[i].lastData = data;
      nodes[i].rssi = rssi;
      nodes[i].lastUpdate = currentTime;
      nodes[i].dataReceived = true;
      nodes[i].packetCount++;
      nodeUpdated = true;
      break;
    }
  }
}


// <--- sendBeaconMessage Function --->
void sendBeaconMessage() {
  LoRa.beginPacket();
  LoRa.print("BEACON:" + currentNodeName);
  LoRa.endPacket();
  Serial.println("BEACON msg sent successfully");
}


// Main Loop function
void loop() {
  // Check if a packet is received
 for (int i = 0; i < 700; i += 10) { // Loop for approximately 4 seconds
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String receivedText = "";
    while (LoRa.available()) {
      receivedText += (char)LoRa.read();
    }
    // Process the received packet
    processPacket(receivedText, LoRa.packetRssi());
   // break;   Exit the loop if a packet is successfully received
  }
  delay(10); // Small delay to maintain receive mode and reduce CPU load
}
       Serial.println("Waiting for the packets");
      // delay(2000);
 
if(requestRecieved && !nearestNodeDetermined){
  sendBeaconMessage();
  delay(4000);
}

  // Minimal delay to prevent CPU hogging
  delay(1);
}


