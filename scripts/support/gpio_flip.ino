
#define SIG_PIN 4
#define RELAY_CNTRL 5
#define LENGTH 100

int i = 0;
/* lambda = 175*/
/*
  float vals[100] = {
  184.0,151.0,182.0,164.0,168.0,196.0,168.0,169.0,175.0,159.0,165.0,184.0,147.0,
  153.0,153.0,179.0,181.0,167.0,188.0,165.0,166.0,190.0,202.0,177.0,175.0,169.0,
  169.0,201.0,164.0,176.0,166.0,180.0,171.0,163.0,166.0,181.0,151.0,175.0,183.0,
  179.0,169.0,171.0,168.0,174.0,200.0,192.0,180.0,172.0,175.0,157.0,172.0,171.0,
  169.0,151.0,174.0,194.0,177.0,172.0,164.0,188.0,171.0,180.0,179.0,188.0,170.0,
  156.0,173.0,192.0,165.0,188.0,178.0,172.0,179.0,189.0,179.0,168.0,184.0,167.0,
  161.0,180.0,153.0,168.0,181.0,176.0,174.0,178.0,153.0,181.0,174.0,179.0,164.0,
  162.0,177.0,177.0,147.0,163.0,156.0,170.0,190.0,195.0
  };
*/
/* lambda = 250*/
/*
  float vals[100] = {
  268.0,280.0,237.0,246.0,240.0,250.0,248.0,250.0,258.0,259.0,255.0,206.0,261.0,292.0,
  255.0,230.0,273.0,252.0,240.0,264.0,236.0,238.0,261.0,244.0,259.0,258.0,205.0,227.0,
  246.0,264.0,225.0,224.0,250.0,229.0,221.0,266.0,267.0,236.0,232.0,269.0,249.0,237.0,
  253.0,252.0,256.0,258.0,241.0,250.0,273.0,255.0,230.0,248.0,247.0,261.0,230.0,253.0,
  213.0,243.0,227.0,277.0,240.0,255.0,255.0,281.0,284.0,260.0,235.0,222.0,225.0,280.0,
  250.0,263.0,241.0,249.0,212.0,256.0,247.0,263.0,248.0,250.0,245.0,232.0,237.0,261.0,
  231.0,243.0,251.0,251.0,271.0,268.0,256.0,260.0,266.0,237.0,249.0,233.0,246.0,244.0,
  230.0,228.0
  };
  /* lamdba = 275 */
/*
  float vals[100] = {
  279.0,309.0,273.0,269.0,261.0,276.0,264.0,245.0,249.0,266.0,279.0,286.0,251.0,263.0,
  275.0,287.0,272.0,288.0,282.0,261.0,274.0,267.0,244.0,245.0,280.0,295.0,254.0,262.0,
  267.0,316.0,267.0,282.0,286.0,298.0,251.0,271.0,261.0,268.0,275.0,257.0,286.0,293.0,
  270.0,257.0,300.0,282.0,271.0,312.0,288.0,267.0,275.0,257.0,229.0,282.0,273.0,293.0,
  329.0,281.0,266.0,288.0,284.0,265.0,255.0,274.0,319.0,244.0,270.0,261.0,276.0,245.0,
  284.0,269.0,271.0,271.0,266.0,282.0,299.0,300.0,259.0,297.0,272.0,288.0,280.0,283.0,
  258.0,287.0,292.0,254.0,275.0,267.0,274.0,258.0,287.0,289.0,286.0,264.0,304.0,296.0,
  288.0,280.0
  };
*/
/*lambda = 150*/
/*
  float vals[100] = {
  144.0,143.0,143.0,137.0,169.0,145.0,147.0,152.0,140.0,157.0,135.0,135.0,
  173.0,152.0,168.0,135.0,140.0,130.0,163.0,162.0,146.0,133.0,158.0,161.0,
  140.0,153.0,153.0,145.0,124.0,145.0,144.0,162.0,162.0,150.0,127.0,138.0,
  149.0,148.0,163.0,140.0,147.0,147.0,134.0,139.0,159.0,156.0,168.0,155.0,
  159.0,128.0,150.0,164.0,143.0,156.0,163.0,130.0,143.0,167.0,153.0,137.0,
  145.0,147.0,131.0,148.0,164.0,185.0,146.0,160.0,153.0,135.0,133.0,148.0,
  152.0,146.0,161.0,169.0,129.0,166.0,112.0,147.0,146.0,144.0,126.0,155.0,
  157.0,180.0,146.0,151.0,146.0,159.0,161.0,145.0,185.0,135.0,138.0,154.0,
  149.0,141.0,152.0,140.0
  };
*/
/*lambda = 125*/
/*
  float vals[100] = {
  145.0,120.0,107.0,120.0,126.0,128.0,126.0,120.0,121.0,109.0,129.0,117.0,117.0,130.0,
  117.0,116.0,130.0,119.0,131.0,119.0,129.0,127.0,115.0,128.0,111.0,119.0,119.0,137.0,
  135.0,119.0,152.0,124.0,117.0,116.0,114.0,122.0,95.0,132.0,144.0,135.0,141.0,118.0,
  142.0,111.0,121.0,131.0,138.0,116.0,124.0,109.0,124.0,136.0,117.0,119.0,126.0,132.0,
  129.0,120.0,122.0,134.0,140.0,116.0,128.0,129.0,119.0,150.0,111.0,129.0,127.0,127.0,
  124.0,114.0,114.0,113.0,146.0,148.0,141.0,139.0,133.0,119.0,137.0,126.0,114.0,134.0,
  129.0,117.0,122.0,107.0,124.0,116.0,116.0,121.0,139.0,102.0,129.0,125.0,101.0,121.0,137.0,107.0 };
  8?
  /* lambda = 110*/
