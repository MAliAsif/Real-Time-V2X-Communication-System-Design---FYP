//   Node 4

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
String currentNodeName = "Node4";
//String destinationNodeName = "Node4";
bool routeDiscovered = false;
String discoveredRoute = "";
String nearestNode = "";
String source = "";
String destination = "";
String route = "";
	
static bool ackSent = false;
static bool nearestNodeDetermined = false;
static bool requestRecieved = false;



// Setup Function - executed once  
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

  if (packet.startsWith("BEACON:")) {
    handleBeaconReply(packet);
  } 
  if (packet.startsWith("RREQ:")) {
    handleRouteRequest(packet);
  }
  // if (packet.startsWith("BREPLY:") && !nearestNodeDetermined) {
  //   forwardRouteRequest(packet,rssi);
  // }
  
  // else if (packet.startsWith("RREP:")) {
  //   handleRouteReply(packet);
  // }
  else if (packet.startsWith("DATA:")) {

int firstColon = packet.indexOf(':');
int secondColon = packet.indexOf(':', firstColon + 1);
int thirdColon = packet.indexOf(':', secondColon + 1);
 
int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;  // Start after "PacketDest:"
int packetDestEnd = packet.indexOf(':', packetDestStart);        // End at next colon
String packetDest = packet.substring(packetDestStart, packetDestEnd);
  
  if(packetDest == currentNodeName) {   handleDataPacket(packet);   }
  else{ return; }

   }  
  else {
    updateNodeInfo(packet, packet, rssi);
  }

}



// <--- handleBeaconReply Function --->
void handleBeaconReply(String packet){
  
//  extracting the sender of beacon BEACON:Node1

  String sender = packet.substring(7);
  Serial.println("Preparing to send back Beacon Reply "  );  // printing to note execution time of belo loop
for (int i = 0; i < 700; i += 10) { // Send the ACK 5 times
    LoRa.beginPacket();
    LoRa.print("BREPLY:PacketDest:" + sender + ":" + currentNodeName);
    LoRa.endPacket();
    delay(10);  // Add a small delay between transmissions
  }

  Serial.println("BEACON sent back successfully  "  );   //  above loop executed
  Serial.println("_______________________________________ "  );

}



// <--- sendBackAck Function --->
void sendBackAck(String ackTO) {
//     ackTO = route, passed from handleRouteReq func. route = Node1,Node2

int pos = route.lastIndexOf("," + currentNodeName); // Find position of currentNodeName in route
String previousNode = route.substring(route.lastIndexOf(",", pos - 1) + 1, pos);  
Serial.println("Previous Node: " + previousNode);

  Serial.println("Preparing to send back Ack________ "  );  // printing to note execution time of belo loop
for (int i = 0; i < 2000; i += 10) { // Send the ACK 5 times
    LoRa.beginPacket();
    LoRa.print("ACK:PacketDest:" + previousNode);
    LoRa.endPacket();
    delay(10);  // Add a small delay between transmissions
  }

  Serial.println("ACK sent back successfully  "  );   //  above loop executed
  Serial.println("_______________________________________ "  );

}



// <--- handleRouteRequest Function --->
void handleRouteRequest(String packet) {
 
 //  coming requuest as  RREQ:PacketDest:Node2:Node1:Node4:Node1,

// Find positions using colons as delimiters
int firstColon = packet.indexOf(':');
int secondColon = packet.indexOf(':', firstColon + 1);
int thirdColon = packet.indexOf(':', secondColon + 1);
int fourthColon = packet.indexOf(':', thirdColon + 1);
int fifthColon = packet.indexOf(':', fourthColon + 1);

int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;  // Start after "PacketDest:"
int packetDestEnd = packet.indexOf(':', packetDestStart);        // End at next colon
String packetDest = packet.substring(packetDestStart, packetDestEnd);

  source = packet.substring(thirdColon + 1, fourthColon);    // Extract source (third segment after RREQ:PacketDest:)
  destination = packet.substring(fourthColon + 1, fifthColon);    // Extract destination (fourth segment)
  route = packet.substring(fifthColon+1);      // Extract remaining route

    // Ignore the packet if it is not intended for this node
    if (packetDest != currentNodeName) {
        Serial.println("🚫 Ignoring packet: Not intended for this node.");
        return;
    }

  if(ackSent)
  {   
    Serial.println(" Discarding request since Ack is already sent! ");
    //sendBackAck();    //  agar node 1 ne 1st time ack signal recieve nhi kya, or wo dobara request kr rhas, to aik bar phir se Ack signal bhej dya
    return;
  }

  Serial.println("\n📨 Route Request Received 📨");
  Serial.println("Source: " + source);
  Serial.println("Destination: " + destination);
  Serial.println("Route: " + route);

    requestRecieved = true;

  // Forward if not already in route
  if (currentNodeName != source &&
      route.indexOf(currentNodeName) == -1) {
    route = route + currentNodeName;
    sendBackAck(route);     
    

  }

 forwardRouteReply(source,destination,route);

}



