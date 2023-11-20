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
Motor_t MotorStatus = {true, true, 100, 2000};
Ventilation_t VentStatus = {true};
Fuel_t FuelStatus = {true, 10.03};

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
    char *motorPayload = "Checking motor.";
    char *ventPayload = "Checking vent.";
    char *fuelPayload = "Checking fuel.";
    char *answer = NULL;
    TickType_t longTick = pdMS_TO_TICKS(1000);
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);
    TickType_t smallTick = pdMS_TO_TICKS(10);
    BaseType_t qStatus;

    while (1)
    {
        xQueueSend(motorQueue, (void *)&motorPayload, xTicksToWait);
        vTaskDelay(xTicksToWait);

        xQueueReceive(fuelQueue, (void *)&answer, xTicksToWait);

        xQueueSend(dashQueue, (void *)&answer, xTicksToWait);
        vTaskSuspend(Check_handle);
    }
}

void dashBoard(void *parameters) // the shared resource. prints to the serial monitor
{
    char *receivedPayload = NULL;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {
        if (xSemaphore != NULL)
        {
            if (xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE)
            {
                xQueueReceive(dashQueue, &receivedPayload, xTicksToWait);
                Serial.println(receivedPayload);
                xSemaphoreGive(xSemaphore);
                vTaskDelay(xTicksToWait);
            }
            else
            {
                Serial.println("Mutex was not taken.");
                vTaskDelay(xTicksToWait);
            }
        }

        vTaskSuspend(Dash_handle);
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
    TickType_t smallTick = pdMS_TO_TICKS(10);

    while (1)
    {
        qStatus = xQueueReceive(motorQueue, &receivedPayload, xTicksToWait);
        if (qStatus == pdPASS && strcmp(receivedPayload, "Checking motor.") == 0)
        {
            if (motor->motorCheck == 1 && motor->GearboxCheck == 1)
            {
                message = (char *)pvPortMalloc(60 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, "\nM.G is OK, ");

                if (motor->Speed != 0)
                {
                    strcat(message, "speed ");
                    sprintf(speed, "%i", motor->Speed);
                    strcat(message, speed);
                    strcat(message, " and Rpm ");
                    sprintf(rpm, "%i", motor->Rpm);
                    strcat(message, rpm);
                    xQueueSend(ventQueue, &message, smallTick);
                    vPortFree(message);
                    vTaskSuspend(Motor_handle);
                }
            }
            else
            {
                message = (char *)pvPortMalloc(20 * sizeof(char));
                strcat(message, "x01:ERROR: M.||Gb.\n");
                xQueueSend(ventQueue, &message, smallTick);
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
    char *receivedPayload = NULL;
    char *sendingPayload = NULL;
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);
    TickType_t smallTick = pdMS_TO_TICKS(10);

    while (1)
    {

        qStatus = xQueueReceive(ventQueue, &receivedPayload, smallTick);
        if (qStatus == pdPASS)
        {
            if (xVent->ventilationCheck == true)
            {
                sendingPayload = (char *)pvPortMalloc(70 * sizeof(char));
                strcpy(sendingPayload, receivedPayload);
                strcat(sendingPayload, "\nVent. is ok.");
                xQueueSend(fuelQueue, &sendingPayload, smallTick);
                vPortFree(sendingPayload);
                vTaskSuspend(Ventilation_handle);
            }

            else
            {
                sendingPayload = (char *)pvPortMalloc(70 * sizeof(char));
                strcpy(sendingPayload, receivedPayload);
                strcat(sendingPayload, "\nx02:ERROR: Vent\n");
                xQueueSend(fuelQueue, &sendingPayload, smallTick);
                vPortFree(sendingPayload);
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
    char *sendingPayload = NULL;
    char fuelPercentage[6] = "";
    BaseType_t qStatus;
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    while (1)
    {
        qStatus = xQueueReceive(fuelQueue, &receivedPayload, xTicksToWait);
        if (qStatus == pdPASS)
        {
            if (Fuel->fuelCheck == true)
            {
                sendingPayload = (char *)pvPortMalloc(80 * sizeof(char));
                strcpy(sendingPayload, receivedPayload);
                strcat(sendingPayload, "\nFuel is ");
                sprintf(fuelPercentage, "%.2f", Fuel->fuelGauge);
                strcat(sendingPayload, fuelPercentage);
                strcat(sendingPayload, "%\n");
                xQueueSend(fuelQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
                vTaskSuspend(Fuel_handle);
            }

            else if (Fuel->fuelCheck != true && Fuel->fuelGauge < 10)
            {
                sendingPayload = (char *)pvPortMalloc(70 * sizeof(char));
                strcpy(sendingPayload, receivedPayload);
                strcpy(sendingPayload, "\n0x3: low fuel\n");
                xQueueSend(fuelQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
                vTaskSuspend(Fuel_handle);
            }

            else
            {
                sendingPayload = (char *)pvPortMalloc(70 * sizeof(char));
                strcpy(sendingPayload, receivedPayload);
                strcpy(sendingPayload, "\n0x4 good fuel\n");
                xQueueSend(fuelQueue, &sendingPayload, xTicksToWait);
                vPortFree(sendingPayload);
                vTaskSuspend(Fuel_handle);
            }
        }
    }
}
void loop()
{
}
