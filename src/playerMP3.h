
int q = 5;
byte returnCodes[10];
byte returnByteCounter;

void sendDFCommand(byte Command, int ParData) {
  byte commandData[10]; //This holds all the command data to be sent
  byte q;
  int checkSum;
  //Each command value is being sent in Hexadecimal
  commandData[0] = 0x7E;//Start of new command
  commandData[1] = 0xFF;//Version information
  commandData[2] = 0x06;//Data length (not including parity) or the start and version
  commandData[3] = Command;//The command that was sent through
  commandData[4] = 0x00;//1 = Чип будет посылать ответ о выполнении любой команды, даже той, которой не нужен ответ . 0 = не отвечает.
  commandData[5] = highByte(ParData);//High byte of the data sent over
  commandData[6] = lowByte(ParData);//low byte of the data sent over
  checkSum = -(commandData[1] + commandData[2] + commandData[3] + commandData[4] + commandData[5] + commandData[6]);
  commandData[7] = highByte(checkSum);//High byte of the checkSum
  commandData[8] = lowByte(checkSum);//low byte of the checkSum
  commandData[9] = 0xEF;//End bit
  for (q = 0; q < 10; q++) {
   Serial2.write(commandData[q]);
  }
  for (q = 0; q < 10; q++) {
  }
  delay(100);
}

uint8_t answerCommand(uint8_t qwery){
  uint8_t readByte;
  while (Serial2.available()) {   // если данные получены
    readByte = Serial2.read(); // прочитал 1 байт
    if(returnByteCounter == 0){ // если это первый байт в посылке  и он равен 7Е то начинаем собирать ответ
      if(readByte == 0x7E){
        returnCodes[returnByteCounter] = readByte;
        returnByteCounter++;
      }
    }else{
        returnCodes[returnByteCounter] = readByte;
       returnByteCounter++; 
    }
    if(returnByteCounter > 9){
      returnByteCounter = 0;
    }
    // if (qwery == returnCodes[3]){
    //   return(returnCodes[6]);  
    // }
  }
  return(returnCodes[6]); 
}

//play a specific track number
void playTrack(int tracknum){
  sendDFCommand(0x03, tracknum);
}

//Проиграть трек с номером fileNumber
// Имя папки должно иметь название «MP3» 
//количество файлов 1-9999. Файлы в папке начинаются с  4 Разрядного номера 0001
void playMp3Folder(int fileNumber){
  sendDFCommand(0x12, fileNumber);
}

//plays the next track
void nextTrack(){
  sendDFCommand(0x01, 0);
}
//volume increase by 1
void volumeUp() {
  sendDFCommand(0x04, 0);
}
//volume decrease by 1
void volumeDown() {
  sendDFCommand(0x05, 0);
}
//set volume to specific value
void setVolume(int thevolume) {
  sendDFCommand(0x06, thevolume);
  delay(100);
}

// Получение текущего уровня громкости
uint8_t getVolume(){
  sendDFCommand(0x43, 0);
 return(answerCommand(0x43));
}

