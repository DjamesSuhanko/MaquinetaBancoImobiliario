//#include <Arduino.h> //inclua se utilizar VS Code
#include <SPI.h>
#include <MFRC522.h>
#include <OneWire.h>
#include <Keypad.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

#define SS_PIN  53
#define RST_PIN 46
#define BANK     1
#define BITCOIN  2

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 //4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

const uint8_t NUM_OF_PLAYERS = 6;
const byte ROWS              = 4; 
const byte COLS              = 4;

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {13, 12, 11, 10}; 
byte colPins[COLS] = {9, 8, 7, 6};

bool moneyValue = false; //usado para indicar ao keypad qndo imprimir valores R$

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

MFRC522::MIFARE_Key key; 

OneWire ibutton(2);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Init array that will store new NUID 
byte nuidPICC[4];

String player                             = ""; //rfid do jogador
String tag                                = ""; //tag do jogador
String players[NUM_OF_PLAYERS]            = {""}; //array de jogadores
String tags[NUM_OF_PLAYERS]               = {""}; //array das tags dos jogadores

String bitcoin_value                      = ""; //guarda o valor atual do bitcoin       
unsigned long int bitcoin[NUM_OF_PLAYERS] = {0};//guarda o valor investido em bitcoin por cada player

String transfer_to                        = "";  //transferir dinheiro para esse rfid

unsigned long int saldos[NUM_OF_PLAYERS]; //saldos dos jogadores
String on_invest[NUM_OF_PLAYERS]; //guarda o valor do bitcoin quando o jogador investiu

bool card_is_same      = false;
bool game_started      = false;
bool operation_started = false;

void printDec(byte *buffer, byte bufferSize);
String printHex(byte *buffer, byte bufferSize);

void playerHandle(String target);

String getBitcoinValue();

//MRFC522
void readCard();

//iButton
void iButtonGetValue();

//keypad
String readKeyPad();

//Pega o player pela tag relacionada
void getPlayerFromTag(String tagToSearch);

//cadastrar os jogadores - chamar em setup()
void start();

//gerencia o teclado
void operations(uint8_t op, uint8_t idx);

//executa operacao com cartão
void transaction(String op,String from);

//texto no display
void testscrolltext(void);

void setup() { 
    Wire.begin();

    for (uint8_t i=0;i<NUM_OF_PLAYERS;i++){
        saldos[i] = 2558000;
    } 
    Serial.begin(9600);
    Serial1.begin(9600); //serial com o esp-01
  
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }

    SPI.begin(); // Init SPI bus
    rfid.PCD_Init(); // Init MFRC522 

    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }  

    Serial.println(F("This code scan the MIFARE Classsic NUID."));
    Serial.print(F("Using the following key:"));
    printHex(key.keyByte, MFRC522::MF_KEY_SIZE);
    start();
}
 
void loop() {
    readCard();
    iButtonGetValue();
}


/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
String printHex(byte *buffer, byte bufferSize) {
  String full_value = "";
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
    full_value = full_value + String(buffer[i]) + " ";
  }
  return full_value;
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

void readCard(){
 // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial()){
      return;
  }
    

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] || 
    rfid.uid.uidByte[1] != nuidPICC[1] || 
    rfid.uid.uidByte[2] != nuidPICC[2] || 
    rfid.uid.uidByte[3] != nuidPICC[3] ) {
    Serial.println(F("A new card has been detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
   
    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));

    player = printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.print("\nPLAYER: ");
    Serial.println(player);
    Serial.println();
    playerHandle(player);

    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
  }
  else{
    Serial.println(F("Card read previously."));
    //TODO: quem cadastrou por ultimo é o ultimo, sem problemas.
    //Quando fizer a operação, a confirmação cairá aqui. Criar um verificador String card_verify
    //e a variavel que guardara o valor da operacao, positivo ou negativo
    card_is_same = true;
  } 

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

