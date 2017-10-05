//////////
/////////  All Serial Handling Code,
/////////  It's Changeable with the 'outputType' variable
/////////  It's declared at start of code.
/////////
void serialOutput(){   // Decide How To Output Serial.
  Serial.print("BPM = ");
  Serial.print(BPM);
  Serial.print(", IBI = ");
  Serial.print(IBI);
  Serial.print(", Signal = ");
  Serial.println(Signal);
}

//  Sends Data to Pulse Sensor Processing App, Native Mac App, or Third-party Serial Readers.
void sendDataToSerial(char symbol, int data ){
  Serial.print(symbol);
  Serial.println(data);
}
