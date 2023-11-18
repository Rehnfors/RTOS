#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <portable.h>
#include <string.h>

typedef struct Motor_t
{
    bool motorCheck;
    bool GearboxCheck;
    int Speed;
    int Rpm;

} Motor_t;

typedef struct Ventilation_t
{
    bool ventilationCheck;

} Ventilation_t;

typedef struct Fuel_t
{
    bool fuelCheck;
    float fuelGauge;

} Fuel_t;

// Initialize structs
Motor_t MotorStatus = {true, true, 100, 2000};
Ventilation_t VentStatus = {true};
Fuel_t FuelStatus = {true, 10.03};

// TaskHandles
TaskHandle_t Motor_handle, Ventilation_handle, Fuel_handle, Gearbox_handle, Check_handle, Gate_handle;

// QueueHandles
QueueHandle_t motorQueue, ventQueue, fuelQueue, gateQueue, dashQueue;

// SemaphoreHandle
SemaphoreHandle_t xSemaphore = NULL;

void setup()
{
    Serial.begin(9600);

    xSemaphore = xSemaphoreCreateMutex();

    motorQueue = xQueueCreate(5, sizeof(char *));
    ventQueue = xQueueCreate(5, sizeof(char *));
    fuelQueue = xQueueCreate(5, sizeof(char *));
    gateQueue = xQueueCreate(5, sizeof(char *));
    dashQueue = xQueueCreate(5, sizeof(char *));

    // xTaskCreate(gate, "Gatekeeper", 128, NULL, 1, &Gate_handle);
    xTaskCreate(checkEngine, "Brains of the operation", 128, NULL, 1, &Check_handle);
    xTaskCreate(motor, "Motor simulation", 128, &MotorStatus, 1, &Motor_handle);
    // xTaskCreate(ventilation, "Ventilation simulation", 128, &VentStatus, 1, &Ventilation_handle);
    // xTaskCreate(fuel, "Fuel simulation", 128, NULL, 1, &Fuel_handle);
    vTaskStartScheduler();
}

void dashBoard() // the shared resource. prints to the serial monitor
{
}

void gate(void *parameters)
{
}
void checkEngine(void *xMotor1)
{
    char *motorPayload = "Checking motor.";
    char *ventPayload = "Checking vent.";
    char *fuelPayload = "Checking fuel.";
    char *motorAnswer = NULL;
    char *ventAnswer = NULL;
    char *fuelAnswer = NULL;
    TickType_t xTicksToWait = pdMS_TO_TICKS(1000);
    BaseType_t qStatus;


    while (1)
    {

        if (xSemaphore != NULL)
        {
            if (xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE)
            {
                xQueueSend(motorQueue, (void *)&motorPayload, (TickType_t)10);
                xSemaphoreGive(xSemaphore);
            }
            else
            {
                Serial.println("The semaphore was not taken, motor1");
            }
        }

        vTaskDelay(xTicksToWait/10);
        
        if (xSemaphore != NULL)
        {
            if (xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE)
            {
                xQueueReceive(motorQueue, (void *)&motorAnswer, (TickType_t)10);
                Serial.println(motorAnswer);
                xSemaphoreGive(xSemaphore);
            }
            else
            {
                Serial.println("The semaphore was not taken, motor2");
            }
        }
        vTaskSuspend(Check_handle);
        /*
                if(xSemaphore != NULL)
                {
                  if(xSemaphoreTake(xSemaphore, (TickType_t) 100 ) == pdTRUE)
                  {
                  xQueueSend(motorQueue, &ventPayload, xTicksToWait);
                  vTaskDelay(xTicksToWait);
                  xQueueReceive(motorQueue, &ventAnswer, xTicksToWait);
                  vTaskDelay(xTicksToWait);
                  Serial.println(ventAnswer);
                  xSemaphoreGive(xSemaphore);
                  vTaskDelay(xTicksToWait);
                  }
                  else
                  {
                    Serial.println("The semaphore was not taken, vent");
                  }
                }
                /*
                if(xSemaphoreTake(xSemaphore, xTicksToWait) == pdTRUE)
                {
                  xQueueSend(motorQueue, &fuelPayload, xTicksToWait);
                  vTaskDelay(xTicksToWait);
                  xQueueReceive(motorQueue, &fuelAnswer, xTicksToWait);
                  Serial.println(fuelAnswer);
                  xSemaphoreGive(xSemaphore);
                  vTaskDelay(xTicksToWait);
                }*/
        // xQueueReceive(motorQueue, &ventAnswer, xTicksToWait);
        // xQueueReceive(motorQueue, &fuelAnswer, xTicksToWait);
        // Serial.println(motorAnswer);
        // Serial.println(ventAnswer);
        // Serial.println(fuelAnswer);

        // vTaskDelay(xTicksToWait);
        // xQueueReceive(ventQueue, &ventAnswer, xTicksToWait);

        // Serial.println(ventAnswer);*/
    }
}