void iButtonGetValue(){ //TODO: montar a string e alimentar a string tag
    uint8_t *reading = new uint8_t [8];
    if (ibutton.search(reading)){
    Serial.println(*reading);
        if (reading[0] == 1){
            Serial.print("Tag: ");
            for (uint8_t i=4;i>0;i--){
                Serial.print(reading[i],HEX);
                Serial.print(" ");
            }

            tag = String(reading[4]) + " " + String(reading[3]) + " " + String(reading[2]) + " " + String(reading[1]); 
            Serial.println(" ");
            getPlayerFromTag(tag);
            delay(1000);
        }
    }
    ibutton.reset();
    delete [] reading;
    delay(10);
}

void playerHandle(String target){
    for (uint8_t i=0;i<NUM_OF_PLAYERS;i++){
        if (players[i].length() < 5) {
            break; //se entrou em um indice vazio, não tem o target. 
            Serial.println("Nao encontrado - 210 playerHandle");
        }
        
        //Se houve segunda leitura, é transferência
        if (operation_started && players[i].equals(target)){
            transfer_to = target;
        }
        //INICIO DE OPERACAO. Primeira leitura
        else if (players[i].equals(target)){
            Serial.println("Operacao:");
            Serial.println("A - Saldo\nB - Pagar\nC - Receber\nD - De/Para");
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println("A Saldo");
                display.println("B Pagar");
                display.println("C Receber");
                display.println("D De/Para");
                display.display();
            String opt = readKeyPad();
            moneyValue = true;
            transaction(opt,target);
        }
        //Se não é início de operação, o cartão da segunda leitura não está
        //cadastrado. Retorna 0.
        //else {
        //    transfer_to = "INVALID";
        //}
    }
}

//Função genérica que retorna um String quando # for apertado.
String readKeyPad(){
    char customKey = customKeypad.getKey();
    String rs_value = ""; // armazena valor a transferir ou receber
    String value = "";
    uint8_t pos = 0;
    String choice = "";
    while (customKey != '#'){ 
        if (customKey){
            if (moneyValue){
                    display.clearDisplay();
                    display.setCursor(0, 0);
                    display.println("Valor:");
                    if (customKey < 65 || customKey > 68){
                        rs_value = rs_value + customKey;
                    }
                    
                    display.println(rs_value);
                    display.display();
                    pos +=1;
            }
            else{
                choice = "Opcao " + String(customKey);
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println(choice);
                display.println("# confirma");
                display.println("* limpa");
                display.display();
            }

            if (customKey == '*')
            {
                Serial.println("matriz limpa");
                value = "";
            }
            else
            {
                Serial.print(customKey);
                value = value + customKey;
            }
        }
        customKey = customKeypad.getKey();
    }
    moneyValue = false;
    return value;
    
}

void start(){
    uint8_t i = 0;
    String player_x = "";
    delay(200);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(":Cadastro:");
    display.println("RFID & iBT");
    display.display();

    while (!card_is_same){
        readCard();
        //se deu outra volta, incrementou i.
        //se o cartão é o mesmo, então encerrou o cadastro
        players[i] = player;  

        if (player.length() >2){
            display.clearDisplay();
            display.setCursor(0, 0);
            player_x = "Jogador " + String(i+1);
            display.println(player_x);
            display.println("iButton...");
            display.display();
        }
        

        if (player.length() > 3){
            //se o card é diferente do anterior (seja "" ou número diferente), cadastrar a tag:
            while (tag == ""){
                iButtonGetValue(); //coloca o valor lido em tag, se tiver algum
                tags[i] = tag;
                if (tag.length() > 2){
                    display.clearDisplay();
                    display.setCursor(0, 0);
                    display.println(player_x);
                    display.println("iBT ok");
                    display.display();
                }
            }
            tag    = "";
            player = "";
            i++;
        }
    }
            
    card_is_same = false;
    Serial.println("Cadastro finalizado"); 
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Banco");
    display.println("operando");
    display.println("Bom jogo");
    display.display();

    game_started = true;

}

