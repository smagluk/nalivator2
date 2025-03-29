//#include <SPI.h>
#include <TFT_eSPI.h>               // Библиотека экрана 
#include <ezButton.h>               //  Библиотека для использования SW  пина энкодера
#include "ESP32Encoder.h"           //  Библиотека энкодера
#include "esp_task_wdt.h"
#include <DFMiniMp3.h>            //  Библиотека работы с мелодиями dfplayer
#include <Adafruit_NeoPixel.h>      //  Библиотека адресной ленты
//#include "ESP32Servo.h"             //  Библиотека работы с сервомотором
#include <ServoSmooth.h>            //  Библиотека работы с сервомотором


//#include "driver/gpio.h"  //  Библиотека для работы с GPIO портами ввода-вывода
 
#define TFT_GREY 0x5AEB
#define TFT_LIGHT_BLUE 0x261B
#define SW_PIN 15                 //  пин GPIO15 ESP32 назначаем для кнопки KEY энкодера
#define S2_PIN 19                 //  пин GPIO19 ESP32 назначаем для S2 выхода энкодера
#define S1_PIN 4                  //  пин GPIO4 ESP32 назначаем для S1 выхода энкодера
#define SHORT_PRESS_TIME 1000     //  1000 milliseconds
#define LONG_PRESS_TIME 1000      //  1000 milliseconds

//======= Задачи ==================================================================================
TaskHandle_t Task1; // Задача мигания 6 светодиодом светодиодной лентой
TaskHandle_t Task2; // Задача работы с опросом кнопок и энкодера
TaskHandle_t Task3; // Задача работы с адресной лентой

//======= Семафоры ================================================================================
SemaphoreHandle_t CvetoMuzikSemaphore = NULL; // Семафор для задачи CvetoMuzik  

//======= Адресная лента ==========================================================================
#define PIN_WS2812B	13  //  Пин подключения адресной ленты
#define NUM_PIXELS  6   //  Колличество светодиодов в адресной ленте WS2812b, оно же равно колличеству рюмок
Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800); //  Создаем объект адресной ленты

//======= Концевики ===============================================================================
const uint8_t SW_pins[] = {32, 33, 25, 26, 27, 35};             //  пины концевиков ESP32 для 6 стопок
static bool sw_state[NUM_PIXELS] = {false, false, false, false, false, false};   // массив  состояний  концевиков после налива в рюмки(когда отработала помпа) 
static bool pins_state[NUM_PIXELS] = {false, false, false, false, false, false}; // массив состояний концевиков, есть или нет рюмки

//======= Серва ===================================================================================
ServoSmooth myservo;   
//Servo myservo;                    //  создаем servo object to control a servo
//uint32_t servo_time_work  = 1000; //  Время задержки перед включением помпы необходимое для поворота сервы
#define SERVO_ATTACH  14          //  attaches the серва на пин 14 to the servo object
const uint8_t Pos[] = {15, 45, 87, 133, 144, 177, 0};  //  массив,в котором хранятся позиции сервы, в последней переменной хранится позиция парковки

//======= Плейер ==================================================================================
// #define PLAYER_SERIAL_TIMEOUT 1500  //таймаут(мс) ожидания данных с сериал порта плеера, если не верно читает количество треков или глюки, пробуем увеличивать сразу до 2000 мс
// #define PLAYER_MH2024K_24SS  // тормозной плеер, PLAYER_SERIAL_TIMEOUT увеличиваем до 1500, вводятся задержки для нормальной работы плеера, в работе наливатора будут некоторые затупы
// DFMiniMp3<HardwareSerial, Mp3ChipOriginal> myMP3(Serial2);
// forward declare the notify class, just the name
class Mp3Notify;
// define a handy type using serial and our notify class
typedef DFMiniMp3<HardwareSerial, Mp3Notify> DfMp3;
// instance a DfMp3 object, 
DfMp3 dfmp3(Serial2);
#define BUSY_PIN 34 //  пин готовности DF плеера высокий уровень когда играет песня, и низкий когда не играет, не подключать на gpio12!!  
#define RXD2 16
#define TXD2 17
class Mp3Notify
{
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action)
  {
    if (source & DfMp3_PlaySources_Sd) 
    {
        Serial.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb) 
    {
        Serial.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash) 
    {
        Serial.print("Flash, ");
    }
    Serial.println(action);
  }
  static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track)
  {
    Serial.print("Play finished for #");
    Serial.println(track);  

    // start next track
    track += 1;
    // this example will just start back over with 1 after track 3
    if (track > 3) 
    {
      track = 1;
    }
    dfmp3.playMp3FolderTrack(track);  // sd:/mp3/0001.mp3, sd:/mp3/0002.mp3, sd:/mp3/0003.mp3
  }
  static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
  }
};




