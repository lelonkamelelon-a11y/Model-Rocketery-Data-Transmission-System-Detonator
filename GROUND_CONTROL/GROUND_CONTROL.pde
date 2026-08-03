import processing.serial.*;

Serial myPort;
String rawTelemetry = "WAITING FOR DATA...";

// Parsed Telemetry Data
float alt = 0;
float pitch = 0, yaw = 0, roll = 0;
float accX = 0, accY = 0, accZ = 0;
String gpsData = "NO LOCK";
String satCount = "0";
float latVal = 0.0, lonVal = 0.0;

// Barometer Baseline Variables
float baselinePressure = 0.0;
boolean baselineSet = false;

// Graph History Arrays
int graphWidth = 400;
float[] altHistory = new float[graphWidth];
float[] accXHistory = new float[graphWidth];
float[] accYHistory = new float[graphWidth];
float[] accZHistory = new float[graphWidth];
float[] latHistory = new float[graphWidth];
float[] lonHistory = new float[graphWidth];

void setup() {
  size(1050, 800); 
  
  println("Attempting to connect to COM6...");
  
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
  
  // Title
  fill(0, 255, 150);
  textSize(24);
  text("PHOENIX GROUND CONTROL DASHBOARD", 30, 40);
  
  // Draw Raw Data string
  fill(200);
  textSize(14);
  text("RAW: " + rawTelemetry, 30, 70);
  
  // 1. Draw Altitude Graph (Barometric Relative)
  drawGraph("RELATIVE ALTITUDE (m)", 30, 100, 600, 200, altHistory, color(0, 200, 255));
  
  // 2. Draw Acceleration Multi-Graph (Middle Left)
  drawMultiGraph("ACCELERATION (m/s2)", 30, 330, 600, 200, 
                 accXHistory, accYHistory, accZHistory, 
                 color(255, 100, 100), color(100, 255, 100), color(100, 150, 255));
  
  // 3. Draw Stats Panel (Bottom Left)
  drawStatsPanel(30, 560);
  
  // 4. Draw Gyro / Orientation Bars (Top Right)
  drawCenterBar("PITCH", 680, 100, 250, 30, pitch, -180, 180, color(255, 100, 100));
  drawCenterBar("YAW", 680, 150, 250, 30, yaw, -180, 180, color(100, 255, 100));
  drawCenterBar("ROLL", 680, 200, 250, 30, roll, -180, 180, color(100, 100, 255));
  
  // 5. Draw Accelerometer Bars (Middle Right)
  drawCenterBar("ACC X", 680, 330, 250, 30, accX, -30, 30, color(255, 100, 100));
  drawCenterBar("ACC Y", 680, 380, 250, 30, accY, -30, 30, color(100, 255, 100));
  drawCenterBar("ACC Z", 680, 430, 250, 30, accZ, -30, 60, color(100, 150, 255));

  // 6. GPS Tracker Plot / Coordinate Drift (Bottom Right)
  drawGPSGraph("GPS LAT / LON DRIFT", 680, 560, 340, 170, latHistory, lonHistory);
}

// ==========================================
// KEYBOARD CONTROLS
// ==========================================
void keyPressed() {
  if (key == 'z' || key == 'Z') {
    baselineSet = false; // Forces the dashboard to grab a new baseline pressure on the next tick
    println("[SYSTEM] Altitude manually zeroed to current pressure.");
  }
}

