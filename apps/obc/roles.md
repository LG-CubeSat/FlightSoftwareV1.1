Currently I'm thinking we'll have the OBC do the following roles:
1. Commands: Its job is to query info from other boards, relay messages, and ensure communication flows as it is the master of the I2C bus (will switch to CAN in later prototype iterations)
2. Compute: This does any heavy lifting the MCUs cannot do, handles image compression, and any other task that requires heavy compute.
3. Data: The OBC will manage the data storage and file system.
4. FDIR: Fault Detection, Isolation, and Discovery - This processes health, memory, load, manages fall back scripts, and runs the watchdog
5. Mission: The OBC must be in charge of the mission, meaning it runs the experiments, timeline, and autonomy of the research. This is done by commanding the payload system.
6. Time: Must keep syncronization and time.
7. IPC: Its job is to manage the communication between processes. IPC = Internal Process Communication
8. Supervisor: this guys job is to manage everyone else.