//======= TFT ===================================================================================== 
#include "TimesNRCyr8.h"
#include "TimesNRCyr16.h"
#define TNR20 &TimesNRCyr16pt8b
#define TNR8 &TimesNRCyr8pt8b
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
TFT_eSprite sprite = TFT_eSprite(&tft); // Sprite объект для начальной заставки
//TFT_eSprite img = TFT_eSprite(&tft); // Sprite объект для начальной заставки

//======= Энкодер =================================================================================
// Обязательно на сигнальные провода энкодера вешать керамические конденсаторы 0.1мкф
bool isPressing = false;
bool isLongDetected = false;
unsigned long pressedTime  = 0; //  Время нажатия на кнопку энкодера
unsigned long releasedTime = 0; //  Время отпускания кнопки энкодера
ezButton button(SW_PIN);        //  создаем обьект ezButton на 15 пине;
// Обработчик прерываний
static IRAM_ATTR void enc_cb(void* arg) {
  ESP32Encoder* enc = (ESP32Encoder*) arg;
}
ESP32Encoder encoder(true, enc_cb);
extern bool loopTaskWDTEnabled;
extern TaskHandle_t loopTaskHandle;
static const char* LOG_TAG = "main";

//======= Помпа ===================================================================================
#define PUMP_PIN 12               //  Пин GPIO14 ESP32 назначаем для помпы. 
uint32_t pump_time_work  = 3000;  //  Время работы помпы
uint32_t servo_time_work = 1000; //  Время задержки перед включением помпы необходимое для поворота сервы

//======= Функция налива ==========================================================================
//  Функция налива включает помпу на установленное время для налития всех рюмок
//  Во время налива играет мелодия и включается светомозыка
//  После окончания налива выключается светомозыка и индикатор наполнености рюмок светится красным 
//  Проверяет , если рюмку не убирали , то налива не происходит 
void naliv() {
  tft.fillScreen(TFT_BLACK);  // Заливает экран черным цветом (очищаем)
  tft.setTextSize(2);
  tft.drawString("Наливаем по 50 мл", 30, 20); 
  tft.drawString("в  -ю рюмку        %", 30, 50); 
  for (int i = 0; i < NUM_PIXELS; i++) {     // Перебираем все рюмки
  //  myservo.tick();   // здесь происходит движение серво по встроенному таймеру!
  //   if(!digitalRead(SW_pins[i])){
      if (sw_state[i])     // Если рюмку не убирали
      {
        //       lcd.clear();
        //       lcd.setCursor(3, 0);              // Устанавливает курсор в (позицию,Строка)
        //       lcd.print("РЮМКА "); 
        //       lcd.print(i+1, DEC);          // Печатаем номер рюмки в которую льем
        //       lcd.setCursor(3, 1);              // Устанавливает курсор в (позицию,Строка) 
        //       lcd.print("налита"); 
        //       delay(1000);
        //       lcd.clear();
        //       lcd.setCursor(2, 0);         // Устанавливает курсор в (Позиция,Строка) 
        //       lcd.print("Смените рюмку");
        //       delay(1000);
        //       lcd.clear();
        //       lcd.setCursor(0, 0);              // Устанавливает курсор в (позицию,Строка)
        //       lcd.print("HAЛИBAЮ  по 50MЛ"); 
        //       lcd.setCursor(0, 1);              // Устанавливает курсор в (позицию,Строка) 
        //       lcd.print("B  -Ю РЮMKУ    %"); 
      }else
      {
        tft.drawNumber( i+1, 50, 50);        // Устанавливает курсор в (Позиция,Строка)  и Печатаем номер рюмки в которую льем
        myservo.setTargetDeg(Pos[i]);
        while(myservo.tick() == 0){};         // не понятная конструкция, но с ней работает
        //myservo.write(Pos[i]);                // Предвигаем серву в позицию " i "
        delay(servo_time_work);               // Время задержки перед включением помпы
        digitalWrite(PUMP_PIN, HIGH);         // ВКЛЮЧАЕМ помпу
        int pump_time_work_start = millis();  // Время начала налива
        // Пока концевик не отпущен и время налива не закончилось крутим цикл в котором помпа работает
        // И выводим на экран проценты налива
        int t = 0; // Переменная для подсчета времени
        t =  pump_time_work + pump_time_work_start; 
        while((millis() < t) && pins_state[i]){
          int p = map(millis(), pump_time_work_start, t, 0, 100); // Переменная для вывода процентов
          tft.drawNumber( p, 210, 50);       // Устанавливает курсор в позицию отображенния процентов налива
          delay(100);
         };
        tft.drawNumber( 100, 210, 50);       // Устанавливает курсор в позицию отображенния процентов налива при окончании налива
        digitalWrite(PUMP_PIN, LOW);          // ВыКЛЮЧАЕМ помпу
        sw_state[i] = true;                   //  обновляем состояние концевика
        delay(1000); 
        tft.fillRect(210, 51, 50, 28, TFT_BLACK); 
        //tft.drawNumber( 0, 210, 50);       // Устанавливает курсор в позицию отображенния процентов налива при окончании налива
      }  
    }
    myservo.setTargetDeg(Pos[6]);
    //myservo.write(Pos[6]);            //  возвращаем серву в парковочную позицию        
}