/*
  float vals[100] = {
  99.0,95.0,122.0,124.0,101.0,114.0,116.0,119.0,113.0,103.0,110.0,97.0,102.0,94.0,
  108.0,97.0,112.0,110.0,107.0,104.0,104.0,114.0,99.0,100.0,99.0,106.0,111.0,112.0,
  107.0,121.0,116.0,137.0,112.0,97.0,118.0,110.0,95.0,115.0,121.0,105.0,132.0,93.0,
  102.0,113.0,107.0,112.0,104.0,103.0,116.0,112.0,102.0,120.0,92.0,112.0,95.0,134.0,
  105.0,110.0,114.0,109.0,108.0,117.0,123.0,131.0,102.0,91.0,96.0,96.0,121.0,103.0,
  117.0,102.0,102.0,91.0,122.0,90.0,116.0,118.0,110.0,107.0,104.0,99.0,115.0,124.0,
  114.0,77.0,124.0,110.0,91.0,115.0,119.0,111.0,98.0,114.0,102.0,99.0,120.0,120.0,
  115.0,129.0
  };*/
/* lambda = 100 */

float vals[100] = {
  105.0, 95.0, 104.0, 114.0, 94.0, 104.0, 108.0, 95.0, 87.0, 101.0, 108.0, 104.0, 89.0,
  106.0, 107.0, 92.0, 114.0, 110.0, 94.0, 111.0, 108.0, 81.0, 112.0, 105.0, 99.0, 91.0,
  92.0, 99.0, 101.0, 107.0, 105.0, 99.0, 112.0, 108.0, 107.0, 94.0, 119.0, 99.0, 101.0,
  90.0, 106.0, 96.0, 104.0, 107.0, 121.0, 88.0, 97.0, 105.0, 93.0, 114.0, 106.0, 96.0,
  110.0, 113.0, 109.0, 78.0, 106.0, 101.0, 107.0, 90.0, 93.0, 100.0, 99.0, 80.0, 103.0,
  92.0, 105.0, 109.0, 94.0, 90.0, 96.0, 101.0, 81.0, 92.0, 92.0, 82.0, 93.0, 114.0,
  107.0, 106.0, 96.0, 105.0, 90.0, 114.0, 96.0, 109.0, 103.0, 107.0, 99.0, 100.0, 89.0,
  108.0, 102.0, 88.0, 98.0, 110.0, 104.0, 103.0, 92.0, 92.0
};

