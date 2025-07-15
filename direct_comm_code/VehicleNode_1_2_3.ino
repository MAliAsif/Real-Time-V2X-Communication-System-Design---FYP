#include <SPI.h>
#include <LoRa.h>

// Define pins
#define SS 5
#define RST 14
#define DIO0 2

// Timing constants
const unsigned long SEND_INTERVAL = 10000;     // Send every 10 seconds
const unsigned long RECEIVE_DURATION = 10000;   // Listen for 5 seconds
const unsigned long NODE_TIMEOUT = 30000;      // Consider node offline after 30 seconds

// Path loss model constants
const int A = -40;  // RSSI value at 1 meter (reference RSSI)
const float n = 2.0;  // Path loss exponent

// Structure to store node information
struct NodeInfo {
  String name;
  String lastData;
  int rssi;
  bool dataReceived;
  unsigned long lastUpdate;
  int packetCount;
  float avgRSSI;
  float distance;    // Distance in meters
};

// Array to store nodes
NodeInfo nodes[3];  // 
unsigned long lastPacketSentTime = 0;  // Store the time when we last sent a packet

void setup() {
  Serial.begin(115200);  // baud rate = 115200
  while (!Serial);
  
  Serial.println("LoRa Transceiver Node");
  
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa initialization failed!");
    while (1);
  }
  
  // Set the LoRa parameters
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  
  // Initialize nodes
  for (int i = 0; i < 3; i++) {
    nodes[i].dataReceived = false;
    nodes[i].packetCount = 0;
    nodes[i].avgRSSI = 0;
    nodes[i].lastData = "";
    nodes[i].distance = 0;
  }
  
  // Set the name for this specific node (modify as needed)
  nodes[0].name = "NODE 2";
  
  Serial.println("\nLoRa Initialized Successfully");
  Serial.println("Listening for packets...");
}

// New distance calculation using RSSI path loss model
void calculateDistance(int nodeIndex) {
  int rssi = nodes[nodeIndex].rssi;
  
  // Calculate distance
  nodes[nodeIndex].distance = pow(10, ( A - rssi ) / (10.0 * n));
  
  // Limit the distance to something reasonable (e.g., 1km)
  if (nodes[nodeIndex].distance > 1000 || nodes[nodeIndex].distance <= 0) {
    nodes[nodeIndex].distance = 0; // Invalid measurement
  }
}

String determineNearestNode() {
  float minDistance = 999999;
  int nearestIndex = -1;
  String result = "No nearby nodes";
  
  // Distance comparison here for i<3 times b/w nodes[i] 
  for (int i = 0; i < 3; i++) {
    // Skip checking the current node
    if (i == 0) continue;
    
    if (nodes[i].dataReceived && nodes[i].distance > 0 && nodes[i].distance < minDistance) {
      minDistance = nodes[i].distance;
      nearestIndex = i;
    }
  }
  

  
  if (nearestIndex != -1) {
    result = nodes[nearestIndex].name;
    if (nodes[nearestIndex].distance > 0) {
      result += String(" (Distance: ") + String(nodes[nearestIndex].distance, 1) + "m)";
    } else {
      result += String(" (RSSI: ") + String(nodes[nearestIndex].rssi) + "dBm)";
    }
  }
  
  return result;
}

void updateNodeInfo(String nodeName, String data, int rssi) {
  unsigned long currentTime = millis();
  bool nodeUpdated = false;
  
  // First try to update existing node
  for (int i = 1; i < 3; i++) {  // Start from index 1 to skip the current node
    
    //  1 node k lye 1 dfa hi run ho gi below if statement
//  nodes[i].name == nodeName  this will ensure k jo nodename pass hua, sirf usi ki info update ho. 
//  for e-g a nodename is passed. for i=1 , nodes[i].name == nodeName  will be true, node info updated.
// for i = 2, the if condition, nodes[2].name == nodeName , is false so no updation. Same for i=3.
//   nodes[i].name.length() == 0 , just simply checks if this node is stored early or not 
   
    if (nodes[i].name == nodeName || nodes[i].name.length() == 0) {
      nodes[i].name = nodeName;
      nodes[i].lastData = data;
      nodes[i].rssi = rssi;
      nodes[i].lastUpdate = currentTime;
      nodes[i].dataReceived = true;
      nodes[i].packetCount++;
      
      // Update average RSSI
      if (nodes[i].packetCount > 1) {
        nodes[i].avgRSSI = ((nodes[i].avgRSSI * (nodes[i].packetCount - 1)) + rssi) / nodes[i].packetCount;
      } else {
        nodes[i].avgRSSI = rssi;
      }
      
      // Passing the node index in cal dist function.
      calculateDistance(i);
      
      nodeUpdated = true;
      break;
    }
  }
  
  if (!nodeUpdated) {
    Serial.println("Could not update node information - array full");
  }
}

