#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>  

#define SS 5
#define RST 14
#define DIO0 2

#define GPSTX 17 // TX output from ESP32 - RX input on NEO-6M
#define GPSRX 16 // RX input on ESP32 - TX output on NEO-6M
#define GPSBAUD 9600 // Baud rate for NEO-6M GPS module

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
String currentNodeName = "Node1";
String destinationNodeName = "Node4";
bool routeDiscovered = false;
String discoveredRoute = "";
String nearestNode = "";
String source = "";
String destination = "";
String route = "";

static bool ackReceived = false;
static bool finalAck = false;
static bool nearestNodeDetermined = false;

String sensorData = "";
HardwareSerial gpsSerial(2); // UART2 for GPS
TinyGPSPlus gps; // TinyGPS++ instance



void setup() {

  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa DSR Node: " + currentNodeName);

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa initialization failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  for (int i = 0; i < MAX_NODES; i++) {
    nodes[i].dataReceived = false;
    nodes[i].packetCount = 0;
    nodes[i].lastData = "";
  }

  // Initialize GPS
  gpsSerial.begin(GPSBAUD, SERIAL_8N1, GPSRX, GPSTX);
  Serial.println("GPS Initialized Successfully");

  LoRa.receive();
  Serial.println("LoRa Initialized Successfully");


}


//           <--- processPacket  Function   --> 
void processPacket(String packet, int rssi) {

    if (packet.startsWith("BREPLY:") && !nearestNodeDetermined) {
      
// Find positions using colons as delimiters    BREPLY:PacketDest:Node1:Node2(sender)

int firstColon = packet.indexOf(':');
int secondColon = packet.indexOf(':', firstColon + 1);
int thirdColon = packet.indexOf(':', secondColon + 1);
 
int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;  // Start after "PacketDest:"
int packetDestEnd = packet.indexOf(':', packetDestStart);        // End at next colon
String packetDest = packet.substring(packetDestStart, packetDestEnd);
  
  if(packetDest == currentNodeName) {   handleBeaconReply(packet,rssi);   }
  else{ return; }

    }
   
 
  if (packet.startsWith("ACK:") && !ackReceived) {
    //  Acknowledge packet format ->  ACK:PacketDest:Node1:Node2(sender)

int firstColon = packet.indexOf(':');
int secondColon = packet.indexOf(':', firstColon + 1);
int thirdColon = packet.indexOf(':', secondColon + 1);
 
int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;  // Start after "PacketDest:"
int packetDestEnd = packet.indexOf(':', packetDestStart);        // End at next colon
String packetDest = packet.substring(packetDestStart, packetDestEnd);
  
  if(packetDest==currentNodeName){  handleAck(packet);  }
  else{  return;  }
  } 

  else if (packet.startsWith("RREP:")) {

//    Route Reply packet format -> RREP:PacketDest:dest:source:destination:route

int firstColon = packet.indexOf(':');
int secondColon = packet.indexOf(':', firstColon + 1);
int thirdColon = packet.indexOf(':', secondColon + 1);
 
int packetDestStart = packet.indexOf(':', firstColon + 1) + 1;  // Start after "PacketDest:"
int packetDestEnd = packet.indexOf(':', packetDestStart);        // End at next colon
String packetDest = packet.substring(packetDestStart, packetDestEnd);
  
  if(packetDest == currentNodeName) {   handleRouteReply(packet);   }
  else{ return; }
 

  }
  else if (packet.startsWith("ACK to Source:")) {
    handleFinalAck(packet);
  } 
  else {
    updateNodeInfo(packet, packet, rssi);
  }
}


//           <--- handleAck  Function   --> 
void handleAck(String packet){
     
        ackReceived = true;
//    ACK:PacketDest:thisnodename:sendernodename
         LoRa.print("ACK:" + currentNodeName);
         String node_name = packet.substring( packet.lastIndexOf(':') + 1 );
         Serial.println("\n✅ ACK Received from " + node_name); 
       
}



//           <--- handleFinalAck  Function   --> 
void handleFinalAck(String packet){
     
    // Check if the ACK is from Node2 
         Serial.println("\n✅ ACK Received from Node2, data forwarded at destination");

        // Set the Final ACK received flag
        finalAck = true;

}



//           <--- handleRouteReply  Function   --> 
void handleRouteReply(String packet) {
 
// Acknowledge packet format -> RREP:PacketDest:dest:source:destination:route
  int firstColon = packet.indexOf(':');
  int secondColon = packet.indexOf(':', firstColon + 1);
  int thirdColon = packet.indexOf(':', secondColon + 1);
  int fourthColon = packet.indexOf(':', thirdColon + 1);
  int fifthColon = packet.indexOf(':', fourthColon + 1);

  
  
   source = packet.substring(thirdColon + 1, fourthColon);       // Extract Source
   destination = packet.substring(fourthColon + 1, fifthColon);  // Extract Destination
   route = packet.substring(fifthColon + 1);     

  Serial.println("\n📬 Route Reply Received 📬");
  Serial.println("Source: " + source);
  Serial.println("Route: " + route);

  if (currentNodeName == source ) {
    routeDiscovered = true;
    discoveredRoute = route;
    Serial.println("\n🎉 ROUTE DISCOVERED 🎉");
    Serial.println("Full Route: " + discoveredRoute);
  }
}



