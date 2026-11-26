
import os
import json
import logging
import pymysql
import paho.mqtt.client as mqtt

# MQTT
BROKER = "localhost"
PORT = 1883
TOPIC = "iiot/fugas"

# MARIADB
DB_HOST = "localhost"
DB_USER = "iiot_user"
DB_PASSWORD = os.getenv("IIOT_DB_PASSWORD")
DB_NAME = "iiot_db"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s"
)


def guardar_registro(datos):
    conexion = pymysql.connect(
        host=DB_HOST,
        user=DB_USER,
        password=DB_PASSWORD,
        database=DB_NAME,
        charset="utf8mb4"
    )

    try:
        with conexion.cursor() as cursor:
            sql = """
            INSERT INTO registros (
                registro, tiempo_ms, frecuencia_Hz,
                caudal_L_min, litros_totales,
                agua_detectada, bomba_on, fuga_sospechada
            ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
            """

            valores = (
                datos["registro"],
                datos["tiempo_ms"],
                datos["frecuencia_Hz"],
                datos["caudal_L_min"],
                datos["litros_totales"],
                datos["agua_detectada"],
                datos["bomba_on"],
                datos["fuga_sospechada"]
            )

            cursor.execute(sql, valores)

        conexion.commit()

    finally:
        conexion.close()


def al_conectar(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        logging.info("Conectado al broker MQTT")
        client.subscribe(TOPIC)
        logging.info("Suscrito al topico: %s", TOPIC)
    else:
        logging.error("Error MQTT: %s", reason_code)


def recibir_mensaje(client, userdata, mensaje):
    try:
        datos = json.loads(mensaje.payload.decode("utf-8"))
        guardar_registro(datos)

        logging.info(
            "Registro guardado: %s | Caudal: %s L/min",
            datos["registro"],
            datos["caudal_L_min"]
        )

    except (ValueError, KeyError, TypeError) as error:
        logging.error("Mensaje invalido: %s", error)

    except pymysql.MySQLError as error:
        logging.error("Error MariaDB: %s", error)


def main():
    if not DB_PASSWORD:
        raise RuntimeError(
            "No se encontro la variable de entorno IIOT_DB_PASSWORD"
        )

    cliente = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id="UNOQ_Registro_IIOT"
    )

    cliente.on_connect = al_conectar
    cliente.on_message = recibir_mensaje

    logging.info("Iniciando receptor IIoT")
    cliente.connect(BROKER, PORT, 60)
    cliente.loop_forever()


if __name__ == "__main__":
    main()
