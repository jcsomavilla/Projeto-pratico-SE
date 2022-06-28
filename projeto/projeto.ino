#include <Arduino_FreeRTOS.h>    //inclusão do FreeRtos para arduino
#include <queue.h>               //biblioteca para utlização de filas
#include <task.h>                //biblioteca para utilização de tarefas
#include <semphr.h>              //biblioteca para utilização de semáforos.
#include <Wire.h>                //biblioteca para uso de dispositivos com comunicação I2C.
#include <LiquidCrystal_I2C.h>   //biblioteca do display
#include <Ultrasonic.h>          //biblioteca do sensor ultrasonico

//DEFINIÇÕES

#define BAUDRATE_SERIAL     115200    //Utilizado para debug, no serial monitor

/*LCD*/
#define LCD_16X2_CLEAN_LINE     "                "     //evita efeito de flick, devido a atualizaçao com frequencia muito grande
#define LCD_16X2_I2C_ADDRESS    0x27  // endereço I2C do display
#define LCD_16X2_COLS           16
#define LCD_16X2_ROWS           2
#define LCD_TIMER_VERIFIC       1000 //LCD_TEMPO_ENTRE_VERIICAÇÃO // a cada segundo, a tarefa do display verifica se há novas medições disponíveis.

/*Sensor Ultrassonico*/
#define ULTRASSONICO_TRIGGER        13 // GPIO
#define ULTRASSONICO_ECHO           12 // GPIO
#define ULTRASSONICO_TIMER_LEITURA  500 //ULTRASSONICO_ENTRE_LEITURAS //tempo de verificação de leitura do sensor(a cada meio segundo)

/*Sensor MQ2*/
#define ANALOG_A0           0   //ENTRA_ANALOGICA_AO // 
#define MQ2_TIMER_LEITURA   1000 //MQ2_TEMPO_ENTRE_LEITURAS //utiliza a leitura em 1 segundo

/*Tomada de Controle*/
#define TIMER_SEMPH_WAIT    ( TickType_t ) 100 //TEMPO_PARA_AGUARDAR_SEMAFORO //tempo para aguardar a tomada de controle do semáforo.
#define TIMER_QUEUE_WAIT    ( TickType_t ) 100 //TEMPO_PARA_AGUARDAR_FILA //tempo para aguardar tomada de controle da fila.

/*Filas*/
QueueHandle_t fila_MQ2;       //fila mq2
QueueHandle_t fila_ultrassonico; //fila sensor ultrassonico

/*Semafoto*/
SemaphoreHandle_t semaforo_serial; //semaforo usado para controlar o uso do serial, que é compartilhado entre ambas tarefas.

LiquidCrystal_I2C lcd(LCD_16X2_I2C_ADDRESS, LCD_16X2_COLS, LCD_16X2_ROWS); //controle do lcd, parametros: endereço, coluna e linhas

Ultrasonic ultrassonic(ULTRASSONICO_TRIGGER, ULTRASSONICO_ECHO); //controle do sensor ultrassonico

/*Protótipo das Tarefas*/
/*Todas as tarefas dos dispositivos, retornam void, e também recebem um ponteiro do 
tipo void como parametro. Segue este padrão como requisito para criação de tarefas no FreeRTOS.*/
void task_lcd(void *pvParameters);
void task_MQ2(void *pvParameters);
void task_ultrassonico(void *pvParameters);

void setup() {
  Serial.begin(BAUDRATE_SERIAL); //inicializa o serial

  lcd.init(); //inicializa o LCD
  lcd.backlight(); //liga o backlight
  lcd.clear();// limpa o LCD.

  /*Criação das filas*/
  fila_MQ2 = xQueueCreate( 1, sizeof(int)); //aloca-se um espaço de memória com a função xQueueCreate
  fila_ultrassonico = xQueueCreate(1, sizeof(float));

  /*Validação das filas*/
  if((fila_MQ2 == NULL) || (fila_ultrassonico == NULL)){
    Serial.println("Fila MQ2 ou Ultrasonico não criado.");
    Serial.println("Encerrando o programa.");
    while(1){
      }
    }
  /*Criação do semaforo*/
  semaforo_serial = xSemaphoreCreateMutex(); 

  /*Valida o Semaforo*/
  if (semaforo_serial == NULL){
    Serial.println("Semaforo não criado.");
    Serial.println("Encerrando o programa.");
    while(1){
      }
    }
  /*Tarefas*/ 
  xTaskCreate(
    task_lcd
    ,   (const portCHAR *)"lcd"   //nome
    ,   156                       //tamanho (em palavra)
    ,   NULL                      // parametro passado (como nao possui, usa-se null
    ,   1                         //prioridade da tarefa
    ,   NULL );                   // handle da tarefa (opcional)

  xTaskCreate(
    task_MQ2
    ,   (const portCHAR *) "MQ2"
    ,   156
    ,   NULL
    ,   2
    ,   NULL );

   xTaskCreate(
    task_ultrassonico
    ,   (const portCHAR *)"ultrassonico"
    ,  156  
    ,  NULL
    ,  3 
    ,  NULL );
    
}