/* lambda = 200ms*/
/*
  float vals[100] = {
  184.0,194.0,191.0,185.0,188.0,171.0,198.0,217.0,209.0,198.0,199.0,215.0,203.0,206.0,
  186.0,199.0,195.0,214.0,207.0,199.0,232.0,235.0,195.0,190.0,228.0,201.0,216.0,191.0,
  208.0,218.0,207.0,210.0,179.0,202.0,194.0,180.0,177.0,201.0,202.0,196.0,191.0,209.0,
  222.0,201.0,206.0,195.0,204.0,200.0,204.0,203.0,182.0,182.0,213.0,208.0,194.0,191.0,
  193.0,230.0,202.0,194.0,223.0,212.0,211.0,195.0,200.0,218.0,195.0,187.0,190.0,210.0,
  214.0,194.0,200.0,208.0,212.0,197.0,211.0,208.0,190.0,227.0,201.0,189.0,197.0,205.0,
  199.0,212.0,193.0,192.0,208.0,217.0,191.0,195.0,201.0,203.0,215.0,205.0,181.0,217.0,
  204.0,193.0
  };
*/
/* lambda = 300ms*/
/*
  float vals[100] = {
  300.0,294.0,286.0,310.0,277.0,278.0,309.0,298.0,333.0,284.0,317.0,292.0,306.0,
  295.0,305.0,307.0,324.0,283.0,292.0,283.0,278.0,264.0,308.0,309.0,308.0,325.0,
  273.0,304.0,305.0,339.0,293.0,324.0,300.0,305.0,311.0,315.0,312.0,299.0,285.0,
  316.0,304.0,311.0,324.0,269.0,305.0,329.0,304.0,303.0,317.0,301.0,310.0,302.0,
  297.0,296.0,292.0,276.0,268.0,312.0,312.0,317.0,290.0,322.0,293.0,315.0,283.0,
  310.0,310.0,303.0,299.0,287.0,300.0,335.0,255.0,290.0,292.0,312.0,304.0,289.0,
  304.0,313.0,291.0,292.0,292.0,305.0,302.0,306.0,286.0,295.0,331.0,316.0,319.0,
  301.0,306.0,304.0,309.0,299.0,263.0,311.0,314.0,323.0
  };
*/
/* lambda = 325ms*/
/*
  float vals[100] = {
  346.0,291.0,321.0,339.0,333.0,330.0,316.0,286.0,337.0,325.0,346.0,342.0,343.0,322.0,
  286.0,308.0,337.0,321.0,361.0,329.0,345.0,281.0,328.0,318.0,319.0,341.0,336.0,341.0,
  341.0,319.0,346.0,345.0,339.0,283.0,327.0,320.0,332.0,336.0,307.0,328.0,348.0,274.0,
  292.0,325.0,303.0,324.0,331.0,339.0,330.0,322.0,339.0,337.0,332.0,351.0,327.0,342.0,
  319.0,325.0,349.0,320.0,315.0,297.0,335.0,340.0,335.0,342.0,313.0,301.0,340.0,350.0,
  310.0,352.0,325.0,339.0,330.0,312.0,319.0,304.0,326.0,316.0,287.0,319.0,326.0,301.0,
  328.0,323.0,331.0,312.0,315.0,368.0,359.0,278.0,354.0,357.0,323.0,357.0,351.0,295.0,
  312.0,333.0 };
*/
/* lambda = 350ms */
/*
  float vals[100] = {
  337.0,344.0,378.0,384.0,350.0,322.0,360.0,354.0,345.0,367.0,374.0,350.0,364.0,353.0,
  352.0,338.0,345.0,343.0,340.0,373.0,376.0,343.0,373.0,362.0,354.0,366.0,347.0,359.0,
  340.0,338.0,371.0,373.0,337.0,378.0,364.0,362.0,361.0,365.0,358.0,353.0,374.0,355.0,
  336.0,332.0,369.0,337.0,335.0,369.0,375.0,367.0,365.0,346.0,368.0,358.0,371.0,334.0,
  354.0,329.0,342.0,362.0,355.0,341.0,385.0,360.0,353.0,378.0,345.0,359.0,324.0,337.0,
  362.0,364.0,355.0,333.0,363.0,356.0,364.0,317.0,336.0,346.0,342.0,366.0,341.0,334.0,
  348.0,362.0,337.0,382.0,345.0,358.0,312.0,328.0,348.0,347.0,354.0,330.0,329.0,367.0,
  336.0,347.0
  };*/

