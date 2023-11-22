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
Motor_t MotorStatus = {false, true, 90, 2500};
Ventilation_t VentStatus = {false};
Fuel_t FuelStatus = {true, 50.03};

// TaskHandles
TaskHandle_t Motor_handle, Ventilation_handle, Fuel_handle, Gearbox_handle, Check_handle, Dash_handle;

// QueueHandles
QueueHandle_t motorQueue, ventQueue, dashQueue, fuelQueue, ventfuelcom;

// SemaphoreHandle
SemaphoreHandle_t xSemaphore = NULL;

// Timerhandle
TimerHandle_t mainTimer;

// TickTypes
const TickType_t xFrequency = pdMS_TO_TICKS(1000);
TickType_t xTicksToWait = pdMS_TO_TICKS(100);
TickType_t xLastWakeTime = xTaskGetTickCount();

void setup()
{
    Serial.begin(9600);

    xSemaphore = xSemaphoreCreateMutex();
    if (xSemaphore == NULL)
    {
        Serial.println("Mutex creation failed!");
    }

    motorQueue = xQueueCreate(5, sizeof(char *));
    ventQueue = xQueueCreate(5, sizeof(char *));
    fuelQueue = xQueueCreate(5, sizeof(char *));
    dashQueue = xQueueCreate(5, sizeof(char *));
    ventfuelcom = xQueueCreate(5, sizeof(char *));

    mainTimer = xTimerCreate("mainTimer", xFrequency, pdTRUE, (void *)0, checkEngine);

    if (mainTimer == NULL)
    {
        Serial.println("TIMER ERROR not created");
        vTaskDelay(xTicksToWait);
    }

    else
    {
        if (xTimerStart(mainTimer, 0) != pdPASS)
        {
            Serial.println("Timer not activated");
            vTaskDelay(xTicksToWait);
        }
    }

    xTaskCreate(dashBoard, "Dash", 256, NULL, 1, &Dash_handle);
    xTaskCreate(motor, "Motor simulation", 256, &MotorStatus, 1, &Motor_handle);
    xTaskCreate(ventilation, "Ventilation simulation", 256, &VentStatus, 1, &Ventilation_handle);
    xTaskCreate(fuel, "Fuel simulation", 256, &FuelStatus, 1, &Fuel_handle);
    vTaskStartScheduler();
}

void checkEngine(TimerHandle_t xTimer)
{
    char *motorPayload = "Checking motor.\n";
    char *answer = NULL;

    xQueueSend(motorQueue, &motorPayload, portMAX_DELAY);
    xQueueReceive(fuelQueue, &answer, portMAX_DELAY);
    xQueueSend(dashQueue, &answer, portMAX_DELAY);
}

void dashBoard(void *parameters) // the shared resource. prints to the serial monitor
{
    char *receivedPayload = NULL;
    BaseType_t qStatus;

    while (1)
    {
        if (xSemaphore != NULL)
        {
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
            {

                qStatus = xQueueReceive(dashQueue, &receivedPayload, portMAX_DELAY);

                if (qStatus == pdPASS && receivedPayload != NULL)
                {
                    Serial.println(receivedPayload);
                }

                xSemaphoreGive(xSemaphore);
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
            }
            else
            {
                Serial.println("Mutex was not taken.");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
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

    while (1)
    {
        qStatus = xQueueReceive(motorQueue, &receivedPayload, portMAX_DELAY);
        if (qStatus == pdPASS && receivedPayload != NULL)
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
                    xQueueSend(ventQueue, &message, portMAX_DELAY);
                    vPortFree(message);
                }
            }
            else
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, "x01:ERROR: M.||Gb.\n");
                xQueueSend(ventQueue, &message, portMAX_DELAY);
                vPortFree(message);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
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

    while (1)
    {
        qStatus = xQueueReceive(ventQueue, &receivedPayload, portMAX_DELAY);
        if (qStatus == pdPASS && receivedPayload != NULL)
        {
            if (xVent->ventilationCheck == true)
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, ventPayload);
                strcat(message, "Vent. is ok.\n");
                xQueueSend(ventfuelcom, &message, portMAX_DELAY);
                xQueueSend(dashQueue, &ventOK, portMAX_DELAY);
                vPortFree(message);
            }

            else
            {
                message = (char *)pvPortMalloc(200 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, ventPayload);
                strcat(message, "x02:ERROR: Vent\n");
                xQueueSend(ventfuelcom, &message, portMAX_DELAY);
                xQueueSend(dashQueue, &ventFAIL, portMAX_DELAY);
                vPortFree(message);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void fuel(void *xFuel)
{
    Fuel_t *Fuel = (Fuel_t *)xFuel;
    char *receivedPayload = NULL;
    char *fuelPayload = "Checking fuel.\n";
    char *fuelOK = "h$h$\n";
    char *fuelFAIL = "U$U$\n";
    char *message = NULL;
    char fuelPercentage[6] = "";
    BaseType_t qStatus;

    while (1)
    {
        qStatus = xQueueReceive(ventfuelcom, &receivedPayload, portMAX_DELAY);
        if (qStatus == pdPASS && receivedPayload != NULL)
        {
            if (Fuel->fuelCheck == true && Fuel->fuelGauge > 10)
            {
                message = (char *)pvPortMalloc(250 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, fuelPayload);
                strcat(message, "Fuel is ");
                sprintf(fuelPercentage, "%.2f", Fuel->fuelGauge);
                strcat(message, fuelPercentage);
                strcat(message, "%\n");
                xQueueSend(fuelQueue, &message, portMAX_DELAY);
                xQueueSend(dashQueue, &fuelOK, portMAX_DELAY);
                vPortFree(message);
            }

            else if (Fuel->fuelCheck == true && Fuel->fuelGauge < 10 || Fuel->fuelCheck != true && Fuel->fuelGauge < 10)
            {
                message = (char *)pvPortMalloc(250 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, fuelPayload);
                strcat(message, "0x3: low fuel\n");
                xQueueSend(fuelQueue, &message, portMAX_DELAY);
                xQueueSend(dashQueue, &fuelFAIL, portMAX_DELAY);
                vPortFree(message);
            }

            else
            {
                message = (char *)pvPortMalloc(250 * sizeof(char));
                strcpy(message, receivedPayload);
                strcat(message, fuelPayload);
                strcat(message, "0x4 good fuel\n");
                xQueueSend(fuelQueue, &message, portMAX_DELAY);
                xQueueSend(dashQueue, &fuelOK, portMAX_DELAY);
                vPortFree(message);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
void loop()
{
}