void loop() {
  // tudo é realizado pelas tarefas, portando não é necessário programar neste bloco.:

}

void task_lcd(void *pvParameters ){
  float distancia = 0.0; 
  int leitura_MQ2 = 0;
  char linha_str[16] = {0x00}; //formata a linha a ser escrita no display
  int distancia_cm = 0; //mostra somente a parte inteira da distancia, porém o decimal é transportado para fila 

  while(1){
    // xQueuePeek -> "espia" a fila do sensor
    if( xQueuePeek(fila_ultrassonico, &distancia, TIMER_QUEUE_WAIT) ){ //escreve a ultima leitura do sensor
      lcd.setCursor(0,0); //posiciono no começo da linha no display
      lcd.print(LCD_16X2_CLEAN_LINE);//escrevo uma linha "em branco"
      lcd.setCursor(0,0);//reposiciono no começo da linha

      distancia_cm - (int)distancia;
      sprintf (linha_str, "Dist: %d cm", distancia_cm); //formato a escrita no displau
      lcd.print(linha_str); 
      
      }
     if(xQueuePeek(fila_MQ2, &leitura_MQ2, TIMER_QUEUE_WAIT)){
      lcd.setCursor(0,1);
      lcd.print(LCD_16X2_CLEAN_LINE);
      lcd.setCursor(0,1);

      sprintf(linha_str, "MQ2: %d", leitura_MQ2);
      lcd.print(linha_str);
      
      }
      vTaskDelay (LCD_TIMER_VERIFIC / portTICK_PERIOD_MS); // tempo de verificação de atualização do display
      // porTICK_PERIOD_MS converte o tempo setado em ms no LCD_TIMER_VERIFIC em ticks de processador.
    }
  }
void task_MQ2( void *pvParameters){
  int leitura_analog = 0;
  while(1){
      leitura_analog = analogRead(ANALOG_A0);

      //Insere leiturana fila
      xQueueOverwrite(fila_MQ2, (void *)&leitura_analog);

      //escreve a leiturana serial. Tentativa de controle do semafoto é feita
      //até o tempo definido em TIME_SEMPH_WAIT

      if(xSemaphoreTake(semaforo_serial, TIMER_SEMPH_WAIT) == pdTRUE){
        Serial.print("- Leitura MQ-2: ");
        Serial.println(leitura_analog);
        xSemaphoreGive(semaforo_serial); 
      }
      vTaskDelay(MQ2_TIMER_LEITURA / portTICK_PERIOD_MS); //aguarda tempo determinado em MQ2_TIMER_LEITURA para realizar prox leitura;
    }
  }
  
void task_ultrassonico(void *pvParameters){
  float distancia_cm = 0.0;
  long microsec = 0;

  while(1){
    //mede distância em CM
    microsec = ultrassonic.timing(); 
    distancia_cm = ultrassonic.convert(microsec, Ultrasonic::CM);

    //Insere leitura na fila
    xQueueOverwrite(fila_ultrassonico, (void *)&distancia_cm); //fila unitária, prioriza sempre a ultima leitura
    //escreve a leiturana serial. Tentativa de controle do semafoto é feita
    //até o tempo definido em TIME_SEMPH_WAIT

    if(xSemaphoreTake(semaforo_serial, TIMER_SEMPH_WAIT) == pdTRUE){
      Serial.print("- Distancia: ");
      Serial.print(distancia_cm);
      Serial.println("cm");
      xSemaphoreGive(semaforo_serial);
    }

     vTaskDelay(ULTRASSONICO_TIMER_LEITURA / portTICK_PERIOD_MS); 
  }
}
