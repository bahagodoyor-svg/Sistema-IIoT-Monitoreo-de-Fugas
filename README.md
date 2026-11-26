# Sistema IIoT para Monitoreo y Detección de Fugas de Agua

## Descripción del proyecto

Este proyecto consiste en el desarrollo de un sistema de Internet Industrial de las Cosas (IIoT) orientado al monitoreo de variables relacionadas con el flujo de agua y la detección de posibles fugas.

El sistema utiliza un ESP32 para adquirir y procesar información, un Arduino UNO Q para recibir y almacenar los datos y herramientas de visualización y respaldo en la nube para consultar los registros.

## Objetivo general

Desarrollar un sistema IIoT capaz de monitorear variables de flujo de agua, identificar condiciones asociadas a posibles fugas y almacenar la información para su supervisión y análisis.

## Arquitectura del sistema

1. **ESP32:** adquisición y procesamiento de datos de los sensores, control del relevador y envío de información.
2. **MQTT:** protocolo de comunicación utilizado para transmitir los datos.
3. **Arduino UNO Q:** recepción de mensajes MQTT y almacenamiento de registros.
4. **MariaDB:** administración de la base de datos.
5. **Grafana:** visualización de las variables mediante paneles y gráficas.
6. **Google Sheets:** sincronización y respaldo de los registros en la nube.

## Variables monitoreadas

- Frecuencia (Hz).
- Caudal (L/min).
- Litros acumulados.
- Detección de agua.
- Estado de la bomba.
- Detección de posibles fugas.

## Tecnologías utilizadas

- ESP32 DEVKIT V1
- Arduino UNO Q
- MQTT / Mosquitto
- Python
- MariaDB
- Grafana
- Google Sheets
- Wi-Fi

## Organización del repositorio

```text
Sistema-IIoT-Monitoreo-de-Fugas/
│
├── ESP32/
│   └── codigo_esp32.ino
│
├── Arduino_UNO_Q/
│   ├── receptor_mqtt_db.py
│   └── sincronizar_google_sheets.py
│
├── .gitignore
└── README.md
```

## Descripción de los archivos

### ESP32/codigo_esp32.ino

Programa desarrollado para el ESP32. Se encarga de procesar las mediciones, controlar el relevador, evaluar condiciones de posibles fugas y transmitir los datos mediante MQTT.

### Arduino_UNO_Q/receptor_mqtt_db.py

Programa encargado de recibir los mensajes MQTT, interpretar los datos JSON y almacenarlos en la base de datos MariaDB.

### Arduino_UNO_Q/sincronizar_google_sheets.py

Programa encargado de consultar los registros almacenados en MariaDB y sincronizarlos periódicamente con Google Sheets.

## Consideraciones

- Los archivos publicados son versiones destinadas a la documentación del proyecto.
- Las credenciales y configuraciones privadas deben proporcionarse mediante archivos o variables de entorno externos.
- El programa del ESP32 publicado tiene habilitado el modo de prueba (`MODO_PRUEBA = true`), por lo que genera valores simulados para comprobar el funcionamiento del sistema.
- Las direcciones IP y credenciales deben configurarse de acuerdo con la red y los equipos donde se implemente el proyecto.

## Autoría

Proyecto académico de desarrollo IIoT.
