#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timersFreeRTOS.h>
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
Motor_t MotorStatus = {true, true, 90, 2500};
Ventilation_t VentStatus = {false};
Fuel_t FuelStatus = {true, 50.03};

// TaskHandles
TaskHandle_t Motor_handle, Ventilation_handle, Fuel_handle, Gearbox_handle, Check_handle, Dash_handle;

// QueueHandles
QueueHandle_t motorQueue, ventQueue, dashQueue, fuelQueue;

// SemaphoreHandle
SemaphoreHandle_t xSemaphore = NULL;

void setup()
{
    Serial.begin(9600);

    xSemaphore = xSemaphoreCreateMutex();
    if (xSemaphore == NULL)
    {
        Serial.println("Mutex creation failed!");
        // Handle the error, possibly by blocking here or resetting the system
    }

    motorQueue = xQueueCreate(5, sizeof(char *));
    ventQueue = xQueueCreate(5, sizeof(char *));
    fuelQueue = xQueueCreate(5, sizeof(char *));
    dashQueue = xQueueCreate(5, sizeof(char *));

    // xTimerCreate("mainTimer", PERIOD, pdTRUE,(void *)0, checkEngine);

    xTaskCreate(dashBoard, "Dash", 128, NULL, 2, &Dash_handle);
    xTaskCreate(checkEngine, "Brains of the operation", 128, NULL, 3, &Check_handle);
    xTaskCreate(motor, "Motor simulation", 128, &MotorStatus, 2, &Motor_handle);
    // xTimerStart(mainTimer, 0);
    xTaskCreate(ventilation, "Ventilation simulation", 128, &VentStatus, 2, &Ventilation_handle);
    xTaskCreate(fuel, "Fuel simulation", 128, &FuelStatus, 2, &Fuel_handle);
    vTaskStartScheduler();
}

void checkEngine(void *parameters)
{
    char *motorPayload = "Checking motor.\n";
    char *answer = NULL;
    TickType_t longTick = pdMS_TO_TICKS(1000);
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);
    TickType_t smallTick = pdMS_TO_TICKS(10);
    BaseType_t qStatus;

    while (1)
    {
        xQueueSend(motorQueue, &motorPayload, xTicksToWait);
        vTaskDelay(xTicksToWait);

        xQueueReceive(fuelQueue, (void *)&answer, xTicksToWait);

        xQueueSend(dashQueue, (void *)&answer, xTicksToWait);
        vTaskSuspend(Check_handle);
    }
}

void dashBoard(void *parameters) // the shared resource. prints to the serial monitor
{
    char *receivedPayload = NULL;
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {
        if (xSemaphore != NULL)
        {
            if (xSemaphoreTake(xSemaphore, xTicksToWait) == pdTRUE)
            {
                qStatus = xQueueReceive(dashQueue, &receivedPayload, xTicksToWait);
                if(qStatus == pdPASS)
                {
                  Serial.println(receivedPayload);
                }
                xSemaphoreGive(xSemaphore);
            }
            else
            {
                Serial.println("Mutex was not taken.");
                vTaskDelay(xTicksToWait);
            }
        }
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
        qStatus = xQueueReceive(motorQueue, &receivedPayload, xTicksToWait);
        if (qStatus == pdPASS)
        {
            if (motor->motorCheck == 1 && motor->GearboxCheck == 1)
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
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
                    xQueueSend(ventQueue, &message, xTicksToWait);
                    vPortFree(message);
                    vTaskSuspend(Motor_handle);
                }
            }
            else
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcat(message, "x01:ERROR: M.||Gb.\n");
                xQueueSend(ventQueue, &message, xTicksToWait);
                vPortFree(message);
                vTaskSuspend(Motor_handle);
            }
        }
    }
    vTaskSuspend(Motor_handle);
}


void ventilation(void *xVentilation)
{
    Ventilation_t *xVent = (Ventilation_t *)xVentilation;
    char *ventPayload = "Checking vent.\n";
    char *ventOK = "Y*Y*\n";
    char *ventFAIL = "N*N*\n";
    char *receivedPayload = NULL;
    char *message = NULL;
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {

        qStatus = xQueueReceive(ventQueue, &receivedPayload, xTicksToWait);
        if (qStatus == pdPASS)
        {
            if (xVent->ventilationCheck == true)
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, ventPayload);
                strcat(message, "Vent. is ok.\n");
                xQueueSend(fuelQueue, &message, xTicksToWait);
                xQueueSend(dashQueue, &ventOK, xTicksToWait);
                vPortFree(message);
                vTaskSuspend(Ventilation_handle);
            }

            else
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, ventPayload);
                strcat(message, "x02:ERROR: Vent\n");
                xQueueSend(fuelQueue, &message, xTicksToWait);
                xQueueSend(dashQueue, &ventFAIL, xTicksToWait);
                vPortFree(message);
                vTaskSuspend(Ventilation_handle);
            }
        }
        vTaskSuspend(Ventilation_handle);
    }
}

void fuel(void *xFuel)
{
    Fuel_t *Fuel = (Fuel_t *)xFuel;
    char *receivedPayload;
    char *fuelPayload = "Checking fuel.\n";
    char *fuelOK = "h$h$\n";
    char *fuelFAIL = "U$U$\n";
    char *message = NULL;
    char fuelPercentage[6] = "";
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {
        qStatus = xQueueReceive(fuelQueue, &receivedPayload, xTicksToWait);
        if (qStatus == pdPASS)
        {
            if (Fuel->fuelCheck == true && Fuel->fuelGauge > 10)
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, fuelPayload);
                strcat(message, "Fuel is ");
                sprintf(fuelPercentage, "%.2f", Fuel->fuelGauge);
                strcat(message, fuelPercentage);
                strcat(message, "%\n");
                xQueueSend(fuelQueue, &message, xTicksToWait);
                xQueueSend(dashQueue, &fuelOK, xTicksToWait);
                vPortFree(message);
                vTaskSuspend(Fuel_handle);
            }

            else if (Fuel->fuelCheck == true && Fuel->fuelGauge < 10 || Fuel->fuelCheck != true && Fuel->fuelGauge < 10)
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, fuelPayload);
                strcat(message, "0x3: low fuel\n");
                xQueueSend(fuelQueue, &message, xTicksToWait);
                xQueueSend(dashQueue, &fuelFAIL, xTicksToWait);
                vPortFree(message);
                vTaskSuspend(Fuel_handle);
            }

            else
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, fuelPayload);
                strcat(message, "0x4 good fuel\n");
                xQueueSend(fuelQueue, &message, xTicksToWait);
                xQueueSend(dashQueue, &fuelOK, xTicksToWait);
                vPortFree(message);
                vTaskSuspend(Fuel_handle);
            }
        }
    }
}
void loop()
{
}