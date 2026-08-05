Every task should have a pretty similar design:

1. Read or receive some sort of info (i.e. Sensor data, Recieve Packet). DATA

2. Do something to/with that data. (i.e. update state, math). FUNCTION

3. Publish or Send the result of your FUNCTION (i.e. send packet, send results, publish attitude) SEND

4. Delay some amount of time (i.e. 10 milliseconds, 50 milliseconds) DELAY

This is the pattern:
DATA -> FUNCTION -> SEND -> DELAY ... *repeat*

To make a task use FreeRTOS xTaskCreate along with a TaskType and Buffer.
You will also want a queue.

Ensure you have both a:
__________________
void some_task_init(void);

void some_task(void *argument);
__________________

Init:
- Creates Task
- Obtains queue handles
- Other task specific inits

Task:
- No init at top of the function
- TickType_t lastWake = xTaskGetTickCount();
- While superloop contains the actual actions of the task
- DON'T Put any actual math, compute, algorithm, etc. Rather use functions that call other files.

| Task | Purpose | Trigger Type | Rate | Period | Priority (0–5) | Typical Blocking | Notes |
|------|---------|--------------|------|--------|----------------|------------------|------|
| **Sensor Task** | Read IMU, magnetometer, sun sensors, camera status | Periodic | 100 Hz | 10 ms | **4** | `vTaskDelayUntil()` | Reads all sensors and publishes a `sensor_packet_t`. Keep hardware access here. |

| **Estimation Task** | Estimate spacecraft attitude | Queue | 50–100 Hz | 10–20 ms | **4** | `xQueueReceive(sensorQueue)` | Runs EKF/complementary filter after new sensor data arrives. Publishes `attitude_state_t`. |

| **Control Task** | Compute actuator commands | Queue | 50 Hz | 20 ms | **5** | `xQueueReceive(attitudeQueue)` | Highest priority. Computes desired magnetic dipole and commands magnetorquers. |

| **Command Task** | Receive commands from OBC/ground | Event | As Needed | N/A | **3** | `xQueueReceive(commandQueue)` | Sleeps until a command arrives. Updates ADCS manager. |

| **Telemetry Task** | Send ADCS telemetry | Periodic | 5–10 Hz | 100–200 ms | **2** | `vTaskDelayUntil()` | Packages attitude, sensor data, mode, temperatures, etc. |

| **Camera Task** | Capture Earth images | Periodic/Event | 0.5–5 Hz | 200–2000 ms | **2** | `vTaskDelayUntil()` or event | Only active during imaging modes. Usually disabled otherwise. |

| **Housekeeping Task** | Monitor software health | Periodic | 1 Hz | 1000 ms | **1** | `vTaskDelayUntil()` | Checks stack usage, CPU load, temperatures, watchdog, memory. |

| **Logging Task** *(Optional)* | Save logs to flash/SD | Queue | Event | N/A | **1** | `xQueueReceive(logQueue)` | Writes logs asynchronously so control isn't delayed. |

| **Idle Task** | FreeRTOS idle task | Automatic | N/A | N/A | **0** | Idle | Created by FreeRTOS automatically. |