//======= Функция вывод на экран уровня громкости =================================================
// Функция увеличивает или уменьшает громкость,
// и выводит на экран индикатор уровеня громкости 
void volum_level_new( bool step){
  //Получаем уровень громкости из плеера, хотя лучше использовать статическую переменную
  tft.fillScreen(TFT_BLACK);  // Заливает экран выбранным цветом
  int i=0;
  //int volume=0;
  int volume  = dfmp3.getVolume();  // Получаем уровень громкости
  for ( int i = 0; i = volume; i++){
    tft.drawSmoothRoundRect((10*i)+1, 160-(3*i), 3, 0, 3, (3*i)+3, TFT_LIGHT_BLUE, TFT_LIGHT_BLUE);
  }
  
  if (step)  {
    if (volume< 30) ++volume;
    tft.drawSmoothRoundRect((10*volume)+1, 160-(3*volume), 3, 0, 3, (3*volume)+3, TFT_LIGHT_BLUE, TFT_LIGHT_BLUE);  
    // Рисуем новую индикаторную палочку громкости
    //dfmp3.increaseVolume();  //  Увеличивавем громкость динамика 
    Serial.println(volume);
    //delay(1000);
  }
  else if (volume>0) {
    // Затираем последнюю индикаторную палочку громкости
    tft.drawSmoothRoundRect((10*volume)+1, 160-(3*volume), 3, 0, 3, (3*volume)+3, TFT_BLACK, TFT_BLACK);
    --volume;
    //dfmp3.decreaseVolume();      //  Уменьшаем громкость динамика
    Serial.println(volume);
    //delay(3000);
  }

  tft.setTextSize(2);
  tft.drawString("Громкость", 30, 180); 
  // рисовать прямоугольник который затирает старое значение
  tft.fillRect(240, 180, 50, 30, TFT_BLACK);
  tft.drawNumber(volume, 240, 180);
  //tft.drawSmoothRoundRect(10, 160, 3, 0, 3, 15, TFT_WHITE, TFT_WHITE);  //  Выводит на экран квадрат с ккординатами верхняя точка X, Y, ширина, высота, радиус, цветом 
  tft.setTextSize(1);
}
 
