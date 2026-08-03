import processing.serial.*;

Serial myPort;
String rawTelemetry = "WAITING FOR DATA...";

// Parsed Telemetry Data
float alt = 0;
float tempVal = 0;
float pitch = 0, yaw = 0, roll = 0;
float accX = 0, accY = 0, accZ = 0;
String gpsData = "NO LOCK";
int satCountVal = 0;
float latVal = 0.0, lonVal = 0.0;

// Graph History Arrays
int graphWidth = 400;
float[] altHistory = new float[graphWidth];
float[] accXHistory = new float[graphWidth];
float[] accYHistory = new float[graphWidth];
float[] accZHistory = new float[graphWidth];
float[] satHistory = new float[graphWidth];
float[] tempHistory = new float[graphWidth];

PrintWriter logFile;

void settings() {
  fullScreen();
}

void setup() {
  println("Attempting to connect to COM6...");
  
  // Create timestamped log filename: YYYY-MM-DD_HH-MM-SS_LOG.txt
  String timestamp = nf(year(), 4) + "-" + nf(month(), 2) + "-" + nf(day(), 2) + "_" + 
                     nf(hour(), 2) + "-" + nf(minute(), 2) + "-" + nf(second(), 2);
  String fileName = timestamp + "_LOG.txt";
  
  // Directly point to the Windows Downloads folder using System property
  String userHome = System.getProperty("user.home");
  String downloadsPath = userHome + "/Downloads/" + fileName;
  
  logFile = createWriter(downloadsPath);
  println("📁 Log file created in Windows Downloads: " + downloadsPath);
  
  try {
    myPort = new Serial(this, "COM6", 115200); 
    myPort.bufferUntil('\n'); 
    println("Successfully connected to COM6!");
  } catch (Exception e) {
    println("⚠️ ERROR: Could not connect to COM6. Make sure the port is not in use.");
  }
}

void draw() {
  background(15, 20, 25); // Dark aerospace theme
  
  float leftColW = width * 0.58;  
  float rightColX = leftColW + 60;
  float rightColW = width - rightColX - 40;
  
  float graphH = height * 0.22;
  float panelH = height * 0.18;
  float barH = height * 0.035;
  
  // Title
  fill(0, 255, 150);
  textSize(24);
  text("GROUND CONTROL DASHBOARD", 30, 40);
  
  // Draw Raw Data string
  fill(200);
  textSize(14);
  text("RAW: " + rawTelemetry, 30, 70);
  
  // ==========================================
  // LEFT COLUMN (Graphs & Stats)
  // ==========================================
  drawGraph("BARO ALTITUDE (m)", 30, 100, leftColW, graphH, altHistory, color(0, 200, 255));
  drawMultiGraph("ACCELERATION (m/s2)", 30, 130 + graphH, leftColW, graphH, 
                   accXHistory, accYHistory, accZHistory, 
                   color(255, 100, 100), color(100, 255, 100), color(100, 150, 255));
  drawStatsPanel(30, 160 + (graphH * 2), leftColW, panelH);
  
  // ==========================================
  // RIGHT COLUMN (Bars & Secondary Graphs)
  // ==========================================
  float rY = 100;
  drawCenterBar("PITCH", rightColX, rY, rightColW, barH, pitch, -180, 180, color(255, 100, 100));
  rY += barH + 15;
  drawCenterBar("YAW", rightColX, rY, rightColW, barH, yaw, -180, 180, color(100, 255, 100));
  rY += barH + 15;
  drawCenterBar("ROLL", rightColX, rY, rightColW, barH, roll, -180, 180, color(100, 100, 255));
  
  rY += barH + 25;
  drawCenterBar("ACC X", rightColX, rY, rightColW, barH, accX, -30, 30, color(255, 100, 100));
  rY += barH + 15;
  drawCenterBar("ACC Y", rightColX, rY, rightColW, barH, accY, -30, 30, color(100, 255, 100));
  rY += barH + 15;
  drawCenterBar("ACC Z", rightColX, rY, rightColW, barH, accZ, -30, 60, color(100, 150, 255));

  rY += barH + 25;
  float smallGraphH = (height - rY - 40) / 2.0;
  drawSatGraph("SATELLITE COUNT", rightColX, rY, rightColW, smallGraphH, satHistory, color(255, 200, 0));
  
  rY += smallGraphH + 20;
  drawTempGraph("BARO TEMPERATURE (°C)", rightColX, rY, rightColW, smallGraphH, tempHistory, color(255, 100, 200));
}

