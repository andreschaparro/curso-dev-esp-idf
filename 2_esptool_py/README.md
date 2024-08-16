# Capítulo 2: Esptool.py

## Reiniciar al ESP-32 en el modo DOWNLOAD_BOOT o SPI_FAST_FLASH_BOOT

1. Abrir la `ESP-IDF CMD`.
2. Ejecutar `python -m serial.tools.miniterm COM10 115200`.
3. Mantener presionado el botón `IO0`.
4. Presionar el botón `EN` durante 1 segundo y liberarlo.
5. Liberar el botón `IO0`.

![DOWNLOAD_BOOT](download_boot.png)

5. Presionar el botón `EN` durante 1 segundo y liberarlo.

![SPI_FAST_FLASH_BOOT](spi_fast_flash_boot.png)

## Obtener las características del ESP32

1. Mantener presionado el botón `IO0`.
2. Ejecutar `esptool.py --chip esp32 --port COM10 flash_id`.
3. Liberar el botón `IO0`.

![flash_id](flash_id.png)

## Borrar la memoria flash

1. Mantener presionado el botón `IO0`.
2. Ejecutar `esptool.py --chip esp32 --port COM10 erase_flash`.
3. Liberar el botón `IO0`.

![erase_flash](erase_flash.png)

## Grabar los archivos binarios del proyecto

1. Ejecutar `cd C:\Users\achaparro\curso-dev-esp-idf\hola_mundo`.
2. Ejecutar `copy /Y .\build\bootloader\bootloader.bin .\bootloader.bin`.
3. Ejecutar `copy /Y .\build\partition_table\partition-table.bin .\partition-table.bin`.
4. Ejecutar `copy /Y .\build\hola_mundo.bin .\hola_mundo.bin`.
5. Mantener presionado el botón `IO0`.
6. Ejecutar `esptool.py --chip esp32 --port COM10 write_flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 hola_mundo.bin`.
7. Liberar el botón `IO0`.

![write_flash](write_flash.png)

## Documentación oficial

- [Boot Mode Selection](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html).
- [Esptool.py Documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32/).