//======= Функция опроса энкодера =================================================================
//  Возвращает:       
//  0 - ничего не нажато 1 - короткое нажатие 2 - длинное нажатие 3 - поворот вправо  4 - поворот влево
int opros_encoder(){ 
  int8_t otvet = 0;
  button.loop();  // опрос кнопки энкодера
  if(button.isPressed()){  // Кнопка энкодера нажата
    pressedTime = millis();
    isPressing = true;
    isLongDetected = false;
    }     
  if(button.isReleased()){  // Кнопка энкодера отпущена
    isPressing = false;
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if ( pressDuration < SHORT_PRESS_TIME ){
      //Serial.println("Короткое нажатие зафиксировано"); 
      otvet = 1;
    }
  }
  if (isPressing == true && isLongDetected == false) {
    long pressDuration = millis() - pressedTime;
    if ( pressDuration > LONG_PRESS_TIME ) {
      //Serial.println("Длинное нажатие определено");
      isLongDetected = true;
      otvet = 2;
    }
  }
  delay(10); 
  int i=encoder.getCount();
  if (i>0){  //  Вращение вправо
    //Serial.println("Повернули вправо");
    otvet = 3;
  }
  else if (i<0){ //Вращение влево
   // Serial.println("Повернули влево");
    otvet = 4;
  }
  encoder.clearCount(); //  обнуляем энкодер 
  return(otvet);
}

//======= Функция светомозыки =====================================================================
void CvetoMuzik() {
//  int j=0;
//   while (j<10){   // Времннно!!!!!!!!!!!!!!!!!!!!!!! вернуть нижнюю строчку
//   j++;
  while (!digitalRead(BUSY_PIN)){   // Пока проигрывает музыка выполняем 
  // Rainbow cycle along whole ws2812b. Pass delay time (in ms) between frames.
  // Hue of first pixel runs 3 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 3*65536. Adding 256 to firstPixelHue each time
  // means we'll make 3*65536/256 = 768 passes through this outer loop:
    for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
      for(int i=0; i<ws2812b.numPixels(); i++) { // For each pixel in ws2812b...
        // Offset pixel hue by an amount to make one full revolution of the
        // color wheel (range of 65536) along the length of the ws2812b
        // (ws2812b.numPixels() steps):
        int pixelHue = firstPixelHue + (i * 65536L / ws2812b.numPixels());
        // ws2812b.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
        // optionally add saturation and value (brightness) (each 0 to 255).
        // Here we're using just the single-argument hue variant. The result
        // is passed through ws2812b.gamma32() to provide 'truer' colors
        // before assigning to each pixel:
        ws2812b.setPixelColor(i, ws2812b.gamma32(ws2812b.ColorHSV(pixelHue)));
      }
      ws2812b.show(); // Update ws2812b with new contents
      delay(1);  // Pause for a moment
    }
  }
}

//======= Функции задач  ==========================================================================
//Task1code: мигает светодиодом на ленте раз в секунду
void Task1code( void * pvParameters ){
  for(;;){
    // ws2812b.setPixelColor(5, ws2812b.Color(0, 200, 0));
    // ws2812b.show();   // Send the updated pixel colors to the hardware.
    // delay(1000);
    // ws2812b.setPixelColor(5, ws2812b.Color(200, 0, 0));
    // ws2812b.show();   // Send the updated pixel colors to the hardware.
    delay(10);
  } 
  vTaskDelete(NULL);  // Удалить запущенную задачу
}

//Task2code: Опрашивает кнопки и энкодер
//======= Опрос концевиков ========================================================================
// Опрашивает концевики  синий цвет если рюмки нет, красный цвет если рюмка стоит пустая, зеленый если был вызов функции налив и концевик на обнулялся
// возвращает состояние концевиков false - если рюмка стоит 
// В результате работы функции получаем глобальный массив состояний концевиков sw_state[5]
// Статическая переменная pins_state[5] запоминает состояние концевиков
//
void Task2code( void * pvParameters ){ 
  for(;;){

    for (int i = 0; i < NUM_PIXELS; i++) {      // опрашиваем концевики
      if(digitalRead(SW_pins[i])){              // если концевик нажат(логический 1) то это стоит рюмка
        
        if (!pins_state[i]) {                   // если рюмки не было выполняем воспроизведение мелодии "дзынь"
        //  Serial.println("Будем проигрывать мелодию Дзынь");
        //  dfmp3.playAdvertisement(1); // sd:/advert/0001.mp3 воспроизводит мелодию "Дзынь" установка рюмки из рекламной папки advert  
          //Serial.println("Проиграли мелодию Дзынь");
          pins_state[i] = true;                 // поставили рюмку 
        }  
        
        if(sw_state[i]){                        // состояние концевика(рюмка была ) true , состояние меняется в функции Naliv()
          ws2812b.setPixelColor(i, ws2812b.Color(0, 128, 0));  // указываем зеленый цвет пикселя  
        }else{
          ws2812b.setPixelColor(i, ws2812b.Color(250, 0, 0));  // указываем красный цвет пикселя когда рюмка стоит пустая
        }  
      }else {
        ws2812b.setPixelColor(i, ws2812b.Color(0, 0, 250));  // указываем синий цвет пикселя
        sw_state[i] = false; // обнуляем состояние концевика.
        pins_state[i] = false; 
      } 
    }
     ws2812b.show();  //  Передаем состояние пикселей в ленту
    delay(10);
  }
  vTaskDelete(NULL);  // Удалить запущенную задачу
}