// ==========================================
// SERIAL EVENT LOGIC
// ==========================================
void serialEvent(Serial myPort) {
  String inString = myPort.readStringUntil('\n');
  if (inString != null) {
    inString = inString.trim();
    
    // Log every single incoming line immediately to Downloads
    if (logFile != null) {
      String fileTimestamp = nf(year(), 4) + "-" + nf(month(), 2) + "-" + nf(day(), 2) + " " + 
                             nf(hour(), 2) + ":" + nf(minute(), 2) + ":" + nf(second(), 2) + "." + nf(millis() % 1000, 3);
      logFile.println("[" + fileTimestamp + "] " + inString);
      logFile.flush(); 
    }
    
    if (inString.startsWith("[ALL DATA] ")) {
      rawTelemetry = inString.substring(11); 
      
      float[] accVals = extractList(rawTelemetry, "ACC:");
      if (accVals.length > 0) accX = accVals[0];
      if (accVals.length > 1) accY = accVals[1];
      if (accVals.length > 2) accZ = accVals[2];
      
      float[] gyrVals = extractList(rawTelemetry, "GYR:");
      if (gyrVals.length > 0) pitch = applyDeadZone(gyrVals[0], 2.0);
      if (gyrVals.length > 1) yaw = applyDeadZone(gyrVals[1], 2.0);
      if (gyrVals.length > 2) roll = applyDeadZone(gyrVals[2], 2.0);
      
      float[] barVals = extractList(rawTelemetry, "BAR:");
      if (barVals.length > 0) tempVal = barVals[0]; 
      if (barVals.length > 1) {
         float pressure_hPa = barVals[1]; 
         if (pressure_hPa > 0) {
             alt = 44330.0 * (1.0 - pow(pressure_hPa / 1013.25, 0.1903));
         }
      }
      
      int gpsIdx = rawTelemetry.indexOf("GPS:");
      if (gpsIdx != -1) {
        int start = gpsIdx + 4;
        int end = rawTelemetry.indexOf(' ', start);
        if (end == -1) end = rawTelemetry.length();
        gpsData = rawTelemetry.substring(start, end);

        String[] coords = split(gpsData, ',');
        if (coords.length >= 2) {
          try {
            latVal = Float.parseFloat(coords[0]);
            lonVal = Float.parseFloat(coords[1]);
          } catch(Exception e) {}
        }
      }
      
      int satIdx = rawTelemetry.indexOf("SAT:");
      if (satIdx != -1) {
        int start = satIdx + 4;
        int end = rawTelemetry.indexOf(' ', start);
        if (end == -1) end = rawTelemetry.length();
        try {
          satCountVal = Integer.parseInt(rawTelemetry.substring(start, end).trim());
        } catch(Exception e) {
          satCountVal = 0;
        }
      }
      
      for (int i = 0; i < graphWidth - 1; i++) {
        altHistory[i] = altHistory[i+1];
        accXHistory[i] = accXHistory[i+1];
        accYHistory[i] = accYHistory[i+1];
        accZHistory[i] = accZHistory[i+1];
        satHistory[i] = satHistory[i+1];
        tempHistory[i] = tempHistory[i+1];
      }
      altHistory[graphWidth - 1] = alt; 
      accXHistory[graphWidth - 1] = accX;
      accYHistory[graphWidth - 1] = accY;
      accZHistory[graphWidth - 1] = accZ;
      satHistory[graphWidth - 1] = satCountVal;
      tempHistory[graphWidth - 1] = tempVal;
    }
  }
}

void stop() {
  if (logFile != null) {
    logFile.flush();
    logFile.close();
  }
  super.stop();
}

// ==========================================
// UI & MATH HELPER FUNCTIONS
// ==========================================

float applyDeadZone(float val, float deadzoneThreshold) {
  if (abs(val) < deadzoneThreshold) return 0.0;
  return val;
}

