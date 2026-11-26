
import os
import json
import time
import logging
import pymysql
import urllib.request
from pathlib import Path

# =====================================================
# CONFIGURACION
# =====================================================

# Ruta del archivo externo de configuracion
RUTA_ENV = os.getenv(
    "IIOT_ENV_FILE",
    "/etc/iiot-google-sheets.env"
)

# Cargar configuracion externa
def cargar_env(ruta):
    configuracion = {}

    with open(ruta, "r") as archivo:
        for linea in archivo:
            linea = linea.strip()

            if not linea or linea.startswith("#") or "=" not in linea:
                continue

            clave, valor = linea.split("=", 1)
            configuracion[clave.strip()] = (
                valor.strip().strip('"').strip("'")
            )

    return configuracion


# =====================================================
# VALIDAR CONFIGURACION
# =====================================================

ENV = cargar_env(RUTA_ENV)

URL = ENV.get("GOOGLE_SCRIPT_URL")
TOKEN = ENV.get("GOOGLE_TOKEN")

DB_CONFIG = {
    "host": ENV.get("DB_HOST"),
    "user": ENV.get("DB_USER"),
    "password": ENV.get("DB_PASSWORD"),
    "database": ENV.get("DB_NAME"),
    "cursorclass": pymysql.cursors.DictCursor,
    "connect_timeout": 10
}

ESTADO = str(
    Path.home() / ".iiot_sheets_last_id"
)

INTERVALO = 15
TAMANO_BLOQUE = 100

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s: %(message)s"
)


def validar_configuracion():
    if not URL or not TOKEN:
        raise RuntimeError(
            "Falta configurar Google Sheets URL o TOKEN"
        )

    for clave in ("host", "user", "password", "database"):
        if not DB_CONFIG[clave]:
            raise RuntimeError(
                "Falta configurar la conexion a MariaDB"
            )


# =====================================================
# CONTROL DEL ULTIMO REGISTRO
# =====================================================

def obtener_ultimo_id():
    try:
        with open(ESTADO, "r") as archivo:
            return int(archivo.read().strip())

    except FileNotFoundError:
        return 0


def guardar_ultimo_id(registro_id):
    temporal = ESTADO + ".tmp"

    with open(temporal, "w") as archivo:
        archivo.write(str(registro_id))

    os.replace(temporal, ESTADO)


# =====================================================
# CONSULTAR MARIADB
# =====================================================

def consultar_registros(ultimo_id):
    conexion = pymysql.connect(**DB_CONFIG)

    try:
        with conexion.cursor() as cursor:
            cursor.execute("""
                SELECT id, registro, tiempo_ms, frecuencia_Hz,
                       caudal_L_min, litros_totales,
                       agua_detectada, bomba_on,
                       fuga_sospechada, recibido_en
                FROM registros
                WHERE id > %s
                ORDER BY id ASC
                LIMIT %s
            """, (ultimo_id, TAMANO_BLOQUE))

            return cursor.fetchall()

    finally:
        conexion.close()


# =====================================================
# ENVIAR DATOS A GOOGLE SHEETS
# =====================================================

def enviar_google(registros):
    datos = {
        "token": TOKEN,
        "registros": registros
    }

    contenido = json.dumps(datos).encode("utf-8")

    solicitud = urllib.request.Request(
        URL,
        data=contenido,
        headers={"Content-Type": "application/json"},
        method="POST"
    )

    with urllib.request.urlopen(
        solicitud,
        timeout=60
    ) as respuesta:

        resultado = json.loads(
            respuesta.read().decode("utf-8")
        )

    if not resultado.get("ok"):
        raise RuntimeError(
            "Google Sheets rechazo el bloque: " +
            str(resultado.get("error", "respuesta no valida"))
        )

    if resultado.get("recibidos") != len(registros):
        raise RuntimeError(
            "Cantidad de registros confirmada incorrecta"
        )


# =====================================================
# PROCESO DE SINCRONIZACION
# =====================================================

def sincronizar():
    ultimo_id = obtener_ultimo_id()
    filas = consultar_registros(ultimo_id)

    if not filas:
        return False

    registros = []

    for fila in filas:
        registros.append({
            "registro": fila["registro"],
            "tiempo_ms": fila["tiempo_ms"],
            "frecuencia_Hz": float(fila["frecuencia_Hz"]) if fila["frecuencia_Hz"] is not None else None,
            "caudal_L_min": float(fila["caudal_L_min"]) if fila["caudal_L_min"] is not None else None,
            "litros_totales": float(fila["litros_totales"]) if fila["litros_totales"] is not None else None,
            "agua_detectada": fila["agua_detectada"],
            "bomba_on": fila["bomba_on"],
            "fuga_sospechada": fila["fuga_sospechada"],
            "recibido_en": str(fila["recibido_en"]) if fila["recibido_en"] else None
        })

    enviar_google(registros)

    nuevo_id = filas[-1]["id"]
    guardar_ultimo_id(nuevo_id)

    logging.info(
        "Bloque enviado: %s registros. Ultimo ID MariaDB: %s",
        len(registros),
        nuevo_id
    )

    return True


# =====================================================
# PROGRAMA PRINCIPAL
# =====================================================

def main():
    validar_configuracion()

    logging.info(
        "Sincronizador Google Sheets iniciado."
    )

    while True:
        try:
            sincronizar()

        except Exception:
            logging.exception(
                "Error durante la sincronizacion."
            )

        time.sleep(INTERVALO)


if __name__ == "__main__":
    main()