//           <--- sendRouteRequest  Function   --> 
void sendRouteRequest() {
  String routeRequestPacket = "RREQ:PacketDest:" + nearestNode + ":" + currentNodeName + ":" + 
                             destinationNodeName + ":" + currentNodeName + ",";
  
  LoRa.beginPacket();
  LoRa.print(routeRequestPacket);
  LoRa.endPacket();

  Serial.println("🚀 ROUTE REQUEST SENT 🚀");
  Serial.println("Source: " + currentNodeName);
  Serial.println("Destination: " + destinationNodeName);
  LoRa.receive();

}


//           <--- updateNodeInfo  Function   --> 
void updateNodeInfo(String nodeName, String data, int rssi) {
  bool nodeUpdated = false;
  for (int i = 1; i < MAX_NODES; i++) {
    if (nodes[i].name == nodeName || nodes[i].name.length() == 0) {
      nodes[i].name = nodeName;
      nodes[i].lastData = data;
      nodes[i].rssi = rssi;
      nodes[i].dataReceived = true;
      nodes[i].packetCount++;
      nodeUpdated = true;
      break;
    }
  }
}


//           <--- sendBeaconMessage  Function   --> 
void sendBeaconMessage(){

  LoRa.beginPacket();
  LoRa.print("BEACON:" + currentNodeName);
  LoRa.endPacket();
  Serial.println("BEACON msg sent successfully  "  );   //  above loop executed

}



//           <--- captureSensorData  Function   --> 
void captureSensorData(){

      // Read GPS Data
  while (gpsSerial.available() > 0) {
    char gpsData = gpsSerial.read();
    gps.encode(gpsData);
  }

  // Check if valid GPS data is available
  if (gps.location.isValid()) {
    sensorData = String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    Serial.println("✅ Valid GPS Data Captured: " + sensorData);
  } else {
    sensorData = "Dummy Sensor Data";
    Serial.println("⚠️ No Valid GPS Data, Using Dummy Data!");
  }

  // Construct Packet DATA:Route:Source:Destination:Payload
int startIdx = discoveredRoute.indexOf(currentNodeName) + currentNodeName.length() + 1;
String nextNode = discoveredRoute.substring(startIdx, discoveredRoute.indexOf(",", startIdx) == -1 ? discoveredRoute.length() : discoveredRoute.indexOf(",", startIdx));

  Serial.println("Next node is: " + nextNode);

  String dataPacket = "DATA:PacketDest:"  + nextNode + ":ROUTE:" +  discoveredRoute + ":SOURCE:" + source + ":DESTINATION:" + destination  + ":PAYLOAD:" + sensorData;

   for (int i = 0; i < 700; i += 10) {
  LoRa.beginPacket();
  LoRa.print(dataPacket);
  LoRa.endPacket();
   }
  Serial.println("📡 Data Packet Sent: " + dataPacket);

}



//           <--- handleBeaconReply  Function   --> 
void handleBeaconReply(String packet,int rssi){

//     BREPLY:PacketDest:Node1:nodethatsendedthisreply
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

    nearestNodeDetermined = true;    //  now beacon msg will not be send from loop

    Serial.print("📌 Nearest Node Selected: ");
    Serial.println(nearestNode);

    }




// Main Loop Code
void loop() {

 // In a loop, detect and decode if any packet available
 for (int i = 0; i < 700; i += 10) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        String receivedText = "";
        while (LoRa.available()) {
            receivedText += (char)LoRa.read();
        }
        
        // Process the received packet
        processPacket(receivedText, LoRa.packetRssi());      //  agar "ack" signal aya hua, to yhan se ackReceived = true ho kr aye ga
    
    }
    delay(10);
  }

     //  If Nearest Node is not determined send beacon message, to get reply. From this reply, distace is measured and nearest nod eis determined
         if (!nearestNodeDetermined) {                   
        sendBeaconMessage();
        delay(4000);
    } 

    // Only send route requests if we've received ACK Signal i.e ackReceived = false, !false = true
    if ( nearestNodeDetermined && !ackReceived && !routeDiscovered) {                   
        sendRouteRequest();
        delay(4000);
    } 

      else{
          Serial.println("Waiting for route replies " );
          delay(1000);
           }
       
    

    // capture sensor data only after route is discovered 
     if ( routeDiscovered && !finalAck ) {                   
        captureSensorData();
        delay(4000);
    }

    delay(1);
}