float[] extractList(String input, String key) {
  int idx = input.indexOf(key);
  if (idx != -1) {
    int start = idx + key.length();
    int end = input.indexOf(' ', start);
    if (end == -1) end = input.length();
    String block = input.substring(start, end);

    String[] parts = split(block, ',');
    float[] vals = new float[parts.length];
    
    for (int i = 0; i < parts.length; i++) {
      String clean = parts[i].replaceAll("[^\\d.-]", "");
      try {
        if (clean.length() > 0) vals[i] = Float.parseFloat(clean);
        else vals[i] = 0.0;
      } catch (Exception e) {
        vals[i] = 0.0;
      }
    }
    return vals;
  }
  return new float[0];
}

void drawGraph(String title, float x, float y, float w, float h, float[] data, color c) {
  fill(30); stroke(100);
  rect(x, y, w, h);
  
  fill(c); textSize(16);
  text(title + ": " + nf(data[data.length-1], 0, 1), x, y - 10);
  
  float maxVal = 100; 
  for (int i = 0; i < data.length; i++) {
    if (data[i] > maxVal) maxVal = data[i];
  }
  
  float graphMax = maxVal * 1.1;
  float graphMin = 0;

  fill(150); textSize(11);
  textAlign(RIGHT, CENTER);
  text(nf(graphMax, 0, 1), x - 5, y);
  text(nf(graphMax / 2, 0, 1), x - 5, y + h / 2);
  text(nf(graphMin, 0, 1), x - 5, y + h);
  
  textAlign(CENTER, TOP);
  text("-400", x, y + h + 5);
  text("-200", x + w / 2, y + h + 5);
  text("0", x + w, y + h + 5);
  textAlign(LEFT, BASELINE);

  noFill(); stroke(c); strokeWeight(2);
  beginShape();
  for (int i = 0; i < data.length; i++) {
    float xPos = map(i, 0, data.length-1, x, x + w);
    float yPos = map(data[i], 0, graphMax, y + h, y); 
    vertex(xPos, yPos);
  }
  endShape();
  strokeWeight(1); 
}

void drawMultiGraph(String title, float x, float y, float w, float h, 
                    float[] d1, float[] d2, float[] d3, color c1, color c2, color c3) {
  fill(30); stroke(100);
  rect(x, y, w, h);
  
  fill(255); textSize(16);
  text(title, x, y - 10);
  
  float maxV = 5;  
  float minV = -5;
  
  for (int i = 0; i < d1.length; i++) {
    if (d1[i] > maxV) maxV = d1[i];
    if (d2[i] > maxV) maxV = d2[i];
    if (d3[i] > maxV) maxV = d3[i];
    
    if (d1[i] < minV) minV = d1[i];
    if (d2[i] < minV) minV = d2[i];
    if (d3[i] < minV) minV = d3[i];
  }
  
  maxV *= 1.1; 
  minV *= 1.1;

  fill(150); textSize(11);
  textAlign(RIGHT, CENTER);
  text(nf(maxV, 0, 1), x - 5, y);
  text(nf((maxV + minV) / 2, 0, 1), x - 5, y + h / 2);
  text(nf(minV, 0, 1), x - 5, y + h);
  
  textAlign(CENTER, TOP);
  text("-400", x, y + h + 5);
  text("-200", x + w / 2, y + h + 5);
  text("0", x + w, y + h + 5);
  textAlign(LEFT, BASELINE);

  float zeroY = map(0, minV, maxV, y + h, y);
  stroke(150); strokeWeight(1);
  line(x, zeroY, x + w, zeroY);
  
  noFill(); stroke(c1); strokeWeight(2);
  beginShape();
  for (int i = 0; i < d1.length; i++) vertex(map(i, 0, d1.length-1, x, x + w), map(d1[i], minV, maxV, y + h, y));
  endShape();
  
  stroke(c2); 
  beginShape();
  for (int i = 0; i < d2.length; i++) vertex(map(i, 0, d2.length-1, x, x + w), map(d2[i], minV, maxV, y + h, y));
  endShape();
  
  stroke(c3); 
  beginShape();
  for (int i = 0; i < d3.length; i++) vertex(map(i, 0, d3.length-1, x, x + w), map(d3[i], minV, maxV, y + h, y));
  endShape();
  
  strokeWeight(1);
  
  textSize(12);
  fill(c1); text("X: " + nf(d1[d1.length-1], 0, 2), x + w - 180, y + 20);
  fill(c2); text("Y: " + nf(d2[d2.length-1], 0, 2), x + w - 120, y + 20);
  fill(c3); text("Z: " + nf(d3[d3.length-1], 0, 2), x + w - 60, y + 20);
}

