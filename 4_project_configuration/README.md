# Capítulo 4: Project Configuration

![Example Configuration](example_configuration.png)

## Crear el proyecto **conexion_wifi**

1. Abrir la `ESP-IDF CMD`.
2. Ejecutar `cd C:\Users\achaparro\curso-dev-esp-idf`.
3. Ejecutar `idf.py create-project conexion_wifi`.
4. Ejecutar `cd conexion_wifi`.
5. Ejecutar `idf.py set-target esp32`.
6. Ejecutar `idf.py menuconfig`.
7. Ir a `Serial flasher config ---> Flash size`.
8. Seleccionar `4 MB`.
9. Presionar `ESC`.
10. Presionar `ESC`.
11. Presionar `Y`.
12. Ejecutar `code .`.
13. Presionar `CTRL+SHIFT+P`.
14. Seleccionar `ESP-IDF: Add vscode Configuration Folder`.
15. Copiar el contenido del archivo fuente `conexion_wifi.c` que está en el repositorio.
16. Pegar el contenido en el archivo fuente `conexion_wifi.c` del proyecto.

## Crear la configuración inicial del proyecto

1. Ejecutar `idf.py save-defconfig`.

El paso anterior crea el archivo `sdkconfig.default` que toma la configuración actual del proyecto y la establece como la inicial.

## Habilitar el linter del lenguaje Kconfig en Visual Studio Code

1. Ir a `File>Preferences>Settings`.
2. Marcar el checkbox `useIDFKconfigStyle`.

## Crear nuevas opciones de configuración para el proyecto

1. Crear el archivo `Kconfig.projbuild` dentro del componente `main`.
2. Copiar el contenido del archivo `Kconfig.projbuild` que está en el repositorio.
3. Pegar el contenido en el archivo `Kconfig.projbuild` del proyecto.

## Correr el fixer del lenguaje Kconfig

1. Ejecutar `python -m kconfcheck main/Kconfig.projbuild`.

Si hay errores, el paso anterior crea una versión corregida del archivo `Kconfig.projbuild` llamada `Kconfig.projbuild.new`.

2. Copiar el contenido de `Kconfig.projbuild.new` en `Kconfig.projbuild`.
3. Borrar el archivo `Kconfig.projbuild.new`.

## Configurar las nuevas opciones del proyecto

1. Ejecutar `idf.py menuconfig`.
2. Ir a `Example Configuration ---> STA Configuration ---> WiFi Remote AP SSID`.
3. Ingresar el nombre de nuestra red Wi-Fi.
4. Ir a `Example Configuration ---> STA Configuration ---> WiFi Remote AP Password`.
5. Ingresar la contraseña de nuestra red Wi-Fi.
6. Presionar `ESC`.
7. Presionar `ESC`.
8. Presionar `ESC`.
9. Presionar `Y`.

## Compilar, grabar, y monitorear el proyecto

1. Ejecutar `idf.py all`.
2. Mantener presionado el botón `IO0`.
3. Ejecutar `idf.py -p COM10 flash monitor`.
4. Liberar el botón `IO0`.
5. Presionar `CTRL+]` para cerrar el monitor.

![Monitor](monitor.png)

## Restaurar la configuración inicial del proyecto

1. Borrar el archivo `sdkconfig`.
2. Ejecutar `idf.py menuconfig`.

## Documentación oficial

- [Project Configuration](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-reference/kconfig.html).
- [Build System](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-guides/build-system.html).
- [Wi-Fi](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-reference/network/esp_wifi.html).
- [Wi-Fi Driver](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-guides/wifi.html).
- [Wi-Fi Station Example](https://github.com/espressif/esp-idf/tree/v5.2.2/examples/wifi/getting_started/station).
- [Wi-Fi SoftAP Example](https://github.com/espressif/esp-idf/tree/v5.2.2/examples/wifi/getting_started/softAP).
- [Wi-Fi SoftAP & Station Example](https://github.com/espressif/esp-idf/tree/v5.2.2/examples/wifi/softap_sta).
- [Event Loop Library](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-reference/system/esp_event.html).
- [Error Handling](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-guides/error-handling.html).
- [Logging library](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-reference/system/log.html).
- [FreeRTOS (IDF)](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-reference/system/freertos_idf.html).
- [FreeRTOS event groups](https://wap.freertos.org/Documentation/02-Kernel/02-Kernel-features/06-Event-groups).