void motor(void *xMotor)
{
    Motor_t *motor = (Motor_t *)xMotor;
    char *receivedPayload = NULL;
    char *message = NULL;
    char speed[10] = "";
    char rpm[10] = "";
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {
        if (xSemaphore != NULL)
        {
            if (xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE)
            {
                qStatus = xQueueReceive(motorQueue, &receivedPayload, xTicksToWait);
                if (qStatus == pdPASS && strcmp(receivedPayload, "Checking motor.") == 0)
                {
                    if (motor->motorCheck == 1 && motor->GearboxCheck == 1)
                    {
                        message = (char *)pvPortMalloc(40 * sizeof(char));
                        strcat(message, "M.G is OK, ");

                        if (motor->Speed != 0)
                        {
                            strcat(message, "speed ");
                            sprintf(speed, "%i", motor->Speed);
                            strcat(message, speed);
                            strcat(message, " and Rpm ");
                            sprintf(rpm, "%i", motor->Rpm);
                            strcat(message, rpm);
                            strcat(message, "\n");
                            xQueueSend(motorQueue, &message, xTicksToWait);
                            vTaskDelay(xTicksToWait);
                            vPortFree(message);
                            xSemaphoreGive(xSemaphore);
                            vTaskSuspend(Motor_handle);
                        }
                    }

                    else
                    {
                        message = (char *)pvPortMalloc(20 * sizeof(char));
                        strcat(message, "x01:ERROR: M.||Gb.\n");
                        xQueueSend(motorQueue, &message, xTicksToWait);
                        vPortFree(message);
                        xSemaphoreGive(xSemaphore);
                        vTaskSuspend(Motor_handle);
                    }
                }
                vTaskSuspend(Motor_handle);
            }
        }
    }
}

void ventilation(void *xVentilation)
{
    Ventilation_t *xVent = (Ventilation_t *)xVentilation;
    char *receivedPayload = NULL;
    char *sendingPayload = NULL;
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {
        qStatus = xQueueReceive(motorQueue, &receivedPayload, xTicksToWait);
        if (qStatus == pdPASS && strcmp(receivedPayload, "Checking vent.") == 0)
        {
            Serial.println("Hee");
            vTaskDelay(xTicksToWait);
            if (xVent->ventilationCheck == true)
            {
                sendingPayload = (char *)pvPortMalloc(15 * sizeof(char));
                strcpy(sendingPayload, "Vent. is ok.\n");
                Serial.println("Test2");
                xQueueSend(motorQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
            }

            else
            {
                sendingPayload = (char *)pvPortMalloc(15 * sizeof(char));
                strcpy(sendingPayload, "x02:ERROR: Vent\n");
                xQueueSend(motorQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
            }
        }
    }
}

void fuel(void *xFuel)
{
    Fuel_t *Fuel = (Fuel_t *)xFuel;
    char *receivedPayload;
    char *sendingPayload = NULL;
    char fuelPercentage[6] = "";
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {
        qStatus = xQueueReceive(motorQueue, &receivedPayload, xTicksToWait);
        if (qStatus == pdPASS && strcmp(receivedPayload, "Checking fuel.") == 0 && receivedPayload != NULL)
        {
            vTaskDelay(xTicksToWait);
            if (Fuel->fuelCheck == true)
            {
                sendingPayload = (char *)pvPortMalloc(30 * sizeof(char));
                strcat(sendingPayload, "Fuel is ");
                sprintf(fuelPercentage, "%f", Fuel->fuelGauge);
                strcat(sendingPayload, "%\n");
                Serial.println("Test3");
                xQueueSend(motorQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
                vTaskSuspend(Fuel_handle);
            }

            else if (Fuel->fuelCheck != true && Fuel->fuelGauge < 10)
            {
                sendingPayload = (char *)pvPortMalloc(15 * sizeof(char));
                strcpy(sendingPayload, "0x3: low fuel\n");
                xQueueSend(motorQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
                vTaskSuspend(Fuel_handle);
            }

            else
            {
                sendingPayload = (char *)pvPortMalloc(15 * sizeof(char));
                strcpy(sendingPayload, "0x4 good fuel\n");
                xQueueSend(motorQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
                vTaskSuspend(Fuel_handle);
            }
        }
    }
}
void loop()
{
}