// Task3code: Вызывает функцию CvetoMuzik() каждый раз, когда
//           проходит 10 мс.
//  Если в задаче больше нет надобности, нужно явно удалить запущенную задачу с помощью vTaskDelete(NULL);.
void Task3code( void * pvParameters ){ 
  for(;;){
    //CvetoMuzik();
    delay(10);
  }
  vTaskDelete(NULL); // Удалить запущенную задачу
}

//===== Функция начальной заставки ================================================================
void StartScreen() {
  int x = 10, y = 0; 
  sprite.setColorDepth(8);  // 8-bit цвет для спрайта
  sprite.setAttribute(UTF8_SWITCH, false);
  sprite.setFreeFont(TNR20);
  sprite.setTextColor(TFT_LIGHT_BLUE, TFT_BLACK);  // Adding a background colour erases previous text automatically
  //sprite.setTextSize(1);
  sprite.createSprite(270, 80);
  for (int y = 0; y < 50; y++) {
    sprite.fillSprite(TFT_BLACK);
    sprite.drawString("НАЛИВАТОР 2.0", x, y);   
    sprite.pushSprite(x, y);
    delay(100);
  }
  sprite.deleteSprite();
}

void setup(void) {
  Serial.begin(115200);
  //===== Энкодер =================================================================================
  loopTaskWDTEnabled = true;
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachSingleEdge( S2_PIN, S1_PIN);
  encoder.clearCount();
  encoder.setFilter(1023);

  //===== Экран ===================================================================================
  tft.init();
  tft.setAttribute(UTF8_SWITCH, false);
  //метод setFreeFont принимает ссылку на пользовательский шрифт
  tft.setFreeFont(TNR8);
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK); 
  tft.setTextColor(TFT_LIGHT_BLUE, TFT_BLACK);  // Adding a background colour erases previous text automatically


  //===== Лента адресных светодиодов ==============================================================
  pinMode(PIN_WS2812B, OUTPUT);
  ws2812b.begin();  //  инициализация адресной ленты WS2812B 

  //===== Серва ===================================================================================
  pinMode(SERVO_ATTACH, OUTPUT);
  myservo.attach(SERVO_ATTACH); //прикрепляет сервопривод на пин 14 к сервообъекту
  myservo.setAutoDetach(false);	// отключить автоотключение (detach) при достижении целевого угла (по умолчанию включено)


  //===== Помпа ===================================================================================
  pinMode(PUMP_PIN, OUTPUT);    // настроем пин помпы в качестве выходного вывода
  digitalWrite(PUMP_PIN, LOW);  // ВЫКЛЮЧАЕМ помпу

  //======= Концевики =============================================================================  
  pinMode(SW_pins[4], INPUT);  // настроем пины 5 рюмки в качестве входного вывода    

  //========Плейер=================================================================================
  //pinMode(BUSY_PIN, INPUT_PULLUP);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(BUSY_PIN, INPUT);   // настроем пин готовности DF плеера в качестве входного вывода
  dfmp3.begin();
  delay(10);
  dfmp3.reset();  // сброс DF плеера
  
  uint16_t version = dfmp3.getSoftwareVersion();
  Serial.print("version ");
  Serial.println(version);

  uint16_t volume = dfmp3.getVolume();
  // Serial.print("volume ");
  // Serial.println(volume);
  dfmp3.setVolume(24);
  
  uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  // Serial.print("files ");
  // Serial.println(count);
  
  // Serial.println("starting...");

  // start the first track playing
  dfmp3.playMp3FolderTrack(1);  // sd:/mp3/0001.mp3
  // myMP3.begin(9600, PLAYER_SERIAL_TIMEOUT);  // скорость порта, таймаут порта мс.
  // delay(1000);
  // myMP3.setEq(DfMp3_Eq_Normal);
  // delay(100);                                  //Между двумя командами необходимо делать задержку 100 миллисекунд, в противном случае некоторые команды могут работать не стабильно.
  // myMP3.setVolume(20);
  // delay(100);
  // myMP3.playFolderTrack(1, 1);
  // delay(100);

  //===== Семафоры ================================================================================
  CvetoMuzikSemaphore = xSemaphoreCreateBinary();

  //===== Задачи ==================================================================================
  //создаем задачу, которая будет выполняться на ядре 1 с максимальным приоритетом (1)
  //Если значение равно tskNO_AFFINITY, созданная задача не привязана ни к какому процессору, и планировщик может запустить ее на любом доступном ядре.
  xTaskCreatePinnedToCore(
    Task1code,   /* Функция задачи. */
    "Task1",     /* Ее имя. */
    10000,       /* Размер стека функции */
    NULL,        /* Параметры */
    1,           /* Приоритет */ 
    &Task1,      /* Дескриптор задачи для отслеживания */
    1);          /* Указываем номер ядра для данной задачи */                  
 