void displayNodeStatus() {
  Serial.println("\n====== Current NODE STATE Information ======");
  Serial.println("Current Node: " + nodes[0].name);
  Serial.println("🎯 Nearest Node: " + determineNearestNode());
  
  Serial.println("\n Data recieved from vehicles: " );

  for (int i = 1; i < 3; i++) {
    if (nodes[i].dataReceived) {
     // Serial.println("\nNode " + String(i + 1) + ": " + nodes[i].name);
      Serial.println("\n From: \"" + nodes[i].name + "\"");
      Serial.println("Last Data: \"" + nodes[i].lastData + "\"");
      Serial.println("Signal Strength: " + String(nodes[i].rssi) + " dBm");
      
      // Display RSSI-based distance estimation
      if (nodes[i].distance > 0) {
        Serial.println("Estimated Distance (RSSI): " + String(nodes[i].distance, 1) + " meters");
      }
      
      Serial.println("Total Packets: " + String(nodes[i].packetCount));
      Serial.println("_______________________________________________________" );

      unsigned long timeSinceLastUpdate = millis() - nodes[i].lastUpdate;
      // if (timeSinceLastUpdate > NODE_TIMEOUT) {
      //   Serial.println("Status: OFFLINE (Last seen " + String(timeSinceLastUpdate / 1000) + " seconds ago)");
      // } else {
      //   Serial.println("Status: ONLINE (" + String(timeSinceLastUpdate / 1000) + " seconds ago)");
      // }
    }
  }
 // Serial.println("==========================");
}

void sendPacket() {
  // Send a packet with the current node's name
  LoRa.beginPacket();
  String packetText = "Beacon Message from Vehicle NODE 2"  ;
  LoRa.print(packetText);
  LoRa.endPacket();
  
  Serial.println("\nSent packet: " + packetText);
}

void receivePacket(unsigned long duration) {
  unsigned long startTime = millis();
  int packetsThisCycle = 0;

// this   while loop will run for 10 sec 
  while (millis() - startTime < duration) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String receivedText = "";
      while (LoRa.available()) {
        receivedText += (char)LoRa.read();
      }
      
      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();
      
      String nodeName = receivedText;
      if (receivedText.startsWith("Beacon Message from Vehicle ")) {
        nodeName = receivedText.substring(28);
      }

      else if (receivedText.startsWith("DATA:")) {

    int startPacketDest = receivedText.indexOf("DESTINATION:") + 12; // Move after "PacketDest:"
    int endPacketDest = receivedText.indexOf(":", startPacketDest); // End at next colon
    nodeName = receivedText.substring(startPacketDest, endPacketDest);
      // Extract only the payload
    int payloadStart = receivedText.indexOf("PAYLOAD:") + 8; // Start after "PAYLOAD:"
    String payload = receivedText.substring(payloadStart);

      // Extracting the source, node from which this data is coming
      int StartSource =  receivedText.indexOf("SOURCE:") + 7; // Move after "PacketDest:"
      int EndSource = receivedText.indexOf(":", StartSource); // End at next colon
      String source = receivedText.substring(StartSource, EndSource);

        updateNodeInfo(source, payload, rssi);

        Serial.println("\n ====== CHANNEL STATE information ======");
        Serial.println("\n📦 PACKET RECEIVED 📦");
        Serial.println("────────────────────────");
        Serial.println("📡 From: " + source);
        Serial.println("📝 Data: \"" + payload + "\"");
        Serial.println("📊 Signal: " + String(rssi) + " dBm");
        Serial.println("📈 SNR: " + String(snr) + " dB");
        Serial.println("📏 Size: " + String(packetSize) + " bytes");
        Serial.println("────────────────────────");
        //  updateNodeInfo(nodeName, receivedText, rssi);
        //    displayNodeStatus();
        packetsThisCycle++;

     return;
      }



      // Ignore packets from the current node
      if (nodeName != nodes[0].name) {
        updateNodeInfo(nodeName, receivedText, rssi);
        
        Serial.println("\n ====== CHANNEL STATE information ======");
        Serial.println("\n📦 PACKET RECEIVED 📦");
        Serial.println("────────────────────────");
        Serial.println("📡 From: " + nodeName);
        Serial.println("📝 Data: \"" + receivedText + "\"");
        Serial.println("📊 Signal: " + String(rssi) + " dBm");
        Serial.println("📈 SNR: " + String(snr) + " dB");
        Serial.println("📏 Size: " + String(packetSize) + " bytes");
        Serial.println("────────────────────────");
        
        packetsThisCycle++;
      }
    }
    delay(10);

  }
  
  // Always display node status, even if no packets received
  displayNodeStatus();
}

// Iterations untill currentTime = 10 sec( 10 sec since last reset ), this loop will execyte in milliseconds.
// After that, it will take approx 10 seconds for each iteration.
// Simply remember 10 seconds to start, and agter that 10 seconds for complete for loop to execute 1 time

void loop() {

  static unsigned long lastSendTime = 0;
  unsigned long currentTime = millis();    // unsigned means it can hold only +ve values 
  
  // Periodically send and receive packets
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendPacket();    // takes 2-5 ms
    lastSendTime = currentTime;  
    receivePacket(RECEIVE_DURATION);   //    takes 10sec to execute recivepacket function
  }
  
  // Check for incoming packets between send intervals. Runs only if a packet is available during these intervals
  //  will take max 100ms if a packet is available
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    receivePacket(100);
  }
  
  // Add a small delay to prevent tight looping - 10 ms
  delay(10); 

}