void drawSatGraph(String title, float x, float y, float w, float h, float[] data, color c) {
  fill(30); stroke(100);
  rect(x, y, w, h);
  
  fill(c); textSize(14);
  text(title + ": " + int(data[data.length-1]), x, y - 8);
  
  float maxVal = 20; 
  float minVal = 0;

  fill(150); textSize(11);
  textAlign(RIGHT, CENTER);
  text("20", x - 5, y);                        
  text("10", x - 5, y + h / 2);                      
  text("0", x - 5, y + h);                    
  
  textAlign(CENTER, TOP);
  text("-400", x, y + h + 5);
  text("-200", x + w / 2, y + h + 5);
  text("0", x + w, y + h + 5);
  textAlign(LEFT, BASELINE);

  noFill(); stroke(c); strokeWeight(2);
  beginShape();
  for (int i = 0; i < data.length; i++) {
    float xPos = map(i, 0, data.length - 1, x, x + w);
    float yPos = map(data[i], minVal, maxVal, y + h, y);
    vertex(xPos, yPos);
  }
  endShape();
  strokeWeight(1);
}

void drawTempGraph(String title, float x, float y, float w, float h, float[] data, color c) {
  fill(30); stroke(100);
  rect(x, y, w, h);
  
  fill(c); textSize(14);
  text(title + ": " + nf(data[data.length-1], 0, 1) + "°C", x, y - 8);
  
  float maxVal = 40; 
  float minVal = 0;
  for (int i = 0; i < data.length; i++) {
    if (data[i] > maxVal) maxVal = data[i];
    if (data[i] < minVal) minVal = data[i];
  }
  maxVal += 5;
  minVal = min(0, minVal - 5);

  fill(150); textSize(11);
  textAlign(RIGHT, CENTER);
  text(nf(maxVal, 0, 0), x - 5, y);                        
  text(nf((maxVal + minVal) / 2, 0, 0), x - 5, y + h / 2);                      
  text(nf(minVal, 0, 0), x - 5, y + h);                    
  
  textAlign(CENTER, TOP);
  text("-400", x, y + h + 5);
  text("-200", x + w / 2, y + h + 5);
  text("0", x + w, y + h + 5);
  textAlign(LEFT, BASELINE);

  noFill(); stroke(c); strokeWeight(2);
  beginShape();
  for (int i = 0; i < data.length; i++) {
    float xPos = map(i, 0, data.length - 1, x, x + w);
    float yPos = map(data[i], minVal, maxVal, y + h, y);
    vertex(xPos, yPos);
  }
  endShape();
  strokeWeight(1);
}

void drawCenterBar(String label, float x, float y, float w, float h, float val, float minV, float maxV, color c) {
  fill(30); stroke(100);
  rect(x, y, w, h);
  
  stroke(150);
  line(x + w/2, y, x + w/2, y + h);
  
  noStroke(); fill(c);
  float mappedVal = map(val, minV, maxV, 0, w);
  mappedVal = constrain(mappedVal, 0, w);
  
  if (val >= 0) {
    rect(x + w/2, y, mappedVal - w/2, h); 
  } else {
    rect(x + mappedVal, y, (w/2) - mappedVal, h); 
  }
  
  fill(255); textSize(14);
  text(label + ": " + nf(val, 0, 2), x, y - 8);
}

void drawStatsPanel(float x, float y, float w, float h) {
  fill(30); stroke(100);
  rect(x, y, w, h);
  
  fill(200); textSize(20);
  text("FLIGHT COMPUTERS / SYSTEM DATA", x + 20, y + 30);
  
  textSize(16);
  fill(255, 255, 0);
  text("GPS COORDS: " + gpsData + " (SATS: " + satCountVal + ")", x + 20, y + 70);
  
  fill(0, 255, 200);
  text("LATEST ALTITUDE: " + nf(alt, 0, 2) + " m", x + 20, y + 100);
  
  fill(200);
  text("ACC MAX G: " + nf(max(abs(accX), abs(accY), abs(accZ)) / 9.81, 0, 2) + " Gs", x + 20, y + 130);
}