/* lambda = 400ms*/
/*
  float vals[100] = {
  367.0,415.0,411.0,432.0,413.0,381.0,408.0,394.0,435.0,372.0,408.0,428.0,359.0,405.0,
  401.0,407.0,387.0,404.0,390.0,413.0,405.0,382.0,388.0,438.0,416.0,398.0,406.0,392.0,
  433.0,392.0,414.0,398.0,412.0,397.0,405.0,380.0,420.0,408.0,422.0,441.0,405.0,370.0,
  408.0,416.0,412.0,419.0,403.0,395.0,388.0,400.0,400.0,382.0,436.0,402.0,401.0,425.0,
  393.0,417.0,422.0,396.0,409.0,404.0,394.0,375.0,409.0,401.0,422.0,385.0,396.0,389.0,
  435.0,428.0,409.0,393.0,427.0,413.0,368.0,429.0,407.0,410.0,416.0,393.0,399.0,400.0,
  359.0,399.0,401.0,416.0,380.0,437.0,383.0,396.0,397.0,409.0,396.0,389.0,406.0,395.0,
  399.0,410.0
  };

*/
/* lambda = 25ms */
/*
  float vals[100] = {
  32.0,22.0,19.0,24.0,15.0,33.0,26.0,25.0,26.0,24.0,
  29.0,33.0,27.0,26.0,26.0,28.0,19.0,24.0,19.0,28.0,
  20.0,23.0,29.0,27.0,26.0,21.0,25.0,25.0,18.0,38.0,
  21.0,18.0,34.0,20.0,27.0,20.0,19.0,28.0,36.0,22.0,
  23.0,27.0,18.0,20.0,29.0,25.0,26.0,31.0,27.0,30.0,
  27.0,25.0,24.0,30.0,28.0,25.0,33.0,21.0,30.0,16.0,
  28.0,23.0,27.0,23.0,23.0,25.0,31.0,30.0,29.0,21.0,
  24.0,24.0,32.0,21.0,31.0,27.0,20.0,27.0,25.0,39.0,
  27.0,26.0,20.0,24.0,29.0,21.0,25.0,21.0,20.0,25.0,
  26.0,30.0,28.0,25.0,22.0,17.0,19.0,23.0,27.0,26.0
  };*/
/*
  #define NO_WAIT
*/
#define MS_PER_SEC 1000

void setup() {
  // put your setup code here, to run once:
  pinMode(SIG_PIN, OUTPUT);
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(RELAY_CNTRL,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);
  digitalWrite(LED_BUILTIN,LOW);
  
  Serial.begin(9600);
}



void loop() {
  // put your main code here, to run repeatedly:
  // Wait for serial command to start
  while(1) {
    if(Serial.available())
    { char command = Serial.read();
      // Start gpio pulses
      if(command == 'S') {
        Serial.println("Here");
        digitalWrite(LED_BUILTIN, HIGH); //Turn on led
        delay(20);
        break;
      }
      // Enable relay so we can run programmer
      if(command == 'P') {
        digitalWrite(RELAY_CNTRL, HIGH); // Connect programmer to board
        delay(5*MS_PER_SEC); // TODO: figure out if this is enough time
      }
      // Disable the connection to the programmer if we're on harvested energy
      if(command == 'H') {
        digitalWrite(RELAY_CNTRL, LOW);
        delay(20);
      }
      
    }
  }
  int flag = 0;
  while(1) {
    flag = 0;
    for (i = 0; i < LENGTH; i++) {
      digitalWrite(SIG_PIN, HIGH);
  #ifndef NO_WAIT
      delay(20);
  #else
      delay(2);
  #endif
      digitalWrite(SIG_PIN, LOW);
      // Now check if there's a stop command
      if (Serial.available()) {
        char stuff = Serial.read();
        if(stuff == 'E') {
          Serial.println("Done!");
          digitalWrite(LED_BUILTIN, LOW); // Turn off led
          delay(1*MS_PER_SEC);
          flag = 1;
          break;
        }
      }
      // Delay some length of time determined by the list index
  #ifndef NO_WAIT
      delay(vals[i]);
  #else
      delay(2);
  #endif
    }
    if (flag == 1) {
      break;
    }
  }
}