void getPlayerFromTag(String tagToSearch){
    for (uint8_t i=0;i<NUM_OF_PLAYERS;i++){
        if (tags[i] == tagToSearch){
            Serial.print("PLAYER DA TAG: ");
            Serial.println(players[i]);
            if (game_started){
                operations(BITCOIN,i);
            }
            break;
        }
    }
    
}

//op: BANK ou BITCOIN
void operations(uint8_t op, uint8_t idx){
    if (!game_started) return;
    String val = "";
    Serial.println("0 - Saldo \n1 - Investir\n2 - Retirar");

    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    bitcoin_value = "";
    while (bitcoin_value.charAt(0)-48 < 0 || bitcoin_value.charAt(0)-48 > 9){
        bitcoin_value = getBitcoinValue();
        delay(1000);
        Serial.println("tentando obter o valor");
    }
    Serial.print("BTC (now):   ");
    Serial.print(bitcoin_value);
    

    if (op == BITCOIN){
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("0: Saldo");
        display.println("1: Investe");
        display.println("2: Retira");
        display.display();
        val = readKeyPad();

        if (val == "0"){
            Serial.println(bitcoin_value);
            display.clearDisplay();
            display.setCursor(0, 0);
            String bitcoin_value_msg = "BTC " + bitcoin_value.substring(0,6);
            display.println(bitcoin_value_msg);

            //TODO: fazer o ganho corretamente. a conta abaixo sai a porcentagem correta
            int gain = bitcoin_value.toInt()/on_invest[idx].toInt()-1;
            Serial.print("on_invest");
            Serial.println(on_invest[idx]);
            String older = "OLD " + on_invest[idx].substring(0,6);
            display.println(older);
            Serial.print("em inteiro: ");
            Serial.println(on_invest[idx].toInt());
            Serial.print("index idx: ");
            Serial.println(idx);

            //ganha a diferença vezes o numero de investimentos teste: 285750
            long int lucro = (bitcoin_value.toInt()-on_invest->toInt())*(bitcoin[idx]/200000);
            bitcoin[idx] = bitcoin[idx] + lucro;

            //gain = on_invest[idx].toInt() / 100 * gain;
            //uint32_t lucro  = bitcoin[idx] + (bitcoin[idx]/100*gain);
            Serial.print("valor investido:   ");
            Serial.println(bitcoin[idx]);
            Serial.print("% ganho/perda:   ");
            Serial.println(gain);

            String saldo = "S   " + String(bitcoin[idx]);
            display.println(saldo);
        
            display.display();
        }
        else if (val == "1"){
            display.clearDisplay();
            display.setCursor(0, 0);

            bitcoin[idx] += 200000;
            Serial.print("Investido: ");
            display.println("Investido:");
            Serial.println(bitcoin[idx]);
            display.println(String(bitcoin[idx]));
            display.display();
            on_invest[idx] = bitcoin_value;
        }
        else if (val == "2"){
            display.clearDisplay();
            display.setCursor(0, 0);
            Serial.println("Retira BTC");
            display.println("Retira BTC");
            String transfer = "<-> " +String(bitcoin[idx]);
            display.println(transfer);
            saldos[idx] += bitcoin[idx];
            bitcoin[idx] = 0;
            String money = "S " + String(saldos[idx]);
            display.println(money);
            display.display();

        }
        else{
            Serial.println("Jamais voce deve ver isso ");
            Serial.println(val);
        }
    }
    Serial.print("keypad: ");
    Serial.println(val);
}