//Создаем задачу, которая будет выполняться на ядре 0 с наивысшим приоритетом (1)
  xTaskCreatePinnedToCore(
    Task2code,   /* Функция задачи. */
    "Task2",     /* Имя задачи. */
    10000,       /* Размер стека */
    NULL,        /* Параметры задачи */
    1,           /* Приоритет */
    &Task2,      /* Дескриптор задачи для отслеживания */
    0);          /* Указываем номер ядра для данной задачи */ 

//Создаем задачу, которая будет выполняться на ядре 1 с наивысшим приоритетом (1)
  xTaskCreatePinnedToCore(
    Task3code,   /* Функция задачи. */
    "Task3",     /* Имя задачи. */
    10000,       /* Размер стека */
    NULL,        /* Параметры задачи */
    1,           /* Приоритет */
    &Task3,      /* Дескриптор задачи для отслеживания */
    1);          /* Указываем номер ядра для данной задачи */   
  
  StartScreen();
 }
 
 void loop() {
   // желаемая позиция задаётся методом setTarget (импульс) или setTargetDeg (угол), далее
  // при вызове tick() производится автоматическое движение сервы
  // с заданным ускорением и ограничением скорости
  myservo.tick();   // здесь происходит движение серво по встроенному таймеру!
  int i = opros_encoder();  // опрос энкодера

switch (i) {
    case 1:
      // выполнить, если значение == 1 Это короткое нажатие на энкодер
      //tft.drawString("Это короткое нажатие на энкодер", 0, 0);    
      //dfmp3.playFolderTrack16(5, 1);  
      //Serial.println("Нажата кнопка");
      //myservo.setTargetDeg(Pos[1]);
      //myservo.write(Pos[1]);  
     // myservo.tick();   // здесь происходит движение серво по встроенному таймеру!
      naliv();

    break;
    case 2:
      // выполнить, если значение == 2 Это длинное нажатие на энкодер
      //tft.drawString("Это длинное нажатие на энкодер", 0, 0); 
      //myservo.setTargetDeg(Pos[2]);
      //myservo.write(Pos[2]); 
      //menuEnter(0);  //  Переход в меню
      
    break;
    case 3:
      // выполнить, если значение == 3 Это поворот вправо
      //tft.drawString("Это поворот вправо", 0, 0);
      //myservo.setTargetDeg(Pos[0]);
      //myservo.write(Pos[0]);  
      volum_level_new(1);
    break;
    case 4:
      // выполнить, если значение == 4 Это поворот влево
      //tft.drawString("Это поворот влево", 0, 0);
      //myservo.setTargetDeg(Pos[5]);
      //myservo.write(Pos[5]);  
      volum_level_new(0);
    break;
  }

 }
 


 