// <--- forwardRouteReply Function --->
void forwardRouteReply(String source, String destination, String route) {
  
// route = Node1,Node2,Node3,Node4
String prevNode = route.substring(route.lastIndexOf(",", route.lastIndexOf("," + currentNodeName) - 1) + 1, route.lastIndexOf("," + currentNodeName));

  
 for (int i = 0; i < 700; i += 10) { 
  LoRa.beginPacket();
  LoRa.print("RREP:PacketDest:" + prevNode + ":" + source + ":" + destination + ":" + route);
  LoRa.endPacket();
  delay(10);
   }
 // LoRa.receive();

  Serial.println("Generated Route Reply from Destination node to " + prevNode);
  Serial.println("_____________________________________________________");

}



// <--- sendFinalAck Function --->
void sendFinalAck() {

  Serial.println("Preparing to send back final Ack________ "  );  // printing to note execution time of belo loop
for (int i = 0; i < 2000; i += 10) { // Send the ACK 5 times
    LoRa.beginPacket();
    LoRa.print("ACK to Source:" + currentNodeName);
    LoRa.endPacket();
    delay(10);  // Add a small delay between transmissions
  }

  Serial.println("Final ACK sent back  "  );   //  above loop executed
  Serial.println("_______________________________________ "  );

}



// <--- forwardRouteRequest Function --->
void forwardRouteRequest(String packet, int rssi) {

 String nodeName = packet.substring(packet.lastIndexOf(':') + 1); // Extract node name from beacon reply
    float minDistance = pow(10, (-40 - rssi) / (10.0 * 2)); // Calculate distance for first responder

    Serial.print("Received Beacon Reply from: ");
    Serial.print(nodeName);
    Serial.print(" | RSSI: ");
    Serial.print(rssi);
    Serial.print(" | Distance: ");
    Serial.println(minDistance);

    String closestNode = nodeName; // Assume first responder is the closest

    // Wait for more replies for a fixed time (e.g., 3 seconds)
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

                Serial.print("Received Another Beacon Reply from: ");
                Serial.print(newNodeName);
                Serial.print(" | RSSI: ");
                Serial.print(newRssi);
                Serial.print(" | Distance: ");
                Serial.println(newDistance);

                // Update nearest node if new node is closer
                if (newDistance < minDistance) {
                    minDistance = newDistance;
                    closestNode = newNodeName;
                }
            }
        }
        delay(10);
    }

    // Finalize the nearest node
    nearestNode = closestNode;
    nearestNodeDetermined = true;
    Serial.print("📌 Nearest Node Selected: ");
    Serial.println(nearestNode);

//    now forward the request
for (int i = 0; i < 700; i += 10) { // Loop for approximately 4 seconds
  LoRa.beginPacket();
  LoRa.print("RREQ:PacketDest:" + nearestNode + ":" + source + ":" + destination + ":" + route);
  LoRa.endPacket();
  delay(10); // Small delay ensures a consistent loop and avoids CPU hogging
}

  // LoRa.receive();

  Serial.println("Forwarded Route Request");

}



// <--- handleDataPacket Function --->
void handleDataPacket(String packet){

//    DATA:PacketDest:Node2:ROUTE:Node1,Node2,Node3,Node4:SOURCE:Node1:DESTINATION:Node4:PAYLOAD:


  Serial.println("Data Packet Recieved successfully as follows: ✅ "   );
    Serial.println("\n______________________________________\n");
  Serial.println( packet );
  Serial.println("______________________________________");
  // sendFinalAck();

    
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


// Main Loop function
void loop() {
 
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
 
 

  // Minimal delay to prevent CPU hogging
  delay(1);
}






