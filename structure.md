**/apps**
This contains the different computers/mcus that we will use on the satellite. This includes obc, adcs, and many others.

**/drivers**
This contains all of the drivers for the hardware, seperated into different devices.

**/platform**
This contains the code that is different depending on whether the code is in simulation or real mode. Examples include SPI, time, UART, etc.

**/rtos**
This contains the source code for FreeRTOS. Within it is source, configurations, and ports for both POSIX (simulation) and real (ARM-3)

**/shared**
This contains the code that will be shared across the different computers such as packet format, CRC, etc.

**/docs**
This contains documentation and information about the code. We will aim to udpate this continously.

**/build**
This is the main build folder for CMake.

**Simulation**
So how will we simulate. We have multiple CMake setups to allow us to run singular boards, such as the ADCS, or all together through a root CMake file.
We will use POSIX for non-hardware simulation.
Additionally, we are going to implement a V-BUS mock SPI implementation that will allow computers to communicate differently.