//TODO: função de operação com cartão:
//A - saldo
//B - de/para
//C - receber (banco)
//D - pagar (banco)
//
void transaction(String op,String from){
    int idx = -1;
    for (uint8_t j=0;j<NUM_OF_PLAYERS;j++){
        if (players[j] == from){
            idx = j;
            Serial.println("A C H A D O");
            Serial.println(idx);
            break;
        }
    }
    
    Serial.print("transaction: idx da origem: ");
    Serial.println(idx);
    Serial.print("valor na string from: ");
    Serial.println(from);

    Serial.print("player 0: ");
    Serial.println(players[0]);
    Serial.print("player 1: ");
    Serial.println(players[1]);
    
    String value;
    if (op == "A"){   
        Serial.print("\nSaldo: ");
        Serial.println(saldos[idx]);

        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Saldo (R$)");
        display.println(String(saldos[idx]));
        display.display();
    }
    else if (op == "B"){
        Serial.println("P A G A M E N T O");
        operation_started = true;
        value = readKeyPad();
        if (saldos[idx] < (unsigned long) value.toInt()){
            Serial.println("Sem saldo:");
            Serial.println(saldos[idx]);

            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println("Sem saldo");
            display.println(String(saldos[idx]));
            display.display();

            operation_started = false;
            return;
        }
        unsigned long debit = value.toInt();
        saldos[idx] = saldos[idx] - debit;
        Serial.print("Debitar: ");
        Serial.println(debit); 
        Serial.print("Saldo (-): ");
        Serial.println(saldos[idx]);

        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Debito (-)");
        display.println(String(debit));
        display.println("Saldo:");
        display.println(String(saldos[idx]));
        display.display();

        operation_started = false;

    }
    else if (op == "C"){
        Serial.println("R E C E B I M E N T O");
        operation_started = true;
        value = readKeyPad();

        unsigned long credit = value.toInt();
        saldos[idx] = saldos[idx] + credit;
        Serial.print("Saldo (+)");
        Serial.println(saldos[idx]);

        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Recebe:");
        display.println(String(credit));
        display.println("Saldo (+)");
        display.println(String(saldos[idx]));
        display.display();

        operation_started = false;

    }
    else if (op == "D"){ // <<<<< B é banco. corrigir
        operation_started = true;
        Serial.print("\nValor: ");
        /*
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Valor:");
        display.println(String(credit));
        display.println("Saldo (+)");
        display.println(String(saldos[idx]));
        display.display();*/

        value = readKeyPad();
  
        Serial.println("\nPara:");
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Para...");
        display.display();

        //leitura do beneficiado
        while (transfer_to.length() < 5){
            readCard();
        }
        
        if (transfer_to.equals("INVALID")){
            Serial.println("\nInvalido");
        }
        else{
            int to = -1;
            for (uint8_t j=0;j<NUM_OF_PLAYERS;j++){
                if (players[j] == transfer_to){
                    to = j;
                    break;   
                }
            }
            if (saldos[idx] < value.toInt()){
                Serial.println("Sem saldo:");
                Serial.println(saldos[idx]);
                operation_started = false;
                return;
            }
            saldos[idx] = saldos[idx]-value.toInt();
            saldos[to]  = saldos[to]+value.toInt();


            String saldo_to = "(+)" + String(saldos[to]);
            String money_to = "Jogador " + String(to);
            String saldo_idx = "(-)" + String(saldos[idx]);

            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 1);
            
            display.println(money_to);

            
            display.println(saldo_to);
            display.println("Pagador:");
            display.println(saldo_idx);
            display.display();
            
            Serial.print("Saldo (-): ");
            Serial.println(saldos[idx]);
            Serial.print("Saldo (+): ");
            Serial.println(saldos[to]);
            transfer_to = "";
            operation_started = false;

            Serial.println(to);
            Serial.println(idx);
            Serial.println(players[0]);
            Serial.println(players[1]);
        }
    }
}

void testscrolltext(void) {
  display.clearDisplay();

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(bitcoin_value);
  display.display();      // Show initial text
  delay(100);

  // Scroll in various directions, pausing in-between:
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
  delay(1000);
}

String getBitcoinValue(){
    Serial1.println("@");
    String btc = "";
    delay(100);
    bitcoin_value = "";
    //3 tentativas
    for (uint8_t i=0;i<3;i++){
        while (Serial1.available()){ 
            char c = Serial1.read();
            btc = btc + c;
        }

        if (btc.length() > 5){
            return btc;
        }
    }
    return "Repita";
}