// ==========================================
// SERIAL EVENT LOGIC
// ==========================================
void serialEvent(Serial myPort) {
  String inString = myPort.readStringUntil('\n');
  if (inString != null) {
    inString = inString.trim();
    
    if (inString.startsWith("[ALL DATA] ")) {
      rawTelemetry = inString.substring(11); 
      
      // Parse Acceleration (ACC:x,y,z)
      float[] accVals = extractList(rawTelemetry, "ACC:");
      if (accVals.length > 0) accX = accVals[0];
      if (accVals.length > 1) accY = accVals[1];
      if (accVals.length > 2) accZ = accVals[2];
      
      // Parse Gyro (GYR:pitch,yaw,roll)
      float[] gyrVals = extractList(rawTelemetry, "GYR:");
      if (gyrVals.length > 0) pitch = applyDeadZone(gyrVals[0], 2.0);
      if (gyrVals.length > 1) yaw = applyDeadZone(gyrVals[1], 2.0);
      if (gyrVals.length > 2) roll = applyDeadZone(gyrVals[2], 2.0);
      
      // Parse Barometer & Calculate RELATIVE Altitude (BAR:temp,pressure)
      float[] barVals = extractList(rawTelemetry, "BAR:");
      if (barVals.length > 1) {
         float pressure_hPa = barVals[1]; 
         
         // Basic sanity check to ensure sensor isn't throwing garbage zeros
         if (pressure_hPa > 500) { 
             
             // Lock in the baseline pressure on startup (or after pressing 'Z')
             if (!baselineSet) {
                 baselinePressure = pressure_hPa;
                 baselineSet = true;
             }
             
             // Calculate altitude relative to the baseline pressure
             alt = 44330.0 * (1.0 - pow(pressure_hPa / baselinePressure, 0.1903));
         }
      }
      
      // Parse GPS (GPS:lat,lon)
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
      
      // Parse SAT
      int satIdx = rawTelemetry.indexOf("SAT:");
      if (satIdx != -1) {
        int start = satIdx + 4;
        int end = rawTelemetry.indexOf(' ', start);
        if (end == -1) end = rawTelemetry.length();
        satCount = rawTelemetry.substring(start, end);
      }
      
      // Shift graph arrays left and add newest data
      for (int i = 0; i < graphWidth - 1; i++) {
        altHistory[i] = altHistory[i+1];
        accXHistory[i] = accXHistory[i+1];
        accYHistory[i] = accYHistory[i+1];
        accZHistory[i] = accZHistory[i+1];
        latHistory[i] = latHistory[i+1];
        lonHistory[i] = lonHistory[i+1];
      }
      altHistory[graphWidth - 1] = alt; 
      accXHistory[graphWidth - 1] = accX;
      accYHistory[graphWidth - 1] = accY;
      accZHistory[graphWidth - 1] = accZ;
      latHistory[graphWidth - 1] = latVal;
      lonHistory[graphWidth - 1] = lonVal;
    }
  }
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
  
  noFill(); stroke(c); strokeWeight(2);
  beginShape();
  for (int i = 0; i < data.length; i++) {
    float xPos = map(i, 0, data.length-1, x, x + w);
    float yPos = map(data[i], 0, maxVal * 1.1, y + h, y); 
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

void drawGPSGraph(String title, float x, float y, float w, float h, float[] lats, float[] lons) {
  fill(30); stroke(100);
  rect(x, y, w, h);
  
  fill(255, 255, 0); textSize(14);
  text(title, x, y - 8);
  
  noFill(); stroke(255, 255, 0); strokeWeight(2);
  beginShape();
  for (int i = 0; i < lats.length; i++) {
    float xPos = map(i, 0, lats.length - 1, x, x + w);
    float yPos = map(lats[i], latVal - 0.001, latVal + 0.001, y + h, y);
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

void drawStatsPanel(float x, float y) {
  fill(30); stroke(100);
  rect(x, y, 600, 170);
  
  fill(200); textSize(20);
  text("FLIGHT COMPUTERS / SYSTEM DATA", x + 20, y + 30);
  
  textSize(16);
  fill(255, 255, 0);
  text("GPS COORDS: " + gpsData + " (SATS: " + satCount + ")", x + 20, y + 70);
  
  fill(0, 255, 200);
  text("LATEST ALTITUDE: " + nf(alt, 0, 2) + " m (Press 'Z' to zero)", x + 20, y + 100);
  
  fill(200);
  text("ACC MAX G: " + nf(max(abs(accX), abs(accY), abs(accZ)) / 9.81, 0, 2) + " Gs", x + 20, y + 130);
}
