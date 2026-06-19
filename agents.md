# Agent Skill: Embedded MCU Display & IMU Development

## Context & Environment
- **Target Hardware:** ESP32-S3 (or matching on-shelf MCU, specify on initialization)
- **Framework:** ESP-IDF / C++ (or Arduino/LovyanGFX - adjust as needed)
- **Toolchain:** `idf.py` (including `idf.py monitor` for log-based debugging)

## Execution Protocols & Auto-Debugging
1. **Compilation & Flash:** Always run the official build pipeline via the integrated terminal surface.
2. **Log Monitoring:** Actively listen to the serial monitor output.
3. **Panic & Stacktrace Analysis:**
   - Upon encountering a target crash (e.g., *Guru Meditation Error*, *LoadProhibited*), automatically pipe the stacktrace through the ELF-decoder.
   - Do not hallucinate or guess the error location; trace it to the exact line in the C++ file.
4. **Refactoring:** Modify the source code to resolve memory leaks, I2C/SPI timing collisions, or pointer errors. Document all changes inside the **Artifacts** before re-flashing.

## Restrictions
- Write non-blocking code. Use FreeRTOS tasks efficiently.
- Keep the main loop clean and abstract sensor filtering (IMU Quaternion/Complementary filter) from UI rendering logic.

## Environment Paths
- Use the existing system ESP-IDF installation.
- Do not install a separate toolchain.
- Target compiler and tools are located in the standard system PATH.
